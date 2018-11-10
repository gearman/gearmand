/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2013 Data Differential, http://datadifferential.com/
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

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define GEARMAN_CORE
#include "libgearman/common.h"
#include "libgearman/packet.hpp"

#include "libgearman/universal.hpp"
#include "libgearman/protocol/echo.h"
#include "libgearman/protocol/work_exception.h"

#include "tests/regression.h"
#include <typeinfo>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

static void error_logger(const char* message, gearman_verbose_t, void*)
{
  Error << message;
}

static test_return_t GEARMAN_COMMAND_ECHO_REQ_TEST(void *)
{
  vchar_t data;
  vchar::make(data, 2048);

  gearman_universal_st universal;
  gearman_set_log_fn(universal, error_logger, NULL, GEARMAN_VERBOSE_ERROR);
  universal.ssl(libtest::is_ssl());

  gearman_packet_st message;

  gearman_string_t workload= { vchar_param(data) };

  ASSERT_EQ(GEARMAN_SUCCESS, libgearman::protocol::echo(universal, message, workload));

  gearman_connection_st *connection1;
  ASSERT_TRUE(connection1= gearman_connection_create(universal, GEARMAN_DEFAULT_TCP_HOST, libtest::default_port()));
  ASSERT_TRUE(connection1);

  ASSERT_EQ(GEARMAN_SUCCESS, connection1->send_packet(message, true));

  size_t length;
  gearman_return_t ret;
  gearman_packet_st recv_message;
  ASSERT_TRUE(connection1->receiving(recv_message, ret, true));
  ASSERT_EQ(GEARMAN_SUCCESS, ret);
  ASSERT_TRUE(recv_message.value(length));
  ASSERT_EQ(length, data.size());

  delete connection1;
  gearman_packet_free(&recv_message);
  gearman_packet_free(&message);
  gearman_universal_free(universal);

  return TEST_SUCCESS;
}

#if 0
static test_return_t GEARMAN_COMMAND_WORK_EXCEPTION_TEST(void *)
{
  vchar_t handle;
  vchar::make(handle, GEARMAN_JOB_HANDLE_SIZE);
  handle.resize(GEARMAN_JOB_HANDLE_SIZE +1); // Add a null

  vchar_t exception;
  vchar::make(exception, 2048);

  gearman_universal_st universal;
  gearman_set_log_fn(universal, error_logger, NULL, GEARMAN_VERBOSE_ERROR);
  universal.ssl(libtest::is_ssl());

  gearman_packet_st message;

  gearman_string_t handle_string= { &handle[0], GEARMAN_JOB_HANDLE_SIZE };
  gearman_string_t exception_string= { vchar_param(exception) };

  Error << "handle: " << &handle[0] << " size: " << handle.size() << " size:" << exception.size();

  ASSERT_EQ(GEARMAN_SUCCESS, libgearman::protocol::work_exception(universal, message, handle_string, exception_string));

  gearman_connection_st *connection1;
  ASSERT_TRUE(connection1= gearman_connection_create(universal, GEARMAN_DEFAULT_TCP_HOST, libtest::default_port()));
  ASSERT_TRUE(connection1);

  ASSERT_EQ(GEARMAN_SUCCESS, connection1->send_packet(message, true));

  size_t length;
  gearman_return_t ret;
  gearman_packet_st recv_message;
  ASSERT_TRUE(connection1->receiving(recv_message, ret, true));
  ASSERT_EQ(GEARMAN_SUCCESS, ret);
  ASSERT_TRUE(recv_message.value(length));
  ASSERT_EQ(length, exception.size());

  delete connection1;
  gearman_packet_free(&recv_message);
  gearman_packet_free(&message);
  gearman_universal_free(universal);

  return TEST_SUCCESS;
}
#endif

static test_return_t GEARMAN_COMMAND_ECHO_REQ_overrun_TEST(void *)
{
  vchar_t data;
  vchar::make(data, 2048);

  gearman_universal_st universal;
  gearman_set_log_fn(universal, error_logger, NULL, GEARMAN_VERBOSE_ERROR);
  universal.ssl(libtest::is_ssl());

  gearman_packet_st message;

  gearman_string_t workload= { vchar_param(data) };

  ASSERT_EQ(GEARMAN_SUCCESS, libgearman::protocol::echo(universal, message, workload));

  gearman_connection_st *connection1;
  ASSERT_TRUE(connection1= gearman_connection_create(universal, GEARMAN_DEFAULT_TCP_HOST, libtest::default_port()));
  ASSERT_TRUE(typeid(decltype(*connection1)).name()==typeid(gearman_connection_st).name());

  for (size_t x= 0; x < 1000; ++x)
  {
    ASSERT_EQ(GEARMAN_SUCCESS, connection1->send_packet(message, true));
  }

  size_t length;
  gearman_return_t ret;
  gearman_packet_st recv_message;
  ASSERT_TRUE(connection1->receiving(recv_message, ret, true));
  ASSERT_EQ(GEARMAN_SUCCESS, ret);
  ASSERT_TRUE(recv_message.value(length));
  ASSERT_EQ(length, data.size());

  delete connection1;
  gearman_packet_free(&recv_message);
  gearman_packet_free(&message);
  gearman_universal_free(universal);

  return TEST_SUCCESS;
}

test_st GEARMAN_COMMAND_ECHO_REQ_TESTS[] ={
  {"GEARMAN_COMMAND_ECHO_REQ check", 0, GEARMAN_COMMAND_ECHO_REQ_TEST },
  {"GEARMAN_COMMAND_ECHO_REQ overrun", 0, GEARMAN_COMMAND_ECHO_REQ_overrun_TEST },
  {0, 0, 0}
};

test_st GEARMAN_COMMAND_WORK_EXCEPTION_TESTS[] ={
#if 0
  {"GEARMAN_COMMAND_WORK_EXCEPTION check", 0, GEARMAN_COMMAND_WORK_EXCEPTION_TEST },
#endif
  {0, 0, 0}
};

collection_st collection[] ={
  {"GEARMAN_COMMAND_ECHO_REQ", 0, 0, GEARMAN_COMMAND_ECHO_REQ_TESTS},
  {"GEARMAN_COMMAND_WORK_EXCEPTION", 0, 0, GEARMAN_COMMAND_WORK_EXCEPTION_TESTS},
  {0, 0, 0, 0}
};

static void *world_create(server_startup_st& servers, test_return_t&)
{
  /**
    We start up everything before we allocate so that we don't have to track memory in the forked process.
  */
  SKIP_UNLESS(server_startup(servers, "gearmand", libtest::default_port(), NULL));

  return NULL;
}

void get_world(libtest::Framework *world)
{
  world->collections(collection);
  world->create(world_create);
}

