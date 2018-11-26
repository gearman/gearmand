/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Test Libmemcached, memcached for Gearman
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
 *  Copyright (C) 2008 Brian Aker, Eric Day
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

#if defined(HAVE_LIBMEMCACHED) && HAVE_LIBMEMCACHED
#  include <libmemcached-1.0/memcached.h>
#  if defined(HAVE_LIBMEMCACHED_UTIL_H) && HAVE_LIBMEMCACHED_UTIL_H
#    include <libmemcachedutil-1.0/util.h>
#  endif
#endif

#include <libgearman/gearman.h>

#include <tests/basic.h>
#include <tests/context.h>

#include "libgearman/client.hpp"
#include "libgearman/worker.hpp"
using namespace org::gearmand;

#include "tests/workers/v2/called.h"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

static in_port_t memcached_port= 0;

static test_return_t gearmand_basic_option_test(void *)
{
  const char *args[]= {
    "--check-args",
    "--queue-type=libmemcached",
    "--libmemcached-servers=localhost:1024",
    0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));

  return TEST_SUCCESS;
}

static test_return_t lp_1054377_TEST(void *object)
{
  Context *test= (Context *)object;
  ASSERT_TRUE(test);
  server_startup_st &servers= test->_servers;

  memcached_port= libtest::get_free_port();
  ASSERT_TRUE(server_startup(servers, "memcached", memcached_port, NULL));

  char memcached_server_string[1024];
  int length= snprintf(memcached_server_string, 
                       sizeof(memcached_server_string),
                       "--libmemcached-servers=localhost:%d",
                       int(memcached_port));
  ASSERT_TRUE(size_t(length) < sizeof(memcached_server_string));

#if defined(HAVE_LIBMEMCACHED) && HAVE_LIBMEMCACHED
# if defined(HAVE_LIBMEMCACHED_UTIL_H) && HAVE_LIBMEMCACHED_UTIL_H
  ASSERT_TRUE(libmemcached_util_ping("localhost", memcached_port, NULL));
# endif
#endif

  const char *argv[]= {
    "--queue-type=libmemcached",
    memcached_server_string,
    0 };


  const int32_t inserted_jobs= 3;
  {
    in_port_t first_port= libtest::get_free_port();

    ASSERT_TRUE(server_startup(servers, "gearmand", first_port, argv));
#if 0
    libtest::Server* server= servers.pop_server();
#endif

    {
      libgearman::Worker worker(first_port);
      ASSERT_EQ(gearman_worker_register(&worker, __func__, 0), GEARMAN_SUCCESS);
    }

    {
      libgearman::Client client(first_port);
      ASSERT_EQ(gearman_client_echo(&client, test_literal_param("This is my echo test")), GEARMAN_SUCCESS);
      gearman_job_handle_t job_handle;
      for (int32_t x= 0; x < inserted_jobs; ++x)
      {
        ASSERT_EQ(gearman_client_do_background(&client,
                                                  __func__, // func
                                                  NULL, // unique
                                                  test_literal_param("foo"),
                                                  job_handle), GEARMAN_SUCCESS);
      }
    }

#if 0
    delete server;
#endif
  }

  {
    in_port_t first_port= libtest::get_free_port();

    ASSERT_TRUE(server_startup(servers, "gearmand", first_port, argv));

    {
      libgearman::Worker worker(first_port);
      Called called;
      gearman_function_t counter_function= gearman_function_create(called_worker);
      ASSERT_EQ(gearman_worker_define_function(&worker,
                                                  test_literal_param(__func__),
                                                  counter_function,
                                                  3000, &called), GEARMAN_SUCCESS);

      const int32_t max_timeout= 4;
      int32_t max_timeout_value= max_timeout;
      int32_t job_count= 0;
      gearman_return_t ret;
      do
      {
        ret= gearman_worker_work(&worker);
        if (gearman_success(ret))
        {
          job_count++;
          max_timeout_value= max_timeout;
          if (job_count == inserted_jobs)
          {
            break;
          }
        }
        else if (ret == GEARMAN_TIMEOUT)
        {
          if ((--max_timeout_value) < 0)
          {
            break;
          }
        }
      } while (ret == GEARMAN_TIMEOUT or ret == GEARMAN_SUCCESS);

      ASSERT_EQ(called.count(), inserted_jobs);
    }
  }

  return TEST_SUCCESS;
}

static test_return_t collection_init(void *object)
{
  Context *test= (Context *)object;
  assert(test);

  memcached_port= libtest::get_free_port();
  ASSERT_TRUE(server_startup(test->_servers, "memcached", memcached_port, NULL));

  char memcached_server_string[1024];
  int length= snprintf(memcached_server_string, 
                       sizeof(memcached_server_string),
                       "--libmemcached-servers=localhost:%d",
                       int(memcached_port));
  ASSERT_TRUE(size_t(length) < sizeof(memcached_server_string));
  const char *argv[]= {
    memcached_server_string,
    "--queue-type=libmemcached",
    0 };

  ASSERT_TRUE(test->initialize(argv));

  return TEST_SUCCESS;
}

static test_return_t collection_init_with_prefix(void *object)
{
  Context *test= (Context *)object;
  assert(test);

  memcached_port= libtest::get_free_port();
  ASSERT_TRUE(server_startup(test->_servers, "memcached", memcached_port, NULL));

  char memcached_server_string[1024];
  int length= snprintf(memcached_server_string,
                       sizeof(memcached_server_string),
                       "--libmemcached-servers=localhost:%d --libmemcached-prefix=prefix_",
                       int(memcached_port));
  ASSERT_TRUE(size_t(length) < sizeof(memcached_server_string));
  const char *argv[]= {
    memcached_server_string,
    "--queue-type=libmemcached",
    0 };

  ASSERT_TRUE(test->initialize(argv));

  return TEST_SUCCESS;
}

static test_return_t collection_cleanup(void *object)
{
  Context *test= (Context *)object;
  test->reset();

  memcached_port= 0;

  return TEST_SUCCESS;
}


static void *world_create(server_startup_st& servers, test_return_t& error)
{
  if (has_memcached() == false or has_libmemcached() == false)
  {
    error= TEST_SKIPPED;
    return NULL;
  }

  return new Context(servers);
}

static bool world_destroy(void *object)
{
  Context *test= (Context *)object;

  delete test;

  return TEST_SUCCESS;
}

test_st gearmand_basic_option_tests[] ={
  {"--queue-type=libmemcached --libmemcached-servers=", 0, gearmand_basic_option_test },
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

test_st queue_restart_TESTS[] ={
  {"lp:1054377", 0, lp_1054377_TEST },
  {0, 0, 0}
};


collection_st collection[] ={
  {"gearmand options", 0, 0, gearmand_basic_option_tests},
  {"memcached queue", collection_init, collection_cleanup, tests},
  {"memcached queue with prefix", collection_init_with_prefix, collection_cleanup, tests},
  {"queue restart", 0, collection_cleanup, queue_restart_TESTS},
  {0, 0, 0, 0}
};

void get_world(libtest::Framework *world)
{
  world->collections(collection);
  world->create(world_create);
  world->destroy(world_destroy);
}
