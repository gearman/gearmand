/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
 *  Copyright (C) 2008 Brian Aker, Eric Day
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include <libgearman/gearman.h>

#include <tests/basic.h>
#include <tests/context.h>

#include "libgearman/client.hpp"
using namespace org::gearmand;

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

static in_port_t mysqld_port= 0;

static test_return_t gearmand_basic_option_test(void *)
{
  const char *args[]= { "--check-args",
    "--queue-type=MySQL",
    "--mysql-host=localhost",
    "--mysql-user=mysql",
    "--mysql-password=mysql",
    "--mysql-db=gearman",
    "--mysql-table=gearman",
    0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));

  return TEST_SUCCESS;
}

static test_return_t collection_init(void *object)
{
  Context *test= (Context *)object;
  fatal_assert(test);

  mysqld_port= libtest::get_free_port();
  ASSERT_TRUE(server_startup(test->_servers, "mysqld", mysqld_port, NULL));

  ASSERT_TRUE(mysqld_create_database(mysqld_port, "gearman"));

  char mysql_port_string[1024];
  int length= snprintf(mysql_port_string,
                       sizeof(mysql_port_string),
                       "--mysql-port=%d",
                       int(mysqld_port));
  ASSERT_TRUE(size_t(length) < sizeof(mysql_port_string));

  const char *argv[]= {
    "--queue-type=MySQL",
    "--mysql-host=127.0.0.1",
    mysql_port_string,
    "--mysql-user=root",
    "--mysql-db=gearman",
    "--mysql-table=gearman",
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

// Regression test for #126: _mysql_queue_add() retried a failed INSERT
// against CR_SERVER_LOST unconditionally and unboundedly. MySQL closes the
// connection outright (rather than rejecting just that one query) when a
// query exceeds max_allowed_packet, so a job whose data was too large made
// the add path retry the exact same oversized query forever instead of
// failing cleanly. collection_init() starts mysqld with a small
// max_allowed_packet (see MySQLd::build() in libtest/mysqld.cc) so a
// multi-megabyte payload reliably triggers that path.
static test_return_t mysql_oversized_packet_does_not_hang(void *object)
{
  Context *test= (Context *)object;
  ASSERT_TRUE(test);

  libgearman::Client client(test->port());

  std::string big_payload(2 * 1024 * 1024, 'x');

  {
    gearman_job_handle_t job_handle= {};
    gearman_return_t rc= gearman_client_do_background(&client, "oversized_test", NULL,
                                                       big_payload.data(), big_payload.size(), job_handle);
    ASSERT_EQ(rc, GEARMAN_QUEUE_ERROR);
  }

  // Confirm the connection (and mysqld) is still healthy afterward -- a
  // normal-sized job should still persist and succeed.
  {
    gearman_job_handle_t job_handle= {};
    gearman_return_t rc= gearman_client_do_background(&client, "oversized_test", NULL,
                                                       test_literal_param("small payload"), job_handle);
    ASSERT_EQ(rc, GEARMAN_SUCCESS);
    ASSERT_TRUE(job_handle[0]);
  }

  return TEST_SUCCESS;
}

static void *world_create(server_startup_st& servers, test_return_t&)
{
  SKIP_IF(HAVE_UUID_UUID_H != 1);
  SKIP_IF(has_mysqld() == false);

  return new Context(servers);
}

static bool world_destroy(void *object)
{
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
  {"#126 oversized packet does not hang", 0, mysql_oversized_packet_does_not_hang },
  {0, 0, 0}
};

collection_st collection[] ={
  {"gearmand options", 0, 0, gearmand_basic_option_tests},
  {"mysql queue", collection_init, collection_cleanup, tests},
  {"regressions", collection_init, collection_cleanup, regressions},
  {0, 0, 0, 0}
};

void get_world(libtest::Framework *world)
{
  world->collections(collection);
  world->create(world_create);
  world->destroy(world_destroy);
}
