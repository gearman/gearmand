/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Test that libgearman packets are sent to server in FIFO order.
 *
 *  Copyright (C) 2026 Edward J. Sabol
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

#include <cstring>
#include <cassert>

#include <libgearman-1.0/gearman.h>

#include <tests/context.h>
#include <tests/start_worker.h>

#include "libgearman/client.hpp"

static struct OrderRecorder
{
  char buffer[4];
  int pos;
} recorder;

static gearman_return_t fifo_echo_worker(gearman_job_st* job, void* /*context_arg*/)
{
  /* Echo the workload back as the result (required for the client's
     complete callback to receive the original "1"/"2"/"3" strings). */
  return gearman_job_send_data(job,
                               gearman_job_workload(job),
                               gearman_job_workload_size(job));
}

static gearman_return_t fifo_complete(gearman_task_st *task)
{
  if (recorder.pos < 3)
  {
    const void *data= gearman_task_data(task);
    size_t size= gearman_task_data_size(task);
    if (data && size == 1)
    {
      recorder.buffer[recorder.pos++]= *static_cast<const char *>(data);
    }
  }
  return GEARMAN_SUCCESS;
}

/** Test that verifies gearman_packet_create() now builds a FIFO queue
    (append-to-tail) instead of LIFO (prepend-to-head).

    This directly exercises the packet linked-list change for issue #395.
    Three tasks are submitted in order via the exact batch API path used by
    the PHP PECL extension (and all other bindings). Completion order is
    recorded via the client-level complete callback. After the patch the
    order must be "123"; before the patch it would be "321". */

test_return_t fifo_test(void *object)
{
  Context *context= (Context *)object;
  ASSERT_TRUE(context);

  const char *function_name= "fifo_echo";

  /* Start a background worker that will echo the workload (required so
     the client receives results and can fire complete callbacks). */
  gearman_function_t echo_fn= gearman_function_create(fifo_echo_worker);
  struct worker_handle_st *worker_handle=
    test_worker_start(context->port(),
                      NULL,
                      function_name,
                      echo_fn,
                      NULL,
                      gearman_worker_options_t());
  ASSERT_TRUE(worker_handle);

  /* Client that will submit the batch of tasks. Note: the Client class lives
     in namespace org::gearmand::libgearman. */
  org::gearmand::libgearman::Client client(context->port());

  /* Reset the shared recorder before submitting tasks. */
  recorder.pos= 0;

  /* Register the client-level complete callback. */
  gearman_client_set_complete_fn(&client, fifo_complete);

  /* Submit tasks in FIFO order using the exact API path that populates
     gearman_universal_st::packet_list (the code changed in packet.cc). */
  const char *job_data[3]= {"1", "2", "3"};
  for (int i= 0; i < 3; ++i)
  {
    gearman_return_t rc;
    gearman_task_st* task=
      gearman_client_add_task(&client,
                              NULL,         /* let library allocate task */
                              NULL,         /* not used */
                              function_name,
                              NULL,         /* unique */
                              job_data[i],
                              1,            /* workload size */
                              &rc);
    ASSERT_TRUE(task != NULL);
    ASSERT_EQ(GEARMAN_SUCCESS, rc);
  }

  /* This is where the packet-list ordering matters: run_tasks() walks
     the list and sends packets in the order they were inserted. */
  gearman_return_t ret= gearman_client_run_tasks(&client);
  ASSERT_EQ(GEARMAN_SUCCESS, ret);

  /* Shutdown the worker. */
  worker_handle->shutdown();
  delete worker_handle;

  /* === FIFO ASSERTION === */
  /* Before the packet.cc patch: would be "321" (LIFO). */
  /* After the patch: must be "123" (FIFO). */
  ASSERT_EQ(3, recorder.pos);
  ASSERT_EQ('1', recorder.buffer[0]);
  ASSERT_EQ('2', recorder.buffer[1]);
  ASSERT_EQ('3', recorder.buffer[2]);

  return TEST_SUCCESS;
}

/* ====================================================================
   Required for standalone test binary (provides get_world for libtest)
   ==================================================================== */

extern "C" void get_world(libtest::Framework *framework)
{
  framework->create<Context>();
}
