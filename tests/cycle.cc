/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Cycle the Gearmand server
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
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


/*
  Test that we are cycling the servers we are creating during testing.
*/

#include "gear_config.h"
#include <libtest/test.hpp>

using namespace libtest;
#include <libgearman/gearman.h>

#include "tests/start_worker.h"

struct cycle_context_st 
{
  cycle_context_st(libtest::server_startup_st& arg) :
    servers(arg),
    port(0)
  {
    reset();
  }

  void reset()
  {
    servers.clear();
    port= get_free_port();
  }

  server_startup_st& servers;
  in_port_t port;
};

static gearman_return_t success_fn(gearman_job_st*, void* /* context */)
{
  return GEARMAN_SUCCESS;
}

static test_return_t single_cycle(void* object)
{
  cycle_context_st *context= (cycle_context_st*)object;

  gearman_function_t success_function= gearman_function_create(success_fn);
  worker_handle_st *worker= test_worker_start(context->port, NULL, "success", success_function, NULL, gearman_worker_options_t());
  ASSERT_TRUE(worker);
  ASSERT_TRUE(worker->shutdown());
  delete worker;

  return TEST_SUCCESS;
}

static test_return_t kill_test(void *)
{
  libtest::dream(2, 0);

  return TEST_SUCCESS;
}

static test_return_t __server_startup_TEST(cycle_context_st* context, const int count)
{
  for (int x= 0; x < count; ++x)
  {
    test_skip(true, server_startup(context->servers, "gearmand", libtest::get_free_port(), NULL));
  }
  ASSERT_EQ(true, context->servers.shutdown());

  return TEST_SUCCESS;
}

static test_return_t server_startup_single_TEST(void *obj)
{
  ASSERT_EQ(__server_startup_TEST((cycle_context_st*)obj, 1), TEST_SUCCESS);
  return TEST_SUCCESS;
}

static test_return_t server_startup_multiple_TEST(void *obj)
{
  ASSERT_EQ(__server_startup_TEST((cycle_context_st*)obj, 20), TEST_SUCCESS);
  return TEST_SUCCESS;
}

// We can't really test for this just yet, because we don't know if the server
// we attach to is really the one we expect to attach too.
static test_return_t server_startup_conflict_TEST(void*)
{
#if 0
  cycle_context_st *context= (cycle_context_st*)object;

  in_port_t bind_port= libtest::get_free_port();
  ASSERT_EQ(true, server_startup(context->servers, "gearmand", bind_port, NULL, false));
  ASSERT_EQ(false, server_startup(context->servers, "gearmand", bind_port, NULL, false));
#endif

  return TEST_SUCCESS;
}

static test_return_t shutdown_and_remove_TEST(void *obj)
{
  cycle_context_st *context= (cycle_context_st*)obj;
  context->servers.clear();

  return TEST_SUCCESS;
}

test_st server_startup_TESTS[] ={
  {"server_startup(1)", false, (test_callback_fn*)server_startup_single_TEST },
  {"server_startup(many)", false, (test_callback_fn*)server_startup_multiple_TEST },
  {"shutdown_and_remove()", false, (test_callback_fn*)shutdown_and_remove_TEST },
  {"server_startup(many)", false, (test_callback_fn*)server_startup_multiple_TEST },
  {"server_startup() with bind() conflict", false, (test_callback_fn*)server_startup_conflict_TEST },
  {0, 0, 0}
};

test_st kill_tests[] ={
  {"kill", true, (test_callback_fn*)kill_test },
  {0, 0, 0}
};

test_st worker_tests[] ={
  {"single startup/shutdown", true, (test_callback_fn*)single_cycle },
  {0, 0, 0}
};

static test_return_t collection_INIT(void *object)
{
  cycle_context_st *context= (cycle_context_st*)object;
  test_zero(context->servers.count());
  context->reset();

  test_skip(true, server_startup(context->servers, "gearmand", context->port, NULL));

  return TEST_SUCCESS;
}

static test_return_t validate_sanity_INIT(void *object)
{
  cycle_context_st *context= (cycle_context_st*)object;

  test_zero(context->servers.count());
  context->reset();

  return TEST_SUCCESS;
}

static test_return_t collection_FINAL(void *object)
{
  cycle_context_st *context= (cycle_context_st*)object;
  context->reset();

  return TEST_SUCCESS;
}

collection_st collection[] ={
  {"kill", validate_sanity_INIT, collection_FINAL, kill_tests },
  {"worker", collection_INIT, collection_FINAL, worker_tests },
  {"server_startup()", validate_sanity_INIT, collection_FINAL, server_startup_TESTS },
  {0, 0, 0, 0}
};

static void *world_create(server_startup_st& servers, test_return_t& error)
{
  SKIP("unreliable test");
  if (jenkins_is_caller())
  {
    error= TEST_SKIPPED;
    return NULL;
  }

  return new cycle_context_st(servers);
}

static bool world_destroy(void *object)
{
  cycle_context_st *context= (cycle_context_st*)object;
  delete context;

  return TEST_SUCCESS;
}

void get_world(libtest::Framework *world)
{
  world->collections(collection);
  world->create(world_create);
  world->destroy(world_destroy);
}
