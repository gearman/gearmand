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
#include <string>
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

// Issue #64: gearman_client_set_server_selection_by_unique() opts a client
// into routing a job to a server picked by hashing its unique, instead of
// the default "first idle connection in add-server order". Confirm against
// real servers that it actually does so:
//
// - Each unique is queued on exactly one of the two servers, checked by
//   asking each server directly via GET_STATUS_UNIQUE.
// - Both servers receive some of the jobs. The default selection would put
//   every job on the same server, so this is what fails if hashing is not
//   in effect. The ring is keyed on host:port and the ports are random per
//   run, so no single unique can be relied on to map to a given server;
//   with kUniqueCount uniques the chance of all of them hashing to one
//   server is negligible.
// - A second, independent client with the same server list and the option
//   enabled lands every unique on the same server as the first, which the
//   server proves by coalescing the resubmission into the queued job and
//   returning the identical job_handle.
//
// The per-server clients are always fresh clones: an earlier test in this
// collection (multi_client_test) drives connected_to_1_client() into
// GEARMAN_COULD_NOT_CONNECT while its server is down, and reusing that
// cached clone against the restarted server can hang rather than reconnect.
static test_return_t server_selection_by_unique_test(void *object)
{
  multi_client_test_st *test_client= (multi_client_test_st*)object;
  ASSERT_TRUE(test_client);

  static const size_t kUniqueCount= 32;
  gearman_string_t value= { test_literal_param("background_test") };
  const char *worker_function= "nothing";

  std::vector<std::string> uniques;
  for (size_t x= 0; x < kUniqueCount; ++x)
  {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "hash_selection_routing_unique_%u", unsigned(x));
    uniques.push_back(buffer);
  }

  test_client->reset_connected_to_both_clone();
  gearman_client_st *client_to_both= test_client->connected_to_both_client();
  ASSERT_TRUE(client_to_both);
  gearman_client_set_server_selection_by_unique(client_to_both, true);
  ASSERT_TRUE(gearman_client_server_selection_by_unique(client_to_both));

  std::vector<std::string> job_handles;
  for (size_t x= 0; x < kUniqueCount; ++x)
  {
    gearman_job_handle_t job_handle;
    test_compare(GEARMAN_SUCCESS,
                 gearman_client_do_background(client_to_both, worker_function, uniques[x].c_str(), gearman_string_param(value), job_handle));
    job_handles.push_back(job_handle);
  }

  test_client->reset_connected_to_1_clone();
  test_client->reset_connected_to_2_clone();
  gearman_client_st *client_to_1= test_client->connected_to_1_client();
  gearman_client_st *client_to_2= test_client->connected_to_2_client();
  ASSERT_TRUE(client_to_1);
  ASSERT_TRUE(client_to_2);

  size_t on_server_1= 0;
  size_t on_server_2= 0;
  for (size_t x= 0; x < kUniqueCount; ++x)
  {
    gearman_status_t status_1= gearman_client_unique_status(client_to_1, uniques[x].c_str(), uniques[x].size());
    test_compare(GEARMAN_SUCCESS, gearman_status_return(status_1));
    gearman_status_t status_2= gearman_client_unique_status(client_to_2, uniques[x].c_str(), uniques[x].size());
    test_compare(GEARMAN_SUCCESS, gearman_status_return(status_2));

    bool known_1= gearman_status_is_known(status_1);
    bool known_2= gearman_status_is_known(status_2);
    ASSERT_TRUE_(known_1 != known_2, "%s queued on %s", uniques[x].c_str(),
                 known_1 ? "both servers" : "neither server");

    if (known_1)
    {
      ++on_server_1;
    }
    else
    {
      ++on_server_2;
    }
  }

  ASSERT_TRUE_(on_server_1 > 0 and on_server_2 > 0,
               "all %u jobs landed on one server (server 1: %u, server 2: %u)",
               unsigned(kUniqueCount), unsigned(on_server_1), unsigned(on_server_2));

  test_client->reset_connected_to_both_clone();
  gearman_client_st *client_to_both_again= test_client->connected_to_both_client();
  ASSERT_TRUE(client_to_both_again);
  gearman_client_set_server_selection_by_unique(client_to_both_again, true);

  for (size_t x= 0; x < kUniqueCount; ++x)
  {
    gearman_job_handle_t job_handle;
    test_compare(GEARMAN_SUCCESS,
                 gearman_client_do_background(client_to_both_again, worker_function, uniques[x].c_str(), gearman_string_param(value), job_handle));
    ASSERT_STREQ(job_handles[x].c_str(), job_handle);
  }

  return TEST_SUCCESS;
}

// With hash-based selection enabled, a task submitted with an empty unique
// (unique_length == 0, e.g. a caller that passes "" rather than a real
// unique and isn't using GEARMAN_CLIENT_GENERATE_UNIQUE) must fall back to
// the default head-of-list connection selection instead of hashing zero
// bytes. This confirms the unique_length > 0 gate in
// use_hash_server_selection() (run.cc) actually takes effect end-to-end,
// not just that submission with a real unique works.
static test_return_t server_selection_by_unique_empty_unique_test(void *object)
{
  multi_client_test_st *test_client= (multi_client_test_st*)object;
  ASSERT_TRUE(test_client);

  test_client->reset_connected_to_both_clone();
  gearman_client_st *client_to_both= test_client->connected_to_both_client();
  ASSERT_TRUE(client_to_both);
  gearman_client_set_server_selection_by_unique(client_to_both, true);

  (void)gearman_client_set_context(client_to_both, const_cast<char *>("nothing"));
  gearman_string_t value= { test_literal_param("background_test") };
  const char *worker_function= (const char *)gearman_client_context(client_to_both);
  ASSERT_TRUE(worker_function);

  gearman_job_handle_t job_handle;
  test_compare(GEARMAN_SUCCESS,
               gearman_client_do_background(client_to_both, worker_function, "", gearman_string_param(value), job_handle));

  return TEST_SUCCESS;
}

// With hash-based selection enabled, submitting while the hash-picked
// server happens to be down must still succeed by failing over, in ring
// order, to the one surviving server -- exercising the GEARMAN_COULD_NOT_CONNECT
// retry path in _client_run_task() (run.cc) for the hash-selection case.
static test_return_t server_selection_by_unique_failover_test(void *object)
{
  multi_client_test_st *test_client= (multi_client_test_st*)object;
  ASSERT_TRUE(test_client);

  server_startup_st& server_container= test_client->server_container();

  test_client->reset_connected_to_both_clone();
  gearman_client_st *client_to_both= test_client->connected_to_both_client();
  ASSERT_TRUE(client_to_both);
  gearman_client_set_server_selection_by_unique(client_to_both, true);

  (void)gearman_client_set_context(client_to_both, const_cast<char *>("nothing"));
  gearman_string_t value= { test_literal_param("background_test") };
  const char *worker_function= (const char *)gearman_client_context(client_to_both);
  ASSERT_TRUE(worker_function);

  in_port_t gearmand_port_1= test_client->port(0);
  in_port_t gearmand_port_2= test_client->port(1);

  // server_startup_st never reuses or reindexes slots: shutdown()+
  // server_startup() (as multi_client_test() above already does once) always
  // *appends* the restarted server rather than replacing its old, now-dead
  // entry. So the two currently-live servers are always the *last* two
  // entries pushed, not indices 0/1 -- shutting down a stale, already-dead
  // index here would silently no-op and leave the real server 1 running,
  // masking the fail-over path entirely.
  uint32_t server_1_index= server_container.count() - 2;

  ASSERT_TRUE(server_container.shutdown(server_1_index));
  libtest::reserve_port(gearmand_port_1);

  const char* unique= "hash_selection_failover_unique";
  gearman_job_handle_t job_handle;

  // Right after shutdown there is an inherent race -- also relied on by
  // multi_client_test() above, which expects its own first post-shutdown
  // submission to come back GEARMAN_LOST_CONNECTION rather than a clean
  // failure -- where the very first send can land in the dying server's
  // kernel accept backlog and get a misleading GEARMAN_SUCCESS or
  // GEARMAN_LOST_CONNECTION before the process is actually gone, instead of
  // the GEARMAN_COULD_NOT_CONNECT that triggers the fail-over path being
  // tested here. Submit the same unique once first to flush that race out;
  // only the submission after it is guaranteed to see the server as truly
  // down.
  {
    gearman_return_t warmup_ret= gearman_client_do_background(client_to_both, worker_function, unique, gearman_string_param(value), job_handle);
    ASSERT_TRUE(warmup_ret == GEARMAN_SUCCESS or warmup_ret == GEARMAN_LOST_CONNECTION or warmup_ret == GEARMAN_COULD_NOT_CONNECT);
  }

  test_compare(GEARMAN_SUCCESS,
               gearman_client_do_background(client_to_both, worker_function, unique, gearman_string_param(value), job_handle));

  // client_to_2's connection was never driven into a failure state (server 2
  // stayed up throughout), so a fresh clone here is just defensive, not a
  // workaround for anything broken.
  test_client->reset_connected_to_2_clone();
  gearman_client_st *client_to_2= test_client->connected_to_2_client();
  {
    gearman_status_t status= gearman_client_unique_status(client_to_2, unique, strlen(unique));
    test_compare(GEARMAN_SUCCESS, gearman_status_return(status));
    ASSERT_TRUE(gearman_status_is_known(status));
  }

  // Bring both servers back up so later tests see them running.
  server_container.shutdown();
  libtest::reserve_port(gearmand_port_2);
  ASSERT_TRUE(server_startup(server_container, "gearmand", gearmand_port_1, server_argv));
  ASSERT_TRUE(server_startup(server_container, "gearmand", gearmand_port_2, server_argv));

  return TEST_SUCCESS;
}

static void *stale_packet_worker(gearman_job_st*, void*, size_t *result_size, gearman_return_t *ret_ptr)
{
  *result_size= 0;
  *ret_ptr= GEARMAN_SUCCESS;

  return NULL;
}

// A blocking call (gearman_client_do_background() here) has to return once
// its reply has arrived, whatever shows up on the client's other connection.
// _client_run_tasks() used to read a ready connection with a blocking
// universal, so after a packet that belongs to no task it waited for one
// more packet on that same connection while the JOB_CREATED it was after sat
// unread on the other one. With the 1000 ms timeout of this collection that
// showed up as GEARMAN_TIMEOUT; without a timeout the call never returned.
//
// The packet that belongs to no task is the WORK_COMPLETE of a foreground
// job whose task the client gave up before a worker picked the job up.
static test_return_t blocking_call_stale_packet_on_other_connection_test(void *object)
{
  multi_client_test_st *test_client= (multi_client_test_st*)object;
  ASSERT_TRUE(test_client);

  test_client->reset_connected_to_both_clone();
  gearman_client_st *client_to_both= test_client->connected_to_both_client();
  ASSERT_TRUE(client_to_both);

  const char *worker_function= "nothing";
  const char *stale_function= "stale_packet_on_other_connection";
  const char *large_unique= "stale_packet_large_unique";
  const char *stale_unique= "stale_packet_stale_unique";
  gearman_string_t value= { test_literal_param("background_test") };

  // The first workload does not fit the send buffer, which keeps the
  // connection it is sent on (the head of the list) busy while the next task
  // is started: that one goes to the other server.
  std::vector<char> large_workload(2 * 1024 * 1024, 'x');

  gearman_return_t ret;
  ASSERT_TRUE(gearman_client_add_task_background(client_to_both, NULL, NULL, worker_function, large_unique,
                                                 &large_workload[0], large_workload.size(), &ret));
  test_compare(GEARMAN_SUCCESS, ret);
  ASSERT_TRUE(gearman_client_add_task(client_to_both, NULL, NULL, stale_function, stale_unique,
                                      gearman_string_param(value), &ret));
  test_compare(GEARMAN_SUCCESS, ret);

  // Nobody works on stale_function yet, the client gives the batch up.
  test_compare(GEARMAN_TIMEOUT, gearman_client_run_tasks(client_to_both));
  gearman_client_task_free_all(client_to_both);

  // Confirm that the two jobs are on different servers, and find the one
  // that has the abandoned job.
  test_client->reset_connected_to_1_clone();
  gearman_client_st *client_to_1= test_client->connected_to_1_client();
  ASSERT_TRUE(client_to_1);
  gearman_status_t large_status= gearman_client_unique_status(client_to_1, large_unique, strlen(large_unique));
  test_compare(GEARMAN_SUCCESS, gearman_status_return(large_status));
  gearman_status_t stale_status= gearman_client_unique_status(client_to_1, stale_unique, strlen(stale_unique));
  test_compare(GEARMAN_SUCCESS, gearman_status_return(stale_status));
  ASSERT_TRUE_(gearman_status_is_known(large_status) != gearman_status_is_known(stale_status),
               "both tasks were sent to the %s server", gearman_status_is_known(stale_status) ? "first" : "second");
  in_port_t stale_port= gearman_status_is_known(stale_status) ? test_client->port(0) : test_client->port(1);

  // Complete the abandoned job.
  gearman_worker_st *worker= gearman_worker_create(NULL);
  ASSERT_TRUE(worker);
  gearman_worker_set_timeout(worker, 1000);
  test_compare(GEARMAN_SUCCESS, gearman_worker_add_server(worker, "127.0.0.1", stale_port));
  test_compare(GEARMAN_SUCCESS, gearman_worker_add_function(worker, stale_function, 0, stale_packet_worker, NULL));
  test_compare(GEARMAN_SUCCESS, gearman_worker_work(worker));
  // Answered after the server has passed the WORK_COMPLETE on to the client.
  test_compare(GEARMAN_SUCCESS, gearman_worker_echo(worker, test_literal_param("sync")));
  gearman_worker_free(worker);

  // These are submitted on the connection that is not the one with the stale
  // packet. More than one, since the JOB_CREATED of the first can still win
  // the race against the stale packet.
  for (size_t x= 0; x < 3; ++x)
  {
    gearman_job_handle_t job_handle;
    test_compare(GEARMAN_SUCCESS,
                 gearman_client_do_background(client_to_both, worker_function, NULL, gearman_string_param(value), job_handle));
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
  {"server_selection_by_unique routes consistently", 0, server_selection_by_unique_test },
  {"server_selection_by_unique falls back for an empty unique", 0, server_selection_by_unique_empty_unique_test },
  {"server_selection_by_unique fails over when primary is down", 0, server_selection_by_unique_failover_test },
  {"blocking call with a stale packet on the other connection", 0, blocking_call_stale_packet_on_other_connection_test },
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
