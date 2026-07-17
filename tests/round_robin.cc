/* Gearman server and library
 * Copyright (C) 2008 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

#include "gear_config.h"
#include <libtest/test.hpp>

using namespace libtest;

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <unistd.h>

#include <libgearman/gearman.h>

#include "libgearman/client.hpp"
#include "libgearman/worker.hpp"
using namespace org::gearmand;

#include "tests/start_worker.h"

struct Context
{
  bool run_worker;
  in_port_t _port;
  uint32_t _retries;
  server_startup_st& servers;

  Context(server_startup_st& arg, in_port_t port_arg) :
    run_worker(false),
    _port(port_arg),
    _retries(0),
    servers(arg)
  {
  }

  uint32_t retries()
  {
    return _retries;
  }

  uint32_t retries(const uint32_t arg)
  {
    return _retries= arg;
  }

  in_port_t port() const
  {
    return _port;
  }

  ~Context()
  {
    servers.clear();
    libtest::release_port(_port);
  }

  void reset()
  {
    servers.clear();
    libtest::release_port(_port);
    _port= libtest::get_free_port();
    _retries= 0;
  }
};

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

/* append test for worker */
static gearman_return_t append_function_WORKER(gearman_job_st* job, void *context_arg)
{
  /* this will will set the last char in the context (buffer) to the */
  /* first char of the work */
  char *buf = (char *)context_arg;
  assert(buf);

  char *work = (char *)gearman_job_workload(job);
  buf+= strlen(buf);
  *buf= *work;

  return GEARMAN_SUCCESS;
}

static test_return_t queue_add(void *object)
{
  Context *context= (Context *)object;
  ASSERT_TRUE(context);

  libgearman::Client client(context->port());
  char job_handle[GEARMAN_JOB_HANDLE_SIZE];

  uint8_t *value= (uint8_t *)malloc(1);
  *value= uint8_t('0');
  size_t value_length= 1;

  context->run_worker= false;

  /* send strings "0", "1" ... "9" to alternating between 2 queues */
  /* queue1 = 1,3,5,7,9 */
  /* queue2 = 0,2,4,6,8 */
  for (uint32_t x= 0; x < 10; x++)
  {
    ASSERT_EQ(GEARMAN_SUCCESS,
                 gearman_client_do_background(&client, (x % 2) ? "queue1" : "queue2", NULL,
                                              value, value_length,
                                              job_handle));

    *value = (uint8_t)(*value +1);
  }

  free(value);

  context->run_worker= true;
  return TEST_SUCCESS;
}

static test_return_t queue_worker(void *object)
{
  Context *context= (Context *)object;
  ASSERT_TRUE(context);

  libgearman::Worker worker(context->port());

  char buffer[11];
  memset(buffer, 0, sizeof(buffer));

  ASSERT_TRUE(context->run_worker);

  gearman_function_t append_function_FN= gearman_function_create(append_function_WORKER);

  ASSERT_EQ(GEARMAN_SUCCESS, gearman_worker_define_function(&worker,
                                                               test_literal_param("queue1"),
                                                               append_function_FN,
                                                               0, buffer));

  ASSERT_EQ(GEARMAN_SUCCESS, gearman_worker_define_function(&worker,
                                                               test_literal_param("queue2"),
                                                               append_function_FN,
                                                               0, buffer));

  for (uint32_t x= 0; x < 10; x++)
  {
    ASSERT_EQ(GEARMAN_SUCCESS, gearman_worker_work(&worker));
  }

  // expect buffer to be reassembled in a predictable round robin order
  test_strcmp("1032547698", buffer);

  return TEST_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * round_robin_epoch_does_not_block_regular_TEST
 *
 * Regression test for the con_next loop-exit bug in gearman_server_job_take().
 *
 * When round-robin is enabled, the inner worker loop moves the chosen worker
 * to the tail of the list (GEARMAND_LIST_DEL + _server_con_worker_list_append).
 * After the append, server_worker->con_next is NULL.  If the moved worker's
 * function has only future-epoch jobs (none runnable right now), the loop
 * used to advance via the now-NULL con_next and exit early, skipping all
 * remaining workers -- including any with immediately runnable regular jobs.
 *
 * This test registers one worker for two functions (epoch function first),
 * submits a far-future epoch job to the first function and an immediately
 * runnable job to the second, then verifies the regular job is dispatched.
 * On the unfixed code gearman_client_do() would time out; on fixed code it
 * returns GEARMAN_SUCCESS promptly.
 * -------------------------------------------------------------------------*/

static gearman_return_t rr_epoch_noop_WORKER(gearman_job_st *job, void *)
{
  (void)job;
  return GEARMAN_SUCCESS;
}

static gearman_return_t rr_regular_echo_WORKER(gearman_job_st *job, void *)
{
  gearman_job_send_data(job,
                        gearman_job_workload(job),
                        gearman_job_workload_size(job));
  return GEARMAN_SUCCESS;
}

static test_return_t round_robin_epoch_does_not_block_regular_TEST(void *object)
{
  Context *context= (Context *)object;
  ASSERT_TRUE(context);

  /* Submit a far-future epoch job to "rr_epoch_fn" before the worker
     connects, so the server has a non-runnable job at the head of that
     function's NORMAL priority queue when the first GRAB_JOB arrives. */
  libgearman::Client client(context->port());
  gearman_task_attr_t epoch_attr=
    gearman_task_attr_init_epoch(time(NULL) + 3600, GEARMAN_JOB_PRIORITY_NORMAL);
  gearman_argument_t empty= gearman_argument_make(0, 0, NULL, 0);
  gearman_task_st *epoch_task= gearman_execute(&client,
                                               test_literal_param("rr_epoch_fn"),
                                               NULL, 0,
                                               &epoch_attr, &empty, NULL);
  ASSERT_TRUE(epoch_task);
  gearman_task_free(epoch_task);

  /* Start a background worker registered for both functions.  "rr_epoch_fn"
     is registered first so it appears first in server_con->worker_list,
     which is the condition that triggers the con_next early-exit bug. */
  libgearman::Worker w(context->port());
  gearman_function_t epoch_fn= gearman_function_create(rr_epoch_noop_WORKER);
  gearman_function_t regular_fn= gearman_function_create(rr_regular_echo_WORKER);
  ASSERT_EQ(GEARMAN_SUCCESS,
            gearman_worker_define_function(&w,
                                           test_literal_param("rr_epoch_fn"),
                                           epoch_fn, 0, NULL));
  ASSERT_EQ(GEARMAN_SUCCESS,
            gearman_worker_define_function(&w,
                                           test_literal_param("rr_regular_fn"),
                                           regular_fn, 0, NULL));
  std::unique_ptr<worker_handle_st> handle(worker_run(w));
  ASSERT_TRUE(handle.get());

  /* Submit a regular foreground job to "rr_regular_fn" and wait for the
     result.  With the bug the worker spins (NO_JOB -> PRE_SLEEP -> NOOP ->
     repeat) and gearman_client_do() times out.  With the fix the worker
     advances past the epoch function and dispatches the regular job. */
  gearman_client_set_timeout(&client, 5000);
  gearman_return_t rc;
  size_t result_len;
  void *result= gearman_client_do(&client, "rr_regular_fn", NULL,
                                  test_literal_param("ping"),
                                  &result_len, &rc);
  free(result);

  handle->shutdown();

  ASSERT_EQ(GEARMAN_SUCCESS, rc);

  return TEST_SUCCESS;
}

struct Limit
{
  uint32_t _count;
  uint32_t _expected;
  bool _limit;

  Limit(uint32_t expected_arg, bool limit_arg= false) :
    _count(0),
    _expected(expected_arg),
    _limit(limit_arg)
  { }

  void increment()
  {
    _count++;
  }

  void reset()
  {
    _count= 0;
  }

  uint32_t count()
  {
    return _count;
  }

  uint32_t expected()
  {
    return _expected;
  }

  gearman_return_t response() const
  {
    if (_limit)
    {
      return GEARMAN_SUCCESS;
    }

    return GEARMAN_FATAL;
  }

  bool complete()
  {
    if (_limit and _count == _expected)
    {
      return true;
    }

    return false;
  }
};

// The idea is to return GEARMAN_ERROR until we hit limit, then return
// GEARMAN_SUCCESS
static gearman_return_t job_retry_WORKER(gearman_job_st* job, void *context_arg)
{
  (void)(job);
  assert(gearman_job_workload_size(job) == 0);
  assert(gearman_job_workload(job) == NULL);
  Limit *limit= (Limit*)context_arg;

  if (limit->complete())
  {
    return GEARMAN_SUCCESS;
  }

  limit->increment();

  return GEARMAN_ERROR;
}

static test_return_t _job_retry_TEST(Context *context, Limit& limit)
{
  gearman_function_t job_retry_FN= gearman_function_create(job_retry_WORKER);
  std::unique_ptr<worker_handle_st> handle(test_worker_start(context->port(),
                                                             NULL,
                                                             __func__,
                                                             job_retry_FN,
                                                             &limit,
                                                             gearman_worker_options_t(),
                                                             0)); // timeout
  libgearman::Client client(context->port());

  gearman_return_t rc;
  test_null(gearman_client_do(&client,
                              __func__,
                              NULL, // unique
                              NULL, 0, // workload
                              NULL, // result size
                              &rc));

  // Join the background worker thread before inspecting limit's counters:
  // job_retry_WORKER (running on that thread) writes limit->_count with no
  // synchronization, so reading it here without a join first is a data race
  // (and can observe a stale/torn value if the worker is still mid-retry).
  handle->shutdown();

  ASSERT_EQ(uint32_t(limit.expected()), uint32_t(limit.count()));
  ASSERT_EQ(limit.response(), rc);

  return TEST_SUCCESS;
}

static test_return_t job_retry_GEARMAN_SUCCESS_TEST(void *object)
{
  Context *context= (Context *)object;

  Limit limit(context->retries() -1, true);

  return _job_retry_TEST(context, limit);
}

static test_return_t job_retry_limit_GEARMAN_SUCCESS_TEST(void *object)
{
  Context *context= (Context *)object;

  if (context->retries() < 2)
  {
    return TEST_SKIPPED;
  }

  for (uint32_t x= 1; x < context->retries(); x++)
  {
    Limit limit(uint32_t(x -1), true);
    ASSERT_EQ(TEST_SUCCESS, _job_retry_TEST(context, limit));
  }

  return TEST_SUCCESS;
}

static test_return_t job_retry_GEARMAN_FATAL_TEST(void *object)
{
  Context *context= (Context *)object;

  Limit limit(context->retries());

  return _job_retry_TEST(context, limit);
}

static test_return_t round_robin_SETUP(void *object)
{
  Context *context= (Context *)object;

  const char *argv[]= { "--round-robin", 0 };
  if (server_startup(context->servers, "gearmand", context->port(), argv))
  {
    return TEST_SUCCESS;
  }

  return TEST_FAILURE;
}

static test_return_t _job_retries_SETUP(Context *context)
{
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--job-retries=%u", uint32_t(context->retries()));
  const char *argv[]= { buffer, 0};
  if (server_startup(context->servers, "gearmand", context->port(), argv))
  {
    return TEST_SUCCESS;
  }

  return TEST_FAILURE;
}

static test_return_t job_retries_once_SETUP(void *object)
{
  Context *context= (Context *)object;
  context->retries(1);

  return _job_retries_SETUP(context);
}

static test_return_t job_retries_twice_SETUP(void *object)
{
  Context *context= (Context *)object;
  context->retries(2);

  return _job_retries_SETUP(context);
}

static test_return_t job_retries_ten_SETUP(void *object)
{
  Context *context= (Context *)object;
  context->retries(10);

  return _job_retries_SETUP(context);
}

static test_return_t _TEARDOWN(void *object)
{
  Context *context= (Context *)object;
  context->reset();

  return TEST_SUCCESS;
}

static void *world_create(server_startup_st& servers, test_return_t&)
{
  Context *context= new Context(servers, libtest::get_free_port());

  return context;
}

static bool world_destroy(void *object)
{
  Context *context= (Context *)object;
  
  delete context;

  return TEST_SUCCESS;
}

test_st round_robin_TESTS[] ={
  {"add", 0, queue_add },
  {"worker", 0, queue_worker },
  {"epoch job does not block regular job dispatch", 0, round_robin_epoch_does_not_block_regular_TEST },
  {0, 0, 0}
};

test_st job_retry_TESTS[] ={
  {"GEARMAN_FATAL", 0, job_retry_GEARMAN_FATAL_TEST },
  {"GEARMAN_SUCCESS", 0, job_retry_GEARMAN_SUCCESS_TEST },
  {"limit", 0, job_retry_limit_GEARMAN_SUCCESS_TEST },
  {0, 0, 0}
};

collection_st collection[] ={
  {"round_robin", round_robin_SETUP, _TEARDOWN, round_robin_TESTS },
  {"--job-retries=1", job_retries_once_SETUP, _TEARDOWN, job_retry_TESTS },
  {"--job-retries=2", job_retries_twice_SETUP, _TEARDOWN, job_retry_TESTS },
  {"--job-retries=10", job_retries_ten_SETUP, _TEARDOWN, job_retry_TESTS },
  {0, 0, 0, 0}
};

void get_world(libtest::Framework *world)
{
  world->collections(collection);
  world->create(world_create);
  world->destroy(world_destroy);
}
