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
#include <libgearman/common.h>
#include <libgearman/add.hpp>
#include <libgearman/universal.hpp>
#include <libgearman/do.hpp>

#include "libgearman/assert.hpp"

#include <cerrno>
#include <cstring>

void *client_do(gearman_client_st *client_shell, gearman_command_t command,
                const char *function_name,
                const char *unique,
                const void *workload_str, size_t workload_size,
                size_t *result_size, gearman_return_t *ret_ptr)
{
  gearman_string_t function= { gearman_string_param_cstr(function_name) };
  gearman_unique_t local_unique= gearman_unique_make(unique, unique ? strlen(unique) : 0);
  gearman_string_t workload= { static_cast<const char*>(workload_str), workload_size };
  
  // Set to zero in case of error
  size_t unused_result_size;
  if (result_size == NULL)
  {
    result_size= &unused_result_size;
  }
  *result_size= 0;
  
  gearman_return_t unused;
  if (ret_ptr == NULL)
  {
    ret_ptr= &unused;
  }

  if (client_shell == NULL or client_shell->impl() == NULL)
  {
    *ret_ptr= GEARMAN_INVALID_ARGUMENT;
    return NULL;
  }
  Client* client= client_shell->impl();

  gearman_task_st* do_task= add_task(*(client), NULL, NULL, command,
                                     function,
                                     local_unique,
                                     workload,
                                     time_t(0),
                                     gearman_actions_do_default());

  if (do_task == NULL)
  {
    *ret_ptr= client->universal.error_code();
    return NULL;
  }

  gearman_return_t ret= gearman_client_run_block_tasks(client, do_task);

  const void *returnable= NULL;
  
  // gearman_client_run_block_tasks failed
  if (gearman_failed(ret))
  {
    gearman_error(client->universal, ret, "occurred during gearman_client_run_tasks()");

    *ret_ptr= ret;
    *result_size= 0;
  }
  else // Now we check the task itself
  {
    assert(ret == GEARMAN_SUCCESS); // Programmer mistake
    if (gearman_success(do_task->impl()->error_code()))
    {
      *ret_ptr= do_task->impl()->error_code();
      if (gearman_task_result(do_task))
      {
        gearman_string_t result= gearman_result_take_string(do_task->impl()->result());
        *result_size= gearman_size(result);
        returnable= gearman_c_str(result);
      }
      else // NULL SUCCESSFUL job
      { }
    }
    else // gearman_client_run_block_tasks() was successful, but the task was not
    {
      gearman_error(client->universal, do_task->impl()->error_code(), "occurred during gearman_client_run_tasks()");

      *ret_ptr= do_task->impl()->error_code();
      *result_size= 0;
    }
  }

  assert(client->task_list);
  gearman_task_free(do_task);
  client->new_tasks= 0;
  client->running_tasks= 0;

  return const_cast<void *>(returnable);
}

gearman_return_t client_do_background(gearman_client_st *client_shell,
                                      gearman_command_t command,
                                      gearman_string_t &function,
                                      gearman_unique_t &unique,
                                      gearman_string_t &workload,
                                      gearman_job_handle_t job_handle)
{
  if (client_shell == NULL or client_shell->impl() == NULL)
  {
    return GEARMAN_INVALID_ARGUMENT;
  }
  Client* client= client_shell->impl();

  gearman_task_st* do_task= add_task(*(client), NULL, 
                                     client, 
                                     command,
                                     function,
                                     unique,
                                     workload,
                                     time_t(0),
                                     gearman_actions_do_default());
  if (do_task == NULL)
  {
    return client->universal.error_code();
  }

  gearman_task_clear_fn(do_task);

  gearman_return_t ret= gearman_client_run_block_tasks(client, do_task);
  assert(ret != GEARMAN_IO_WAIT);
  if (ret != GEARMAN_IO_WAIT)
  {
    if (job_handle)
    {
      strncpy(job_handle, do_task->impl()->job_handle, GEARMAN_JOB_HANDLE_SIZE);
    }
    client->new_tasks= 0;
    client->running_tasks= 0;
  }
  gearman_task_free(do_task);

  return ret;
}
