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



/**
 * @file
 * @brief Server job definitions
 */

#include "gear_config.h"
#include "libgearman-server/common.h"
#include <string.h>

#include <libgearman-server/queue.h>

/*
 * Private declarations
 */

/**
 * @addtogroup gearman_server_job_private Private Server Job Functions
 * @ingroup gearman_server_job
 * @{
 */

/**
 * Get a server job structure from the unique ID. If data_size is non-zero,
 * then unique points to the workload data and not a real unique key.
 */
static gearman_server_job_st * _server_job_get_unique(gearman_server_st *server, uint32_t unique_key,
                                                      gearman_server_function_st *server_function,
                                                      const char *unique, size_t data_size)
{
  gearman_server_job_st *server_job;

  for (server_job= server->unique_hash[unique_key % server->hashtable_buckets];
       server_job != NULL; server_job= server_job->unique_next)
  {
    if (data_size == 0)
    {
      if (server_job->function == server_function &&
          server_job->unique_key == unique_key &&
          !strcmp(server_job->unique, unique))
      {
        return server_job;
      }
    }
    else
    {
      if (server_job->function == server_function &&
          server_job->unique_key == unique_key &&
          server_job->data_size == data_size &&
          memcmp(server_job->data, unique, data_size) == 0)
      {
        return server_job;
      }
    }
  }

  return NULL;
}

/** @} */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

/*
 * Public definitions
 */
gearman_server_job_st * gearman_server_job_add(gearman_server_st *server,
                                               const char *function_name, size_t function_name_size,
                                               const char *unique, size_t unique_size,
                                               const void *data, size_t data_size,
                                               gearman_job_priority_t priority,
                                               gearman_server_client_st *server_client,
                                               gearmand_error_t *ret_ptr,
                                               int64_t when)
{
  return gearman_server_job_add_reducer(server,
                                        function_name, function_name_size,
                                        unique, unique_size, 
                                        NULL, 0, // reducer 
                                        data, data_size,
                                        priority, server_client, ret_ptr, when);
}

gearman_server_job_st *
gearman_server_job_add_reducer(gearman_server_st *server,
                               const char *function_name, size_t function_name_size,
                               const char *unique, size_t unique_size,
                               const char *reducer_name, size_t reducer_size,
                               const void *data, size_t data_size,
                               gearman_job_priority_t priority,
                               gearman_server_client_st *server_client,
                               gearmand_error_t *ret_ptr,
                               int64_t when)
{
  gearman_server_function_st *server_function= gearman_server_function_get(server, function_name, function_name_size);
  if (server_function == NULL)
  {
    *ret_ptr= GEARMAND_MEMORY_ALLOCATION_FAILURE;
    return NULL;
  }

  uint32_t key;
  gearman_server_job_st *server_job;
  if (unique_size == 0)
  {
    server_job= NULL;
    key= 0;
  }
  else
  {
    if (unique_size == 1 && *unique ==  '-')
    {
      if (data_size == 0)
      {
        key= 0;
        server_job= NULL;
      }
      else
      {
        /* Look up job via unique data when unique = '-'. */
        key= _server_job_hash((const char*)data, data_size);
        server_job= _server_job_get_unique(server, key, server_function, (const char*)data, data_size);
      }
    }
    else
    {
      /* Look up job via unique ID first to make sure it's not a duplicate. */
      key= _server_job_hash(unique, unique_size);
      server_job= _server_job_get_unique(server, key, server_function, unique, 0);
    }
  }

  if (server_job == NULL)
  {
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Comparing queue %u to limit %u for priority %u",
                       server_function->job_total, server_function->max_queue_size[priority],
                       priority);
    if (server_function->max_queue_size[priority] > 0 &&
        server_function->job_total >= server_function->max_queue_size[priority])
    {
      *ret_ptr= GEARMAND_JOB_QUEUE_FULL;
      return NULL;
    }

    server_job= gearman_server_job_create(server);
    if (server_job == NULL)
    {
      *ret_ptr= GEARMAND_MEMORY_ALLOCATION_FAILURE;
      return NULL;
    }

    server_job->priority= priority;

    server_job->function= server_function;
    server_function->job_total++;

    int checked_length;
    checked_length= snprintf(server_job->job_handle, GEARMAND_JOB_HANDLE_SIZE, "%s:%u",
                             server->job_handle_prefix, server->job_handle_count);

    if (checked_length >= GEARMAND_JOB_HANDLE_SIZE || checked_length < 0)
    {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "Job handle plus handle count beyond GEARMAND_JOB_HANDLE_SIZE: %s:%u",
                         server->job_handle_prefix, server->job_handle_count);
    }

    server_job->unique_length= unique_size;
    checked_length= snprintf(server_job->unique, GEARMAN_MAX_UNIQUE_SIZE, "%.*s",
                             (int)unique_size, unique);
    if (checked_length >= GEARMAN_MAX_UNIQUE_SIZE || checked_length < 0)
    {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "We received a unique beyond GEARMAN_MAX_UNIQUE_SIZE: %.*s", (int)unique_size, unique);
    }

    server->job_handle_count++;
    server_job->data= data;
    server_job->data_size= data_size;
    server_job->when= when; 

    if (reducer_size)
    {
      strncpy(server_job->reducer, reducer_name, reducer_size);
      server_job->reducer[reducer_size]= 0;
    }
    else
    {
      server_job->reducer[0]= 0;
    }

    server_job->unique_key= key;
    key= key % server->hashtable_buckets;
    GEARMAND_HASH_ADD(server->unique, key, server_job, unique_);

    key= _server_job_hash(server_job->job_handle,
                          strlen(server_job->job_handle));
    server_job->job_handle_key= key;
    key= key % server->hashtable_buckets;
    GEARMAND_HASH__ADD(server->job, key, server_job);

    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "JOB %s :%u",
                       server_job->job_handle, server_job->job_handle_key);

    if (server->state.queue_startup)
    {
      server_job->job_queued= true;
    }
    else if (server_client == NULL)
    {
      *ret_ptr= gearman_queue_add(server,
                                  server_job->unique, unique_size,
                                  function_name,
                                  function_name_size,
                                  data, data_size, priority, 
                                  when);
      if (gearmand_failed(*ret_ptr))
      {
        server_job->data= NULL;
        gearman_server_job_free(server_job);
        return NULL;
      }

      server_job->job_queued= true;
    }

    *ret_ptr= gearman_server_job_queue(server_job);
    if (gearmand_failed(*ret_ptr))
    {
      if (server_client == NULL)
      {
        /* Do our best to remove the job from the queue. */
        (void)gearman_queue_done(server,
                                 server_job->unique, unique_size,
                                 server_job->function->function_name,
                                 server_job->function->function_name_size);
      }

      gearman_server_job_free(server_job);
      return NULL;
    }
  }
  else
  {
    *ret_ptr= GEARMAND_JOB_EXISTS;
  }

  if (server_client)
  {
    server_client->job= server_job;
    GEARMAND_LIST_ADD(server_job->client, server_client, job_);
  }

  return server_job;
}

void gearman_server_job_free(gearman_server_job_st *server_job)
{
  if (server_job)
  {
    if (server_job->worker != NULL)
    {
      server_job->function->job_running--;
    }

    server_job->function->job_total--;

    if (server_job->data != NULL)
    {
      free((void *)(server_job->data));
      server_job->data= NULL;
    }

    while (server_job->client_list != NULL)
    {
      gearman_server_client_free(server_job->client_list);
    }

    if (server_job->worker != NULL)
    {
      GEARMAND_LIST_DEL(server_job->worker->job, server_job, worker_);
    }

    uint32_t key= server_job->unique_key % Server->hashtable_buckets;
    GEARMAND_HASH_DEL(Server->unique, key, server_job, unique_);

    key= server_job->job_handle_key % Server->hashtable_buckets;
    GEARMAND_HASH__DEL(Server->job, key, server_job);

    if (Server->free_job_count < GEARMAND_MAX_FREE_SERVER_JOB)
    {
      gearman_server_st *server= Server;
      GEARMAND_LIST__ADD(server->free_job, server_job);
    }
    else
    {
      destroy_gearman_server_job_st(server_job);
    }
  }
}

gearmand_error_t gearman_server_job_queue(gearman_server_job_st *job)
{
  if (job->worker)
  {
    job->retries++;
    if (Server->job_retries != 0 && Server->job_retries == job->retries)
    {
      gearmand_log_notice(GEARMAN_DEFAULT_LOG_PARAM,
                          "Dropped job due to max retry count: %s %.*s",
                          job->job_handle,
                          (int)job->unique_length, job->unique);

      for (gearman_server_client_st* client= job->client_list; client != NULL; client= client->job_next)
      {
        gearmand_error_t ret= gearman_server_io_packet_add(client->con, false,
                                                           GEARMAN_MAGIC_RESPONSE,
                                                           GEARMAN_COMMAND_WORK_FAIL,
                                                           job->job_handle,
                                                           (size_t)strlen(job->job_handle),
                                                           NULL);
        if (gearmand_failed(ret))
        {
          gearmand_log_gerror_warn(GEARMAN_DEFAULT_LOG_PARAM, ret, "Failed to send WORK_FAIL packet to %s:%s", client->con->host(), client->con->port());
        }
      }

      /* Remove from persistent queue if one exists. */
      if (job->job_queued)
      {
        gearmand_error_t ret= gearman_queue_done(Server,
                                                 job->unique, job->unique_length,
                                                 job->function->function_name,
                                                 job->function->function_name_size);
        if (gearmand_failed(ret))
        {
          gearmand_log_gerror_warn(GEARMAN_DEFAULT_LOG_PARAM, ret, "Failed to removed %.*s from persistent queue", int(job->unique_length), job->unique);
        }
      }

      gearman_server_job_free(job);
      return GEARMAND_SUCCESS;
    }

    GEARMAND_LIST_DEL(job->worker->job, job, worker_);
    job->worker= NULL;
    job->function->job_running--;
    job->function_next= NULL;
    job->numerator= 0;
    job->denominator= 0;
  }

  /* Queue NOOP for possible sleeping workers. */
  if (job->function->worker_list != NULL)
  {
    gearman_server_worker_st *worker= job->function->worker_list;
    uint32_t noop_sent= 0;

    do
    {
      if (worker->con->is_sleeping && ! (worker->con->is_noop_sent))
      {
        gearmand_error_t ret= gearman_server_io_packet_add(worker->con, false,
                                                           GEARMAN_MAGIC_RESPONSE,
                                                           GEARMAN_COMMAND_NOOP, NULL);
        if (gearmand_failed(ret))
        {
          gearmand_log_gerror_warn(GEARMAN_DEFAULT_LOG_PARAM, ret, "Failed to send NOOP packet to %s:%s", worker->con->host(), worker->con->port());
        }
        else
        {
          worker->con->is_noop_sent= true;
          noop_sent++;
        }
      }

      worker= worker->function_next;
    }
    while (worker != job->function->worker_list &&
           (Server->worker_wakeup == 0 ||
            noop_sent < Server->worker_wakeup));

    job->function->worker_list= worker;
  }

  /* Queue the job to be run. */
  if (job->function->job_list[job->priority] == NULL)
  {
    job->function->job_list[job->priority]= job;
  }
  else
  {
    job->function->job_end[job->priority]->function_next= job;
  }

  job->function->job_end[job->priority]= job;
  job->function->job_count++;

  return GEARMAND_SUCCESS;
}
#pragma GCC diagnostic pop
