/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
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
#include <libgearman/common.h>
#include <libgearman/universal.hpp>

#include <libgearman/add.hpp>
#include <libgearman/packet.hpp>

#include "libgearman/assert.hpp"
#include "libgearman/log.hpp"

#include "libgearman/vector.h"
#include "libgearman/uuid.hpp"
#include "libhashkit-1.0/hashkit.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace {

  bool is_background(gearman_command_t command)
  {
    switch (command)
    {
    case GEARMAN_COMMAND_SUBMIT_JOB_EPOCH:
    case GEARMAN_COMMAND_SUBMIT_JOB_SCHED:
    case GEARMAN_COMMAND_SUBMIT_JOB_BG:
    case GEARMAN_COMMAND_SUBMIT_JOB_LOW_BG:
    case GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG:
    case GEARMAN_COMMAND_SUBMIT_REDUCE_JOB_BACKGROUND:
      return true;

    case GEARMAN_COMMAND_SUBMIT_REDUCE_JOB:
    case GEARMAN_COMMAND_SUBMIT_JOB:
    case GEARMAN_COMMAND_SUBMIT_JOB_LOW:
    case GEARMAN_COMMAND_SUBMIT_JOB_HIGH:
      return true;

    case GEARMAN_COMMAND_ALL_YOURS:
    case GEARMAN_COMMAND_CANT_DO:
    case GEARMAN_COMMAND_CAN_DO:
    case GEARMAN_COMMAND_CAN_DO_TIMEOUT:
    case GEARMAN_COMMAND_ECHO_REQ:
    case GEARMAN_COMMAND_ECHO_RES:
    case GEARMAN_COMMAND_ERROR:
    case GEARMAN_COMMAND_GET_STATUS:
    case GEARMAN_COMMAND_GRAB_JOB:
    case GEARMAN_COMMAND_GRAB_JOB_ALL:
    case GEARMAN_COMMAND_GRAB_JOB_UNIQ:
    case GEARMAN_COMMAND_JOB_ASSIGN:
    case GEARMAN_COMMAND_JOB_ASSIGN_ALL:
    case GEARMAN_COMMAND_JOB_ASSIGN_UNIQ:
    case GEARMAN_COMMAND_JOB_CREATED:
    case GEARMAN_COMMAND_MAX:
    case GEARMAN_COMMAND_NOOP:
    case GEARMAN_COMMAND_NO_JOB:
    case GEARMAN_COMMAND_OPTION_REQ:
    case GEARMAN_COMMAND_OPTION_RES:
    case GEARMAN_COMMAND_PRE_SLEEP:
    case GEARMAN_COMMAND_RESET_ABILITIES:
    case GEARMAN_COMMAND_SET_CLIENT_ID:
    case GEARMAN_COMMAND_STATUS_RES:
    case GEARMAN_COMMAND_TEXT:
    case GEARMAN_COMMAND_UNUSED:
    case GEARMAN_COMMAND_WORK_COMPLETE:
    case GEARMAN_COMMAND_WORK_DATA:
    case GEARMAN_COMMAND_WORK_EXCEPTION:
    case GEARMAN_COMMAND_WORK_FAIL:
    case GEARMAN_COMMAND_WORK_STATUS:
    case GEARMAN_COMMAND_WORK_WARNING:
    case GEARMAN_COMMAND_GET_STATUS_UNIQUE:
    case GEARMAN_COMMAND_STATUS_RES_UNIQUE:
      assert(0);
      break;
    }

    return false;
  }

} // namespace

gearman_task_st *add_task(Client& client,
                          void *context,
                          gearman_command_t command,
                          const gearman_string_t &function,
                          gearman_unique_t &unique,
                          const gearman_string_t &workload,
                          time_t when,
                          const gearman_actions_t &actions)
{
  return add_task(client, NULL, context, command, function, unique, workload, when, actions);
}

gearman_task_st *add_task_ptr(Client& client,
                              gearman_task_st *task,
                              void *context,
                              gearman_command_t command,
                              const char *function_name,
                              const char *unique,
                              const void *workload_str, size_t workload_size,
                              time_t when,
                              gearman_return_t& ret_ptr,
                              const gearman_actions_t &actions)
{
  gearman_string_t function= { gearman_string_param_cstr(function_name) };
  gearman_unique_t local_unique= gearman_unique_make(unique, unique ? strlen(unique) : 0);
  gearman_string_t workload= { static_cast<const char *>(workload_str), workload_size };

  task= add_task(client, task, context, command, function, local_unique, workload, when, actions);
  if (task == NULL)
  {
    ret_ptr= client.universal.error_code();
    return NULL;
  }

  ret_ptr= GEARMAN_SUCCESS;

  return task;
}

gearman_task_st *add_task(Client& client,
                          gearman_task_st *task_shell,
                          void *context,
                          gearman_command_t command,
                          const gearman_string_t &function,
                          gearman_unique_t &unique,
                          const gearman_string_t &workload,
                          time_t when,
                          const gearman_actions_t &actions)
{
  if (gearman_size(function) == 0 or gearman_c_str(function) == NULL or gearman_size(function) > GEARMAN_FUNCTION_MAX_SIZE)
  {
    if (gearman_size(function) > GEARMAN_FUNCTION_MAX_SIZE)
    {
      gearman_error(client.universal, GEARMAN_INVALID_ARGUMENT, "function name longer then GEARMAN_MAX_FUNCTION_SIZE");
    } 
    else
    {
      gearman_error(client.universal, GEARMAN_INVALID_ARGUMENT, "invalid function");
    }

    return NULL;
  }

  if (gearman_size(unique) > GEARMAN_MAX_UNIQUE_SIZE)
  {
    gearman_error(client.universal, GEARMAN_INVALID_ARGUMENT, "unique name longer then GEARMAN_MAX_UNIQUE_SIZE");

    return NULL;
  }

  if ((gearman_size(workload) && gearman_c_str(workload) == NULL) or (gearman_size(workload) == 0 && gearman_c_str(workload)))
  {
    gearman_error(client.universal, GEARMAN_INVALID_ARGUMENT, "invalid workload");
    return NULL;
  }

  task_shell= gearman_task_internal_create(&client, task_shell);
  if (task_shell == NULL or task_shell->impl() == NULL)
  {
    assert(client.universal.error());
    return NULL;
  }
  assert(task_shell->impl()->client);

  Task* task= task_shell->impl();

  task->context= context;
  task->func= actions;

  if (gearman_unique_is_hash(unique))
  {
    task->unique_length= snprintf(task->unique, GEARMAN_MAX_UNIQUE_SIZE, "%u", libhashkit_murmur3(gearman_string_param(workload)));
  }
  else if ((task->unique_length= gearman_size(unique)))
  {
    if (task->unique_length >= GEARMAN_MAX_UNIQUE_SIZE)
    {
      task->unique_length= GEARMAN_MAX_UNIQUE_SIZE -1; // Leave space for NULL byte
    }
/* We check to make sure there's space for the NULL above so this
   warning does not apply */
#pragma GCC diagnostic push
#ifdef __GNUC__
#if __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
#endif
    strncpy(task->unique, gearman_c_str(unique), GEARMAN_MAX_UNIQUE_SIZE);
#pragma GCC diagnostic pop
    task->unique[task->unique_length]= 0;
  }
  else
  {
    if (client.options.generate_unique or is_background(command))
    {
      if (safe_uuid_generate(task->unique, task->unique_length) == -1)
      {
        gearman_log_debug(task->client->universal, "uuid_generate_time_safe() failed or does not exist on this platform");
      }
    }
    else
    {
      task->unique_length= 0;
      task->unique[0]= 0;
    }
  }

  gearman_unique_t final_unique= gearman_unique_make(task->unique, task->unique_length);

  assert(task->client);

  gearman_return_t rc= GEARMAN_INVALID_ARGUMENT;
  switch (command)
  {
  case GEARMAN_COMMAND_SUBMIT_JOB:
  case GEARMAN_COMMAND_SUBMIT_JOB_LOW:
  case GEARMAN_COMMAND_SUBMIT_JOB_HIGH:
    rc= libgearman::protocol::submit(task->client->universal,
                                     task->send,
                                     final_unique,
                                     command,
                                     function,
                                     workload);
    break;

  case GEARMAN_COMMAND_SUBMIT_JOB_EPOCH:
    rc= libgearman::protocol::submit_epoch(task->client->universal,
                                           task->send,
                                           final_unique,
                                           function,
                                           workload,
                                           when);
    break;

  case GEARMAN_COMMAND_SUBMIT_JOB_BG:
  case GEARMAN_COMMAND_SUBMIT_JOB_LOW_BG:
  case GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG:
    rc= libgearman::protocol::submit_background(task->client->universal,
                                                task->send,
                                                final_unique,
                                                command,
                                                function,
                                                workload);
    break;

  case GEARMAN_COMMAND_SUBMIT_REDUCE_JOB:
  case GEARMAN_COMMAND_SUBMIT_REDUCE_JOB_BACKGROUND:
    rc= GEARMAN_INVALID_ARGUMENT;
    assert(rc != GEARMAN_INVALID_ARGUMENT);
    break;

  case GEARMAN_COMMAND_SUBMIT_JOB_SCHED:
  case GEARMAN_COMMAND_ALL_YOURS:
  case GEARMAN_COMMAND_CANT_DO:
  case GEARMAN_COMMAND_CAN_DO:
  case GEARMAN_COMMAND_CAN_DO_TIMEOUT:
  case GEARMAN_COMMAND_ECHO_REQ:
  case GEARMAN_COMMAND_ECHO_RES:
  case GEARMAN_COMMAND_ERROR:
  case GEARMAN_COMMAND_GET_STATUS:
  case GEARMAN_COMMAND_GRAB_JOB:
  case GEARMAN_COMMAND_GRAB_JOB_ALL:
  case GEARMAN_COMMAND_GRAB_JOB_UNIQ:
  case GEARMAN_COMMAND_JOB_ASSIGN:
  case GEARMAN_COMMAND_JOB_ASSIGN_ALL:
  case GEARMAN_COMMAND_JOB_ASSIGN_UNIQ:
  case GEARMAN_COMMAND_JOB_CREATED:
  case GEARMAN_COMMAND_MAX:
  case GEARMAN_COMMAND_NOOP:
  case GEARMAN_COMMAND_NO_JOB:
  case GEARMAN_COMMAND_OPTION_REQ:
  case GEARMAN_COMMAND_OPTION_RES:
  case GEARMAN_COMMAND_PRE_SLEEP:
  case GEARMAN_COMMAND_RESET_ABILITIES:
  case GEARMAN_COMMAND_SET_CLIENT_ID:
  case GEARMAN_COMMAND_STATUS_RES:
  case GEARMAN_COMMAND_TEXT:
  case GEARMAN_COMMAND_UNUSED:
  case GEARMAN_COMMAND_WORK_COMPLETE:
  case GEARMAN_COMMAND_WORK_DATA:
  case GEARMAN_COMMAND_WORK_EXCEPTION:
  case GEARMAN_COMMAND_WORK_FAIL:
  case GEARMAN_COMMAND_WORK_STATUS:
  case GEARMAN_COMMAND_WORK_WARNING:
  case GEARMAN_COMMAND_GET_STATUS_UNIQUE:
  case GEARMAN_COMMAND_STATUS_RES_UNIQUE:
    rc= GEARMAN_INVALID_ARGUMENT;
    assert(rc != GEARMAN_INVALID_ARGUMENT);
    break;
  }

  if (gearman_success(rc))
  {
    client.new_tasks++;
    client.running_tasks++;
    task->options.send_in_use= true;

    return task->shell();
  }

  gearman_task_free(task->shell());

  return NULL;
}

gearman_task_st *add_reducer_task(Client* client,
                                  gearman_command_t command,
                                  const gearman_job_priority_t,
                                  const gearman_string_t &function,
                                  const gearman_string_t &reducer,
                                  const gearman_unique_t &unique,
                                  const gearman_string_t &workload,
                                  const gearman_actions_t &actions,
                                  const time_t,
                                  void *context)
{
  const void *args[5];
  size_t args_size[5];

  if (gearman_size(function) == 0 or gearman_c_str(function) == NULL or gearman_size(function) > GEARMAN_FUNCTION_MAX_SIZE)
  {
    if (gearman_size(function) > GEARMAN_FUNCTION_MAX_SIZE)
    {
      gearman_error(client->universal, GEARMAN_INVALID_ARGUMENT, "function name longer then GEARMAN_MAX_FUNCTION_SIZE");
    } 
    else
    {
      gearman_error(client->universal, GEARMAN_INVALID_ARGUMENT, "invalid function");
    }

    return NULL;
  }

  if (gearman_size(unique) > GEARMAN_MAX_UNIQUE_SIZE)
  {
    gearman_error(client->universal, GEARMAN_INVALID_ARGUMENT, "unique name longer then GEARMAN_MAX_UNIQUE_SIZE");

    return NULL;
  }

  if ((gearman_size(workload) and not gearman_c_str(workload)) or (gearman_size(workload) == 0 && gearman_c_str(workload)))
  {
    gearman_error(client->universal, GEARMAN_INVALID_ARGUMENT, "invalid workload");
    return NULL;
  }

  gearman_task_st *task_shell= gearman_task_internal_create(client, NULL);
  if (task_shell == NULL)
  {
    assert(client->universal.error_code());
    return NULL;
  }

  Task* task= task_shell->impl();
  task->context= context;
  task->func= actions;

  /**
    @todo fix it so that NULL is done by default by the API not by happenstance.
  */
  char function_buffer[1024];
  if (client->universal._namespace)
  {
    char *ptr= function_buffer;
    memcpy(ptr, gearman_string_value(client->universal._namespace), gearman_string_length(client->universal._namespace)); 
    ptr+= gearman_string_length(client->universal._namespace);

    memcpy(ptr, gearman_c_str(function), gearman_size(function) +1);
    ptr+= gearman_size(function);

    args[0]= function_buffer;
    args_size[0]= ptr- function_buffer +1;
  }
  else
  {
    args[0]= gearman_c_str(function);
    args_size[0]= gearman_size(function) + 1;
  }

  if (gearman_unique_is_hash(unique))
  {
    task->unique_length= snprintf(task->unique, GEARMAN_MAX_UNIQUE_SIZE, "%u", libhashkit_murmur3(gearman_string_param(workload)));
  }
  else if ((task->unique_length= gearman_size(unique)))
  {
    if (task->unique_length >= GEARMAN_MAX_UNIQUE_SIZE)
    {
      task->unique_length= GEARMAN_MAX_UNIQUE_SIZE -1; // Leave space for NULL byte
    }
/* We check to make sure there's space for the NULL above so this
   warning does not apply */
#pragma GCC diagnostic push
#ifdef __GNUC__
#if __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
#endif
    strncpy(task->unique, gearman_c_str(unique), GEARMAN_MAX_UNIQUE_SIZE);
#pragma GCC diagnostic pop
    task->unique[task->unique_length]= 0;
  }
  else
  {
    if (client->options.generate_unique or is_background(command))
    {
      safe_uuid_generate(task->unique, task->unique_length);
    }
    else
    {
      task->unique_length= 0;
      task->unique[0]= 0;
    }
  }

  args[1]= task->unique;
  args_size[1]= task->unique_length +1; // +1 is for the needed null

  assert_msg(command == GEARMAN_COMMAND_SUBMIT_REDUCE_JOB or command == GEARMAN_COMMAND_SUBMIT_REDUCE_JOB_BACKGROUND,
             "Command was not appropriate for request");

  char reducer_buffer[1024];
  if (client->universal._namespace)
  {
    char *ptr= reducer_buffer;
    memcpy(ptr, gearman_string_value(client->universal._namespace), gearman_string_length(client->universal._namespace)); 
    ptr+= gearman_string_length(client->universal._namespace);

    memcpy(ptr, gearman_c_str(reducer), gearman_size(reducer) +1);
    ptr+= gearman_size(reducer);

    args[2]= reducer_buffer;
    args_size[2]= ptr- reducer_buffer +1;
  }
  else
  {
    args[2]= gearman_c_str(reducer);
    args_size[2]= gearman_size(reducer) +1;
  }

  char aggregate[1];
  aggregate[0]= 0;
  args[3]= aggregate;
  args_size[3]= 1;

  assert_msg(gearman_c_str(workload), "Invalid workload (NULL)");
  assert_msg(gearman_size(workload), "Invalid workload of zero");
  args[4]= gearman_c_str(workload);
  args_size[4]= gearman_size(workload);

  gearman_return_t rc;
  if (gearman_success(rc= gearman_packet_create_args(client->universal, task->send,
                                                     GEARMAN_MAGIC_REQUEST, command,
                                                     args, args_size,
                                                     5)))
  {
    client->new_tasks++;
    client->running_tasks++;
    task->options.send_in_use= true;
  }
  else
  {
    gearman_gerror(client->universal, rc);
    gearman_task_free(task);
    task= NULL;
  }
  task->type= GEARMAN_TASK_KIND_EXECUTE;

  return task->shell();
}
