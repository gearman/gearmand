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

#include "libgearman/client.hpp"
#include "libgearman/worker.hpp"
using namespace org::gearmand;

#include <fstream>

using namespace libtest;

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

static std::string executable;

static test_return_t postion_TEST(void *)
{
  const char *args[]= { "foo", 0 };

  ASSERT_EQ(EXIT_FAILURE, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t partial_TEST(void *)
{
  const char *args[]= { "--log", 0 };

  ASSERT_EQ(EXIT_FAILURE, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t check_args_test(void *)
{
  const char *args[]= { "--check-args", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_backlog_test(void *)
{
  const char *args[]= { "--check-args", "--backlog=10", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_backlog_test(void *)
{
  const char *args[]= { "--check-args", "-b", "10", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_daemon_test(void *)
{
  const char *args[]= { "--check-args", "--daemon", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_coredump_TEST(void *)
{
  const char *args[]= { "--check-args", "--coredump", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_daemon_test(void *)
{
  const char *args[]= { "--check-args", "-d", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_file_descriptors_test(void *)
{
  const char *args[]= { "--check-args", "--file-descriptors=10", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_file_descriptors_test(void *)
{
  const char *args[]= { "--check-args", "-f", "10", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_help_test(void *)
{
  const char *args[]= { "--check-args", "--help", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_help_test(void *)
{
  const char *args[]= { "--check-args", "-h", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_keepalive_TEST(void *)
{
  const char *args[]= { "--check-args", "--keepalive", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_keepalive_idle_TEST(void *)
{
  const char *args[]= { "--check-args", "--keepalive-idle=10", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_keepalive_interval_TEST(void *)
{
  const char *args[]= { "--check-args", "--keepalive-interval=3", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_keepalive_count_TEST(void *)
{
  const char *args[]= { "--check-args", "--keepalive-count=10", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_keepalive_start_TEST(void *)
{
  in_port_t port= libtest::get_free_port();
  char port_str[1024];
  ASSERT_TRUE(snprintf(port_str, sizeof(port_str), "--port=%d", int32_t(port)) > 0);

  // --check-args only validates arguments; gearmand never binds this port,
  // so there's nothing for anyone else to release it later.
  libtest::release_port(port);

  const char *args[]= {
    port_str,
    "--check-args",
    "--log-file=stderr",
    "--keepalive-count=10", 
    "--keepalive-interval=3",
    "--keepalive-idle=10", 0 };

  Application app(gearmand_binary(), true);

  Application::error_t ret= app.run(args);

  ASSERT_EQ(Application::SUCCESS, ret);

  ret= app.join();
  ASSERT_EQ_(Application::SUCCESS, ret, "ret: %s stderr: '%s' stdout: `%s'",
             Application::toString(ret),
             app.stderr_c_str(), app.stdout_c_str());

  return TEST_SUCCESS;
}

static test_return_t long_log_file_test(void *)
{
  const char *args[]= { "--check-args", "--log-file=\"tmp/foo\"", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_log_file_stderr_TEST(void *)
{
  const char *args[]= { "--check-args", "--log-file=stderr", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_log_file_test(void *)
{
  const char *args[]= { "--check-args", "-l", "\"tmp/foo\"", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_listen_test(void *)
{
  const char *args[]= { "--check-args", "--listen=10", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_listen_test(void *)
{
  const char *args[]= { "--check-args", "-L", "10", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_port_test(void *)
{
  const char *args[]= { "--check-args", "--port=10", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_port_test(void *)
{
  const char *args[]= { "--check-args", "-p", "10", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_pid_file_test(void *)
{
  const char *args[]= { "--check-args", "--pid-file=\"tmp/gearmand.pid\"", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_pid_file_test(void *)
{
  const char *args[]= { "--check-args", "-P", "\"tmp/gearmand.pid\"", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_round_robin_test(void *)
{
  const char *args[]= { "--check-args", "--round-robin", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_round_robin_test(void *)
{
  const char *args[]= { "--check-args", "-R", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_syslog_test(void *)
{
  const char *args[]= { "--check-args", "--syslog", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t SSL_TEST(void *)
{
  const char *args[]= { "--check-args", "--ssl", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_threads_test(void *)
{
  const char *args[]= { "--check-args", "--threads=10", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_threads_test(void *)
{
  const char *args[]= { "--check-args", "-t", "8", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_user_test(void *)
{
  const char *args[]= { "--check-args", "--user=nobody", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_user_test(void *)
{
  const char *args[]= { "--check-args", "-u", "nobody", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_version_test(void *)
{
  const char *args[]= { "--check-args", "--version", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_version_test(void *)
{
  const char *args[]= { "--check-args", "-V", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t verbose_bad_option_TEST(void *)
{
  const char *args[]= { "--check-args", "--verbose=BAD", 0 };

  ASSERT_EQ(EXIT_FAILURE, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t verbose_DEBUG_TEST(void *)
{
  const char *args[]= { "--check-args", "--verbose=DEBUG", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t verbose_ERROR_TEST(void *)
{
  const char *args[]= { "--check-args", "--verbose=ERROR", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t verbose_ALERT_TEST(void *)
{
  const char *args[]= { "--check-args", "--verbose=ALERT", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t verbose_INFO_TEST(void *)
{
  const char *args[]= { "--check-args", "--verbose=INFO", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t verbose_WARNING_TEST(void *)
{
  const char *args[]= { "--check-args", "--verbose=WARNING", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t verbose_NOTICE_TEST(void *)
{
  const char *args[]= { "--check-args", "--verbose=NOTICE", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t verbose_FATAL_TEST(void *)
{
  const char *args[]= { "--check-args", "--verbose=FATAL", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t verbose_CRITICAL_TEST(void *)
{
  const char *args[]= { "--check-args", "--verbose=CRITICAL", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t verbose_deprecated_TEST(void *)
{
  const char *args[]= { "--check-args", "-vvv", 0 };

  ASSERT_EQ(EXIT_FAILURE, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t long_worker_wakeup_test(void *)
{
  const char *args[]= { "--check-args", "--worker-wakeup=4", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_worker_wakeup_test(void *)
{
  const char *args[]= { "--check-args", "-V", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t protocol_test(void *)
{
  const char *args[]= { "--check-args", "--protocol=http", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t queue_test(void *)
{
  const char *args[]= { "--check-args", "--queue-type=builtin", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));

  return TEST_SUCCESS;
}

static test_return_t long_job_retries_test(void *)
{
  const char *args[]= { "--check-args", "--job-retries=4", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t job_handle_prefix_TEST(void *)
{
  const char *args[]= { "--check-args", "--job-handle-prefix=my_own_private_handle", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t hashtable_buckets_TEST(void *)
{
  const char *args[]= { "--check-args", "--hashtable-buckets=2000", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t short_job_retries_test(void *)
{
  const char *args[]= { "--check-args", "-j", "6", 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t http_port_test(void *)
{
  const char *args[]= { "--check-args", "--protocol=http", "--http-port=8090",  0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t config_file_TEST(void *)
{
  ASSERT_EQ(-1, access("etc/gearmand.conf", R_OK));

  const char *args[]= { "--check-args", "--config-file=etc/gearmand.conf", 0 };

  ASSERT_EQ(EXIT_FAILURE, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t config_file_DEFAULT_TEST(void *)
{
  const char *args[]= { "--check-args", "--config-file", 0 };

  ASSERT_EQ(EXIT_FAILURE, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t config_file_FAIL_TEST(void *)
{
  const char *args[]= { "--check-args", "--config-file=etc/grmandfoo.conf", 0 };

  ASSERT_EQ(EXIT_FAILURE, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t config_file_SIMPLE_TEST(void *)
{
  in_port_t port= libtest::get_free_port();
  char port_str[1024];
  ASSERT_TRUE(snprintf(port_str, sizeof(port_str), "%d", int32_t(port)) > 0);

  // --check-args only validates the config file; gearmand never binds this
  // port, so there's nothing for anyone else to release it later.
  libtest::release_port(port);

  std::string config_file= "etc/gearmand.conf";
  {
    char current_directory_buffer[1024];
    char *current_directory= getcwd(current_directory_buffer, sizeof(current_directory_buffer));

    std::string config_path;

    config_path+= current_directory;
    config_path+= "/";
    config_path+= config_file;


    std::fstream file_stream;
    file_stream.open(config_path.c_str(), std::fstream::out | std::fstream::trunc);

    ASSERT_TRUE(file_stream.good());

    file_stream << "--port " << port_str << std::endl;

    ASSERT_TRUE(file_stream.good());
    file_stream.close();
  }
  test_zero(access(config_file.c_str(), R_OK));

  char args_buffer[1024];
  snprintf(args_buffer, sizeof(args_buffer), "--config-file=%s", config_file.c_str()); 
  const char *args[]= { "--check-args", args_buffer, 0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  unlink(config_file.c_str());

  return TEST_SUCCESS;
}

static test_return_t maxqueue_TEST(void *)
{
  return TEST_SKIPPED;
#if 0
  in_port_t port= libtest::get_free_port();
  Application gearmand(gearmand_binary(), true);

  char buffer[1024];
  test_true(snprintf(buffer, sizeof(buffer), "%d", int32_t(port)) > 0);
  gearmand.add_long_option("--port=", buffer);

  ASSERT_EQ(Application::SUCCESS, gearmand.run());

  test::Worker worker(port);
  ASSERT_EQ(GEARMAN_SUCCESS, gearman_worker_register(&worker, __func__, 0));
  ASSERT_EQ(GEARMAN_SUCCESS, gearman_worker_unregister(&worker, __func__));
  ASSERT_EQ(Application::SUCCESS, gearmand.join());

  return TEST_SUCCESS;
#endif
}

test_st bad_option_TESTS[] ={
  {"position argument", 0, postion_TEST },
  {"partial argument", 0, partial_TEST },
  {0, 0, 0}
};

test_st gearmand_option_tests[] ={
  {"--check-args", 0, check_args_test},
  {"--backlog=", 0, long_backlog_test},
  {"-b", 0, short_backlog_test},
  {"--coredump", 0, long_coredump_TEST},
  {"--daemon", 0, long_daemon_test},
  {"-d", 0, short_daemon_test},
  {"--file-descriptors=", 0, long_file_descriptors_test},
  {"-f", 0, short_file_descriptors_test},
  {"--help", 0, long_help_test},
  {"-h", 0, short_help_test},
  {"--keepalive", 0, long_keepalive_TEST},
  {"--keepalive-idle", 0, long_keepalive_idle_TEST},
  {"--keepalive-interval", 0, long_keepalive_interval_TEST},
  {"--keepalive-count", 0, long_keepalive_count_TEST},
  {"--log-file=", 0, long_log_file_test},
  {"--log-file=stderr", 0, long_log_file_stderr_TEST},
  {"-l", 0, short_log_file_test},
  {"--listen=", 0, long_listen_test},
  {"-L", 0, short_listen_test},
  {"--port=", 0, long_port_test},
  {"-p", 0, short_port_test},
  {"--pid-file=", 0, long_pid_file_test},
  {"-P", 0, short_pid_file_test},
  {"--round-robin", 0, long_round_robin_test},
  {"-R", 0, short_round_robin_test},
  {"--ssl", 0, SSL_TEST},
  {"--syslog=", 0, long_syslog_test},
  {"--threads=", 0, long_threads_test},
  {"-T", 0, short_threads_test},
  {"--user=", 0, long_user_test},
  {"-u", 0, short_user_test},
  {"--user=", 0, long_user_test},
  {"-u", 0, short_user_test},
  {"-vvv", 0, verbose_deprecated_TEST},
  {"--verbose=FATAL", 0, verbose_FATAL_TEST},
  {"--verbose=ALERT", 0, verbose_ALERT_TEST},
  {"--verbose=CRITICAL", 0, verbose_CRITICAL_TEST},
  {"--verbose=ERROR", 0, verbose_ERROR_TEST},
  {"--verbose=WARNING", 0, verbose_WARNING_TEST},
  {"--verbose=NOTICE", 0, verbose_NOTICE_TEST},
  {"--verbose=INFO", 0, verbose_INFO_TEST},
  {"--verbose=DEBUG", 0, verbose_DEBUG_TEST},
  {"--verbose=BAD", 0, verbose_bad_option_TEST},
  {"--version", 0, long_version_test},
  {"-V", 0, short_version_test},
  {"--worker_wakeup=", 0, long_worker_wakeup_test},
  {"-w", 0, short_worker_wakeup_test},
  {"--protocol=", 0, protocol_test},
  {"--queue-type=", 0, queue_test},
  {"--job-retries=", 0, long_job_retries_test},
  {"-hashtable-buckets", 0, hashtable_buckets_TEST},
  {"--job-handle-prefix=", 0, job_handle_prefix_TEST},
  {"-j", 0, short_job_retries_test},
  {"--config-file=etc/gearmand.conf no file present", 0, config_file_TEST },
  {"--config-file", 0, config_file_DEFAULT_TEST },
  {"--config-file=etc/grmandfoo.conf", 0, config_file_FAIL_TEST },
  {"--config-file=etc/gearmand.conf", 0, config_file_SIMPLE_TEST },
  {"start server with all --keepalive options", 0, long_keepalive_start_TEST},
  {0, 0, 0}
};

test_st gearmand_httpd_option_tests[] ={
  {"--http-port=", 0, http_port_test},
  {0, 0, 0}
};

test_st maxqueue_TESTS[] ={
  { "maxqueue=", 0, maxqueue_TEST },
  {0, 0, 0}
};

static test_return_t option_SETUP(void*)
{
  unlink("etc/gearmand.conf");
  return TEST_SUCCESS;
}

collection_st collection[] ={
  { "bad options", 0, 0, bad_option_TESTS },
  { "basic options", option_SETUP, 0, gearmand_option_tests },
  { "httpd options", 0, 0, gearmand_httpd_option_tests },
  { "maxqueue", 0, 0, maxqueue_TESTS },
  {0, 0, 0, 0}
};

void get_world(libtest::Framework *world)
{
  world->collections(collection);
}
