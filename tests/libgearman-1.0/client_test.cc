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

#include "tests/start_worker.h"

#include "tests/workers/v1/echo_or_react.h"
#include "tests/workers/v1/echo_or_react_chunk.h"
#include "tests/workers/v2/echo_or_react.h"
#include "tests/workers/v2/echo_or_react_chunk.h"

#define NAMESPACE_KEY "foo123"

#define WORKER_FUNCTION_NAME "client_test"
#define WORKER_CHUNKED_FUNCTION_NAME "reverse_test"

#include "libgearman/client.hpp"
#include "libgearman/worker.hpp"
using namespace org::gearmand;

#include "tests/limits.h"
#include "tests/do.h"
#include "tests/server_options.h"
#include "tests/do_background.h"
#include "tests/execute.h"
#include "tests/gearman_client_do_job_handle.h"
#include "tests/gearman_execute_partition.h"
#include "tests/libgearman-1.0/fork.h"
#include "tests/protocol.h"
#include "tests/regression.h"
#include "tests/task.h"
#include "tests/unique.h"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include "tests/runner.h"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include "tests/libgearman-1.0/client_test.h"
#include "libgearman/interface/client.hpp"
#include "libgearman/is.hpp"

/**
  @note Just here until I fix libhashkit.
*/
static uint32_t internal_generate_hash(const char *key, size_t key_length)
{
  const char *ptr= key;
  uint32_t value= 0;

  while (key_length--)
  {
    uint32_t val= (uint32_t) *ptr++;
    value += val;
    value += (value << 10);
    value ^= (value >> 6);
  }
  value += (value << 3);
  value ^= (value >> 11);
  value += (value << 15);

  return value == 0 ? 1 : (uint32_t) value;
}

/* Prototypes */
static void *client_test_temp_worker(gearman_job_st *, void *,
                                     size_t *result_size, gearman_return_t *ret_ptr)
{
  *result_size= 0;
  *ret_ptr= GEARMAN_SUCCESS;
  return NULL;
}


extern "C"
{
  static __attribute__((noreturn)) void *client_thread(void *object)
  {
    volatile gearman_return_t *ret= (volatile gearman_return_t *)object;
    {
      libgearman::Client client(libtest::default_port());
      gearman_client_set_timeout(&client, 400);

      for (size_t x= 0; x < 5; x++)
      {
        gearman_return_t rc;
        size_t result_size;
        (void)gearman_client_do(&client, "client_test_temp", NULL, NULL, 0, &result_size, &rc);

        if (gearman_failed(rc))
        {
          *ret= rc;
          break;
        }
      }
    }

    pthread_exit(0);
  }
}

static test_return_t init_test(void *)
{
  gearman_client_st client;

  ASSERT_TRUE(gearman_client_create(&client));

  gearman_client_free(&client);

  return TEST_SUCCESS;
}

static test_return_t allocation_test(void *)
{
  gearman_client_st *client;

  ASSERT_TRUE(client= gearman_client_create(NULL));

  gearman_client_free(client);

  return TEST_SUCCESS;
}

static test_return_t clone_test(void *)
{
  {
    gearman_client_st *client= gearman_client_clone(NULL, NULL);

    ASSERT_TRUE(client);
    ASSERT_TRUE(gearman_is_allocated(client));

    gearman_client_free(client);
  }

  {
    gearman_client_st *from= gearman_client_create(NULL);
    ASSERT_TRUE(from);
    gearman_client_st* client= gearman_client_clone(NULL, from);
    ASSERT_TRUE(client);
    gearman_client_free(client);
    gearman_client_free(from);
  }

  {
    gearman_client_st *from_with_host= gearman_client_create(NULL);
    ASSERT_TRUE(from_with_host);
    ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_add_server(from_with_host, "localhost", 12345));
    ASSERT_TRUE(from_with_host->impl()->universal.con_list);
    gearman_client_st* client= gearman_client_clone(NULL, from_with_host);
    ASSERT_TRUE(client);
    ASSERT_TRUE(client->impl()->universal.con_list);
    ASSERT_TRUE(gearman_client_compare(client, from_with_host));
    gearman_client_free(client);
    gearman_client_free(from_with_host);
  }

  return TEST_SUCCESS;
}

static test_return_t option_test(void *)
{
  gearman_client_st *gear;
  gearman_client_options_t default_options;

  gear= gearman_client_create(NULL);
  ASSERT_TRUE(gear);
  { // Initial Allocated, no changes
    ASSERT_TRUE(gearman_is_allocated(gear));
    test_false(gear->impl()->options.non_blocking);
    test_false(gear->impl()->options.unbuffered_result);
    test_false(gear->impl()->options.no_new);
    test_false(gear->impl()->options.free_tasks);
    ASSERT_FALSE(gear->impl()->options.generate_unique);
  }

  /* Set up for default options */
  default_options= gearman_client_options(gear);

  /*
    We take the basic options, and push
    them back in. See if we change anything.
  */
  gearman_client_set_options(gear, default_options);
  { // Initial Allocated, no changes
    ASSERT_TRUE(gearman_is_allocated(gear));
    test_false(gear->impl()->options.non_blocking);
    test_false(gear->impl()->options.unbuffered_result);
    test_false(gear->impl()->options.no_new);
    test_false(gear->impl()->options.free_tasks);
    ASSERT_FALSE(gear->impl()->options.generate_unique);
  }

  /*
    We will trying to modify non-mutable options (which should not be allowed)
  */
  {
    gearman_client_remove_options(gear, GEARMAN_CLIENT_ALLOCATED);
    { // Initial Allocated, no changes
      ASSERT_TRUE(gearman_is_allocated(gear));
      test_false(gear->impl()->options.non_blocking);
      test_false(gear->impl()->options.unbuffered_result);
      test_false(gear->impl()->options.no_new);
      test_false(gear->impl()->options.free_tasks);
      ASSERT_FALSE(gear->impl()->options.generate_unique);
    }
    gearman_client_remove_options(gear, GEARMAN_CLIENT_NO_NEW);
    { // Initial Allocated, no changes
      ASSERT_TRUE(gearman_is_allocated(gear));
      test_false(gear->impl()->options.non_blocking);
      test_false(gear->impl()->options.unbuffered_result);
      test_false(gear->impl()->options.no_new);
      test_false(gear->impl()->options.free_tasks);
      ASSERT_FALSE(gear->impl()->options.generate_unique);
    }
  }

  /*
    We will test modifying GEARMAN_CLIENT_NON_BLOCKING in several manners.
  */
  {
    gearman_client_remove_options(gear, GEARMAN_CLIENT_NON_BLOCKING);
    { // GEARMAN_CLIENT_NON_BLOCKING set to default, by default.
      ASSERT_TRUE(gearman_is_allocated(gear));
      test_false(gear->impl()->options.non_blocking);
      test_false(gear->impl()->options.unbuffered_result);
      test_false(gear->impl()->options.no_new);
      test_false(gear->impl()->options.free_tasks);
    }
    gearman_client_add_options(gear, GEARMAN_CLIENT_NON_BLOCKING);
    { // GEARMAN_CLIENT_NON_BLOCKING set to default, by default.
      ASSERT_TRUE(gearman_is_allocated(gear));
      ASSERT_TRUE(gear->impl()->options.non_blocking);
      test_false(gear->impl()->options.unbuffered_result);
      test_false(gear->impl()->options.no_new);
      test_false(gear->impl()->options.free_tasks);
    }
    gearman_client_set_options(gear, GEARMAN_CLIENT_NON_BLOCKING);
    { // GEARMAN_CLIENT_NON_BLOCKING set to default, by default.
      ASSERT_TRUE(gearman_is_allocated(gear));
      ASSERT_TRUE(gear->impl()->options.non_blocking);
      test_false(gear->impl()->options.unbuffered_result);
      test_false(gear->impl()->options.no_new);
      test_false(gear->impl()->options.free_tasks);
    }
    gearman_client_set_options(gear, GEARMAN_CLIENT_UNBUFFERED_RESULT);
    { // Everything is now set to false except GEARMAN_CLIENT_UNBUFFERED_RESULT, and non-mutable options
      ASSERT_TRUE(gearman_is_allocated(gear));
      test_false(gear->impl()->options.non_blocking);
      ASSERT_TRUE(gear->impl()->options.unbuffered_result);
      test_false(gear->impl()->options.no_new);
      test_false(gear->impl()->options.free_tasks);
    }

    // Test setting GEARMAN_CLIENT_GENERATE_UNIQUE
    {
      gearman_client_set_options(gear, default_options);
      { // See if we return to defaults
        test_false(gear->impl()->options.non_blocking);
        test_false(gear->impl()->options.unbuffered_result);
        test_false(gear->impl()->options.no_new);
        test_false(gear->impl()->options.free_tasks);
        ASSERT_FALSE(gear->impl()->options.generate_unique);
      }

      gearman_client_remove_options(gear, GEARMAN_CLIENT_GENERATE_UNIQUE);
      { // Initial Allocated, no changes
        test_false(gear->impl()->options.non_blocking);
        test_false(gear->impl()->options.unbuffered_result);
        test_false(gear->impl()->options.no_new);
        test_false(gear->impl()->options.free_tasks);
        ASSERT_FALSE(gear->impl()->options.generate_unique);
      }

      gearman_client_set_options(gear, GEARMAN_CLIENT_GENERATE_UNIQUE);
      { // See if we return to defaults
        test_false(gear->impl()->options.non_blocking);
        test_false(gear->impl()->options.unbuffered_result);
        test_false(gear->impl()->options.no_new);
        test_false(gear->impl()->options.free_tasks);
        ASSERT_TRUE(gear->impl()->options.generate_unique);
      }
    }

    /*
      Reset options to default. Then add an option, and then add more options. Make sure
      the options are all additive.
    */
    {
      gearman_client_set_options(gear, default_options);
      { // See if we return to defaults
        ASSERT_TRUE(gearman_is_allocated(gear));
        test_false(gear->impl()->options.non_blocking);
        test_false(gear->impl()->options.unbuffered_result);
        test_false(gear->impl()->options.no_new);
        test_false(gear->impl()->options.free_tasks);
        ASSERT_FALSE(gear->impl()->options.generate_unique);
      }
      gearman_client_add_options(gear, GEARMAN_CLIENT_FREE_TASKS);
      { // All defaults, except timeout_return
        ASSERT_TRUE(gearman_is_allocated(gear));
        test_false(gear->impl()->options.non_blocking);
        test_false(gear->impl()->options.unbuffered_result);
        test_false(gear->impl()->options.no_new);
        ASSERT_TRUE(gear->impl()->options.free_tasks);
        ASSERT_FALSE(gear->impl()->options.generate_unique);
      }
      gearman_client_add_options(gear, (gearman_client_options_t)(GEARMAN_CLIENT_NON_BLOCKING|GEARMAN_CLIENT_UNBUFFERED_RESULT));
      { // GEARMAN_CLIENT_NON_BLOCKING set to default, by default.
        ASSERT_TRUE(gearman_is_allocated(gear));
        ASSERT_TRUE(gear->impl()->options.non_blocking);
        ASSERT_TRUE(gear->impl()->options.unbuffered_result);
        test_false(gear->impl()->options.no_new);
        ASSERT_TRUE(gear->impl()->options.free_tasks);
        ASSERT_FALSE(gear->impl()->options.generate_unique);
      }
    }
    /*
      Add an option, and then replace with that option plus a new option.
    */
    {
      gearman_client_set_options(gear, default_options);
      { // See if we return to defaults
        ASSERT_TRUE(gearman_is_allocated(gear));
        test_false(gear->impl()->options.non_blocking);
        test_false(gear->impl()->options.unbuffered_result);
        test_false(gear->impl()->options.no_new);
        test_false(gear->impl()->options.free_tasks);
        ASSERT_FALSE(gear->impl()->options.generate_unique);
      }
      gearman_client_add_options(gear, GEARMAN_CLIENT_FREE_TASKS);
      { // All defaults, except timeout_return
        ASSERT_TRUE(gearman_is_allocated(gear));
        test_false(gear->impl()->options.non_blocking);
        test_false(gear->impl()->options.unbuffered_result);
        test_false(gear->impl()->options.no_new);
        ASSERT_TRUE(gear->impl()->options.free_tasks);
        ASSERT_FALSE(gear->impl()->options.generate_unique);
      }
      gearman_client_add_options(gear, (gearman_client_options_t)(GEARMAN_CLIENT_FREE_TASKS|GEARMAN_CLIENT_UNBUFFERED_RESULT));
      { // GEARMAN_CLIENT_NON_BLOCKING set to default, by default.
        ASSERT_TRUE(gearman_is_allocated(gear));
        test_false(gear->impl()->options.non_blocking);
        ASSERT_TRUE(gear->impl()->options.unbuffered_result);
        test_false(gear->impl()->options.no_new);
        ASSERT_TRUE(gear->impl()->options.free_tasks);
        ASSERT_FALSE(gear->impl()->options.generate_unique);
      }
    }
  }

  gearman_client_free(gear);

  return TEST_SUCCESS;
}

static test_return_t echo_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  gearman_string_t value= { test_literal_param("This is my echo test") };

  if (GEARMAN_SUCCESS !=  gearman_client_echo(client, gearman_string_param(value)))
  {
    Error << gearman_client_error(client);
  }
  ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_echo(client, gearman_string_param(value)));

  return TEST_SUCCESS;
}

static void log_printer(const char *line, gearman_verbose_t verbose, void*)
{
  Out << gearman_verbose_name(verbose) <<  " : " << line;
}

static test_return_t gearman_client_set_log_fn_TEST(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  gearman_log_fn *func= log_printer;
  gearman_client_set_log_fn(client, func, NULL, GEARMAN_VERBOSE_MAX);

  gearman_string_t value= { test_literal_param("This is my echo test") };

  ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_echo(client, gearman_string_param(value)));

  return TEST_SUCCESS;
}

static test_return_t submit_job_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_TRUE(worker_function);
  gearman_string_t value= { test_literal_param("submit_job_test") };

  size_t result_length;
  gearman_return_t rc;
  void *job_result= gearman_client_do(client, worker_function, NULL, gearman_string_param(value), &result_length, &rc);

  ASSERT_EQ(GEARMAN_SUCCESS, rc);

  ASSERT_TRUE(job_result);
  ASSERT_EQ(gearman_size(value), result_length);

  test_memcmp(gearman_c_str(value), job_result, gearman_size(value));

  free(job_result);

  return TEST_SUCCESS;
}

static test_return_t submit_echo_job_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_echo(client, test_literal_param("foo")));
  
  return submit_job_test(object);
}

static test_return_t submit_null_job_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;

  ASSERT_TRUE(client);

  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_NOT_NULL(worker_function);

  size_t result_length;
  gearman_return_t rc;
  void *job_result= gearman_client_do(client, worker_function, NULL, NULL, 0,
                                      &result_length, &rc);
  ASSERT_EQ(GEARMAN_SUCCESS, rc);
  test_zero(result_length);
  test_false(job_result);

  return TEST_SUCCESS;
}

static test_return_t submit_exception_job_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_TRUE(worker_function);

  size_t result_length;
  gearman_return_t rc;
  void *job_result= gearman_client_do(client, worker_function, NULL,
                                      test_literal_param("exception"),
                                      &result_length, &rc);
  if (gearman_client_has_option(client, GEARMAN_CLIENT_EXCEPTION))
  {
    ASSERT_NOT_NULL(job_result);
    ASSERT_EQ(GEARMAN_WORK_EXCEPTION, rc);
    test_memcmp(EXCEPTION_MESSAGE, job_result, result_length);
    free(job_result);
  }
  else
  {
    ASSERT_NULL(job_result);
    ASSERT_EQ(GEARMAN_WORK_EXCEPTION, rc);
  }

  return TEST_SUCCESS;
}

static test_return_t submit_warning_job_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_TRUE(worker_function);

  size_t result_length;
  gearman_return_t rc;
  void *job_result= gearman_client_do(client, worker_function, NULL,
                                      test_literal_param("warning"),
                                      &result_length, &rc);
  ASSERT_NOT_NULL(job_result);
  ASSERT_EQ(GEARMAN_SUCCESS, rc);
  test_memcmp("warning", job_result, result_length);
  free(job_result);

  return TEST_SUCCESS;
}

static test_return_t submit_fail_job_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);


  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_TRUE(worker_function);

  size_t result_length;
  gearman_return_t rc;
  void *job_result= gearman_client_do(client, worker_function, NULL, "fail", 4,
                                      &result_length, &rc);
  ASSERT_EQ(GEARMAN_WORK_FAIL, rc);
  test_false(job_result);
  test_false(result_length);

  return TEST_SUCCESS;
}

static test_return_t submit_multiple_do(void *object)
{
  for (uint32_t x= 0; x < 100 /* arbitrary */; x++)
  {
    libgearman::Client client((gearman_client_st *)object);
    gearman_client_set_context(&client, gearman_client_context((gearman_client_st *)object));

    uint32_t option= uint32_t(random() %3);

    switch (option)
    {
    case 0:
      ASSERT_EQ(TEST_SUCCESS, submit_null_job_test(&client));
      break;

    case 1:
      ASSERT_EQ(TEST_SUCCESS, submit_job_test(&client));
      break;

    default:
    case 2:
      ASSERT_EQ(TEST_SUCCESS, submit_exception_job_test(&client));
      break;
    }
  }

  return TEST_SUCCESS;
}

static test_return_t gearman_client_job_status_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  gearman_string_t value= { test_literal_param("background_test") };

  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_TRUE(worker_function);

  gearman_job_handle_t job_handle;
  ASSERT_EQ(GEARMAN_SUCCESS,
               gearman_client_do_background(client, worker_function, NULL, gearman_string_param(value), job_handle));

  gearman_return_t ret;
  bool is_known;
  do
  {
    bool is_running;
    uint32_t numerator;
    uint32_t denominator;

    ASSERT_EQ(GEARMAN_SUCCESS,
                 (ret= gearman_client_job_status(client, job_handle, &is_known, &is_running, &numerator, &denominator)));
  } while (gearman_continue(ret) and is_known);

  return TEST_SUCCESS;
}

static void* test_malloc_fn(size_t size, void *context)
{
  bool *malloc_check= (bool *)context;
  *malloc_check= true;
  return malloc(size);
}

static void test_free_fn(void *ptr, void *context)
{
  bool *free_check= (bool *)context;
  *free_check= true;
  free(ptr);
}

static test_return_t gearman_client_set_workload_malloc_fn_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  bool malloc_check= false;
  gearman_client_set_workload_malloc_fn(client, test_malloc_fn, &malloc_check);

  ASSERT_EQ(TEST_SUCCESS, submit_job_test(object));
  ASSERT_EQ(true, malloc_check);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_workload_free_fn_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  bool free_check= false;
  gearman_client_set_workload_free_fn(client, test_free_fn, &free_check);

  ASSERT_EQ(TEST_SUCCESS, submit_job_test(object));
  ASSERT_EQ(true, free_check);

  return TEST_SUCCESS;
}

struct _alloc_test_st {
  int64_t count;
  int64_t total;

  _alloc_test_st():
    count(0),
    total(0)
  { }

  void add() 
  {
    count++;
    total++;
  }

  void subtract() 
  {
    count--;
  }

  bool success() // count is valid as 1 only with the current test
  {
    if (total and count == 1)
    {
      return true;
    }

    std::cerr << __func__ << ":" << __LINE__ << " Total:" <<  total << " Count:" << count << std::endl;

    return false;
  }
};

static void* test_malloc_count_fn(size_t size, void *context)
{
  _alloc_test_st *_foo= (_alloc_test_st *)context;

  _foo->add();
  
  return malloc(size);
}

static void test_free_count_fn(void *ptr, void *context)
{
  _alloc_test_st *_foo= (_alloc_test_st *)context;

  _foo->subtract();

  return free(ptr);
}


static test_return_t gearman_client_set_workload_allocators_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  _alloc_test_st _foo;

  gearman_client_set_workload_malloc_fn(client, test_malloc_count_fn, &_foo);
  gearman_client_set_workload_free_fn(client, test_free_count_fn, &_foo);

  ASSERT_EQ(TEST_SUCCESS, submit_job_test(object));
  ASSERT_TRUE(_foo.success());

  return TEST_SUCCESS;
}

static test_return_t gearman_client_job_status_with_return(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  gearman_string_t value= { test_literal_param("background_test") };

  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_TRUE(worker_function);

  gearman_job_handle_t job_handle;
  ASSERT_EQ(GEARMAN_SUCCESS,
               gearman_client_do_background(client, worker_function, NULL, gearman_string_param(value), job_handle));

  gearman_return_t ret;
  do
  {
    uint32_t numerator;
    uint32_t denominator;

    ret= gearman_client_job_status(client, job_handle, NULL, NULL, &numerator, &denominator);
  } while (gearman_continue(ret));
  ASSERT_EQ(GEARMAN_SUCCESS, ret);

  return TEST_SUCCESS;
}

static test_return_t background_failure_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  gearman_job_handle_t job_handle;
  bool is_known;
  bool is_running;
  uint32_t numerator;
  uint32_t denominator;

  gearman_return_t rc= gearman_client_do_background(client, "does_not_exist", NULL,
                                                    test_literal_param("background_failure_test"),
                                                    job_handle);
  ASSERT_EQ(GEARMAN_SUCCESS, rc);

  do {
    rc= gearman_client_job_status(client, job_handle, &is_known, &is_running,
                                  &numerator, &denominator);
    ASSERT_TRUE(is_known == true and is_running == false and numerator == 0 and denominator == 0);
  } while (gearman_continue(rc)); // We do not test for is_known since the server will keep the job around until a worker comes along
  ASSERT_EQ(GEARMAN_SUCCESS, rc);

  return TEST_SUCCESS;
}

static test_return_t add_servers_test(void *)
{
  libgearman::Client client;

  ASSERT_EQ(GEARMAN_SUCCESS,
               gearman_client_add_servers(&client, "localhost:4730,localhost"));

  if (libtest::check_dns())
  {
    ASSERT_EQ(GEARMAN_GETADDRINFO,
                 gearman_client_add_servers(&client, "www.google.com:7003,does_not_exist.google.com:12345"));
  }

  return TEST_SUCCESS;
}

static test_return_t hostname_resolution(void *)
{
  test_skip_valgrind();

  libgearman::Client client;

  test_skip(GEARMAN_SUCCESS, gearman_client_add_servers(&client, "exist.gearman.info:12345"));

  ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_error_code(&client));

#if defined(__FreeBSD__) && __FreeBSD__
  ASSERT_EQ(GEARMAN_TIMEOUT,
               gearman_client_echo(&client, test_literal_param("foo")));
#else
  ASSERT_EQ(GEARMAN_COULD_NOT_CONNECT,
               gearman_client_echo(&client, test_literal_param("foo")));
#endif

  return TEST_SUCCESS;
}

static test_return_t gearman_client_st_id_t_TEST(gearman_client_st*)
{
  return TEST_SKIPPED;

  // @todo, test the internal value of client, no API is available for this
  // right now.
#if 0
  gearman_client_st *client= gearman_client_create(NULL);
  ASSERT_TRUE(client);
  ASSERT_EQ(false, gearman_id_valid(client));
  gearman_client_free(client);

  return TEST_SUCCESS;
#endif
}

static test_return_t gearman_worker_st_id_t_TEST(gearman_client_st*)
{
  gearman_worker_st *worker= gearman_worker_create(NULL);
  ASSERT_TRUE(worker);
  gearman_id_t id= gearman_worker_id(worker);
  ASSERT_EQ(true, gearman_id_valid(id));
  gearman_worker_free(worker);

  return TEST_SUCCESS;
}

static test_return_t bug_518512_test(void *)
{
  size_t result_size;

  libgearman::Client client(libtest::default_port());

  gearman_client_set_timeout(&client, 0);
  gearman_return_t rc;
  void *result= gearman_client_do(&client, "client_test_temp", NULL, NULL, 0,
                                  &result_size, &rc);
  ASSERT_EQ(GEARMAN_TIMEOUT, rc);
  test_false(result);
  test_zero(result_size);

  gearman_function_t func_arg= gearman_function_create_v1(client_test_temp_worker);
  std::unique_ptr<worker_handle_st> completion_worker(test_worker_start(libtest::default_port(), NULL, "client_test_temp",
                                                                        func_arg, NULL, gearman_worker_options_t()));

  libgearman::Client client2(libtest::default_port());
  gearman_client_set_timeout(&client2, -1);
  result= gearman_client_do(&client2, "client_test_temp", NULL, NULL, 0,
                            &result_size, &rc);
  ASSERT_EQ(GEARMAN_SUCCESS, rc);
  test_false(result);
  test_zero(result_size);

  return TEST_SUCCESS;
}

#define NUMBER_OF_WORKERS 2

static test_return_t loop_test(void *)
{
  pthread_t one;
  pthread_t two;

  struct worker_handle_st *handles[NUMBER_OF_WORKERS];
  gearman_function_t func_arg= gearman_function_create_v1(client_test_temp_worker);
  for (size_t x= 0; x < NUMBER_OF_WORKERS; x++)
  {
    handles[x]= test_worker_start(libtest::default_port(), NULL, "client_test_temp",
                                  func_arg, NULL, gearman_worker_options_t());
  }

  gearman_return_t one_rc= GEARMAN_SUCCESS;
  pthread_create(&one, NULL, client_thread, &one_rc);

  gearman_return_t two_rc= GEARMAN_SUCCESS;
  pthread_create(&two, NULL, client_thread, &two_rc);

  void *unused;
  pthread_join(one, &unused);
  pthread_join(two, &unused);

  for (size_t x= 0; x < NUMBER_OF_WORKERS; x++)
  {
    delete handles[x];
  }

  ASSERT_EQ(GEARMAN_SUCCESS, one_rc);
  ASSERT_EQ(GEARMAN_SUCCESS, two_rc);

  return TEST_SUCCESS;
}

static test_return_t regression_785203_do_test(void *)
{
  libgearman::Client client(libtest::default_port());

  gearman_client_add_options(&client, GEARMAN_CLIENT_FREE_TASKS);
  { // All defaults, except timeout_return
    ASSERT_TRUE(gearman_is_allocated(&client));
    test_false((&client)->impl()->options.non_blocking);
    test_false((&client)->impl()->options.unbuffered_result);
    test_false((&client)->impl()->options.no_new);
    ASSERT_TRUE((&client)->impl()->options.free_tasks);
  }

  gearman_function_t func= gearman_function_create_v2(echo_or_react_worker_v2);
  std::unique_ptr<worker_handle_st> handle(test_worker_start(libtest::default_port(), NULL,
                                                             __func__,
                                                             func, NULL, gearman_worker_options_t()));

  gearman_return_t rc;
  size_t result_length;
  void *result= gearman_client_do(&client, __func__, NULL, 
                                  test_literal_param("keep it rocking and sing"),
                                  &result_length, &rc);
  ASSERT_TRUE(result);
  free(result);

  return TEST_SUCCESS;
}

static test_return_t regression_785203_do_background_test(void *object)
{
  gearman_client_st *original_client= (gearman_client_st *)object;
  ASSERT_TRUE(original_client);

  gearman_function_t echo_react_chunk_fn_v2= gearman_function_create_v2(echo_or_react_worker_v2);
  std::unique_ptr<worker_handle_st> handle(test_worker_start(libtest::default_port(), NULL,
                                                             __func__,
                                                             echo_react_chunk_fn_v2, NULL, gearman_worker_options_t()));

  libgearman::Client client(libtest::default_port());

  gearman_client_add_options(&client, GEARMAN_CLIENT_FREE_TASKS);
  { // All defaults, except timeout_return
    ASSERT_TRUE(gearman_is_allocated((&client)));
    test_false((&client)->impl()->options.non_blocking);
    test_false((&client)->impl()->options.unbuffered_result);
    test_false((&client)->impl()->options.no_new);
    ASSERT_TRUE((&client)->impl()->options.free_tasks);
  }

  gearman_job_handle_t job_handle;
  ASSERT_EQ(GEARMAN_SUCCESS,
               gearman_client_do_background(&client, __func__, 
                                            NULL,  // No unique requested
                                            test_literal_param("keep it rocking and sing"),
                                            job_handle));

  gearman_return_t ret;
  do
  {
    uint32_t numerator;
    uint32_t denominator;

    ret= gearman_client_job_status(&client, job_handle, NULL, NULL, &numerator, &denominator);
  } while (gearman_continue(ret));
  ASSERT_EQ(GEARMAN_SUCCESS, ret);

  return TEST_SUCCESS;
}

static test_return_t regression2_TEST(void *)
{
  libgearman::Client client;

  size_t result_length;
  gearman_return_t rc;
  void *job_result= gearman_client_do(&client,
                                      __func__, // Worker does not exist.
                                      NULL,  // no unique
                                      test_literal_param("submit_log_failure"),
                                      &result_length, &rc);
  ASSERT_EQ(GEARMAN_NO_SERVERS, rc);
  test_false(job_result);
  test_zero(result_length);

  return TEST_SUCCESS;
}

static test_return_t gearman_worker_timeout_TEST(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  gearman_function_t dreaming_fn= gearman_function_create(echo_or_react_worker_v2);
  std::unique_ptr<worker_handle_st> worker_handle(test_worker_start(libtest::default_port(), NULL,
                                                                    __func__,
                                                                    dreaming_fn, NULL,
                                                                    gearman_worker_options_t(),
                                                                    0));

  /*
    The client should get a timeout since the "sleeper" will sleep longer then the timeout.
  */
  size_t result_length;
  gearman_return_t rc;
  void *job_result= gearman_client_do(client,
                                      __func__,  // Our sleeper function
                                      NULL, // No unique 
                                      gearman_literal_param("sleep"), // We send "sleep" to tell the sleeper to sleep
                                      &result_length, &rc);
  ASSERT_EQ(GEARMAN_SUCCESS, rc);
  ASSERT_TRUE(job_result);
  ASSERT_EQ(sizeof("slept") -1, result_length);
  test_memcmp("slept", job_result, 5);
  free(job_result);

  return TEST_SUCCESS;
}

static test_return_t gearman_worker_timeout_TIMEOUT_TEST(void *object)
{
  // This test currently takes to long.
  test_skip_valgrind();

  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  ASSERT_TRUE(ECHO_OR_REACT_DREAM);
  int timeout= ECHO_OR_REACT_DREAM/4;

  gearman_function_t dreaming_fn= gearman_function_create(echo_or_react_worker_v2);
  std::unique_ptr<worker_handle_st> worker_handle(test_worker_start(libtest::default_port(), NULL,
                                                                    __func__,
                                                                    dreaming_fn, NULL,
                                                                    gearman_worker_options_t(),
                                                                    timeout));

  /*
    The client should get a timeout since the "sleeper" will sleep longer then the timeout.
  */
  size_t result_length;
  gearman_return_t rc;
  void *job_result= gearman_client_do(client,
                                      __func__,  // Our sleeper function
                                      NULL, // No unique 
                                      gearman_literal_param("sleep"), // We send "sleep" to tell the sleeper to sleep
                                      &result_length, &rc);
  ASSERT_EQ(GEARMAN_SUCCESS, rc);
  ASSERT_TRUE(job_result);
  ASSERT_EQ(sizeof("slept") -1, result_length);
  test_memcmp("slept", job_result, 5);
  free(job_result);

  return TEST_SUCCESS;
}

static test_return_t regression_975591_TEST(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);

  gearman_function_t dreaming_fn= gearman_function_create(echo_or_react_worker_v2);
  std::unique_ptr<worker_handle_st> worker_handle(test_worker_start(libtest::default_port(), NULL,
                                                                    __func__,
                                                                    dreaming_fn, NULL,
                                                                    gearman_worker_options_t(),
                                                                    0));
  size_t payload_size[] = { 100, 1000, 10000, 1000000, 1000000, 0 };
  libtest::vchar_t payload;
  for (size_t *ptr= payload_size; *ptr; ptr++)
  {
    payload.reserve(*ptr);
    for (size_t x= payload.size(); x < *ptr; x++)
    {
      payload.push_back(rand());
    }

    size_t result_length;
    gearman_return_t rc;
    char *job_result= (char*)gearman_client_do(client, __func__,
                                               NULL, 
                                               &payload[0], payload.size(),
                                               &result_length, &rc);
    ASSERT_EQ(GEARMAN_SUCCESS, rc);
    ASSERT_EQ(payload.size(), result_length);
    test_memcmp(&payload[0], job_result, result_length);
    free(job_result);
  }

  return TEST_SUCCESS;
}

static void log_counter(const char *line, gearman_verbose_t verbose,
                        void *context)
{
  uint32_t *counter= (uint32_t *)context;

  (void)verbose;
  (void)line;

  *counter= *counter + 1;
}

static test_return_t submit_log_failure_TEST(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;
  ASSERT_TRUE(client);
  gearman_string_t value= { test_literal_param("submit_log_failure") };

  gearman_log_fn *func= log_counter;
  uint32_t global_counter= 0;

  gearman_client_set_log_fn(client, func, &global_counter, GEARMAN_VERBOSE_MAX);

  test_null(client->impl()->universal.con_list);

  const char *worker_function= (const char *)gearman_client_context(client);
  ASSERT_TRUE(worker_function);

  size_t result_length;
  gearman_return_t rc;
  test_null(client->impl()->task);
  void *job_result= gearman_client_do(client, worker_function, NULL, 
                                      gearman_string_param(value),
                                      &result_length, &rc);
  ASSERT_EQ(GEARMAN_NO_SERVERS, rc);
  test_false(job_result);
  test_zero(result_length);
  test_null(client->impl()->task);
  ASSERT_TRUE(global_counter);

  return TEST_SUCCESS;
}

static test_return_t strerror_count(void *)
{
  ASSERT_EQ((int)GEARMAN_MAX_RETURN, 53);

  return TEST_SUCCESS;
}

#undef MAKE_NEW_STRERROR

static test_return_t strerror_strings(void *)
{
  uint32_t values[]= {
    2723107532U, 1294272985U, 949848612U, 646434617U, 
    2273096667U, 3411376012U, 978198404U, 2644287234U, 
    1762137345U, 1727436301U, 1103093142U, 2958899803U, 
    3844590487U, 3520316764U, 3288532333U, 697573278U, 
    2328987341U, 1321921098U, 1475770122U, 4011631587U, 
    2468981698U, 2935753385U, 884320816U, 3006705975U, 
    2840498210U, 2953034368U, 501858685U, 1635925784U, 
    880765771U, 15612712U, 1489284002U, 2968621609U, 
    79936336U, 3059874010U, 3562217099U, 13337402U, 
    132823274U, 3950859856U, 237150774U, 290535510U, 
    2101976744U, 2262698284U, 3182950564U, 2391595326U, 
    1764731897U, 3485422815U, 99607280U, 2348849961U, 
    607991020U, 1597605008U, 1377573125U, 723914800U, 3144965656U };

  for (int rc= GEARMAN_SUCCESS; rc < GEARMAN_MAX_RETURN; rc++)
  {
    uint32_t hash_val;
    const char *msg=  gearman_strerror((gearman_return_t)rc);
    hash_val= internal_generate_hash(msg, strlen(msg));
    ASSERT_EQ_(values[rc], hash_val, "New Value for %s at %d: %uU\n", msg, rc, hash_val);
  }

  return TEST_SUCCESS;
}

#define REGRESSION_FUNCTION_833394_54 "54_char_function_name________________________________"
#define REGRESSION_FUNCTION_833394_55 "55_char_function_name_________________________________"
#define REGRESSION_FUNCTION_833394_65 "65_char_function_name___________________________________________"

static test_return_t regression_833394_test(void *object)
{
  gearman_client_st *client= (gearman_client_st *)object;

  gearman_function_t echo_react_chunk_fn_v2= gearman_function_create_v2(echo_or_react_worker_v2);
  std::unique_ptr<worker_handle_st> handle(test_worker_start(libtest::default_port(), NULL, REGRESSION_FUNCTION_833394_54, echo_react_chunk_fn_v2, NULL, gearman_worker_options_t()));
  std::unique_ptr<worker_handle_st> handle1(test_worker_start(libtest::default_port(), NULL, REGRESSION_FUNCTION_833394_55, echo_react_chunk_fn_v2, NULL, gearman_worker_options_t()));
  std::unique_ptr<worker_handle_st> handle2(test_worker_start(libtest::default_port(), NULL, REGRESSION_FUNCTION_833394_65, echo_react_chunk_fn_v2, NULL, gearman_worker_options_t()));

  for (size_t x= 0; x < 100; x++)
  {
    ASSERT_TRUE(client);
    size_t result_length;
    gearman_return_t rc;
    char *job_result= (char*)gearman_client_do(client, REGRESSION_FUNCTION_833394_54, 
                                               NULL, 
                                               test_literal_param("this should be echo'ed"),
                                               &result_length, &rc);
    ASSERT_EQ(GEARMAN_SUCCESS, rc);
    ASSERT_TRUE(job_result);
    ASSERT_EQ(test_literal_param_size("this should be echo'ed"), result_length);
    free(job_result);
  }

  for (size_t x= 0; x < 100; x++)
  {
    ASSERT_TRUE(client);
    size_t result_length;
    gearman_return_t rc;
    char *job_result= (char*)gearman_client_do(client, REGRESSION_FUNCTION_833394_55, 
                                               NULL, 
                                               test_literal_param("this should be echo'ed"),
                                               &result_length, &rc);
    ASSERT_EQ(GEARMAN_SUCCESS, rc);
    ASSERT_TRUE(job_result);
    ASSERT_EQ(test_literal_param_size("this should be echo'ed"), result_length);
    free(job_result);
  }

  for (size_t x= 0; x < 100; x++)
  {
    ASSERT_TRUE(client);
    size_t result_length;
    gearman_return_t rc;
    char *job_result= (char*)gearman_client_do(client, REGRESSION_FUNCTION_833394_65, 
                                               NULL, 
                                               test_literal_param("this should be echo'ed"),
                                               &result_length, &rc);
    ASSERT_EQ(GEARMAN_SUCCESS, rc);
    ASSERT_TRUE(job_result);
    ASSERT_EQ(test_literal_param_size("this should be echo'ed"), result_length);
    free(job_result);
  }

  return TEST_SUCCESS;
}

static test_return_t GEARMAN_SUCCESS_TEST(void*)
{
  ASSERT_EQ(0, int(GEARMAN_SUCCESS));

  return TEST_SUCCESS;
}

static test_return_t GEARMAN_FAIL_COMPAT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_FAIL, GEARMAN_FATAL);
  ASSERT_EQ(GEARMAN_FAIL, GEARMAN_WORK_FAIL);

  return TEST_SUCCESS;
}

static test_return_t GEARMAN_ERROR_COMPAT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_ERROR, GEARMAN_WORK_ERROR);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_identifier_TEST(void* object)
{
  gearman_client_st *client= (gearman_client_st *)object;

  ASSERT_EQ(GEARMAN_SUCCESS,
               gearman_client_set_identifier(client, test_literal_param(__func__)));

  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_identifier_plus_work_TEST(void* object)
{
  ASSERT_EQ(TEST_SUCCESS, gearman_client_set_identifier_TEST(object));
  ASSERT_EQ(TEST_SUCCESS, regression_833394_test(object));

  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_identifier_plus_random_TEST(void* object)
{
  for (size_t x= 0; x < 10; x++)
  {
    switch (random() %3)
    {
    case 0:
      ASSERT_EQ(TEST_SUCCESS, gearman_client_set_identifier_TEST(object));
      break;

    case 1:
      ASSERT_EQ(TEST_SUCCESS, regression_833394_test(object));
      break;

    default:
      ASSERT_EQ(TEST_SUCCESS, regression_975591_TEST(object));
    }
  }

  return TEST_SUCCESS;
}

static test_return_t gearman_client_cancel_job_TEST(void* object)
{
  gearman_client_st *client= (gearman_client_st *)object;

  gearman_job_handle_t job_handle;
  strcpy(job_handle, __func__);

  // For the moment we won't test the return value since this will change once
  // we formalize the behavior.
  test_compare(GEARMAN_JOB_NOT_FOUND, gearman_client_cancel_job(client, job_handle));

  return TEST_SUCCESS;
}


static test_return_t gearman_client_free_TEST(void *)
{
  gearman_client_free(NULL);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_error_TEST(void *)
{
  test_null(gearman_client_error(NULL));

  return TEST_SUCCESS;
}

static test_return_t gearman_client_error_no_error_TEST(void *)
{
  libgearman::Client client;
  test_null(gearman_client_error(&client));

  return TEST_SUCCESS;
}

static test_return_t gearman_client_errno_TEST(void *)
{
  ASSERT_EQ(EINVAL, gearman_client_errno(NULL));

  return TEST_SUCCESS;
}

static test_return_t gearman_client_errno_no_error_TEST(void *)
{
  libgearman::Client client;
  ASSERT_EQ(0, gearman_client_errno(&client));

  return TEST_SUCCESS;
}

static test_return_t gearman_client_options_SSL_TEST(void *)
{
  libgearman::Client client;
  ASSERT_FALSE(gearman_client_has_option(&client, GEARMAN_CLIENT_SSL));
  gearman_client_add_options(&client, GEARMAN_CLIENT_SSL);
  ASSERT_TRUE(gearman_client_has_option(&client, GEARMAN_CLIENT_SSL));

  return TEST_SUCCESS;
}

static test_return_t gearman_client_options_TEST(void *)
{
  ASSERT_EQ(gearman_client_options_t(), gearman_client_options(NULL));

  return TEST_SUCCESS;
}

static test_return_t __SETUP(client_test_st* test, const gearman_function_t& echo_react_fn)
{
  ASSERT_TRUE(test);

  test->set_worker_name(WORKER_FUNCTION_NAME);

  test->push(test_worker_start(libtest::default_port(),
                               test->session_namespace(),
                               test->worker_name(),
                               echo_react_fn, NULL, gearman_worker_options_t()));

  return TEST_SUCCESS;
}

static test_return_t chunk_v1_SETUP(void *object)
{
  gearman_function_t echo_react_chunk_fn= gearman_function_create_v1(echo_or_react_chunk_worker);
  ASSERT_EQ(TEST_SUCCESS, __SETUP((client_test_st *)object, echo_react_chunk_fn));

  return TEST_SUCCESS;
}

static test_return_t chunk_v2_SETUP(void *object)
{
  gearman_function_t echo_react_chunk_fn= gearman_function_create_v2(echo_or_react_chunk_worker_v2);
  ASSERT_EQ(TEST_SUCCESS, __SETUP((client_test_st *)object, echo_react_chunk_fn));

  return TEST_SUCCESS;
}

static test_return_t reset_SETUP(void *object)
{
  client_test_st* test= (client_test_st *)object;
  test->reset_clone();
  return TEST_SUCCESS;
}

static test_return_t default_v2_SETUP(void *object)
{
  gearman_function_t echo_react_fn= gearman_function_create_v2(echo_or_react_worker_v2);
  ASSERT_EQ(TEST_SUCCESS, __SETUP((client_test_st *)object, echo_react_fn));

  return TEST_SUCCESS;
}

static test_return_t GEARMAN_CLIENT_GENERATE_UNIQUE_SETUP(void *object)
{
  client_test_st* test= (client_test_st *)object;
  ASSERT_TRUE(test);

  ASSERT_EQ(TEST_SUCCESS, default_v2_SETUP(object));

  gearman_client_remove_options(test->client(), GEARMAN_CLIENT_GENERATE_UNIQUE);
  test_false(gearman_client_has_option(test->client(), GEARMAN_CLIENT_GENERATE_UNIQUE));

  return TEST_SUCCESS;
}

static test_return_t gearman_client_add_server_localhost_TEST(void*)
{
  libgearman::Client client;

  ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_add_server(&client, "localhost", libtest::default_port()));
  ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_echo(&client, test_literal_param(__func__)));

  return TEST_SUCCESS;
}

static test_return_t gearman_client_add_server_empty_quote_TEST(void*)
{
  libgearman::Client client;

  ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_add_server(&client, "", libtest::default_port()));
  ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_echo(&client, test_literal_param(__func__)));

  return TEST_SUCCESS;
}

static test_return_t gearman_client_create_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_st* clone;
  ASSERT_TRUE((clone= gearman_client_create(NULL)));

  gearman_client_free(clone);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_clone_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_st* clone;
  ASSERT_TRUE((clone= gearman_client_clone(NULL, NULL)));

  gearman_client_free(clone);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_free_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_free(NULL);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_error_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_client_error(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_error_code_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_client_error_code(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_options_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(gearman_client_options_t(GEARMAN_WORKER_MAX), gearman_client_options(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_has_option_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(false, gearman_client_has_option(NULL, GEARMAN_CLIENT_GENERATE_UNIQUE));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_options_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  // Hopefile valgrind will catch any issues that might crop up here
  gearman_client_set_options(NULL, GEARMAN_CLIENT_GENERATE_UNIQUE);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_add_options_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  // Hopefile valgrind will catch any issues that might crop up here
  gearman_client_add_options(NULL, GEARMAN_CLIENT_GENERATE_UNIQUE);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_remove_options_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  // Hopefile valgrind will catch any issues that might crop up here
  gearman_client_remove_options(NULL, GEARMAN_CLIENT_GENERATE_UNIQUE);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_timeout_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(-1, gearman_client_timeout(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_timeout_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_timeout(NULL, 23);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_context_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_client_context(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_context_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_context(NULL, NULL);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_log_fn_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_log_fn(NULL, NULL, NULL, GEARMAN_VERBOSE_FATAL);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_workload_malloc_fn_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_workload_malloc_fn(NULL, NULL, NULL);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_workload_free_fn_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_workload_free_fn(NULL, NULL, NULL);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_memory_allocators_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_client_set_memory_allocators(NULL, NULL, NULL, NULL, NULL, NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_add_server_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_client_add_server(NULL, NULL, 0));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_remove_servers_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_remove_servers(NULL);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_wait_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_client_wait(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_do_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(NULL, gearman_client_do(NULL, NULL, NULL, NULL, 0, NULL, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_do_low_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(NULL, gearman_client_do_low(NULL, NULL, NULL, NULL, 0, NULL, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_do_high_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(NULL, gearman_client_do_high(NULL, NULL, NULL, NULL, 0, NULL, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_do_job_handle_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_client_do_job_handle(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_do_background_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_client_do_background(NULL, NULL, NULL, NULL, 0, NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_do_low_background_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_client_do_low_background(NULL, NULL, NULL, NULL, 0, NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_job_status_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_client_job_status(NULL, NULL, NULL, NULL, NULL, NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_unique_status_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_status_t status= gearman_client_unique_status(NULL, NULL, 0);
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_status_return(status));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_do_high_background_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_client_do_high_background(NULL, NULL, NULL, NULL, 0, NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_add_task_status_by_unique_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(NULL, gearman_client_add_task_status_by_unique(NULL, NULL, NULL, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_echo_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_client_echo(NULL, NULL, 0));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_task_free_all_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_task_free_all(NULL);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_task_context_free_fn_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_task_context_free_fn(NULL, NULL);
  return TEST_SUCCESS;
}

static test_return_t gearman_client_add_task_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(NULL, gearman_client_add_task(NULL, NULL, NULL, NULL, NULL, NULL, 0, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_add_task_high_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(NULL, gearman_client_add_task_high(NULL, NULL, NULL, NULL, NULL, NULL, 0, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_add_task_low_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(NULL, gearman_client_add_task_low(NULL, NULL, NULL, NULL, NULL, NULL, 0, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_add_task_background_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(NULL, gearman_client_add_task_background(NULL, NULL, NULL, NULL, NULL, NULL, 0, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_add_task_high_background_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(NULL, gearman_client_add_task_high_background(NULL, NULL, NULL, NULL, NULL, NULL, 0, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_add_task_low_background_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(NULL, gearman_client_add_task_low_background(NULL, NULL, NULL, NULL, NULL, NULL, 0, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_add_task_status_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(NULL, gearman_client_add_task_status(NULL, NULL, NULL, NULL, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_workload_fn_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_workload_fn(NULL, NULL);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_created_fn_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_created_fn(NULL, NULL);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_status_fn_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_status_fn(NULL, NULL);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_clear_fn_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_clear_fn(NULL);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_fail_fn_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_fail_fn(NULL, NULL);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_complete_fn_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_complete_fn(NULL, NULL);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_exception_fn_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_exception_fn(NULL, NULL);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_run_tasks_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_client_run_tasks(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_compare_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(false, gearman_client_compare(NULL, NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_identifier_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_client_set_identifier(NULL, NULL, 0));
  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_namespace_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_namespace(NULL, NULL, 0);

  return TEST_SUCCESS;
}

static test_return_t gearman_client_set_server_option_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_client_set_namespace(NULL, NULL, 0);

  return TEST_SUCCESS;
}

static test_return_t gearman_task_free_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_task_free(NULL);
  return TEST_SUCCESS;
}

static test_return_t gearman_task_context_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_task_context(NULL);
  return TEST_SUCCESS;
}

static test_return_t gearman_task_set_context_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_task_set_context(NULL, NULL);
  return TEST_SUCCESS;
}

static test_return_t gearman_task_function_name_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_task_function_name(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_task_unique_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_task_unique(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_task_job_handle_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_task_job_handle(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_task_is_known_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(false, gearman_task_is_known(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_task_is_running_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(false, gearman_task_is_running(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_task_numerator_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(0, gearman_task_numerator(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_task_denominator_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(0, gearman_task_denominator(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_task_give_workload_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_task_give_workload(NULL, NULL, 0);
  return TEST_SUCCESS;
}

static test_return_t gearman_task_send_workload_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(0, gearman_task_send_workload(NULL, NULL, 0, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);

  return TEST_SUCCESS;
}

static test_return_t gearman_task_data_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_task_data(NULL));

  return TEST_SUCCESS;
}

static test_return_t gearman_task_data_size_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(0, gearman_task_data_size(NULL));

  return TEST_SUCCESS;
}

static test_return_t gearman_task_take_data_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  size_t taken_space;
  ASSERT_EQ(NULL, gearman_task_take_data(NULL, &taken_space));

  return TEST_SUCCESS;
}

static test_return_t gearman_task_recv_data_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_return_t ret;
  ASSERT_EQ(0, gearman_task_recv_data(NULL, NULL, 0, &ret));
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, ret);

  return TEST_SUCCESS;
}

static test_return_t gearman_task_error_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_task_error(NULL));

  return TEST_SUCCESS;
}

static test_return_t gearman_task_result_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_task_result(NULL));

  return TEST_SUCCESS;
}

static test_return_t gearman_task_return_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_task_return(NULL));

  return TEST_SUCCESS;
}

static test_return_t gearman_task_strstate_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_task_strstate(NULL));

  return TEST_SUCCESS;
}

static test_return_t gearman_job_free_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  gearman_job_free(NULL);
  return TEST_SUCCESS;
}

static test_return_t gearman_job_send_data_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_job_send_data(NULL, NULL, 0));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_send_warning_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_job_send_warning(NULL, NULL, 0));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_send_status_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_job_send_status(NULL, 0, 0));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_send_complete_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_job_send_complete(NULL, 0, 0));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_send_exception_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_job_send_exception(NULL, 0, 0));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_send_fail_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(GEARMAN_INVALID_ARGUMENT, gearman_job_send_fail(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_handle_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_job_handle(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_function_name_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_job_function_name(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_unique_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_job_unique(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_workload_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_job_unique(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_workload_size_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(0, gearman_job_workload_size(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_take_workload_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_job_take_workload(NULL, NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_clone_worker_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_job_clone_worker(NULL));
  return TEST_SUCCESS;
}

static test_return_t gearman_job_error_GEARMAN_INVALID_ARGUMENT_TEST(void*)
{
  ASSERT_EQ(NULL, gearman_job_error(NULL));
  return TEST_SUCCESS;
}

static test_return_t default_v1_SETUP(void *object)
{
  gearman_function_t echo_react_fn= gearman_function_create_v1(echo_or_react_worker);
  ASSERT_EQ(TEST_SUCCESS, __SETUP((client_test_st *)object, echo_react_fn));

  return TEST_SUCCESS;
}

static test_return_t pre_free_tasks(void *object)
{
  client_test_st *test= (client_test_st *)object;
  ASSERT_TRUE(test);

  ASSERT_EQ(TEST_SUCCESS, default_v2_SETUP(object));

  gearman_client_add_options(test->client(), GEARMAN_CLIENT_FREE_TASKS);
  ASSERT_TRUE(gearman_client_has_option(test->client(), GEARMAN_CLIENT_FREE_TASKS));

  return TEST_SUCCESS;
}

static test_return_t namespace_v1_SETUP(void *object)
{
  client_test_st *test= (client_test_st *)object;
  ASSERT_TRUE(test);

  test->session_namespace(NAMESPACE_KEY);
  ASSERT_EQ(TEST_SUCCESS, default_v1_SETUP(object));

  return TEST_SUCCESS;
}

static test_return_t namespace_v2_SETUP(void *object)
{
  client_test_st *test= (client_test_st *)object;
  ASSERT_TRUE(test);

  test->session_namespace(NAMESPACE_KEY);
  ASSERT_EQ(TEST_SUCCESS, default_v2_SETUP(object));

  return TEST_SUCCESS;
}

static test_return_t pre_logging(void *object)
{
  client_test_st *test= (client_test_st *)object;
  ASSERT_TRUE(test);

  test->clear_clone();
  test_null(test->client()->impl()->universal.con_list);
  test->set_worker_name(WORKER_FUNCTION_NAME);

  return TEST_SUCCESS;
}

static void *world_create(server_startup_st& servers, test_return_t& error)
{
  const char *argv[]= { "--exceptions", 0 };
  in_port_t first_port= libtest::default_port();
  ASSERT_TRUE(server_startup(servers, "gearmand", first_port, argv));

#if 0
  if (0)
  {
    const char *null_args[]= { 0 };
    in_port_t second_port= libtest::get_free_port();
    ASSERT_TRUE(server_startup(servers, "gearmand", second_port, null_args));
  }
#endif

  client_test_st *test= new client_test_st();
  ASSERT_TRUE(test);

  test->add_server(NULL, first_port);

  error= TEST_SUCCESS;

  return (void *)test;
}


static bool world_destroy(void *object)
{
  client_test_st *test= (client_test_st *)object;
  delete test;

  return TEST_SUCCESS;
}

test_st client_fork_TESTS[] ={
  {"fork()", 0, check_client_fork_TEST },
  {0, 0, 0}
};

test_st gearman_client_add_server_TESTS[] ={
  {"gearman_client_add_server(localhost)", 0, gearman_client_add_server_localhost_TEST },
  {"gearman_client_add_server(empty quote)", 0, gearman_client_add_server_empty_quote_TEST },
  {0, 0, 0}
};

test_st gearman_client_st_GEARMAN_INVALID_ARGUMENT_TESTS[] ={
  {"gearman_client_create()", 0, gearman_client_create_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_clone()", 0, gearman_client_clone_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_free()", 0, gearman_client_free_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_error()", 0, gearman_client_error_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_error_code()", 0, gearman_client_error_code_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_options()", 0, gearman_client_options_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_has_option()", 0, gearman_client_has_option_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_options()", 0, gearman_client_set_options_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_add_option()", 0, gearman_client_add_options_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_remove_options()", 0, gearman_client_remove_options_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_timeout()", 0, gearman_client_timeout_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_timeout()", 0, gearman_client_set_timeout_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_context()", 0, gearman_client_context_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_context()", 0, gearman_client_set_context_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_log_fn()", 0, gearman_client_set_log_fn_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_workload_malloc_fn()", 0, gearman_client_set_workload_malloc_fn_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_workload_free_fn()", 0, gearman_client_set_workload_free_fn_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_memory_allocators()", 0, gearman_client_set_memory_allocators_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_add_server()", 0, gearman_client_add_server_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_remove_servers()", 0, gearman_client_remove_servers_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_wait()", 0, gearman_client_wait_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_do()", 0, gearman_client_do_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_do_high()", 0, gearman_client_do_high_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_do_low()", 0, gearman_client_do_low_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_do_job_handle()", 0, gearman_client_do_job_handle_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_do_background()", 0, gearman_client_do_background_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_do_high_background()", 0, gearman_client_do_high_background_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_do_low_background()", 0, gearman_client_do_low_background_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_job_status()", 0, gearman_client_job_status_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_unique_status()", 0, gearman_client_unique_status_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_add_task_status_by_unique()", 0, gearman_client_add_task_status_by_unique_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_echo()", 0, gearman_client_echo_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_task_context_free_fn()", 0, gearman_client_set_task_context_free_fn_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_task_free_all()", 0, gearman_client_task_free_all_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_add_task()", 0, gearman_client_add_task_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_add_task_high()", 0, gearman_client_add_task_high_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_add_task_low()", 0, gearman_client_add_task_low_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_add_task_background()", 0, gearman_client_add_task_background_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_add_task_high_background()", 0, gearman_client_add_task_high_background_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_add_task_low_background()", 0, gearman_client_add_task_low_background_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_add_task_status()", 0, gearman_client_add_task_status_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_workload_fn()", 0, gearman_client_set_workload_fn_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_created_fn()", 0, gearman_client_set_created_fn_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_status_fn()", 0, gearman_client_set_status_fn_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_complete_fn()", 0, gearman_client_set_complete_fn_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_exception_fn()", 0, gearman_client_set_exception_fn_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_fail_fn()", 0, gearman_client_set_fail_fn_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_clear_fn()", 0, gearman_client_clear_fn_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_run_tasks()", 0, gearman_client_run_tasks_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_compare()", 0, gearman_client_compare_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_server_option()", 0, gearman_client_set_server_option_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_namespace()", 0, gearman_client_set_namespace_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_client_set_identifier()", 0, gearman_client_set_identifier_GEARMAN_INVALID_ARGUMENT_TEST },
  {0, 0, 0}
};

test_st gearman_task_st_GEARMAN_INVALID_ARGUMENT_TESTS[] ={
  {"gearman_task_free()", 0, gearman_task_free_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_context()", 0, gearman_task_context_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_set_context()", 0, gearman_task_set_context_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_function_name()", 0, gearman_task_function_name_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_unique()", 0, gearman_task_unique_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_job_handle()", 0, gearman_task_job_handle_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_is_known()", 0, gearman_task_is_known_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_is_running()", 0, gearman_task_is_running_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_numerator()", 0, gearman_task_numerator_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_denominator()", 0, gearman_task_denominator_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_give_workload()", 0, gearman_task_give_workload_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_send_workload()", 0, gearman_task_send_workload_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_data()", 0, gearman_task_data_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_data_size()", 0, gearman_task_data_size_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_take_data()", 0, gearman_task_take_data_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_recv_data()", 0, gearman_task_recv_data_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_error()", 0, gearman_task_error_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_result()", 0, gearman_task_result_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_return()", 0, gearman_task_return_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_task_strstate()", 0, gearman_task_strstate_GEARMAN_INVALID_ARGUMENT_TEST },
  {0, 0, 0}
};

test_st gearman_job_st_GEARMAN_INVALID_ARGUMENT_TESTS[] ={
  {"gearman_job_free()", 0, gearman_job_free_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_send_data()", 0, gearman_job_send_data_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_send_warning()", 0, gearman_job_send_warning_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_send_statu()", 0, gearman_job_send_status_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_send_complete()", 0, gearman_job_send_complete_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_send_exception()", 0, gearman_job_send_exception_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_send()", 0, gearman_job_send_fail_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_handle()", 0, gearman_job_handle_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_function_name()", 0, gearman_job_function_name_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_unique()", 0, gearman_job_unique_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_workload()", 0, gearman_job_workload_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_workload_size()", 0, gearman_job_workload_size_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_take_workload()", 0, gearman_job_take_workload_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_clone_worker()", 0, gearman_job_clone_worker_GEARMAN_INVALID_ARGUMENT_TEST },
  {"gearman_job_error()", 0, gearman_job_error_GEARMAN_INVALID_ARGUMENT_TEST },
  {0, 0, 0}
};

test_st gearman_client_set_identifier_TESTS[] ={
  {"gearman_client_set_identifier()", 0, gearman_client_set_identifier_TEST },
  {"gearman_client_set_identifier(with_work)", 0, gearman_client_set_identifier_plus_work_TEST },
  {"gearman_client_set_identifier(RANDOM)", 0, gearman_client_set_identifier_plus_random_TEST },
  {0, 0, 0}
};

test_st gearman_client_cancel_job_TESTS[] ={
  {"gearman_client_cancel_job()", 0, gearman_client_cancel_job_TEST },
  {0, 0, 0}
};

test_st gearman_return_t_TESTS[] ={
  {"GEARMAN_SUCCESS", 0, (test_callback_fn*)GEARMAN_SUCCESS_TEST },
  {"GEARMAN_FAIL == GEARMAN_FATAL == GEARMAN_WORK_FAIL", 0, (test_callback_fn*)GEARMAN_FAIL_COMPAT_TEST },
  {"GEARMAN_ERROR == GEARMAN_WORK_ERROR", 0, (test_callback_fn*)GEARMAN_ERROR_COMPAT_TEST },
  {0, 0, 0}
};

test_st gearman_id_t_TESTS[] ={
  {"gearman_client_st", 0, (test_callback_fn*)gearman_client_st_id_t_TEST },
  {"gearman_worker_st", 0, (test_callback_fn*)gearman_worker_st_id_t_TEST },
  {0, 0, 0}
};

test_st gearman_client_st_init_TESTS[] ={
  {"init", 0, init_test },
  {"allocation", 0, allocation_test },
  {"clone_test", 0, clone_test },
  {"echo", 0, echo_test },
  {"gearman_client_set_log_fn", 0, gearman_client_set_log_fn_TEST },
  {"options", 0, option_test },
  {0, 0, 0}
};

test_st gearman_client_st_TESTS[] ={
  {"submit_job", 0, submit_job_test },
  {"submit_echo_job", 0, submit_echo_job_test },
  {"submit_null_job", 0, submit_null_job_test },
  {"exception", 0, submit_exception_job_test },
  {"warning", 0, submit_warning_job_test },
  {"submit_multiple_do", 0, submit_multiple_do },
  {"gearman_client_job_status()", 0, gearman_client_job_status_test },
  {"gearman_client_job_status() with gearman_return_t", 0, gearman_client_job_status_with_return },
  {"background_failure", 0, background_failure_test },
  {"add_servers", 0, add_servers_test },
  {"gearman_client_add_servers(GEARMAN_GETADDRINFO)", 0, hostname_resolution },
  {"submit_fail_job", 0, submit_fail_job_test }, // Since this drops the worker, it must be last
  {0, 0, 0}
};

test_st loop_TESTS[] ={
  {"loop_test", 0, loop_test },
  {0, 0, 0}
};

test_st coalescence_TESTS[] ={
  {"basic coalescence", 0, coalescence_TEST },
  {"coalescence by hash", 0, coalescence_by_data_hash_TEST },
  {"coalescence by data", 0, coalescence_by_data_TEST },
  {"coalescence by data fail", 0, coalescence_by_data_FAIL_TEST },
  {0, 0, 0}
};

test_st gearman_command_t_tests[] ={
  {"gearman_command_t", 0, check_gearman_command_t },
  {0, 0, 0}
};

test_st gearman_worker_timeout_TESTS[] ={
  {"gearman_worker_timeout(0)", 0, gearman_worker_timeout_TEST },
  {"gearman_worker_timeout(DEFAULT_TIMEOUT)", 0, gearman_worker_timeout_TIMEOUT_TEST },
  {0, 0, 0}
};

test_st tests_log_TESTS[] ={
  {"submit_log_failure", 0, submit_log_failure_TEST },
  {0, 0, 0}
};

test_st regression2_TESTS[] ={
  {"stale client", 0, regression2_TEST },
  {0, 0, 0}
};


test_st gearman_strerror_tests[] ={
  {"count", 0, strerror_count },
  {"strings", 0, strerror_strings },
  {0, 0, 0}
};

test_st unique_tests[] ={
  {"compare sent unique", 0, unique_compare_test },
  {"gearman_client_unique_status(NOT_FOUND)", 0, gearman_client_unique_status_NOT_FOUND_TEST },
  {"gearman_client_unique_status()", 0, gearman_client_unique_status_TEST },
  {0, 0, 0}
};

test_st gearman_client_set_workload_malloc_fn_tests[] ={
  {"gearman_client_set_workload_malloc_fn()", 0, gearman_client_set_workload_malloc_fn_test },
  {"gearman_client_set_workload_free_fn()", 0, gearman_client_set_workload_free_fn_test },
  {"submit job and check for usage of both malloc/free", 0, gearman_client_set_workload_allocators_test },
  {0, 0, 0}
};

test_st regression_tests[] ={
#if 0
  {"lp:768317", 0, regression_768317_test },
#endif
  {"lp:785203 gearman_client_do()", 0, regression_785203_do_test },
  {"lp:785203 gearman_client_do_background()", 0, regression_785203_do_background_test },
  {"lp:833394 long function names", 0, regression_833394_test },
  {"lp:975591 Increase the work size past the standard buffer size", 0, regression_975591_TEST },
  {"lp:518512", 0, bug_518512_test },
  {0, 0, 0}
};

test_st gearman_client_do_tests[] ={
  {"gearman_client_do() fail huge unique", 0, gearman_client_do_huge_unique },
  {"gearman_client_do() with active background task", 0, gearman_client_do_with_active_background_task },
  {0, 0, 0}
};

test_st gearman_execute_tests[] ={
  {"gearman_execute()", 0, gearman_execute_test },
  {"gearman_execute() epoch", 0, gearman_execute_epoch_test },
  {"gearman_execute() epoch and test gearman_job_handle_t", 0, gearman_execute_epoch_check_job_handle_test },
  {"gearman_execute(GEARMAN_TIMEOUT)", 0, gearman_execute_timeout_test },
  {"gearman_execute() background", 0, gearman_execute_bg_test },
  {"gearman_execute() multiple background", 0, gearman_execute_multile_bg_test },
  {"gearman_execute() with NULL workload", 0, gearman_execute_NULL_workload_TEST },
  {"gearman_execute() with NULL workload", 0, gearman_execute_NULL_attr_NULL_workload_TEST },
  {"gearman_execute(GEARMAN_WORK_FAIL)", 0, gearman_execute_fail_test }, // Fail test must be run last
  {0, 0, 0}
};

test_st gearman_client_do_background_tests[] ={
  {"gearman_client_do_background()", 0, gearman_client_do_background_basic },
  {"gearman_client_do_high_background()", 0, gearman_client_do_high_background_basic },
  {"gearman_client_do_low_background()", 0, gearman_client_do_low_background_basic },
  {0, 0, 0}
};

test_st gearman_client_do_job_handle_tests[] ={
  {"gearman_client_do_job_handle() no active tasks", 0, gearman_client_do_job_handle_no_active_task },
  {"gearman_client_do_job_handle() follow do command", 0, gearman_client_do_job_handle_follow_do },
  {0, 0, 0}
};

test_st gearman_execute_partition_tests[] ={
  {"gearman_execute_by_partition() map reduce", 0, gearman_execute_partition_basic },
  {"gearman_execute_by_partition(GEARMAN_ARGUMENT_TOO_LARGE) map reduce", 0, gearman_execute_partition_check_parameters },
  {"gearman_execute_by_partition(GEARMAN_WORK_FAIL) map reduce", 0, gearman_execute_partition_workfail },
  {"gearman_execute_by_partition() fail in reduction", 0, gearman_execute_partition_fail_in_reduction },
#if 0
  {"gearman_execute() with V2 Worker that has aggregate defined", 0, gearman_execute_partition_use_as_function },
  {"gearman_execute_by_partition() no aggregate function", 0, gearman_execute_partition_no_aggregate },
#endif
  {0, 0, 0}
};

test_st gearman_client_set_server_option_tests[] ={
  {"gearman_client_set_server_option(exceptions)", 0, gearman_client_set_server_option_exception},
  {"gearman_client_set_server_option(bad)", 0, gearman_client_set_server_option_bad},
  {0, 0, 0}
};

test_st gearman_task_tests[] ={
  {"gearman_client_add_task() ", 0, gearman_client_add_task_test},
  {"gearman_client_add_task() bad workload", 0, gearman_client_add_task_test_bad_workload},
  {"gearman_client_add_task_background()", 0, gearman_client_add_task_background_test},
  {"gearman_client_add_task_low_background()", 0, gearman_client_add_task_low_background_test},
  {"gearman_client_add_task_high_background()", 0, gearman_client_add_task_high_background_test},
  {"gearman_client_add_task() exception", 0, gearman_client_add_task_exception},
  {"gearman_client_add_task() exception check returned string", 0, gearman_client_add_task_check_exception_TEST},
  {"gearman_client_add_task() warning", 0, gearman_client_add_task_warning},
  {"gearman_client_add_task(GEARMAN_NO_SERVERS)", 0, gearman_client_add_task_no_servers},
  {"gearman_client_set_task_context_free_fn()", 0, gearman_client_set_task_context_free_fn_test},
  {"gearman_client_add_task_status_by_unique(NOT_FOUND)", 0, gearman_client_add_task_status_by_unique_NOT_FOUND_TEST },
  {"gearman_client_add_task_status_by_unique()", 0, gearman_client_add_task_status_by_unique_TEST },
  {"gearman_client_add_task() fail", 0, gearman_client_add_task_test_fail}, // Fail needs to go last
  {0, 0, 0}
};

test_st gearman_client_st_NULL_invocation_TESTS[] ={
  {"gearman_client_free()", 0, gearman_client_free_TEST },
  {"gearman_client_error()", 0, gearman_client_error_TEST },
  {"gearman_client_error() no error", 0, gearman_client_error_no_error_TEST },
  {"gearman_client_errno()", 0, gearman_client_errno_TEST },
  {"gearman_client_errno() no error", 0, gearman_client_errno_no_error_TEST },
  {"gearman_client_options()", 0, gearman_client_options_TEST },
  {"gearman_client_options(SSL)", 0, gearman_client_options_SSL_TEST },
  {0, 0, 0}
};

test_st gearman_task_pause_tests[] ={
  {"gearman_client_add_task(GEARMAN_PAUSE)", 0, gearman_client_add_task_pause_test},
  {0, 0, 0}
};

test_st limit_tests[] ={
  {"GEARMAN_FUNCTION_MAX_SIZE", 0, function_name_limit_test},
  {"GEARMAN_UNIQUE_MAX_SIZE", 0, unique_name_limit_test},
  {0, 0, 0}
};

collection_st collection[] ={
  {"gearman_return_t", 0, 0, gearman_return_t_TESTS},
  {"init", 0, 0, gearman_client_st_init_TESTS},
  {"gearman_client_cancel_job()", 0, 0, gearman_client_cancel_job_TESTS },
  {"gearman_client_st GEARMAN_INVALID_ARGUMENT", 0, 0, gearman_client_st_GEARMAN_INVALID_ARGUMENT_TESTS },
  {"gearman_task_st GEARMAN_INVALID_ARGUMENT", 0, 0, gearman_task_st_GEARMAN_INVALID_ARGUMENT_TESTS },
  {"gearman_job_st GEARMAN_INVALID_ARGUMENT", 0, 0, gearman_job_st_GEARMAN_INVALID_ARGUMENT_TESTS },
  {"gearman_id_t", 0, 0, gearman_id_t_TESTS},
  {"gearman_strerror()", 0, 0, gearman_strerror_tests },
  {"gearman_client_st init", 0, 0, gearman_client_st_init_TESTS },
  {"gearman_client_add_server()", 0, 0, gearman_client_add_server_TESTS },
  {"gearman_client_st", default_v2_SETUP, 0, gearman_client_st_TESTS},
  {"gearman_client_st chunky", chunk_v1_SETUP, 0, gearman_client_st_TESTS}, // Test with a worker that will respond in part
  {"gearman_task_add_task() v1 workers", default_v1_SETUP, 0, gearman_task_tests},
  {"gearman_task_add_task() v2 workers", default_v2_SETUP, 0, gearman_task_tests},
  {"gearman_task_add_task() namespace v1", namespace_v1_SETUP, 0, gearman_task_tests},
  {"gearman_task_add_task() namespace v2", namespace_v2_SETUP, 0, gearman_task_tests},
  {"gearman_task_add_task() chunky v1 workers", chunk_v1_SETUP, 0, gearman_task_tests},
  {"gearman_task_add_task() chunky v2 workers", chunk_v2_SETUP, 0, gearman_task_tests},
  {"gearman_task_add_task(GEARMAN_CLIENT_FREE_TASKS)", pre_free_tasks, 0, gearman_task_tests},
  {"gearman_task_add_task(GEARMAN_CLIENT_GENERATE_UNIQUE)", GEARMAN_CLIENT_GENERATE_UNIQUE_SETUP, 0, gearman_task_tests},
  {"gearman_task_add_task(GEARMAN_PAUSE)", chunk_v1_SETUP, 0, gearman_task_pause_tests},
  {"gearman_task_add_task(GEARMAN_PAUSE)", chunk_v2_SETUP, 0, gearman_task_pause_tests},
  {"unique", unique_SETUP, 0, unique_tests},
  {"gearman_client_set_workload_malloc_fn()", default_v2_SETUP, 0, gearman_client_set_workload_malloc_fn_tests},
  {"gearman_client_do()", default_v2_SETUP, 0, gearman_client_do_tests},
  {"gearman_client_do() namespace v1", namespace_v1_SETUP, 0, gearman_client_do_tests},
  {"gearman_client_do() namespace v2", namespace_v2_SETUP, 0, gearman_client_do_tests},
  {"gearman_client_do(GEARMAN_CLIENT_FREE_TASKS)", pre_free_tasks, 0, gearman_client_do_tests},
  {"gearman_client_do_job_handle()", default_v2_SETUP, 0, gearman_client_do_job_handle_tests},
  {"gearman_client_do_job_handle() namespace v1", namespace_v1_SETUP, 0, gearman_client_do_job_handle_tests},
  {"gearman_client_do_job_handle() namespace v2", namespace_v2_SETUP, 0, gearman_client_do_job_handle_tests},
  {"gearman_client_do_job_handle(GEARMAN_CLIENT_FREE_TASKS)", pre_free_tasks, 0, gearman_client_do_job_handle_tests},
  {"gearman_client_do_background()", default_v2_SETUP, 0, gearman_client_do_background_tests},
  {"gearman_client_do_background(GEARMAN_CLIENT_FREE_TASKS)", pre_free_tasks, 0, gearman_client_do_background_tests},
  {"gearman_client_set_server_option", 0, 0, gearman_client_set_server_option_tests},
  {"gearman_execute()", default_v2_SETUP, 0, gearman_execute_tests},
  {"gearman_execute(GEARMAN_CLIENT_FREE_TASKS)", pre_free_tasks, 0, gearman_execute_tests},
  {"gearman_execute() chunked v1 return", chunk_v1_SETUP, 0, gearman_execute_tests},
  {"gearman_execute() chunked v2 return", chunk_v2_SETUP, 0, gearman_execute_tests},
  {"gearman_execute_partition()", partition_SETUP, 0, gearman_execute_partition_tests},
  {"gearman_execute_partition(GEARMAN_CLIENT_FREE_TASKS)", partition_free_SETUP, 0, gearman_execute_partition_tests},
  {"gearman_command_t", 0, 0, gearman_command_t_tests},
  {"coalescence", 0, 0, coalescence_TESTS},
  {"fork", fork_SETUP, 0, client_fork_TESTS },
  {"loop", 0, 0, loop_TESTS},
  {"limits", 0, 0, limit_tests },
  {"client-logging", pre_logging, 0, tests_log_TESTS },
  {"regression", 0, 0, regression_tests},
  {"regression2", reset_SETUP, 0, regression2_TESTS },
  {"gearman_worker_timeout()", default_v2_SETUP, 0, gearman_worker_timeout_TESTS },
  {"gearman_client_set_identifier()", default_v2_SETUP, 0, gearman_client_set_identifier_TESTS },
  {0, 0, 0, 0}
};

void get_world(libtest::Framework *world)
{
  world->collections(collection);
  world->create(world_create);
  world->destroy(world_destroy);
  world->set_runner(new GearmandRunner);
}
