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

#include "libgearman/client.hpp"
#include "libgearman/worker.hpp"
using namespace org::gearmand;

#include <libtest/test.hpp>
using namespace libtest;

#include <libgearman/gearman.h>


#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

#include <tests/start_worker.h>

#include "tests/workers/v2/echo_or_react.h"

#define WORKER_FUNCTION_NAME "echo_function"

namespace cli {

struct Context
{
  server_startup_st& servers;

  Context(server_startup_st& servers_arg) :
    servers(servers_arg)
  { }

  void push(worker_handle_st *worker_arg)
  {
    _workers.push_back(worker_arg);
  }

  void shutdown_workers()
  {
    for (std::vector<worker_handle_st *>::iterator iter= _workers.begin(); iter != _workers.end(); ++iter)
    {
      delete *iter;
    }
    _workers.clear();
  }

  void clear()
  {
    shutdown_workers();
    servers.clear();
  }

  void port(in_port_t port_)
  {
    _port= port_;
  }

  in_port_t port() const
  {
    return _port;
  }

  ~Context()
  {
    clear();
  }

private:
  std::vector<worker_handle_st *>_workers;
  in_port_t _port;

};

}

static test_return_t gearman_help_test(void *)
{
  const char *args[]= { "-H", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearman", args, true));
  return TEST_SUCCESS;
}

static test_return_t gearman_verbose_TEST(void *)
{
  const char *args[]= { "-H", "-v", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearman", args, true));
  return TEST_SUCCESS;
}

static test_return_t gearman_unknown_test(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "-p %d", int(context->port()));
  const char *args[]= { buffer, "--unknown", 0 };

  // The argument doesn't exist, so we should see an error
  ASSERT_EQ(EXIT_FAILURE, exec_cmdline("bin/gearman", args, true));

  return TEST_SUCCESS;
}

static test_return_t gearman_client_background_test(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "-p %d", int(context->port()));
  const char *args[]= { buffer, "-f", WORKER_FUNCTION_NAME, "-b", "payload", 0 };

  // The argument doesn't exist, so we should see an error
  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearman", args, true));

  return TEST_SUCCESS;
}

#define REGRESSION_FUNCTION_833394 "55_char_function_name_________________________________"

static test_return_t regression_833394_test(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "-p %d", int(context->port()));
  const char *args[]= { buffer, "-f", REGRESSION_FUNCTION_833394, "-b", "payload", 0 };

  // The argument doesn't exist, so we should see an error
  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearman", args, true));

  return TEST_SUCCESS;
}

static test_return_t gearadmin_help_test(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--help", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", args, true));
  return TEST_SUCCESS;
}

static test_return_t gearadmin_shutdown_test(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--shutdown", 0 };

  ASSERT_EQ(EXIT_FAILURE, exec_cmdline("bin/gearadmin", args, true));

  return TEST_SUCCESS;
}

static test_return_t gearadmin_version_test(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--server-version", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", args, true));
  return TEST_SUCCESS;
}

static test_return_t gearadmin_verbose_test(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--server-verbose", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", args, true));
  return TEST_SUCCESS;
}

static test_return_t gearadmin_status_TEST(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--status", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", args, true));
  return TEST_SUCCESS;
}

static test_return_t gearadmin_status_with_jobs_TEST(void* object)
{
  cli::Context *context= (cli::Context*)object;

  {
    libgearman::Client client(context->port());
    for (size_t x= 0; x < 10000; ++x)
    {
      gearman_job_handle_t job_handle;
      ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_do_background(&client,
                                                              __func__,
                                                              NULL, // unique
                                                              NULL, 0, // workload
                                                              job_handle));
    }
  }

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--status", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", args, true));

  return TEST_SUCCESS;
}

static test_return_t gearadmin_priority_status_TEST(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--priority-status", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", args, true));
  return TEST_SUCCESS;
}

static test_return_t gearadmin_show_unique_jobs_TEST(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--show-unique-jobs", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", args, true));
  return TEST_SUCCESS;
}

// issue #81: each line of --show-unique-jobs is "<job handle>\t<unique>", so
// the handle can be passed to --cancel-job.
static test_return_t gearadmin_show_unique_jobs_handle_TEST(void* object)
{
  cli::Context *context= (cli::Context*)object;

  // No worker is registered for this function, so the job stays queued.
  gearman_job_handle_t job_handle;
  {
    libgearman::Client client(context->port());
    ASSERT_EQ(GEARMAN_SUCCESS,
              gearman_client_do_background(&client,
                                           __func__, // function
                                           __func__, // unique
                                           NULL, 0, // workload
                                           job_handle));
  }

  std::string expected(job_handle);
  expected+= "\t";
  expected+= __func__;
  expected+= "\n";

  char port_arg[1024];
  snprintf(port_arg, sizeof(port_arg), "--port=%d", int(context->port()));

  {
    Application app("bin/gearadmin", true);
    app.add_option(port_arg);
    app.add_option("--show-unique-jobs");
    ASSERT_EQ(Application::SUCCESS, app.run());
    ASSERT_EQ(Application::SUCCESS, app.join());

    libtest::vchar_t stdout_result= app.stdout_result();
    std::string output(stdout_result.begin(), stdout_result.end());
    ASSERT_TRUE_(output.find(expected) != std::string::npos,
                 "expected `%s' in `%s'", expected.c_str(), output.c_str());
  }

  std::string cancel_arg("--cancel-job=");
  cancel_arg+= job_handle;
  const char *cancel_args[]= { port_arg, cancel_arg.c_str(), 0 };
  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", cancel_args, true));

  // Cancellation is lazy: the job is only marked as ignored (and freed once
  // a worker would take it), which --show-jobs reports as
  // "<handle>\t<retries>\t<ignore_job>\t<job_queued>".
  std::string cancelled(job_handle);
  cancelled+= "\t0\t1\t0\n";

  {
    Application app("bin/gearadmin", true);
    app.add_option(port_arg);
    app.add_option("--show-jobs");
    ASSERT_EQ(Application::SUCCESS, app.run());
    ASSERT_EQ(Application::SUCCESS, app.join());

    libtest::vchar_t stdout_result= app.stdout_result();
    std::string output(stdout_result.begin(), stdout_result.end());
    ASSERT_TRUE_(output.find(cancelled) != std::string::npos,
                 "expected `%s' in `%s'", cancelled.c_str(), output.c_str());
  }

  return TEST_SUCCESS;
}

static test_return_t gearadmin_cancel_job_TEST(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--cancel-job=test", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", args, true));
  return TEST_SUCCESS;
}

static test_return_t gearadmin_show_jobs_TEST(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--show-jobs", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", args, true));
  return TEST_SUCCESS;
}

static test_return_t gearadmin_workers_test(void* object)
{
  cli::Context *context= (cli::Context*)object;

  libgearman::Worker worker(context->port());
  for (int x= 0; x < 100; ++x)
  {
    char function_name[1024];
    snprintf(function_name, sizeof(function_name), "function_%u", x);
    ASSERT_EQ(GEARMAN_SUCCESS, gearman_worker_register(&worker, function_name, 0));
  }

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--workers", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", args, true));

  return TEST_SUCCESS;
}

static test_return_t gearadmin_create_drop_test(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));

  const char *create_args[]= { buffer, "--create-function=test_function", 0 };
  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", create_args, true));

  const char *drop_args[]= { buffer, "--drop-function=test_function", 0 };
  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", drop_args, true));


  return TEST_SUCCESS;
}

static test_return_t gearadmin_getpid_test(void* object)
{
  cli::Context *context= (cli::Context*)object;

  {
    libgearman::Client client(context->port());
    for (size_t x= 0; x < 4; x++)
    {
      gearman_job_handle_t job_handle;
      ASSERT_EQ(GEARMAN_SUCCESS,
                gearman_client_do_background(&client,
                                             __func__, // function
                                             NULL, // unique
                                             NULL, 0, // workload
                                             job_handle));
    }
  }

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--getpid", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline("bin/gearadmin", args, true));
  return TEST_SUCCESS;
}

static test_return_t gearadmin_unknown_test(void* object)
{
  cli::Context *context= (cli::Context*)object;

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "--port=%d", int(context->port()));
  const char *args[]= { buffer, "--unknown", 0 };

  // The argument doesn't exist, so we should see an error
  ASSERT_EQ(EXIT_FAILURE, exec_cmdline("bin/gearadmin", args, true));

  return TEST_SUCCESS;
}

static test_return_t init_SETUP(void* object)
{
  cli::Context *context= (cli::Context*)object;

  context->port(libtest::get_free_port());
  ASSERT_TRUE(server_startup(context->servers, "gearmand", context->port(), NULL));

  // Echo function
  gearman_function_t echo_react_fn_v2= gearman_function_create(echo_or_react_worker_v2);
  context->push(test_worker_start(context->port(), NULL, WORKER_FUNCTION_NAME, echo_react_fn_v2, NULL, gearman_worker_options_t()));
  context->push(test_worker_start(context->port(), NULL, REGRESSION_FUNCTION_833394, echo_react_fn_v2, NULL, gearman_worker_options_t()));

  return TEST_SUCCESS;
}

static test_return_t init_TEARDOWN(void* object)
{
  cli::Context *context= (cli::Context*)object;

  context->clear();

  return TEST_SUCCESS;
}


static test_return_t server_SETUP(void *object)
{
  cli::Context *context= (cli::Context*)object;

  in_port_t new_port= libtest::get_free_port();
  ASSERT_TRUE(server_startup(context->servers, "gearmand", new_port, NULL));

  return TEST_SUCCESS;
}


test_st gearman_tests[] ={
  { "--help", 0, gearman_help_test },
  { "-H", 0, gearman_help_test },
  { "-v", 0, gearman_verbose_TEST },
  { "--unknown", 0, gearman_unknown_test },
  { "-f echo -b payload", 0, gearman_client_background_test },
  { "lp:833394", 0, regression_833394_test },
  { 0, 0, 0 }
};


test_st gearadmin_tests[] ={
  {"--help", 0, gearadmin_help_test},
  {"--server-version", 0, gearadmin_version_test},
  {"--server-verbose", 0, gearadmin_verbose_test},
  {"--cancel-job", 0, gearadmin_cancel_job_TEST},
  {"--show-jobs", 0, gearadmin_show_jobs_TEST},
  {"--show-unique-jobs", 0, gearadmin_show_unique_jobs_TEST},
  {"gearman_client_do_background() --show-unique-jobs --cancel-job", 0, gearadmin_show_unique_jobs_handle_TEST},
  {"--status", 0, gearadmin_status_TEST},
  {"--priority-status", 0, gearadmin_priority_status_TEST},
  {"gearman_client_do_background(100) --status", 0, gearadmin_status_with_jobs_TEST},
  {"--getpid", 0, gearadmin_getpid_test},
  {"--workers", 0, gearadmin_workers_test},
  {"--create-function and --drop-function", 0, gearadmin_create_drop_test},
  {"--unknown", 0, gearadmin_unknown_test},
  {0, 0, 0}
};

test_st gearadmin_shutdown_tests[] ={
  {"--shutdown", 0, gearadmin_shutdown_test}, // shutdown test is a relict. It doesn't shut down the server anymore
  {0, 0, 0}
};

collection_st collection[] ={
  {"gearman", init_SETUP, init_TEARDOWN, gearman_tests},
  {"gearadmin", init_SETUP, init_TEARDOWN, gearadmin_tests},
  {"gearadmin --shutdown", server_SETUP, init_TEARDOWN, gearadmin_shutdown_tests},
  {0, 0, 0, 0}
};

static void *world_create(server_startup_st& servers, test_return_t& error)
{
#if defined(HAVE_WOLFSSL) && HAVE_WOLFSSL
  SKIP_IF(HAVE_WOLFSSL);
#endif

  cli::Context *context= new cli::Context(servers);
  if (context == NULL)
  {
    error= TEST_FAILURE;
    return NULL;
  }

  return context;
}

static bool world_destroy(void *object)
{
  cli::Context *context= (cli::Context*)object;
  delete context;

  return TEST_SUCCESS;
}


void get_world(libtest::Framework *world)
{
  world->collections(collection);
  world->create(world_create);
  world->destroy(world_destroy);
}
