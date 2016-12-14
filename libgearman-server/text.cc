/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2012-2013 Data Differential, http://datadifferential.com/ All
 *  rights reserved.
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

#include "libgearman-server/common.h"
#include "libgearman-server/log.h"
#include "libgearman/command.h"
#include "libgearman/vector.hpp"

#include <cassert>
#include <cerrno>
#include <cstring>

#define TEXT_SUCCESS "OK\r\n"
#define TEXT_ERROR_ARGS "ERR INVALID_ARGUMENTS An+incomplete+set+of+arguments+was+sent+to+this+command+%.*s\r\n"
#define TEXT_ERROR_CREATE_FUNCTION "ERR CREATE_FUNCTION %.*s\r\n"
#define TEXT_ERROR_UNKNOWN_COMMAND "ERR UNKNOWN_COMMAND Unknown+server+command%.*s\r\n"
#define TEXT_ERROR_INTERNAL_ERROR "ERR UNKNOWN_ERROR\r\n"
#define TEXT_ERROR_UNKNOWN_SHOW_ARGUMENTS "ERR UNKNOWN_SHOW_ARGUMENTS\r\n"
#define TEXT_ERROR_UNKNOWN_JOB "ERR UNKNOWN_JOB\r\n"

gearmand_error_t server_run_text(gearman_server_con_st *server_con,
                                 gearmand_packet_st *packet)
{
  gearman_vector_st data(GEARMAND_TEXT_RESPONSE_SIZE);

  if (packet->argc)
  {
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "text command %.*s %d arguments",
                       packet->arg_size[0],  packet->arg[0],
                       int(packet->argc));
  }

#if 0
  const struct gearman_command_info_st *command= NULL;
#endif
  if (packet->argc == 0)
  {
    data.vec_printf(TEXT_ERROR_UNKNOWN_COMMAND, 4, "NULL");
  }
#if 0
  else if ((command= gearman_command_lookup((char *)(packet->arg[0]), packet->arg_size[0])))
  {
  }
#endif
  else if (strcasecmp("workers", (char *)(packet->arg[0])) == 0)
  {
    for (gearman_server_thread_st *thread= Server->thread_list;
         thread != NULL;
         thread= thread->next)
    {
      int error;
      if ((error= pthread_mutex_lock(&thread->lock)) == 0)
      {
        for (gearman_server_con_st *con= thread->con_list; con != NULL; con= con->next)
        {
          if (con->_host == NULL)
          {
            continue;
          }

          data.vec_append_printf("%d %s %s :", con->con.fd(), con->_host, con->id);

          for (gearman_server_worker_st *worker= con->worker_list; worker != NULL; worker= worker->con_next)
          {
            data.vec_append_printf(" %.*s",
                                   (int)(worker->function->function_name_size),
                                   worker->function->function_name);
          }

          data.vec_append_printf("\n");
        }

        if ((error= (pthread_mutex_unlock(&(thread->lock)))) != 0)
        {
          gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_unlock");
        }
      }
      else
      {
        gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_lock");
      }
    }

    data.vec_append_printf(".\n");
  }
  else if (strcasecmp("prioritystatus", (char *)(packet->arg[0])) == 0)
  {
    uint32_t job_queued[GEARMAN_JOB_PRIORITY_MAX];

    for (uint32_t function_key= 0;
         function_key < GEARMAND_DEFAULT_HASH_SIZE;
         function_key++)
    {
      for (gearman_server_function_st *function= Server->function_hash[function_key];
           function != NULL;
           function= function->next)
      {
        for (size_t priority = 0; priority < GEARMAN_JOB_PRIORITY_MAX; priority++)
        {
          job_queued[priority] = 0;
          for (gearman_server_job_st *server_job= function->job_list[priority];
               server_job != NULL;
               server_job= server_job->next)
          {
            job_queued[priority]++;
          }
        }

        data.vec_append_printf("%.*s\t%u\t%u\t%u\t%u\n",
                               int(function->function_name_size), function->function_name,
                               job_queued[GEARMAN_JOB_PRIORITY_HIGH],
                               job_queued[GEARMAN_JOB_PRIORITY_NORMAL],
                               job_queued[GEARMAN_JOB_PRIORITY_LOW],
                               function->worker_count);
      }
    }
    data.vec_append_printf(".\n");
  }
  else if (strcasecmp("status", (char *)(packet->arg[0])) == 0)
  {
    for (uint32_t function_key= 0;
         function_key < GEARMAND_DEFAULT_HASH_SIZE;
         function_key++)
    {
      for (gearman_server_function_st *function= Server->function_hash[function_key];
           function != NULL;
           function= function->next)
      {
        data.vec_append_printf("%.*s\t%u\t%u\t%u\n",
                               int(function->function_name_size),
                               function->function_name, function->job_total,
                               function->job_running, function->worker_count);
      }
    }
    data.vec_append_printf(".\n");
  }
  else if (packet->argc >= 3 
           and strcasecmp("cancel", (char *)(packet->arg[0])) == 0)
  {
    if (packet->argc == 3
        and strcasecmp("job", (char *)(packet->arg[1])) == 0)
    {
      gearmand_error_t ret= gearman_server_job_cancel(Gearmand()->server, packet->arg[2], strlen(packet->arg[2]));

      if (ret == GEARMAND_SUCCESS)
      {
        data.vec_printf(TEXT_SUCCESS);
      }
      else if (ret != GEARMAND_NO_JOBS)
      {
        data.vec_printf(TEXT_ERROR_INTERNAL_ERROR);
      }
      else
      {
        data.vec_printf(TEXT_ERROR_UNKNOWN_JOB);
      }
    }
  }
  else if (packet->argc >= 2 and strcasecmp("show", (char *)(packet->arg[0])) == 0)
  {
    if (packet->argc == 3
        and strcasecmp("unique", (char *)(packet->arg[1])) == 0
        and strcasecmp("jobs", (char *)(packet->arg[2])) == 0)
    {
      for (size_t x= 0; x < Server->hashtable_buckets; x++)
      {
        for (gearman_server_job_st* server_job= Server->unique_hash[x];
             server_job != NULL;
             server_job= server_job->unique_next)
        {
          data.vec_append_printf("%.*s\n", int(server_job->unique_length), server_job->unique);
        }
      }

      data.vec_append_printf(".\n");
    }
    else if (packet->argc == 2
             and strcasecmp("jobs", (char *)(packet->arg[1])) == 0)
    {
      for (size_t x= 0; x < Server->hashtable_buckets; ++x)
      {
        for (gearman_server_job_st *server_job= Server->job_hash[x];
             server_job != NULL;
             server_job= server_job->next)
        {
          data.vec_append_printf("%s\t%u\t%u\t%u\n", server_job->job_handle, uint32_t(server_job->retries),
                                 uint32_t(server_job->ignore_job), uint32_t(server_job->job_queued));
        }
      }

      data.vec_append_printf(".\n");
    }
    else
    {
      data.vec_printf(TEXT_ERROR_UNKNOWN_SHOW_ARGUMENTS);
    }
  }
  else if (strcasecmp("create", (char *)(packet->arg[0])) == 0)
  {
    if (packet->argc == 3 and strcasecmp("function", (char *)(packet->arg[1])) == 0)
    {
      gearman_server_function_st* function= gearman_server_function_get(Server, (char *)(packet->arg[2]), packet->arg_size[2] -2);

      if (function)
      {
        data.vec_printf(TEXT_SUCCESS);
      }
      else
      {
        data.vec_printf(TEXT_ERROR_CREATE_FUNCTION,
                        (int)packet->arg_size[2], (char *)(packet->arg[2]));
      }
    }
    else
    {
      // create
      data.vec_printf(TEXT_ERROR_ARGS, (int)packet->arg_size[0], (char *)(packet->arg[0]));
    }
  }
  else if (strcasecmp("drop", (char *)(packet->arg[0])) == 0)
  {
    if (packet->argc == 3 and strcasecmp("function", (char *)(packet->arg[1])) == 0)
    {
      bool success= false;
      for (uint32_t function_key= 0; function_key < GEARMAND_DEFAULT_HASH_SIZE;
           function_key++)
      {
        for (gearman_server_function_st *function= Server->function_hash[function_key];
             function != NULL;
             function= function->next)
        {
          if (strcasecmp(function->function_name, (char *)(packet->arg[2])) == 0)
          {
            success= true;
            if (function->worker_count == 0 && function->job_running == 0)
            {
              gearman_server_function_free(Server, function);
              data.vec_append_printf(TEXT_SUCCESS);
            }
            else
            {
              data.vec_append_printf("ERR there are still connected workers or executing clients\r\n");
            }
            break;
          }
        }
      }

      if (success == false)
      {
        data.vec_printf("ERR function not found\r\n");
        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "%s", data.value());
      }
    }
    else
    {
      // drop
      data.vec_printf(TEXT_ERROR_ARGS, (int)packet->arg_size[0], (char *)(packet->arg[0]));
    }
  }
  else if (strcasecmp("maxqueue", (char *)(packet->arg[0])) == 0)
  {
    if (packet->argc == 1)
    {
      data.vec_append_printf(TEXT_ERROR_ARGS, (int)packet->arg_size[0], (char *)(packet->arg[0]));
    }
    else
    {
      uint32_t max_queue_size[GEARMAN_JOB_PRIORITY_MAX];

      for (int priority= 0; priority < GEARMAN_JOB_PRIORITY_MAX; ++priority)
      {
        const int argc= priority +2;
        if (packet->argc > argc)
        {
          const int parameter= atoi((char *)(packet->arg[argc]));
          if (parameter < 0)
          {
            max_queue_size[priority]= 0;
          }
          else
          {
            max_queue_size[priority]= (uint32_t)parameter;
          }
        }
        else
        {
          max_queue_size[priority]= GEARMAND_DEFAULT_MAX_QUEUE_SIZE;
        }
      }

      /* 
        To preserve the existing behavior of maxqueue, ensure that the
         one-parameter invocation is applied to all priorities.
      */
      if (packet->argc <= 3)
      {
        for (int priority= 1; priority < GEARMAN_JOB_PRIORITY_MAX; ++priority)
        {
          max_queue_size[priority]= max_queue_size[0];
        }
      }
       
      for (uint32_t function_key= 0; function_key < GEARMAND_DEFAULT_HASH_SIZE;
           function_key++)
      {
        for (gearman_server_function_st *function= Server->function_hash[function_key];
             function != NULL;
             function= function->next)
        {
          if (strlen((char *)(packet->arg[1])) == function->function_name_size &&
              (memcmp(packet->arg[1], function->function_name, function->function_name_size) == 0))
          {
            gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Applying queue limits to %s", function->function_name);
            memcpy(function->max_queue_size, max_queue_size, sizeof(uint32_t) * GEARMAN_JOB_PRIORITY_MAX);
          }
        }
      }

      data.vec_append_printf(TEXT_SUCCESS);
    }
  }
  else if (strcasecmp("getpid", (char *)(packet->arg[0])) == 0)
  {
    data.vec_printf("OK %d\n", (int)getpid());
  }
  else if (strcasecmp("verbose", (char *)(packet->arg[0])) == 0)
  {
    data.vec_printf("OK %s\n", gearmand_verbose_name(Gearmand()->verbose));
  }
  else if (strcasecmp("version", (char *)(packet->arg[0])) == 0)
  {
    data.vec_printf("OK %s\n", PACKAGE_VERSION);
  }
  else
  {
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Failed to find command %.*s(%" PRIu64 ")",
                       packet->arg_size[0], packet->arg[0], 
                       packet->arg_size[0]);
    data.vec_printf(TEXT_ERROR_UNKNOWN_COMMAND, (int)packet->arg_size[0], (char *)(packet->arg[0]));
  }

  gearman_server_packet_st *server_packet= gearman_server_packet_create(server_con->thread, false);
  if (server_packet == NULL)
  {
    return gearmand_gerror("calling gearman_server_packet_create()", GEARMAND_MEMORY_ALLOCATION_FAILURE);
  }

  server_packet->packet.reset(GEARMAN_MAGIC_TEXT, GEARMAN_COMMAND_TEXT);

  server_packet->packet.options.complete= true;
  server_packet->packet.options.free_data= true;

  if (data.size() == 0)
  {
    data.vec_append_printf(TEXT_ERROR_INTERNAL_ERROR);
  }

  gearman_string_t taken= data.take();
  server_packet->packet.data= gearman_c_str(taken);
  server_packet->packet.data_size= gearman_size(taken);

  int error;
  if ((error= pthread_mutex_lock(&server_con->thread->lock)) == 0)
  {
    GEARMAND_FIFO__ADD(server_con->io_packet, server_packet);
    if ((error= pthread_mutex_unlock(&(server_con->thread->lock))))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_unlock");
    }
  }
  else
  {
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_lock");
  }

  gearman_server_con_io_add(server_con);

  return GEARMAND_SUCCESS;
}
