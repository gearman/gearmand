/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2012 Data Differential, http://datadifferential.com/
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

#include <sqlite3.h>

using namespace libtest;

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <libgearman/gearman.h>

#define GEARMAN_CORE
#include "libgearman/universal.hpp"
#include "libgearman/connection.hpp"
#include "libgearman/packet.hpp"

#define GEARMAN_QUEUE_SQLITE_DEFAULT_TABLE "gearman_queue"

#include <tests/basic.h>
#include <tests/context.h>

#include "libgearman/client.hpp"
#include "libgearman/worker.hpp"
using namespace org::gearmand;

#include "tests/workers/v2/called.h"

#include <climits>

// Prototypes
#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

static int sql_print(void *, int argc, char **argv, char **)
{
  (void)argc;
  (void)argv;
  assert(argc == 2);

  return 0;
}

static int sql_count(void * rows_, int argc, char **argv, char **)
{
  if (argc)
  {
    int *rows= (int*)rows_;
    *rows= atoi(argv[0]);
  }

  return 0;
}

class Sqlite {
public:
  Sqlite(const std::string& schema_)
  {
    if (sqlite3_open(schema_.c_str(), &_db) != SQLITE_OK)
    {
      FAIL(sqlite3_errmsg(_db));
    }
    sqlite3_busy_timeout(_db, 6000);
  }

  ~Sqlite()
  {
    if (_db)
    {
      sqlite3_close(_db);
      _db= NULL;
    }
  }

  int vcount()
  {
    reset_error();
    std::string count_query;
    count_query+= "SELECT count(*) FROM ";
    count_query+= GEARMAN_QUEUE_SQLITE_DEFAULT_TABLE;

    int rows= 0;
    char *err= NULL;
    sqlite3_exec(_db, count_query.c_str(), sql_count, &rows, &err);

    if (err != NULL)
    {
      _error_string= err;
      sqlite3_free(err);
      return -1;
    }

    return rows;
  }

  void vprint_unique()
  {
    reset_error();

    std::string count_query;
    count_query+= "SELECT unique_key, function_name FROM ";
    count_query+= GEARMAN_QUEUE_SQLITE_DEFAULT_TABLE;

    char *err= NULL;
    sqlite3_exec(_db, count_query.c_str(), sql_print, NULL, &err);

    if (err != NULL)
    {
      _error_string= err;
      sqlite3_free(err);
    }
  }

  bool has_error()
  {
    return _error_string.size();
  }

  const std::string& error_string()
  {
    return _error_string;
  }

protected:
  void reset_error()
  {
    _error_string.clear();
  }

  std::string _error_string;

private:
  sqlite3 *_db;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
static bool test_for_HAVE_LIBSQLITE3()
{
  if (HAVE_LIBSQLITE3)
  {
    return true;
  }

  return false;
}
#pragma GCC diagnostic pop

static test_return_t gearmand_basic_option_test(void *)
{
  const char *args[]= { "--check-args",
    "--queue-type=libsqlite3",
    "--libsqlite3-db=var/tmp/gearman.sql",
    "--libsqlite3-table=custom_table", 
    0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  return TEST_SUCCESS;
}

static test_return_t gearmand_basic_option_without_table_test(void *)
{
  std::string sql_file= libtest::create_tmpfile("sqlite");

  char sql_buffer[1024];
  snprintf(sql_buffer, sizeof(sql_buffer), "--libsqlite3-db=%.*s", int(sql_file.length()), sql_file.c_str());
  const char *args[]= { "--check-args",
    "--queue-type=libsqlite3",
    sql_buffer,
    0 };

  ASSERT_EQ(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  ASSERT_EQ(-1, access(sql_file.c_str(), R_OK | W_OK ));

  return TEST_SUCCESS;
}

static test_return_t gearmand_basic_option_shutdown_queue_TEST(void *)
{
  std::string sql_file= libtest::create_tmpfile("sqlite");

  char sql_buffer[1024];
  snprintf(sql_buffer, sizeof(sql_buffer), "--libsqlite3-db=%.*s", int(sql_file.length()), sql_file.c_str());
  const char *args[]= { "--check-args",
    "--queue-type=libsqlite3",
    sql_buffer,
    "--store-queue-on-shutdown",
    0 };

  test_compare(EXIT_SUCCESS, exec_cmdline(gearmand_binary(), args, true));
  test_compare(-1, access(sql_file.c_str(), R_OK | W_OK ));

  return TEST_SUCCESS;
}

static void error_logger(const char* message, gearman_verbose_t, void*)
{
  Error << message;
}

/* Background jobs submitted without a unique ID are separate jobs to the
   server, so each needs its own row in the queue, and finishing one must
   only remove that row. libgearman always fills in a unique ID for a
   background job, so the submissions are made with the protocol directly. */
static test_return_t queue_add_without_unique_TEST(void *object)
{
  Context *test= (Context *)object;
  ASSERT_TRUE(test);
  ASSERT_TRUE(test->_extra_files.size());

  Sqlite sql_handle(test->_extra_files[0]);
  const int inserted_jobs= 3;
  const int rows_before= sql_handle.vcount();
  ASSERT_TRUE(rows_before >= 0);

  gearman_universal_st universal;
  gearman_set_log_fn(universal, error_logger, NULL, GEARMAN_VERBOSE_ERROR);

  gearman_connection_st *connection;
  ASSERT_TRUE(connection= gearman_connection_create(universal, GEARMAN_DEFAULT_TCP_HOST, test->port()));

  for (int x= 0; x < inserted_jobs; ++x)
  {
    char workload[32];
    int workload_length= snprintf(workload, sizeof(workload), "workload %d", x);

    const void *args[3]= { test->worker_function_name(), "", workload };
    const size_t args_size[3]= { strlen(test->worker_function_name()) +1, 1, size_t(workload_length) };

    gearman_packet_st message;
    ASSERT_EQ(GEARMAN_SUCCESS, gearman_packet_create_args(universal, message,
                                                          GEARMAN_MAGIC_REQUEST,
                                                          GEARMAN_COMMAND_SUBMIT_JOB_BG,
                                                          args, args_size, 3));
    ASSERT_EQ(GEARMAN_SUCCESS, connection->send_packet(message, true));

    gearman_return_t ret;
    gearman_packet_st recv_message;
    ASSERT_TRUE(connection->receiving(recv_message, ret, true));
    ASSERT_EQ(GEARMAN_SUCCESS, ret);
    ASSERT_EQ(GEARMAN_COMMAND_JOB_CREATED, recv_message.command);

    gearman_packet_free(&recv_message);
    gearman_packet_free(&message);

    ASSERT_EQ(rows_before +x +1, sql_handle.vcount());
  }

  delete connection;
  gearman_universal_free(universal);

  {
    libgearman::Worker worker(test->port());
    gearman_worker_set_timeout(&worker, 3000);

    Called called;
    gearman_function_t counter_function= gearman_function_create(called_worker);
    ASSERT_EQ(gearman_worker_define_function(&worker,
                                             test->worker_function_name(), strlen(test->worker_function_name()),
                                             counter_function,
                                             0, &called), GEARMAN_SUCCESS);

    for (int x= 0; x < inserted_jobs; ++x)
    {
      ASSERT_EQ(GEARMAN_SUCCESS, gearman_worker_work(&worker));
    }
    ASSERT_EQ(inserted_jobs, called.count());
  }

  // The server removes a job from the queue once it has seen the worker's WORK_COMPLETE.
  for (int x= 0; x < 50 and sql_handle.vcount() != rows_before; ++x)
  {
    libtest::dream(0, 100000);
  }
  ASSERT_EQ(rows_before, sql_handle.vcount());

  return TEST_SUCCESS;
}

static test_return_t collection_init(void *object)
{
  std::string sql_file= libtest::create_tmpfile("sqlite");

  char sql_buffer[1024];
  snprintf(sql_buffer, sizeof(sql_buffer), "--libsqlite3-db=%.*s", int(sql_file.length()), sql_file.c_str());
  const char *argv[]= {
    "--queue-type=libsqlite3", 
    sql_buffer,
    0 };

  Context *test= (Context *)object;
  ASSERT_TRUE(test);
  test->reset();

  ASSERT_TRUE(test->initialize(argv));
  ASSERT_EQ(0, access(sql_file.c_str(), R_OK | W_OK ));

  test->extra_file(sql_file.c_str());
  std::string sql_journal_file(sql_file);
  sql_journal_file+= "-journal";
  test->extra_file(sql_journal_file);

  return TEST_SUCCESS;
}

static test_return_t collection_cleanup(void *object)
{
  Context *test= (Context *)object;
  test->reset();

  return TEST_SUCCESS;
}

static test_return_t lp_1087654_TEST(void* object)
{
  Context *test= (Context *)object;
  server_startup_st &servers= test->_servers;

  const int32_t inserted_jobs= 8;

  std::string sql_file= libtest::create_tmpfile("sqlite");

  Sqlite sql_handle(sql_file);

  char sql_buffer[1024];
  snprintf(sql_buffer, sizeof(sql_buffer), "--libsqlite3-db=%.*s", int(sql_file.length()), sql_file.c_str());
  const char *argv[]= {
    "--queue-type=libsqlite3", 
    sql_buffer,
    "--store-queue-on-shutdown",
    0 };

  {
    in_port_t first_port= libtest::get_free_port();

    ASSERT_TRUE(server_startup(servers, "gearmand", first_port, argv));
    test_compare(0, access(sql_file.c_str(), R_OK | W_OK ));

    {
      libgearman::Worker worker(first_port);
      test_compare(gearman_worker_register(&worker, __func__, 0), GEARMAN_SUCCESS);
    }

    {
      libgearman::Client client(first_port);
      gearman_job_handle_t job_handle;
      for (int32_t x= 0; x < inserted_jobs; ++x)
      {
        switch (random() % 3)
        {
        case 0:
          test_compare(gearman_client_do_background(&client,
                                                    __func__, // func
                                                    NULL, // unique
                                                    test_literal_param("foo"),
                                                    job_handle), GEARMAN_SUCCESS);
          break;

        case 1:
          test_compare(gearman_client_do_low_background(&client,
                                                        __func__, // func
                                                        NULL, // unique
                                                        test_literal_param("fudge"),
                                                        job_handle), GEARMAN_SUCCESS);
          break;

        default:
        case 2:
          test_compare(gearman_client_do_high_background(&client,
                                                         __func__, // func
                                                         NULL, // unique
                                                         test_literal_param("history"),
                                                         job_handle), GEARMAN_SUCCESS);
          break;
        }
      }
    }

    // Before we shutdown we need to see if anything is sitting in the queue
    {
      if (sql_handle.vcount() != 0)
      {
        if (sql_handle.has_error())
        {
          Error << sql_handle.error_string();
        }
        else
        {
          Out << "sql_handle.vprint_unique()";
          sql_handle.vprint_unique();
        }
      }

      test_zero(sql_handle.vcount());
    }

    servers.clear();
  }

  // After shutdown we need to see that the queue was storage
  {
    if (sql_handle.vcount() != inserted_jobs)
    {
      if (sql_handle.has_error())
      {
        Error << sql_handle.error_string();
      }
      else
      {
        Out << "sql_handle.vprint_unique()";
        sql_handle.vprint_unique();
      }
    }

    test_compare(sql_handle.vcount(), inserted_jobs);
  }

  test_compare(0, access(sql_file.c_str(), R_OK | W_OK ));

  {
    in_port_t first_port= libtest::get_free_port();

    ASSERT_TRUE(server_startup(servers, "gearmand", first_port, argv));

    {
      libgearman::Worker worker(first_port);
      Called called;
      gearman_function_t counter_function= gearman_function_create(called_worker);
      test_compare(gearman_worker_define_function(&worker,
                                                  test_literal_param(__func__),
                                                  counter_function,
                                                  3000, &called), GEARMAN_SUCCESS);

      const int32_t max_timeout= 4;
      int32_t max_timeout_value= max_timeout;
      int32_t job_count= 0;
      gearman_return_t ret;
      do
      {
        ret= gearman_worker_work(&worker);
        if (gearman_success(ret))
        {
          job_count++;
          max_timeout_value= max_timeout;
          if (job_count == inserted_jobs)
          {
            break;
          }
        }
        else if (ret == GEARMAN_TIMEOUT)
        {
          Error << " hit timeout";
          if ((--max_timeout_value) < 0)
          {
            break;
          }
        }
      } while (ret == GEARMAN_TIMEOUT or ret == GEARMAN_SUCCESS);
      test_compare(called.count(), inserted_jobs);
    }

    servers.clear();
  }

  {
    if (sql_handle.vcount() != 0)
    {
      Error << "make";
      if (sql_handle.has_error())
      {
        Error << sql_handle.error_string();
      }
      else
      {
        Out << "sql_handle.vprint_unique()";
        sql_handle.vprint_unique();
      }
    }

    test_zero(sql_handle.vcount());
  }

  return TEST_SUCCESS;
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-protector"
static test_return_t queue_restart_TEST(Context const* test, const int32_t inserted_jobs, uint32_t timeout)
{
  SKIP_IF(HAVE_UUID_UUID_H != 1);

  server_startup_st &servers= test->_servers;

  std::string sql_file= libtest::create_tmpfile("sqlite");

  Sqlite sql_handle(sql_file);

  char sql_buffer[1024];
  snprintf(sql_buffer, sizeof(sql_buffer), "--libsqlite3-db=%.*s", int(sql_file.length()), sql_file.c_str());
  const char *argv[]= {
    "--queue-type=libsqlite3", 
    sql_buffer,
    0 };

  {
    in_port_t first_port= libtest::get_free_port();

    ASSERT_TRUE(server_startup(servers, "gearmand", first_port, argv));
    ASSERT_EQ(0, access(sql_file.c_str(), R_OK | W_OK ));

    {
      libgearman::Worker worker(first_port);
      ASSERT_EQ(gearman_worker_register(&worker, __func__, 0), GEARMAN_SUCCESS);
    }

    {
      libgearman::Client client(first_port);
      ASSERT_EQ(gearman_client_echo(&client, test_literal_param("This is my echo test")), GEARMAN_SUCCESS);
      gearman_job_handle_t job_handle;
      for (int32_t x= 0; x < inserted_jobs; ++x)
      {
        switch (random() % 3)
        {
        case 0:
          ASSERT_EQ(gearman_client_do_background(&client,
                                                    __func__, // func
                                                    NULL, // unique
                                                    test_literal_param("foo"),
                                                    job_handle), GEARMAN_SUCCESS);
          break;

        case 1:
          ASSERT_EQ(gearman_client_do_low_background(&client,
                                                        __func__, // func
                                                        NULL, // unique
                                                        test_literal_param("fudge"),
                                                        job_handle), GEARMAN_SUCCESS);
          break;

        default:
        case 2:
          ASSERT_EQ(gearman_client_do_high_background(&client,
                                                         __func__, // func
                                                         NULL, // unique
                                                         test_literal_param("history"),
                                                         job_handle), GEARMAN_SUCCESS);
          break;
        }
      }
    }

    servers.clear();
  }

  {
    if (sql_handle.vcount() != inserted_jobs)
    {
      ASSERT_EQ_(false, sql_handle.has_error(), "sqlite: %s", sql_handle.error_string().c_str());

      Out << "sql_handle.vprint_unique()";
      sql_handle.vprint_unique();
    }

    ASSERT_EQ(sql_handle.vcount(), inserted_jobs);
  }

  ASSERT_EQ(0, access(sql_file.c_str(), R_OK | W_OK ));

  {
    in_port_t first_port= libtest::get_free_port();

    ASSERT_TRUE(server_startup(servers, "gearmand", first_port, argv));

    if (timeout)
    {
      servers.last()->timeout(timeout);
    }

    {
      libgearman::Worker worker(first_port);
      Called called;
      gearman_function_t counter_function= gearman_function_create(called_worker);
      ASSERT_EQ(gearman_worker_define_function(&worker,
                                                  test_literal_param(__func__),
                                                  counter_function,
                                                  3000, &called), GEARMAN_SUCCESS);

      const int32_t max_timeout= 4;
      int32_t max_timeout_value= max_timeout;
      int32_t job_count= 0;
      gearman_return_t ret;
      do
      {
        ret= gearman_worker_work(&worker);
        if (gearman_success(ret))
        {
          job_count++;
          max_timeout_value= max_timeout;
          if (job_count == inserted_jobs)
          {
            break;
          }
        }
        else if (ret == GEARMAN_TIMEOUT)
        {
          Error << " hit timeout";
          if ((--max_timeout_value) < 0)
          {
            break;
          }
        }
      } while (ret == GEARMAN_TIMEOUT or ret == GEARMAN_SUCCESS);
      ASSERT_EQ(called.count(), inserted_jobs);
    }

    servers.clear();
  }

  {
    if (sql_handle.vcount() != 0)
    {
      ASSERT_EQ_(false, sql_handle.has_error(), "sqlite: %s", sql_handle.error_string().c_str());

      Out << "sql_handle.vprint_unique()";
      sql_handle.vprint_unique();
    }

    ASSERT_EQ(0, sql_handle.vcount());
  }

  return TEST_SUCCESS;
}
# pragma GCC diagnostic pop

static test_return_t lp_1054377_TEST(void* object)
{
  Context *test= (Context *)object;
  ASSERT_TRUE(test);

  return queue_restart_TEST(test, 8, 0);
}

static test_return_t lp_1054377x200_TEST(void* object)
{
  test_skip(true, libtest::is_massive());

  Context *test= (Context *)object;
  ASSERT_TRUE(test);

  return queue_restart_TEST(test, 200, 200);
}

static test_return_t skip_SETUP(void*)
{
  SKIP_IF(true);
  return TEST_SUCCESS;
}

static void *world_create(server_startup_st& servers, test_return_t&)
{
  SKIP_IF(HAVE_UUID_UUID_H != 1);
  SKIP_IF(test_for_HAVE_LIBSQLITE3() == false);

  return new Context(servers);
}

static bool world_destroy(void *object)
{
  Context *test= (Context *)object;

  delete test;

  return TEST_SUCCESS;
}

test_st gearmand_basic_option_tests[] ={
  {"--libsqlite3-db=var/tmp/schema --libsqlite3-table=custom_table", 0, gearmand_basic_option_test },
  {"--libsqlite3-db=var/tmp/schema", 0, gearmand_basic_option_without_table_test },
  {"--store-queue-on-shutdown", 0, gearmand_basic_option_shutdown_queue_TEST },
  {0, 0, 0}
};

test_st tests[] ={
  {"gearman_client_echo()", 0, client_echo_test },
  {"gearman_client_echo() fail", 0, client_echo_fail_test },
  {"gearman_worker_echo()", 0, worker_echo_test },
  {"clean", 0, queue_clean },
  {"add", 0, queue_add },
  {"worker", 0, queue_worker },
  {"add without unique", 0, queue_add_without_unique_TEST },
  {0, 0, 0}
};

test_st regressions[] ={
  {"lp:734663", 0, lp_734663 },
  {0, 0, 0}
};

test_st queue_restart_TESTS[] ={
  {"lp:1054377", 0, lp_1054377_TEST },
  {"lp:1054377 x 200", 0, lp_1054377x200_TEST },
  {"lp:1087654", 0, lp_1087654_TEST },
  {0, 0, 0}
};

collection_st collection[] ={
  {"gearmand options", 0, 0, gearmand_basic_option_tests},
  {"sqlite queue", collection_init, collection_cleanup, tests},
  {"queue regression", collection_init, collection_cleanup, regressions},
  {"queue restart", skip_SETUP, 0, queue_restart_TESTS},
#if 0
  {"sqlite queue change table", collection_init, collection_cleanup, tests},
#endif
  {0, 0, 0, 0}
};

void get_world(libtest::Framework *world)
{
  world->collections(collection);
  world->create(world_create);
  world->destroy(world_destroy);
}
