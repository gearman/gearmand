/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2013 Data Differential, http://datadifferential.com/
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following disclaimer
 *  in the documentation and/or other materials provided with the
 *  distribution.
 *
 *      * The names of its contributors may not be used to endorse or
 *  promote products derived from this software without specific prior
 *  written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "gear_config.h"
#include <libtest/test.hpp>

using namespace libtest;

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <libgearman/gearman.h>

#include <tests/basic.h>
#include <tests/context.h>

#include "libgearman/client.hpp"
#include "libgearman/worker.hpp"
using namespace org::gearmand;

#include "tests/workers/v2/called.h"

#define WORKER_FUNCTION "hiredis_queue_test"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

static in_port_t redis_port= 0;
static libtest::Server *redis_server_handle= NULL;
static libtest::Server *gearmand_server_handle= NULL;

static test_return_t gearmand_basic_option_test(void *)
{
  const char *args[]= { "--check-args",
    "--redis-server=localhost",
    "--redis-port=6379",
    0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t collection_init(void *object)
{
  Context *test= (Context *)object;
  assert(test);

  redis_port= libtest::get_free_port();
  ASSERT_TRUE(server_startup(test->_servers, "redis", redis_port, NULL));
  redis_server_handle= test->_servers.last();
  ASSERT_TRUE(redis_server_handle);

  char redis_port_string[1024];
  int length= snprintf(redis_port_string,
                       sizeof(redis_port_string),
                       "--redis-port=%d",
                       int(redis_port));
  ASSERT_TRUE(size_t(length) < sizeof(redis_port_string));

  const char *argv[]= {
    "--queue-type=redis",
    "--redis-server=localhost",
    redis_port_string,
    0 };

  ASSERT_TRUE(test->initialize(argv));
  gearmand_server_handle= test->_servers.last();
  ASSERT_TRUE(gearmand_server_handle);

  return TEST_SUCCESS;
}

static test_return_t collection_init_with_prefix(void *object)
{
  Context *test= (Context *)object;
  assert(test);

  redis_port= libtest::get_free_port();
  ASSERT_TRUE(server_startup(test->_servers, "redis", redis_port, NULL));

  char redis_port_string[1024];
  int length= snprintf(redis_port_string,
                       sizeof(redis_port_string),
                       "--redis-port=%d",
                       int(redis_port));
  ASSERT_TRUE(size_t(length) < sizeof(redis_port_string));

  const char *argv[]= {
    "--queue-type=redis",
    "--redis-server=localhost",
    redis_port_string,
    "--redis-prefix=_prefix_",
    0 };

  ASSERT_TRUE(test->initialize(argv));

  return TEST_SUCCESS;
}

static test_return_t collection_cleanup(void *object)
{
  Context *test= (Context *)object;
  test->reset();

  return TEST_SUCCESS;
}

// Regression test for #45: gearmand used to keep the same broken redis
// connection forever after an outage, requiring a restart to recover. This
// drives a real outage/recovery cycle against the redis server started in
// collection_init() and checks that job submission fails gracefully during
// the outage and succeeds again afterward, without restarting gearmand.
static test_return_t redis_reconnect_after_outage(void *object)
{
  Context *test= (Context *)object;
  ASSERT_TRUE(test);
  ASSERT_TRUE(redis_server_handle);

  libgearman::Client client(test->port());

  {
    gearman_job_handle_t job_handle= {};
    gearman_return_t rc= gearman_client_do_background(&client, test->worker_function_name(), NULL,
                                                       test_literal_param("before outage"), job_handle);
    ASSERT_EQ(rc, GEARMAN_SUCCESS);
    ASSERT_TRUE(job_handle[0]);
  }

  ASSERT_TRUE(redis_server_handle->kill());

  {
    gearman_job_handle_t job_handle= {};
    gearman_return_t rc= gearman_client_do_background(&client, test->worker_function_name(), NULL,
                                                       test_literal_param("during outage"), job_handle);
    ASSERT_EQ(rc, GEARMAN_QUEUE_ERROR);
  }

  // Clear the internal once-per-second reconnect rate limit (#45) before
  // retrying, and give redis-server time to finish starting back up.
  libtest::dream(2, 0);

  ASSERT_TRUE(redis_server_handle->start());
  ASSERT_TRUE(redis_server_handle->wait_for_pidfile());

  {
    gearman_job_handle_t job_handle= {};
    gearman_return_t rc= gearman_client_do_background(&client, test->worker_function_name(), NULL,
                                                       test_literal_param("after recovery"), job_handle);
    ASSERT_EQ(rc, GEARMAN_SUCCESS);
    ASSERT_TRUE(job_handle[0]);
  }

  return TEST_SUCCESS;
}

// Regression test for #159: gearmand encoded prefix-function_name-unique
// into the redis key and parsed it back apart on replay using '-' as the
// delimiter, so a function name containing a hyphen got truncated at the
// first one. Submits a job to a hyphenated function name with no worker
// running (so it persists to redis), restarts gearmand to force a replay,
// and confirms a worker registering the *full* function name receives it.
static test_return_t redis_replay_hyphenated_function_name(void *object)
{
  Context *test= (Context *)object;
  ASSERT_TRUE(test);
  ASSERT_TRUE(gearmand_server_handle);

  const char *function_name= "hyphenated-function-name";

  {
    libgearman::Client client(test->port());

    gearman_job_handle_t job_handle= {};
    gearman_return_t rc= gearman_client_do_background(&client, function_name, NULL,
                                                       test_literal_param("hyphen test payload"), job_handle);
    ASSERT_EQ(rc, GEARMAN_SUCCESS);
    ASSERT_TRUE(job_handle[0]);
  }

  // Server::start() isn't idempotent for gearmand: calling it again on the
  // same handle re-adds --verbose/--log-file/--pid-file on top of what's
  // already there, and gearmand's strict option parser rejects the
  // duplicates. Kill the old one and start a fresh Server via
  // initialize() (same config collection_init() used) instead of trying to
  // restart the existing handle in place.
  ASSERT_TRUE(gearmand_server_handle->kill());

  char redis_port_string[1024];
  int length= snprintf(redis_port_string,
                       sizeof(redis_port_string),
                       "--redis-port=%d",
                       int(redis_port));
  ASSERT_TRUE(size_t(length) < sizeof(redis_port_string));

  const char *argv[]= {
    "--queue-type=redis",
    "--redis-server=localhost",
    redis_port_string,
    0 };

  ASSERT_TRUE(test->initialize(argv));
  gearmand_server_handle= test->_servers.last();
  ASSERT_TRUE(gearmand_server_handle);

  libgearman::Worker worker(test->port());
  gearman_worker_set_timeout(&worker, 3000);

  Called called;
  gearman_function_t function= gearman_function_create(called_worker);
  ASSERT_EQ(gearman_worker_define_function(&worker,
                                           function_name, strlen(function_name),
                                           function,
                                           5, &called), GEARMAN_SUCCESS);

  ASSERT_EQ(gearman_worker_work(&worker), GEARMAN_SUCCESS);
  ASSERT_TRUE(called.count());

  return TEST_SUCCESS;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
static bool test_for_HAVE_HIREDIS()
{
  if (HAVE_HIREDIS)
  {
    return true;
  }

  return false;
}
#pragma GCC diagnostic pop

static void *world_create(server_startup_st& servers, test_return_t&)
{
  SKIP_IF(test_for_HAVE_HIREDIS() == false);
  SKIP_IF(has_redis() == false);
  return new Context(servers);
}

static bool world_destroy(void *object)
{
  SKIP_IF(HAVE_UUID_UUID_H != 1);

  Context *test= (Context *)object;

  delete test;

  return TEST_SUCCESS;
}

test_st gearmand_basic_option_tests[] ={
  {"all options", 0, gearmand_basic_option_test },
  {0, 0, 0}
};

test_st tests[] ={
  {"gearman_client_echo()", 0, client_echo_test },
  {"gearman_client_echo() fail", 0, client_echo_fail_test },
  {"gearman_worker_echo()", 0, worker_echo_test },
  {"clean", 0, queue_clean },
  {"add", 0, queue_add },
  {"worker", 0, queue_worker },
  {0, 0, 0}
};

test_st regressions[] ={
  {"lp:734663", 0, lp_734663 },
  {"#45 reconnect after redis outage", 0, redis_reconnect_after_outage },
  {"#159 replay hyphenated function name", 0, redis_replay_hyphenated_function_name },
  {0, 0, 0}
};

collection_st collection[] ={
  {"gearmand redis options", 0, 0, gearmand_basic_option_tests},
  {"redis queue", collection_init, collection_cleanup, tests},
  {"redis queue with prefix", collection_init_with_prefix, collection_cleanup, tests},
  {"regressions", collection_init, collection_cleanup, regressions},
  {0, 0, 0, 0}
};

void get_world(libtest::Framework *world)
{
  world->collections(collection);
  world->create(world_create);
  world->destroy(world_destroy);
}
