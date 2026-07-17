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

#include <libgearman/gearman.h>
#include "tests/start_worker.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <tests/basic.h>
#include <tests/context.h>

#include "tests/workers/v2/echo_or_react.h"

// Prototypes
#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

static char host_url[1024]= { 0 };
#define WORKER_FUNCTION_NAME "httpd_worker"

static test_return_t curl_no_function_TEST(void *)
{
  Application curl("/usr/bin/curl");
  curl.add_option(host_url);

  ASSERT_EQ(Application::SUCCESS, curl.run());
  ASSERT_EQ(Application::SUCCESS, curl.join());

  return TEST_SUCCESS;
}

static test_return_t curl_function_no_body_TEST(void *)
{
  Application curl("/usr/bin/curl");
  char worker_url[1024 + sizeof(WORKER_FUNCTION_NAME) - 1];
  snprintf(worker_url, sizeof(worker_url), "%s%s", host_url, WORKER_FUNCTION_NAME);
  curl.add_option(worker_url);
  curl.add_option("--header", "\"X-Gearman-Unique: curl_function_no_body_TEST\"");

  ASSERT_EQ(Application::SUCCESS, curl.run());
  ASSERT_EQ(Application::SUCCESS, curl.join());
  test_zero(curl.stdout_result_length());

  return TEST_SUCCESS;
}

static test_return_t curl_function_TEST(void *)
{
  // Cleanup previous run
  unlink("var/tmp/curl_function_TEST.out");

  Application curl("/usr/bin/curl");
  char worker_url[1024 + sizeof(WORKER_FUNCTION_NAME) - 1];
  snprintf(worker_url, sizeof(worker_url), "%s%s", host_url, WORKER_FUNCTION_NAME);
  curl.add_option("--header", "\"X-Gearman-Unique: curl_function_TEST\"");
  curl.add_option("--data", "fubar");
  curl.add_option("--silent");
  curl.add_option("--show-error");
  curl.add_option("--output", "var/tmp/curl_function_TEST.out");
  curl.add_option("--connect-timeout", "1");
  curl.add_option(worker_url);

  ASSERT_EQ(Application::SUCCESS, curl.run());
  ASSERT_EQ(Application::SUCCESS, curl.join());
  test_zero(curl.stdout_result_length());

  struct stat stat_buffer;
  test_zero(stat("var/tmp/curl_function_TEST.out", &stat_buffer));
  ASSERT_TRUE(stat_buffer.st_size >= off_t(146));
  test_zero(unlink("var/tmp/curl_function_TEST.out"));

  return TEST_SUCCESS;
}

static test_return_t GET_TEST(void *)
{
  libtest::http::GET get(host_url);

  ASSERT_EQ(true, get.execute());

  return TEST_SUCCESS;
}

static test_return_t HEAD_TEST(void *)
{
  libtest::http::HEAD head(host_url);

  ASSERT_EQ(true, head.execute());

  return TEST_SUCCESS;
}


static void *world_create(server_startup_st& servers, test_return_t& error)
{
  if (valgrind_is_caller())
  {
    error= TEST_SKIPPED;
    return NULL;
  }

  in_port_t http_port= libtest::get_free_port();
  int length= snprintf(host_url, sizeof(host_url), "http://localhost:%d/", int(http_port));
  fatal_assert(length > 0 and sizeof(length) < sizeof(host_url));

  char buffer[1024];
  length= snprintf(buffer, sizeof(buffer), "--http-port=%d", int(http_port));
  fatal_assert(length > 0 and sizeof(length) < sizeof(buffer));
  const char *argv[]= { "--protocol=http", buffer, 0 };
  bool started= server_startup(servers, "gearmand", libtest::default_port(), argv);

  // http_port is only ever used as the --http-port= value above; it's never
  // the primary port of a Server object, so nothing else will release it.
  libtest::release_port(http_port);

  if (started == false)
  {
    error= TEST_SKIPPED;
    return NULL;
  }

  worker_handles_st *workers= new worker_handles_st();
  gearman_function_t echo_react_fn= gearman_function_create(echo_or_react_worker_v2);
  workers->push(test_worker_start(libtest::default_port(), NULL, WORKER_FUNCTION_NAME, echo_react_fn, NULL, gearman_worker_options_t()));

  return workers;
}

static bool world_destroy(void *object)
{
  worker_handles_st *workers= (worker_handles_st *)object;
  delete workers;

  return false;
}


static test_return_t check_for_libcurl(void *)
{
  test_skip(true, HAVE_LIBCURL);
  return TEST_SUCCESS;
}

static test_return_t check_for_curl(void *)
{
  test_skip(0, access("/usr/bin/curl", X_OK));
  return TEST_SUCCESS;
}

test_st curl_TESTS[] ={
  { "curl /", 0, curl_no_function_TEST },
  { "curl /" WORKER_FUNCTION_NAME, 0, curl_function_no_body_TEST },
  { "curl /" WORKER_FUNCTION_NAME " --data=fubar", 0, curl_function_TEST },
  { 0, 0, 0 }
};


test_st GET_TESTS[] ={
  { "GET /", 0, GET_TEST },
  { 0, 0, 0 }
};

test_st HEAD_TESTS[] ={
  { "HEAD /", 0, HEAD_TEST },
  { 0, 0, 0 }
};

static test_return_t regression_http_version_10_TEST(void *)
{
  Application curl("/usr/bin/curl");
  curl.add_option("--http1.0");
  curl.add_option("--include");
  curl.add_option("--silent");
  curl.add_option(host_url);

  ASSERT_EQ(Application::SUCCESS, curl.run());
  ASSERT_EQ(Application::SUCCESS, curl.join());

  const char *output= curl.stdout_c_str();
  ASSERT_TRUE(output != NULL);
  ASSERT_TRUE(strncmp(output, "HTTP/1.0", 8) == 0);

  return TEST_SUCCESS;
}

static test_return_t regression_http_version_11_TEST(void *)
{
  Application curl("/usr/bin/curl");
  curl.add_option("--http1.1");
  curl.add_option("--include");
  curl.add_option("--silent");
  curl.add_option(host_url);

  ASSERT_EQ(Application::SUCCESS, curl.run());
  ASSERT_EQ(Application::SUCCESS, curl.join());

  const char *output= curl.stdout_c_str();
  ASSERT_TRUE(output != NULL);
  ASSERT_TRUE(strncmp(output, "HTTP/1.1", 8) == 0);
  ASSERT_TRUE(strstr(output, "Connection: close") != NULL);

  return TEST_SUCCESS;
}

test_st regression_TESTS[] ={
  { "issue#391 HTTP/1.0 request gets HTTP/1.0 response", 0, regression_http_version_10_TEST },
  { "issue#391 HTTP/1.1 request gets HTTP/1.1 response", 0, regression_http_version_11_TEST },
  { 0, 0, 0 }
};

collection_st collection[] ={
  { "curl", check_for_curl, 0, curl_TESTS },
  { "GET", check_for_libcurl, 0, GET_TESTS },
  { "regression", check_for_curl, 0, regression_TESTS },
  { 0, 0, 0, 0 }
};

void get_world(libtest::Framework *world)
{
  world->collections(collection);
  world->create(world_create);
  world->destroy(world_destroy);
}
