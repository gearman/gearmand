/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2013 Keyur Govande
 *  Copyright (C) 2011-2012 Data Differential, http://datadifferential.com/
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

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <vector>

#define GEARMAN_CORE
#include <libgearman-1.0/gearman.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include "tests/runner.h"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include "tests/libgearman-1.0/multi_client_test.h"

#define SECOND_SERVER_PORT_OFFSET 2

static const char *server_argv[]= { "--exceptions", 0 };

static test_return_t multi_client_test(void *object)
{
  multi_client_test_st *test_client= (multi_client_test_st*)object;
  ASSERT_TRUE(test_client);

  server_startup_st& server_container= test_client->server_container();

  // make a client that connects to both servers.
  gearman_client_st *client_to_both= test_client->connected_to_both_client();
  ASSERT_TRUE(client_to_both);

  (void)gearman_client_set_context(client_to_both, const_cast<char *>("nothing"));
  gearman_string_t value= { test_literal_param("background_test") };

  const char *worker_function= (const char *)gearman_client_context(client_to_both);
  ASSERT_TRUE(worker_function);

  // queue up first job
  const char* unique_1= "unique_1";
  gearman_job_handle_t job_handle;
  test_compare(GEARMAN_SUCCESS,
               gearman_client_do_background(client_to_both, worker_function, unique_1, gearman_string_param(value), job_handle));

  {
    gearman_return_t ret;
    bool is_known;

    test_compare(GEARMAN_SUCCESS,
                 (ret= gearman_client_job_status(client_to_both, job_handle, &is_known, NULL, NULL, NULL)));
    ASSERT_TRUE(is_known);
  }

  // all OK. now shut down the gearmand server 1
  in_port_t gearmand_port_1= test_client->port(0);
  in_port_t gearmand_port_2= test_client->port(1);
  ASSERT_TRUE(server_container.shutdown(0));

  // Server 1 is restarted on this same port further down, but not until
  // after several job-status round trips below, so hold the port reserved
  // for the whole gap -- otherwise a concurrently-running test process's
  // get_free_port() could grab it first and the restart's bind() would fail.
  libtest::reserve_port(gearmand_port_1);

  // Try to queue up a job again
  const char* unique_2= "unique_2";
  test_compare(GEARMAN_LOST_CONNECTION,
               gearman_client_do_background(client_to_both, worker_function, unique_2, gearman_string_param(value), job_handle));

  const char* unique_3= "unique_3";
  test_compare(GEARMAN_SUCCESS, 
               gearman_client_do_background(client_to_both, worker_function, unique_3, gearman_string_param(value), job_handle));

  // The job unique_2 is truly lost, but unique_3 will end up at the gearmand server 2
  gearman_client_st *client_to_1= test_client->connected_to_1_client();
  gearman_client_st *client_to_2= test_client->connected_to_2_client();

  // First confirming that cannot connect to server 1
  {
    gearman_status_t unique_3_status= gearman_client_unique_status(client_to_1, unique_3, strlen(unique_3));
    test_compare(GEARMAN_COULD_NOT_CONNECT,
                 gearman_status_return(unique_3_status));
  }

  // Next, proving that the job is present on server 2.
  {
    gearman_status_t unique_3_status= gearman_client_unique_status(client_to_2, unique_3, strlen(unique_3));
    test_compare(GEARMAN_SUCCESS,
                 gearman_status_return(unique_3_status));
    ASSERT_TRUE(gearman_status_is_known(unique_3_status));
  }

  // Bring server 1 back up
  server_container.shutdown();
  // Server 2 was just killed too; reserve its port the same way for the
  // same reason as gearmand_port_1 above. gearmand_port_1's reservation is
  // still held from further up.
  libtest::reserve_port(gearmand_port_2);
  ASSERT_TRUE(server_startup(server_container, "gearmand", gearmand_port_1, server_argv));
  ASSERT_TRUE(server_startup(server_container, "gearmand", gearmand_port_2, server_argv));

  // Try adding in a new job.
  const char* unique_4= "unique_4";
  test_compare(GEARMAN_SUCCESS,
               gearman_client_do_background(client_to_both, worker_function, unique_4, gearman_string_param(value), job_handle));

  // Next, proving that the job is present on server 1, but we got a timeout instead!
  {
    gearman_status_t unique_4_status= gearman_client_unique_status(client_to_both, unique_4, strlen(unique_4));
    test_compare(GEARMAN_SUCCESS,
                 gearman_status_return(unique_4_status));
    ASSERT_TRUE(gearman_status_is_known(unique_4_status));
  }

  return TEST_SUCCESS;
}

static void *world_create(server_startup_st& servers, test_return_t&)
{
  multi_client_test_st *test= new multi_client_test_st(servers, 1000); // setting a default timeout
  ASSERT_TRUE(test);

  test->push_port(libtest::get_free_port());
  test->push_port(libtest::get_free_port());

  ASSERT_TRUE(server_startup(servers, "gearmand", test->port(0), server_argv));

  ASSERT_TRUE(server_startup(servers, "gearmand", test->port(1), server_argv));

  test->add_server("127.0.0.1", test->port(0),
                   "127.0.0.1", test->port(1));

  return (void *)test;
}

static bool world_destroy(void *object)
{
  multi_client_test_st *test= (multi_client_test_st *)object;
  delete test;

  return TEST_SUCCESS;
}

test_st multi_client_TESTS[] ={
  {"multi_client_test", 0, multi_client_test },
  {0, 0, 0}
};

collection_st collection[] ={
  {"multi_client", 0, 0, multi_client_TESTS},
  {0, 0, 0, 0}
};

void get_world(libtest::Framework *world)
{
  world->collections(collection);
  world->create(world_create);
  world->destroy(world_destroy);
}
