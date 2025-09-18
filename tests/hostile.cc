/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2012 Data Differential, http://datadifferential.com/
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

#include <libgearman-1.0/gearman.h>

#include <libhostile/hostile.h>

#include "libgearman/client.hpp"
using namespace org::gearmand;

#include "tests/start_worker.h"

#include "tests/workers/v2/echo_or_react.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>

#define WORKER_FUNCTION_NAME "foo"

#ifndef SERVER_TARGET
#  define SERVER_TARGET "hostile-gearmand"
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
static bool has_hostile()
{
#if defined(HAVE_LIBHOSTILE)
  if (HAVE_LIBHOSTILE)
  {
    return true;
  }
#endif

  return false;
}
#pragma GCC diagnostic pop

static in_port_t hostile_server_port= 0;
static in_port_t& current_server_= hostile_server_port;

static void reset_server()
{
  current_server_= hostile_server_port;
}

static in_port_t current_server()
{
  return current_server_;
}

static void set_server(in_port_t& arg)
{
  current_server_= arg;
}

struct client_thread_context_st
{
  size_t count;
  size_t payload_size;

  client_thread_context_st() :
    count(0),
    payload_size(0)
  { }

  void increment()
  {
    count++;
  }
};

extern "C" {
  static __attribute__((noreturn)) void *client_thread(void *object)
  {
    client_thread_context_st *success= (client_thread_context_st *)object;
    fatal_assert(success);
    fatal_assert(success->count == 0);

    {
      libgearman::Client client(current_server());

      libtest::vchar_t payload;
      payload.resize(success->payload_size);

      gearman_client_set_timeout(&client, 1000);
      for (size_t x= 0; x < 100; x++)
      {
        int oldstate;
        pthread_setcanceltype(PTHREAD_CANCEL_DISABLE, &oldstate);
        gearman_return_t rc;
        void *value= gearman_client_do(&client, WORKER_FUNCTION_NAME,
                                       NULL,
                                       payload.size() ? &payload[0] : NULL,
                                       payload.size() ? random() % payload.size() : 0,
                                       NULL, &rc);

        fatal_assert(gearman_client_has_tasks(&client) == false);
        if (gearman_success(rc))
        {
          success->increment();
        }

        if (value)
        {
          free(value);
        }
        pthread_setcanceltype(oldstate, NULL);
      }
      
      fatal_assert(gearman_client_has_tasks(&client) == false);
    }

    pthread_exit(0);
  }
}

namespace {

#if defined(HAVE_PTHREAD_TIMEDJOIN_NP) && HAVE_PTHREAD_TIMEDJOIN_NP
  bool fill_timespec(struct timespec& ts)
  {
#if defined(HAVE_LIBRT) && HAVE_LIBRT
    if (HAVE_LIBRT) // This won't be called on OSX, etc,...
    {
      if (clock_gettime(CLOCK_REALTIME, &ts) == -1) 
      {
        Error << "clock_gettime(CLOCK_REALTIME) " << strerror(errno);
        return false;
      }
    }
#else
    {
      struct timeval tv;
      if (gettimeofday(&tv, NULL) == -1) 
      {
        Error << "gettimeofday() " << strerror(errno);
        return false;
      }

      TIMEVAL_TO_TIMESPEC(&tv, &ts);
    }
#endif

    return true;
  }
#endif

} // namespace

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
static bool join_thread(pthread_t& thread_arg)
{
  int error;

#if defined(HAVE_PTHREAD_TIMEDJOIN_NP) && HAVE_PTHREAD_TIMEDJOIN_NP
  if (HAVE_PTHREAD_TIMEDJOIN_NP)
  {
    int limit= 2;
    while (--limit)
    {
      struct timespec ts;
      if (fill_timespec(ts))
      {
        ts.tv_sec+= 30;
        switch ((error= pthread_timedjoin_np(thread_arg, NULL, &ts)))
        {
        case ETIMEDOUT:
          continue;

        case 0:
          return true;

        case ESRCH:
          return false;

        default:
          Error << "pthread_timedjoin_np() " << strerror(error);
          return false;
        }
      }
    }

    if ((error= pthread_cancel(thread_arg)) != 0)
    {
      Error << "pthread_cancel() " << strerror(error);
      return false;
    }

    return true;
  }
#endif

  if ((error= pthread_join(thread_arg, NULL)) != 0)
  {
    Error << "pthread_join() " << strerror(error);
    return false;
  }

  return true;
}
#pragma GCC diagnostic pop

static test_return_t send_random_port_data_TEST(void* )
{
  set_alarm(1200, 0);

  SimpleClient client("localhost", current_server());

  for (size_t x= 0; x < 200; ++x)
  {
    libtest::vchar_t message_;
    libtest::vchar_t response_;

    libtest::vchar::make(message_, size_t(random() % 1024));

    client.send_data(message_, response_);
  }

  return TEST_SUCCESS;
}

static test_return_t worker_ramp_exec(const size_t payload_size)
{
  set_alarm(1200, 0);

  std::vector<pthread_t> children;
  children.resize(number_of_cpus());

  std::vector<client_thread_context_st>  success;
  success.resize(children.size());

  for (size_t x= 0; x < children.size(); ++x)
  {
    success[x].payload_size= payload_size;
    pthread_create(&children[x], NULL, client_thread, &success[x]);
  }
  
  for (size_t x= 0; x < children.size(); ++x)
  {
    pthread_t& thread= children[x];
    bool join_success= false;
    int limit= 3;
    while (join_success == false and --limit)
    {
      join_success= join_thread(thread);
    }

    if (join_success == false)
    {
      pthread_cancel(thread);
      Error << "Something went very wrong, it is likely threads were not cleaned up";
    }
  }

  return TEST_SUCCESS;
}

static test_return_t worker_ramp_TEST(void *)
{
  return worker_ramp_exec(0);
}

static test_return_t worker_ramp_1K_TEST(void *)
{
  return worker_ramp_exec(1024);
}

static test_return_t worker_ramp_10K_TEST(void *)
{
  return worker_ramp_exec(1024*10);
}

static test_return_t skip_SETUP(void*)
{
  SKIP_IF(true);
  return TEST_SUCCESS;
}

static test_return_t worker_ramp_SETUP(void *object)
{
  worker_handles_st *handles= (worker_handles_st*)object;

  gearman_function_t echo_react_fn= gearman_function_create(echo_or_react_worker_v2);
  for (uint32_t x= 0; x < 10; x++)
  {
    worker_handle_st *worker;
    if ((worker= test_worker_start(current_server(), NULL, WORKER_FUNCTION_NAME, echo_react_fn, NULL, gearman_worker_options_t())) == NULL)
    {
      return TEST_FAILURE;
    }
    handles->push(worker);
  }

  return TEST_SUCCESS;
}

static test_return_t worker_ramp_TEARDOWN(void* object)
{
  worker_handles_st *handles= (worker_handles_st*)object;
  handles->reset();

  reset_server();

  return TEST_SUCCESS;
}

static test_return_t hostile_gearmand_SETUP(void* object)
{
  test_skip(true, has_hostile());
  test_skip_valgrind();
  test_skip(true, libtest::is_massive());

  // Programmer error
  assert(hostile_server_port);

  set_server(hostile_server_port);
  worker_ramp_SETUP(object);

  return TEST_SUCCESS;
}

static test_return_t recv_SETUP(void* object)
{
  test_skip_valgrind();
  test_skip(true, libtest::is_massive());

  worker_ramp_SETUP(object);
  set_recv_close(true, 20, 20);

  return TEST_SUCCESS;
}

static test_return_t getaddrinfo_SETUP(void* object)
{
  test_skip_valgrind();
  test_skip(true, libtest::is_massive());

  worker_ramp_SETUP(object);
  set_getaddrinfo_error(true, 20, 20);

  return TEST_SUCCESS;
}

static test_return_t recv_corrupt_SETUP(void* object)
{
  return TEST_SKIPPED;

  test_skip_valgrind();
  test_skip(true, libtest::is_massive());

  worker_ramp_SETUP(object);
  set_recv_corrupt(true, 20, 20);

  return TEST_SUCCESS;
}

static test_return_t getaddrinfo_TEARDOWN(void* object)
{
  set_getaddrinfo_error(true, 0, 0);

  worker_handles_st *handles= (worker_handles_st*)object;
  handles->kill_all();

  reset_server();

  return TEST_SUCCESS;
}

static test_return_t recv_TEARDOWN(void* object)
{
  set_recv_close(true, 0, 0);

  worker_handles_st *handles= (worker_handles_st*)object;
  handles->kill_all();

  reset_server();

  return TEST_SUCCESS;
}

static test_return_t send_SETUP(void* object)
{
  test_skip_valgrind();
  test_skip(true, libtest::is_massive());

  worker_ramp_SETUP(object);
  set_send_close(true, 20, 20);

  return TEST_SUCCESS;
}

static test_return_t accept_SETUP(void* object)
{
  test_skip_valgrind();
  test_skip(true, libtest::is_massive());

  worker_ramp_SETUP(object);
  set_accept_close(true, 20, 20);

  return TEST_SUCCESS;
}

static test_return_t connect_SETUP(void* object)
{
  test_skip_valgrind();
  test_skip(true, libtest::is_massive());

  worker_ramp_SETUP(object);
  set_connect_close(true, 20, 20);

  return TEST_SUCCESS;
}

static test_return_t poll_HOSTILE_POLL_CLOSED_SETUP(void* object)
{
  test_skip_valgrind();
  test_skip(true, libtest::is_massive());

  worker_ramp_SETUP(object);
  set_poll_close(true, 4, 0, HOSTILE_POLL_CLOSED);

  return TEST_SUCCESS;
}

static test_return_t poll_HOSTILE_POLL_SHUT_WR_SETUP(void* object)
{
  test_skip_valgrind();
  test_skip(true, libtest::is_massive());

  worker_ramp_SETUP(object);
  set_poll_close(true, 4, 0, HOSTILE_POLL_SHUT_WR);

  return TEST_SUCCESS;
}

static test_return_t poll_HOSTILE_POLL_SHUT_RD_SETUP(void* object)
{
  test_skip_valgrind();
  test_skip(true, libtest::is_massive());

  worker_ramp_SETUP(object);
  set_poll_close(true, 4, 0, HOSTILE_POLL_SHUT_RD);

  return TEST_SUCCESS;
}

static test_return_t poll_TEARDOWN(void* object)
{
  set_poll_close(false, 0, 0, HOSTILE_POLL_CLOSED);

  worker_handles_st *handles= (worker_handles_st*)object;
  handles->kill_all();

  reset_server();

  return TEST_SUCCESS;
}

static test_return_t send_TEARDOWN(void* object)
{
  set_send_close(true, 0, 0);

  worker_handles_st *handles= (worker_handles_st*)object;
  handles->kill_all();

  reset_server();

  return TEST_SUCCESS;
}

static test_return_t accept_TEARDOWN(void* object)
{
  set_accept_close(true, 0, 0);

  worker_handles_st *handles= (worker_handles_st*)object;
  handles->kill_all();

  reset_server();

  return TEST_SUCCESS;
}

static test_return_t connect_TEARDOWN(void* object)
{
  set_connect_close(true, 0, 0);

  worker_handles_st *handles= (worker_handles_st*)object;
  handles->kill_all();

  reset_server();

  return TEST_SUCCESS;
}


/*********************** World functions **************************************/

static void *world_create(server_startup_st& servers, test_return_t&)
{
  SKIP_IF(valgrind_is_caller());
  SKIP_UNLESS(has_hostile());

  hostile_server_port= libtest::get_free_port();
  ASSERT_TRUE(server_startup(servers, SERVER_TARGET, hostile_server_port, NULL));

  return new worker_handles_st;
}

static bool world_destroy(void *object)
{
  worker_handles_st *handles= (worker_handles_st *)object;
  delete handles;

  return TEST_SUCCESS;
}

test_st dos_TESTS[] ={
  {"send random port data", 0, send_random_port_data_TEST },
  {0, 0, 0}
};

test_st worker_TESTS[] ={
  {"first pass", 0, worker_ramp_TEST },
  {"second pass", 0, worker_ramp_TEST },
  {"first pass 1K jobs", 0, worker_ramp_1K_TEST },
  {"first pass 10K jobs", 0, worker_ramp_10K_TEST },
  {0, 0, 0}
};

collection_st collection[] ={
  {"dos", skip_SETUP, 0, dos_TESTS },
  {"plain", worker_ramp_SETUP, worker_ramp_TEARDOWN, worker_TESTS },
  {"plain against hostile server", hostile_gearmand_SETUP, worker_ramp_TEARDOWN, worker_TESTS },
  {"hostile recv()", recv_SETUP, recv_TEARDOWN, worker_TESTS },
  {"hostile recv() corrupt", recv_corrupt_SETUP, recv_TEARDOWN, worker_TESTS },
  {"hostile send()", send_SETUP, send_TEARDOWN, worker_TESTS },
  {"hostile accept()", accept_SETUP, accept_TEARDOWN, worker_TESTS },
  {"hostile connect()", connect_SETUP, connect_TEARDOWN, worker_TESTS },
  {"hostile poll(CLOSED)", poll_HOSTILE_POLL_CLOSED_SETUP, poll_TEARDOWN, worker_TESTS },
  {"hostile poll(SHUT_RD)", poll_HOSTILE_POLL_SHUT_RD_SETUP, poll_TEARDOWN, worker_TESTS },
  {"hostile poll(SHUT_WR)", poll_HOSTILE_POLL_SHUT_WR_SETUP, poll_TEARDOWN, worker_TESTS },
  {"hostile getaddrinfo()", getaddrinfo_SETUP, getaddrinfo_TEARDOWN, worker_TESTS },
  {0, 0, 0, 0}
};

void get_world(libtest::Framework *world)
{
  world->collections(collection);
  world->create(world_create);
  world->destroy(world_destroy);
}
