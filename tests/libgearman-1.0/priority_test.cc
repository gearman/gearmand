/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2013 Data Differential, http://datadifferential.com/
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

// Adding thread support for our worker
#include <thread>
#include <chrono>

using namespace libtest;

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <vector>

#include <libgearman-1.0/gearman.h>
#include <libgearman/connection.hpp>
#include "libgearman/command.h"
#include "libgearman/packet.hpp"
#include "libgearman/universal.hpp"
#include "libgearman/is.hpp"
#include "libgearman/interface/worker.hpp"

#include "libgearman/client.hpp"
#include "libgearman/worker.hpp"
using namespace org::gearmand;

#include "tests/start_worker.h"
#include <thread>

static gearman_return_t priority_check_worker(gearman_job_st* job, void *context)
{
  const char* workload= (const char*)gearman_job_workload(job);
  size_t workload_size= gearman_job_workload_size(job);

  std::vector<int>* priority_order= (std::vector<int>*)context;

  // Convert workload to integer priority
  // For single priority tasks, it's just the number
  // For multiple tasks per priority, it's "priority_index", so we extract just the priority part
  int priority;
  if (strchr(workload, '_')) {
    // Extract just the priority part before the underscore
    char priority_str[2];
    priority_str[0] = workload[0];
    priority_str[1] = '\0';
    priority = atoi(priority_str);
  } else {
    priority = atoi(workload);
  }
  priority_order->push_back(priority);

  if (gearman_failed(gearman_job_send_data(job, workload, workload_size)))
  {
    return GEARMAN_ERROR;
  }

  return GEARMAN_SUCCESS;
}

static test_return_t gearman_worker_priority_background_TEST(void *)
{
  libgearman::Client client(libtest::default_port());
  ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_echo(&client, test_literal_param(__func__)));

  // Create a vector to track the order priorities are received
  std::vector<int> priority_order;

  // Add background tasks with different priorities in reverse order (LOW, NORMAL, HIGH)
  // but they should be processed in HIGH, NORMAL, LOW order

  std::vector<gearman_task_st *> tasks;

  // Add low priority background task
  gearman_task_attr_t low_attr = gearman_task_attr_init_background(GEARMAN_JOB_PRIORITY_LOW);
  gearman_argument_t low_arg = gearman_argument_make(NULL, 0, "0", 1);
  gearman_task_st *low_task = gearman_execute(&client,
                                              test_literal_param(__func__),
                                              NULL, 0, // unique
                                              &low_attr, // gearman_task_attr_t
                                              &low_arg, // gearman_argument_t
                                              NULL); // context
  ASSERT_TRUE(low_task);
  tasks.push_back(low_task);

  // Add normal priority background task
  gearman_task_attr_t normal_attr = gearman_task_attr_init_background(GEARMAN_JOB_PRIORITY_NORMAL);
  gearman_argument_t normal_arg = gearman_argument_make(NULL, 0, "1", 1);
  gearman_task_st *normal_task = gearman_execute(&client,
                                                 test_literal_param(__func__),
                                                 NULL, 0, // unique
                                                 &normal_attr, // gearman_task_attr_t
                                                 &normal_arg, // gearman_argument_t
                                                 NULL); // context
  ASSERT_TRUE(normal_task);
  tasks.push_back(normal_task);

  // Add high priority background task
  gearman_task_attr_t high_attr = gearman_task_attr_init_background(GEARMAN_JOB_PRIORITY_HIGH);
  gearman_argument_t high_arg = gearman_argument_make(NULL, 0, "2", 1);
  gearman_task_st *high_task = gearman_execute(&client,
                                              test_literal_param(__func__),
                                              NULL, 0, // unique
                                              &high_attr, // gearman_task_attr_t
                                              &high_arg, // gearman_argument_t
                                              NULL); // context
  ASSERT_TRUE(high_task);
  tasks.push_back(high_task);

  // Ensure all jobs are queued before starting the worker
  bool all_tasks_queued = false;
  int max_queue_attempts = 50; // Prevent infinite loop
  int queue_attempts = 0;

  while (!all_tasks_queued && queue_attempts < max_queue_attempts) {
    all_tasks_queued = true; // Assume all queued unless we find one that isn't
    for (std::vector<gearman_task_st *>::iterator iter = tasks.begin();
         iter != tasks.end(); ++iter) {
      if (!gearman_task_is_known(*iter)) {
        all_tasks_queued = false; // At least one task is not yet known
        break;
      }
    }

    if (!all_tasks_queued) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      queue_attempts++;
    }
  }

  // Verify that all tasks were successfully queued
  ASSERT_TRUE(all_tasks_queued);

  // Start worker for background tasks
  gearman_function_t priority_check_TEST_FN = gearman_function_create(priority_check_worker);
  std::unique_ptr<worker_handle_st> handle(test_worker_start(libtest::default_port(),
                                                             NULL,
                                                             __func__,
                                                             priority_check_TEST_FN,
                                                             &priority_order,
                                                             gearman_worker_options_t(),
                                                             0)); // timeout

  // Wait for worker to process all tasks
  // Check if tasks are still known (queued or running)
  bool tasks_complete = false;
  int max_attempts = 50; // Prevent infinite loop
  int attempts = 0;

  while (!tasks_complete && attempts < max_attempts) {
    tasks_complete = true; // Assume complete unless we find a known task
    for (std::vector<gearman_task_st *>::iterator iter = tasks.begin();
         iter != tasks.end(); ++iter) {
      if (gearman_task_is_known(*iter)) {
        tasks_complete = false; // At least one task is still known
        break;
      }
    }

    if (!tasks_complete) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      attempts++;
    }
  }

  // Verify that tasks were processed in the correct priority order (HIGH, NORMAL, LOW)
  // which corresponds to values 2, 1, 0
  ASSERT_EQ(3U, priority_order.size());
  ASSERT_EQ(2, priority_order[0]); // First should be HIGH priority
  ASSERT_EQ(1, priority_order[1]); // Second should be NORMAL priority
  ASSERT_EQ(0, priority_order[2]); // Third should be LOW priority

  // Cleanup tasks
  for (std::vector<gearman_task_st *>::iterator iter = tasks.begin(); iter != tasks.end(); ++iter)
  {
    gearman_task_free(*iter);
  }

  return TEST_SUCCESS;
}

// Worker callback function that tracks both priority and function
struct PriorityFunction {
  int priority;
  std::string function;

  PriorityFunction(int p, const char* f) : priority(p), function(f) {}
};

static void* priority_function_check_worker(gearman_job_st* job, void *context,
                                          size_t *result_size,
                                          gearman_return_t *ret_ptr)
{
  std::vector<PriorityFunction>* order= (std::vector<PriorityFunction>*)context;
  const char *workload= (const char *)gearman_job_workload(job);
  size_t workload_size= gearman_job_workload_size(job);
  const char* function_name= gearman_job_function_name(job);

  // Parse priority from workload (format: "priority_functionName")
  std::string workload_str(workload, workload_size);
  size_t underscore_pos= workload_str.find('_');
  int priority= atoi(workload_str.substr(0, underscore_pos).c_str());

  // Add to execution order
  order->push_back(PriorityFunction(priority, function_name));

  *result_size = 0;
  *ret_ptr = GEARMAN_SUCCESS;
  return NULL;
}

static test_return_t gearman_worker_priority_multiple_functions_TEST(void *)
{
  libgearman::Client client(libtest::default_port());
  ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_echo(&client, test_literal_param(__func__)));

  // Create a vector to track the order of (priority, function) pairs
  std::vector<PriorityFunction> execution_order;

  // Submit tasks before starting workers
  std::vector<gearman_task_st*> tasks;

  // Submit a low priority task to function1 FIRST (this will be registered first)
  gearman_task_attr_t low_attr = gearman_task_attr_init_background(GEARMAN_JOB_PRIORITY_LOW);
  gearman_argument_t low_arg = gearman_argument_make(NULL, 0, test_literal_param("0_function1"));
  gearman_task_st *low_task = gearman_execute(&client,
                                              test_literal_param("function1"),
                                              NULL, 0, // unique
                                              &low_attr, // gearman_task_attr_t
                                              &low_arg, // gearman_argument_t
                                              NULL); // context
  ASSERT_TRUE_(low_task, "Low priority task creation failed");
  tasks.push_back(low_task);

  // Submit a high priority task to function2 SECOND (this will be registered second)
  gearman_task_attr_t high_attr = gearman_task_attr_init_background(GEARMAN_JOB_PRIORITY_HIGH);
  gearman_argument_t high_arg = gearman_argument_make(NULL, 0, test_literal_param("2_function2"));
  gearman_task_st *high_task = gearman_execute(&client,
                                               test_literal_param("function2"),
                                               NULL, 0, // unique
                                               &high_attr, // gearman_task_attr_t
                                               &high_arg, // gearman_argument_t
                                               NULL); // context
  ASSERT_TRUE_(high_task, "High priority task creation failed");
  tasks.push_back(high_task);

  // Submit a normal priority task to function1 THIRD
  gearman_task_attr_t normal_attr = gearman_task_attr_init_background(GEARMAN_JOB_PRIORITY_NORMAL);
  gearman_argument_t normal_arg = gearman_argument_make(NULL, 0, test_literal_param("1_function1"));
  gearman_task_st *normal_task = gearman_execute(&client,
                                                 test_literal_param("function1"),
                                                 NULL, 0, // unique
                                                 &normal_attr, // gearman_task_attr_t
                                                 &normal_arg, // gearman_argument_t
                                                 NULL); // context
  ASSERT_TRUE_(normal_task, "Normal priority task creation failed");
  tasks.push_back(normal_task);

  // Ensure all jobs are queued before starting the worker
  bool all_tasks_queued = false;
  int max_queue_attempts = 50; // Prevent infinite loop
  int queue_attempts = 0;

  while (!all_tasks_queued && queue_attempts < max_queue_attempts) {
    all_tasks_queued = true; // Assume all queued unless we find one that isn't
    for (std::vector<gearman_task_st *>::iterator iter = tasks.begin();
         iter != tasks.end(); ++iter) {
      if (!gearman_task_is_known(*iter)) {
        all_tasks_queued = false; // At least one task is not yet known
        break;
      }
    }

    if (!all_tasks_queued) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      queue_attempts++;
    }
  }

  // Start a single worker connection for both function1 and function2
  gearman_worker_st *worker = gearman_worker_create(NULL);
  ASSERT_TRUE_(worker, "Failed to create worker");
  ASSERT_EQ(GEARMAN_SUCCESS, gearman_worker_add_server(worker, NULL, libtest::default_port()));

  // Register both functions on the same worker connection
  ASSERT_EQ(GEARMAN_SUCCESS, gearman_worker_add_function(worker, "function1", 0, priority_function_check_worker, &execution_order));
  ASSERT_EQ(GEARMAN_SUCCESS, gearman_worker_add_function(worker, "function2", 0, priority_function_check_worker, &execution_order));
  gearman_worker_work(worker);

  // Verify the actual execution order matches the CORRECT priority-based behavior
  ASSERT_EQ("function2", execution_order[0].function);
  ASSERT_EQ(2, execution_order[0].priority); // High priority processed first

  // Cleanup tasks and Free workers
  for (std::vector<gearman_task_st*>::iterator iter = tasks.begin(); iter != tasks.end(); ++iter)
  {
    gearman_task_free(*iter);
  }
  gearman_worker_free(worker);

  return TEST_SUCCESS;
}

test_st gearman_worker_priority_tests[] = {
  { "gearman_worker_priority_background_TEST", 0, gearman_worker_priority_background_TEST },
  { "gearman_worker_priority_multiple_functions_TEST", 0, gearman_worker_priority_multiple_functions_TEST },
  { 0, 0, 0 }
};

test_st *test_gearman_worker_priority(void)
{
  return gearman_worker_priority_tests;
}
