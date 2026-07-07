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
#include <libgearman/gearman.h>
#include "tests/gearman_execute_partition.h"

#include "tests/libgearman-1.0/client_test.h"

#include "tests/workers/v2/echo_or_react.h"
#include "tests/workers/v2/split.h"
#include "tests/workers/aggregator/cat.h"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#define WORKER_FUNCTION_NAME "partition_client_test"
#define WORKER_SPLIT_FUNCTION_NAME "split_worker"

test_return_t partition_SETUP(void *object)
{
  client_test_st *test= (client_test_st *)object;
  ASSERT_TRUE(test);

  test->set_worker_name(WORKER_FUNCTION_NAME);

  gearman_function_t echo_react_fn= gearman_function_create_v2(echo_or_react_worker_v2);
  test->push(test_worker_start(libtest::default_port(), NULL,
                               test->worker_name(),
                               echo_react_fn, NULL, gearman_worker_options_t()));

  gearman_function_t split_worker_fn= gearman_function_create_partition(split_worker, cat_aggregator_fn);
  test->push(test_worker_start(libtest::default_port(), NULL,
                               WORKER_SPLIT_FUNCTION_NAME,
                               split_worker_fn,  NULL, GEARMAN_WORKER_GRAB_ALL));


  return TEST_SUCCESS;
}

test_return_t partition_free_SETUP(void *object)
{
  client_test_st *test= (client_test_st *)object;

  ASSERT_EQ(TEST_SUCCESS, partition_SETUP(object));

  gearman_client_add_options(test->client(), GEARMAN_CLIENT_FREE_TASKS);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_partition_check_parameters(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  ASSERT_EQ(GEARMAN_SUCCESS,
               gearman_client_echo(client, test_literal_param("this is mine")));

  // This just hear to make it easier to trace when gearman_execute() is
  // called (look in the log to see the failed option setting.
  gearman_client_set_server_option(client, test_literal_param("should fail"));
  gearman_argument_t workload= gearman_argument_make(0, 0, test_literal_param("this dog does not hunt"));

  // Test client as NULL
  gearman_task_st *task= gearman_execute_by_partition(NULL,
                                                      test_literal_param(WORKER_SPLIT_FUNCTION_NAME),
                                                      test_literal_param(WORKER_FUNCTION_NAME),
                                                      NULL, 0,  // unique
                                                      NULL,
                                                      &workload, 0);
  test_null(task);

  // Test no partition function
  task= gearman_execute_by_partition(client,
                                     NULL, 0,
                                     NULL, 0,
                                     NULL, 0,  // unique
                                     NULL,
                                     &workload, 0);
  test_null(task);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_partition_basic(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  ASSERT_EQ(GEARMAN_SUCCESS,
            gearman_client_echo(client, test_literal_param("this is mine")));

  // This just hear to make it easier to trace when
  // gearman_execute_partition() is called (look in the log to see the
  // failed option setting.
  ASSERT_FALSE(gearman_client_set_server_option(client, test_literal_param("should fail")));

  // This is the real work
  gearman_argument_t workload= gearman_argument_make(0, 0, test_literal_param("this dog does not hunt"));

  gearman_task_st *task= gearman_execute_by_partition(client,
                                                      test_literal_param(WORKER_SPLIT_FUNCTION_NAME),
                                                      test_literal_param(WORKER_FUNCTION_NAME),
                                                      NULL, 0,  // unique
                                                      NULL,
                                                      &workload, 0);
  ASSERT_TRUE(task);
  ASSERT_EQ(GEARMAN_SUCCESS, gearman_task_return(task));
  gearman_result_st *result= gearman_task_result(task);
  ASSERT_TRUE(result);
  const char *value= gearman_result_value(result);
  ASSERT_TRUE(value);
  ASSERT_EQ(18UL, gearman_result_size(result));

  // cat_aggregator_fn() concatenates the sub-task results in task-list
  // order, which is submission order (FIFO, since PR #435). split_worker()
  // splits "this dog does not hunt" on spaces in left-to-right order, so
  // the aggregated result is expected to be the words back-to-back in
  // their original order.
  test_memcmp("thisdogdoesnothunt", value, 18);

  gearman_task_free(task);
  gearman_client_task_free_all(client);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_partition_workfail(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  const char *worker_function= (const char *)gearman_client_context(client);

  ASSERT_EQ(GEARMAN_SUCCESS,
               gearman_client_echo(client, test_literal_param("this is mine")));

  gearman_argument_t workload= gearman_argument_make(0, 0, test_literal_param("this dog does not hunt mapper_fail"));

  gearman_task_st *task= gearman_execute_by_partition(client,
                                                      test_literal_param(WORKER_SPLIT_FUNCTION_NAME),
                                                      gearman_string_param_cstr(worker_function),
                                                      NULL, 0,  // unique
                                                      NULL,
                                                      &workload, 0);
  ASSERT_TRUE(task);

  ASSERT_EQ(GEARMAN_WORK_EXCEPTION, gearman_task_return(task));

  gearman_task_free(task);
  gearman_client_task_free_all(client);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_partition_fail_in_reduction(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  const char *worker_function= (const char *)gearman_client_context(client);

  ASSERT_EQ(gearman_client_echo(client, test_literal_param("this is mine")), GEARMAN_SUCCESS);

  gearman_argument_t workload= gearman_argument_make(0, 0, test_literal_param("this dog does not hunt fail"));

  gearman_task_st *task= gearman_execute_by_partition(client,
                                                      test_literal_param(WORKER_SPLIT_FUNCTION_NAME),
                                                      gearman_string_param_cstr(worker_function),
                                                      NULL, 0,  // unique
                                                      NULL,
                                                      &workload, 0);
  ASSERT_TRUE(task);

  ASSERT_EQ(GEARMAN_WORK_FAIL, gearman_task_return(task));

  gearman_task_free(task);
  gearman_client_task_free_all(client);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_partition_use_as_function(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;

  ASSERT_EQ(gearman_client_echo(client, test_literal_param("this is mine")), GEARMAN_SUCCESS);

  // This just hear to make it easier to trace when
  // gearman_execute_partition() is called (look in the log to see the
  // failed option setting.
  gearman_client_set_server_option(client, test_literal_param("should fail"));
  gearman_argument_t workload= gearman_argument_make(0, 0, test_literal_param("this dog does not hunt"));

  gearman_task_st *task= gearman_execute(client,
                                         test_literal_param(WORKER_SPLIT_FUNCTION_NAME),
                                         NULL, 0,  // unique
                                         NULL,
                                         &workload, 0);
  ASSERT_TRUE(task);

  ASSERT_EQ(GEARMAN_SUCCESS, gearman_task_return(task));
  gearman_result_st *result= gearman_task_result(task);
  ASSERT_TRUE(result);
  const char *value= gearman_result_value(result);
  ASSERT_TRUE(value);
  ASSERT_EQ(18UL, gearman_result_size(result));

  gearman_task_free(task);
  gearman_client_task_free_all(client);

  return TEST_SUCCESS;
}

test_return_t gearman_execute_partition_no_aggregate(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;

  gearman_argument_t workload= gearman_argument_make(0, 0, test_literal_param("this dog does not hunt"));

  gearman_task_st *task= gearman_execute_by_partition(client,
                                                      test_literal_param(WORKER_FUNCTION_NAME),
                                                      test_literal_param("count"),
                                                      NULL, 0,  // unique
                                                      NULL,
                                                      &workload, 0);
  ASSERT_TRUE(task);

  ASSERT_EQ(GEARMAN_SUCCESS, 
               gearman_task_return(task));

  ASSERT_EQ(GEARMAN_SUCCESS, gearman_task_return(task));
  test_false(gearman_task_result(task));

  gearman_task_free(task);
  gearman_client_task_free_all(client);

  return TEST_SUCCESS;
}
