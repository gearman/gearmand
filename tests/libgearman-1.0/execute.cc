/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
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
#include <cstring>
#include <ctime>
#include <libgearman/gearman.h>
#include <string>
#include <tests/execute.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

test_return_t gearman_execute_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);
  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_TRUE(worker_function);

  gearman_task_st *task;
  gearman_argument_t value= gearman_argument_make(0, 0, test_literal_param("test load"));

  ASSERT_TRUE(task= gearman_execute(client, test_string_make_from_cstr(worker_function), NULL, 0, NULL, &value, 0));
  ASSERT_EQ(test_literal_param_size("test load"), gearman_result_size(gearman_task_result(task)));
  test_false(gearman_task_is_known(task));
  test_false(gearman_task_is_running(task));

  gearman_task_free(task);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_NULL_workload_TEST(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);
  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_TRUE(worker_function);

  gearman_task_attr_t task_attr= gearman_task_attr_init_background(GEARMAN_JOB_PRIORITY_NORMAL);

  gearman_task_st *task;
  ASSERT_TRUE(task= gearman_execute(client, 
                                  test_string_make_from_cstr(worker_function),
                                  NULL, 0, // unique
                                  &task_attr, // gearman_task_attr_t 
                                  NULL, // argument
                                  0));
  gearman_task_free(task);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_NULL_attr_NULL_workload_TEST(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);
  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_TRUE(worker_function);

  gearman_task_st *task;
  ASSERT_TRUE(task= gearman_execute(client, 
                                  test_string_make_from_cstr(worker_function),
                                  NULL, 0, // unique
                                  NULL, // gearman_task_attr_t 
                                  NULL, // argument
                                  0));
  gearman_task_free(task);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_fail_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  const char *worker_function= (const char *)gearman_client_context(client);
  assert(worker_function);

  gearman_task_st *task;
  gearman_argument_t value= gearman_argument_make(0, 0, test_literal_param("fail"));

  ASSERT_TRUE(task= gearman_execute(client, test_string_make_from_cstr(worker_function), NULL, 0, NULL, &value, 0));
  ASSERT_EQ(GEARMAN_WORK_FAIL, gearman_task_return(task));
  test_false(gearman_task_is_known(task));
  test_false(gearman_task_is_running(task));

  gearman_task_free(task);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_timeout_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  const char *worker_function= (const char *)gearman_client_context(client);
  assert(worker_function);
  ASSERT_TRUE(client);

  gearman_client_set_timeout(client, 4);

  // We should fail since the the timeout is small and the function should
  // not exist.
  gearman_task_st *task;
  gearman_argument_t value= gearman_argument_make(0, 0, test_literal_param("test load"));
  ASSERT_TRUE(task= gearman_execute(client, test_string_make_from_cstr(worker_function), NULL, 0, NULL, &value, 0));
  gearman_task_free(task);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_epoch_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_TRUE(worker_function);

  gearman_task_attr_t task_attr= gearman_task_attr_init_epoch(time(NULL) +5, GEARMAN_JOB_PRIORITY_NORMAL);

  gearman_task_st *task;
  gearman_argument_t value= gearman_argument_make(0, 0, test_literal_param("test load"));
  ASSERT_TRUE(task= gearman_execute(client, test_string_make_from_cstr(worker_function), 
                                  NULL, 0, // unique
                                  &task_attr, &value, 0));
  ASSERT_TRUE(task);
  ASSERT_TRUE(gearman_task_job_handle(task));
  ASSERT_TRUE(gearman_task_is_known(task));
  test_false(gearman_task_is_running(task));
  gearman_task_free(task);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_epoch_check_job_handle_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  const char *worker_function= (const char *)gearman_client_context(client);
  assert(worker_function);

  gearman_task_attr_t task_attr= gearman_task_attr_init_epoch(time(NULL) +5, GEARMAN_JOB_PRIORITY_NORMAL);

  gearman_task_st *task;
  gearman_argument_t value= gearman_argument_make(0, 0, test_literal_param("test load"));
  ASSERT_TRUE(task= gearman_execute(client, test_string_make_from_cstr(worker_function), NULL, 0, &task_attr, &value, 0));

  ASSERT_TRUE(task);
  ASSERT_TRUE(gearman_task_job_handle(task));

  gearman_return_t rc;
  bool is_known;
  size_t limit= 10;
  do {
    if (--limit)
    {
      rc= gearman_client_job_status(client, gearman_task_job_handle(task), &is_known, NULL, NULL, NULL);
    }
    else
    {
      break;
    }
  }  while (gearman_continue(rc) or is_known);

  if (limit)
  {
    ASSERT_EQ(GEARMAN_SUCCESS, rc);
  }

  gearman_task_free(task);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_epoch_wakeup_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_TRUE(worker_function);

  /* Submit an epoch job that becomes runnable in 1 second.
     Without the epoch-timer-wakeup fix the worker goes back to sleep after
     the first GRAB_JOB / NO_JOB exchange and never wakes up again, so
     WORK_COMPLETE is never sent back and gearman_client_run_tasks() times out. */
  gearman_task_attr_t task_attr= gearman_task_attr_init_epoch(time(NULL) + 1, GEARMAN_JOB_PRIORITY_NORMAL);
  gearman_argument_t value= gearman_argument_make(0, 0, test_literal_param("epoch_wakeup_check"));

  gearman_task_st *task;
  ASSERT_TRUE(task= gearman_execute(client, test_string_make_from_cstr(worker_function),
                                    NULL, 0, &task_attr, &value, 0));
  ASSERT_TRUE(gearman_task_job_handle(task));

  /* Drive the client event loop until WORK_COMPLETE arrives.  The epoch
     timer fires after ~1 second; allow 10 seconds total as slack. */
  gearman_client_set_timeout(client, 10000);
  gearman_return_t rc= gearman_client_run_tasks(client);

  gearman_task_free(task);

  ASSERT_EQ(GEARMAN_SUCCESS, rc);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_bg_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  const char *worker_function= (const char *)gearman_client_context(client);
  assert(worker_function);

  gearman_task_attr_t task_attr= gearman_task_attr_init_background(GEARMAN_JOB_PRIORITY_NORMAL);

  gearman_task_st *task;
  gearman_argument_t value= gearman_argument_make(0, 0, test_literal_param("test load"));
  ASSERT_TRUE(task= gearman_execute(client, test_string_make_from_cstr(worker_function), test_literal_param("my id"), &task_attr, &value, 0));

  // Lets make sure we have a task
  ASSERT_TRUE(task);
  ASSERT_TRUE(gearman_task_job_handle(task));

  ASSERT_TRUE(gearman_success(gearman_client_run_tasks(client)));

  gearman_task_free(task);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_multile_bg_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  const char *worker_function= (const char *)gearman_client_context(client);
  assert(worker_function);

  for (uint32_t x= 0; x < 4; /* No reason for number */ x++)
  {
    gearman_task_attr_t task_attr= gearman_task_attr_init_background(GEARMAN_JOB_PRIORITY_NORMAL);

    gearman_task_st *task;
    gearman_argument_t value= gearman_argument_make(0, 0, test_literal_param("test load"));
    ASSERT_TRUE(task= gearman_execute(client, test_string_make_from_cstr(worker_function), NULL, 0, &task_attr, &value, 0));
    
    // Lets make sure we have a task
    ASSERT_TRUE(task);
    ASSERT_TRUE(gearman_task_job_handle(task));
  }

  ASSERT_EQ(GEARMAN_SUCCESS,
               gearman_client_run_tasks(client));

  gearman_client_task_free_all(client);

  return TEST_SUCCESS;
}
