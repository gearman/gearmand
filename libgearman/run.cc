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

#include "libgearman/assert.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if __GNUC__ >= 7
  #pragma GCC diagnostic warning "-Wimplicit-fallthrough"
#endif
gearman_return_t _client_run_task(Task *task)
{
  // This should not be possible
  assert_msg(task->client, "Programmer error, somehow an invalid task was specified");
  if (task->client == NULL)
  {
    return gearman_universal_set_error(task->client->universal, GEARMAN_INVALID_ARGUMENT, GEARMAN_AT,
                                       "Programmer error, somehow an invalid task was specified");
  }

  switch(task->state)
  {
  case GEARMAN_TASK_STATE_NEW:
    
    if (task->client->universal.has_connections() == false)
    {
      assert(task->client->universal.con_count == 0);
      assert(task->client->universal.con_list == NULL);
      task->client->new_tasks--;
      task->client->running_tasks--;
      return gearman_universal_set_error(task->client->universal, GEARMAN_NO_SERVERS, GEARMAN_AT, "no servers provided");
    }

    for (task->con= task->client->universal.con_list; task->con;
         task->con= task->con->next_connection())
    {
      if (task->con->send_state == GEARMAN_CON_SEND_STATE_NONE)
      {
        break;
      }
    }

    if (task->con == NULL)
    {
      task->client->options.no_new= true;
      return gearman_gerror(task->client->universal, GEARMAN_IO_WAIT);
    }

    task->client->new_tasks--;

    if (task->send.command != GEARMAN_COMMAND_GET_STATUS)
    {
      task->created_id= task->con->created_id_next;
      task->con->created_id_next++;
    }
    /* fall-thru */

  case GEARMAN_TASK_STATE_SUBMIT:
    while (1)
    {
      assert(task->con);
      gearman_return_t ret= task->con->send_packet(task->send, task->client->new_tasks == 0 ? true : false);

      if (gearman_success(ret))
      {
        break;
      }
      else if (ret == GEARMAN_IO_WAIT)
      {
        task->set_state(GEARMAN_TASK_STATE_SUBMIT);
        return ret;
      }
      else if (gearman_failed(ret))
      {
        /* Increment this since the job submission failed. */
        task->con->created_id++;

        if (ret == GEARMAN_COULD_NOT_CONNECT)
        {
          for (task->con= task->con->next_connection(); 
               task->con;
               task->con= task->con->next_connection())
          {
            if (task->con->send_state == GEARMAN_CON_SEND_STATE_NONE)
            {
              break;
            }
          }
        }
        else
        {
          task->con= NULL;
        }

        if (not task->con)
        {
          task->error_code(ret);

          if (ret == GEARMAN_COULD_NOT_CONNECT) // If no connection is found, we will let the user try again
          {
            task->set_state(GEARMAN_TASK_STATE_NEW);
            task->client->new_tasks++;
          }
          else
          {
            task->set_state(GEARMAN_TASK_STATE_FAIL);
            task->client->running_tasks--;
          }
          return ret;
        }

        if (task->send.command != GEARMAN_COMMAND_GET_STATUS)
        {
          task->created_id= task->con->created_id_next;
          task->con->created_id_next++;
        }
      }
    }

    if (task->send.data_size > 0 and not task->send.data)
    {
      if (not task->func.workload_fn)
      {
        gearman_error(task->client->universal, GEARMAN_NEED_WORKLOAD_FN,
                      "workload size > 0, but no data pointer or workload_fn was given");
        return GEARMAN_NEED_WORKLOAD_FN;
      }

  case GEARMAN_TASK_STATE_WORKLOAD:
      gearman_return_t ret= task->func.workload_fn(task->shell());
      if (gearman_failed(ret))
      {
        task->set_state(GEARMAN_TASK_STATE_WORKLOAD);
        return ret;
      }
    }

    task->client->options.no_new= false;
    task->set_state(GEARMAN_TASK_STATE_WORK);
    task->con->set_events(POLLIN);
    return GEARMAN_SUCCESS;

  case GEARMAN_TASK_STATE_WORK:
    if (task->recv->command == GEARMAN_COMMAND_JOB_CREATED)
    {
      task->options.is_known= true;
      snprintf(task->job_handle, GEARMAN_JOB_HANDLE_SIZE, "%.*s",
               int(task->recv->arg_size[0]),
               static_cast<char *>(task->recv->arg[0]));

  case GEARMAN_TASK_STATE_CREATED:
      if (task->func.created_fn)
      {
        gearman_return_t ret= task->func.created_fn(task->shell());
        if (gearman_failed(ret))
        {
          task->set_state(GEARMAN_TASK_STATE_CREATED);
          return ret;
        }
      }

      if (task->send.command == GEARMAN_COMMAND_SUBMIT_JOB_BG ||
          task->send.command == GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG ||
          task->send.command == GEARMAN_COMMAND_SUBMIT_JOB_LOW_BG ||
          task->send.command == GEARMAN_COMMAND_SUBMIT_JOB_EPOCH ||
          task->send.command == GEARMAN_COMMAND_SUBMIT_REDUCE_JOB_BACKGROUND)
      {
        task->error_code(GEARMAN_SUCCESS);
        break;
      }
    }
    else if (task->recv->command == GEARMAN_COMMAND_WORK_DATA)
    {
      task->options.is_known= true;
      task->options.is_running= true;

  case GEARMAN_TASK_STATE_DATA:
      if (task->func.data_fn)
      {
        gearman_return_t ret= task->func.data_fn(task->shell());
        if (gearman_failed(ret))
        {
          task->set_state(GEARMAN_TASK_STATE_DATA);
          return ret;
        }
      }
    }
    else if (task->recv->command == GEARMAN_COMMAND_WORK_WARNING)
    {
  case GEARMAN_TASK_STATE_WARNING:
      if (task->func.warning_fn)
      {
        gearman_return_t ret= task->func.warning_fn(task->shell());
        if (gearman_failed(ret))
        {
          task->set_state(GEARMAN_TASK_STATE_WARNING);
          return ret;
        }
      }
    }
    else if (task->recv->command == GEARMAN_COMMAND_WORK_STATUS or
             task->recv->command == GEARMAN_COMMAND_STATUS_RES_UNIQUE or
             task->recv->command == GEARMAN_COMMAND_STATUS_RES)
    {
      uint8_t x;

      if (task->recv->command == GEARMAN_COMMAND_STATUS_RES)
      {
        task->error_code(GEARMAN_SUCCESS);
        if (atoi(static_cast<char *>(task->recv->arg[1])) == 0)
        {
          task->options.is_known= false;
        }
        else
        {
          task->options.is_known= true;
        }

        if (atoi(static_cast<char *>(task->recv->arg[2])) == 0)
        {
          task->options.is_running= false;
        }
        else
        {
          task->options.is_running= true;
        }

        x= 3;
      }
      else if (task->recv->command == GEARMAN_COMMAND_STATUS_RES_UNIQUE)
      {
        task->error_code(GEARMAN_SUCCESS);
        strncpy(task->unique, task->recv->arg[0], GEARMAN_MAX_UNIQUE_SIZE);
        task->unique[GEARMAN_MAX_UNIQUE_SIZE -1]= '\0';
        if (atoi(static_cast<char *>(task->recv->arg[1])) == 0)
        {
          task->options.is_known= false;
        }
        else
        {
          task->options.is_known= true;
        }

        if (atoi(static_cast<char *>(task->recv->arg[2])) == 0)
        {
          task->options.is_running= false;
        }
        else
        {
          task->options.is_running= true;
        }

        x= 3;
      }
      else
      {
        x= 1;
      }

      task->numerator= uint32_t(atoi(static_cast<char *>(task->recv->arg[x])));

      // denomitor
      {
        char status_buffer[11]; /* Max string size to hold a uint32_t. */
        snprintf(status_buffer, 11, "%.*s",
                 int(task->recv->arg_size[x + 1]),
                 static_cast<char *>(task->recv->arg[x + 1]));
        task->denominator= uint32_t(atoi(status_buffer));
      }

      // client_count
      if (task->recv->command == GEARMAN_COMMAND_STATUS_RES_UNIQUE)
      {
        char status_buffer[11]; /* Max string size to hold a uint32_t. */
        snprintf(status_buffer, 11, "%.*s",
                 int(task->recv->arg_size[x +2]),
                 static_cast<char *>(task->recv->arg[x +2]));
        task->client_count= uint32_t(atoi(status_buffer));
      }

  case GEARMAN_TASK_STATE_STATUS:
      if (task->func.status_fn)
      {
        gearman_return_t ret= task->func.status_fn(task->shell());
        if (gearman_failed(ret))
        {
          task->set_state(GEARMAN_TASK_STATE_STATUS);
          return ret;
        }
      }

      if (task->send.command == GEARMAN_COMMAND_GET_STATUS)
      {
        break;
      }

      if (task->send.command == GEARMAN_COMMAND_GET_STATUS_UNIQUE)
      {
        break;
      }
    }
    else if (task->recv->command == GEARMAN_COMMAND_WORK_COMPLETE)
    {
      task->options.is_known= false;
      task->options.is_running= false;
      task->error_code(GEARMAN_SUCCESS);

  case GEARMAN_TASK_STATE_COMPLETE:
      if (task->func.complete_fn)
      {
        gearman_return_t ret= task->func.complete_fn(task->shell());
        if (gearman_failed(ret))
        {
          task->set_state(GEARMAN_TASK_STATE_COMPLETE);
          return ret;
        }
      }

      break;
    }
    else if (task->recv->command == GEARMAN_COMMAND_WORK_EXCEPTION)
    {
      task->options.is_known= false;
      task->options.is_running= false;
      if (task->recv->argc == 1 and task->recv->data_size)
      {
        task->exception.store((const char*)(task->recv->data), task->recv->data_size);
      }
      task->free_result();
      task->error_code(GEARMAN_WORK_EXCEPTION);

  case GEARMAN_TASK_STATE_EXCEPTION:
      if (task->func.exception_fn)
      {
        gearman_return_t ret= task->func.exception_fn(task->shell());
        if (gearman_failed(ret))
        {
          task->set_state(GEARMAN_TASK_STATE_EXCEPTION);
          return ret;
        }
      }

      break;
    }
    else if (task->recv->command == GEARMAN_COMMAND_WORK_FAIL)
    {
      // If things fail we need to delete the result, and set the result_rc
      // correctly.
      task->options.is_known= false;
      task->options.is_running= false;
      task->free_result();
      task->error_code(GEARMAN_WORK_FAIL);

  case GEARMAN_TASK_STATE_FAIL:
      if (task->func.fail_fn)
      {
        gearman_return_t ret= task->func.fail_fn(task->shell());
        if (gearman_failed(ret))
        {
          task->set_state(GEARMAN_TASK_STATE_FAIL);
          return ret;
        }
      }

      break;
    }

    task->set_state(GEARMAN_TASK_STATE_WORK);
    return GEARMAN_SUCCESS;

  case GEARMAN_TASK_STATE_FINISHED:
    break;
  }

  if (task->state != GEARMAN_TASK_STATE_FINISHED)
  {
    task->client->running_tasks--;
    task->set_state(GEARMAN_TASK_STATE_FINISHED);
  }

  // @todo this should never happen... but background tasks can signal it.
  assert(task->error_code() != GEARMAN_UNKNOWN_STATE);

  if (task->client->options.free_tasks and task->type == GEARMAN_TASK_KIND_ADD_TASK)
  {
    gearman_task_free(task->shell());
  }

  return GEARMAN_SUCCESS;
}
