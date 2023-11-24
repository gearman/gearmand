/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2013 Data Differential, http://datadifferential.com/
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
 * @brief Server Definitions
 */

#include "gear_config.h"

#include "libgearman-server/common.h"
#include "libgearman-server/queue.h"
#include "libgearman-server/plugins/base.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "libgearman-1.0/return.h"
#include "libgearman-1.0/strerror.h"
#include "libgearman/magic.h"

/*
 * Private declarations
 */


/**
 * @addtogroup gearman_server_private Private Server Functions
 * @ingroup gearman_server
 * @{
 */

/**
 * Queue an error packet.
 */
static gearmand_error_t _server_error_packet(const char *position_, const char *func_,
                                             gearman_server_con_st *server_con,
                                             gearman_return_t client_return_code, const char *error_string, const size_t error_string_length)
{
  const char* error_code_string= gearman_strerror(client_return_code);
  error_code_string+= 8;

  gearmand_log_warning(position_, func_, "%s:%.*s", gearman_strerror(client_return_code), int(error_string_length), error_string);

  return gearman_server_io_packet_add(server_con, false, GEARMAN_MAGIC_RESPONSE,
                                      GEARMAN_COMMAND_ERROR, error_code_string,
                                      (size_t)(strlen(error_code_string) + 1),
                                      error_string, error_string_length,
                                      NULL);
}

/**
 * Send work result packets with data back to clients.
 */
static gearmand_error_t
_server_queue_work_data(gearman_server_job_st *server_job,
                        gearmand_packet_st *packet, gearman_command_t command);

/** @} */

/*
 * Public definitions
 */

#if __GNUC__ >= 7
  #pragma GCC diagnostic warning "-Wimplicit-fallthrough"
#endif
gearmand_error_t gearman_server_run_command(gearman_server_con_st *server_con,
                                            gearmand_packet_st *packet)
{
  gearmand_error_t ret;
  gearman_server_client_st *server_client= NULL;

  if (packet->magic == GEARMAN_MAGIC_RESPONSE)
  {
    return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_INVALID_MAGIC, gearman_literal_param("Request magic expected"));
  }

  if (uint32_t(packet->command) >= uint32_t(GEARMAN_COMMAND_MAX))
  {
    return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_INVALID_COMMAND, gearman_literal_param("Invalid command expected"));
  }

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                     "PACKET COMMAND: %s", gearmand_strcommand(packet));

  switch (packet->command)
  {
  /* Client/worker requests. */
  case GEARMAN_COMMAND_ECHO_REQ:
    /* Reuse the data buffer and just shove the data back. */
    ret= gearman_server_io_packet_add(server_con, true, GEARMAN_MAGIC_RESPONSE,
                                      GEARMAN_COMMAND_ECHO_RES, packet->data,
                                      packet->data_size, NULL);
    if (gearmand_failed(ret))
    {
      return gearmand_gerror("gearman_server_io_packet_add", ret);
    }

    packet->options.free_data= false;
    break;

  case GEARMAN_COMMAND_SUBMIT_REDUCE_JOB: // Reduce request
    server_client= gearman_server_client_add(server_con);
    if (server_client == NULL)
    {
      return GEARMAND_MEMORY_ALLOCATION_FAILURE;
    }
    /* fall-thru */

  case GEARMAN_COMMAND_SUBMIT_REDUCE_JOB_BACKGROUND:
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "Received reduce submission, Partitioner: %.*s(%lu) Reducer: %.*s(%lu) Unique: %.*s(%lu) with %d arguments",
                         (uint32_t)packet->arg_size[0] -1, packet->arg[0], packet->arg_size[0] -1,
                         (uint32_t)packet->arg_size[2] -1, packet->arg[2], packet->arg_size[2] -1, // reducer
                         (uint32_t)packet->arg_size[1] -1, packet->arg[1], packet->arg_size[1] -1,
                         (int)packet->argc);
      if (packet->arg_size[2] -1 > GEARMAN_UNIQUE_SIZE)
      {
        gearman_server_client_free(server_client);
        gearmand_gerror("unique value too large", GEARMAND_ARGUMENT_TOO_LARGE);
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_ARGUMENT_TOO_LARGE, gearman_literal_param("Unique value too large"));
      }

      gearman_job_priority_t map_priority= GEARMAN_JOB_PRIORITY_NORMAL;

      /* Schedule job. */
      gearman_server_job_st *server_job= gearman_server_job_add_reducer(Server,
                                                                        (char *)(packet->arg[0]), packet->arg_size[0] -1, // Function
                                                                        (char *)(packet->arg[1]), packet->arg_size[1] -1, // unique
                                                                        (char *)(packet->arg[2]), packet->arg_size[2] -1, // reducer
                                                                        packet->data, packet->data_size, map_priority,
                                                                        server_client, &ret, 0);

      if (gearmand_success(ret))
      {
        packet->options.free_data= false;
      }
      else if (ret == GEARMAND_JOB_QUEUE_FULL)
      {
        gearman_server_client_free(server_client);
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_QUEUE_ERROR, gearman_literal_param("Job queue is full"));
      }
      else if (ret != GEARMAND_JOB_EXISTS)
      {
        gearman_server_client_free(server_client);
        gearmand_gerror("gearman_server_job_add", ret);
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_QUEUE_ERROR, gearmand_strerror(ret), strlen(gearmand_strerror(ret)));
      }

      /* Queue the job created packet. */
      ret= gearman_server_io_packet_add(server_con, false, GEARMAN_MAGIC_RESPONSE,
                                        GEARMAN_COMMAND_JOB_CREATED,
                                        server_job->job_handle,
                                        (size_t)strlen(server_job->job_handle),
                                        NULL);
      if (gearmand_failed(ret))
      {
        gearman_server_client_free(server_client);
        return gearmand_gerror("gearman_server_io_packet_add", ret);
      }

      gearmand_log_notice(GEARMAN_DEFAULT_LOG_PARAM,"accepted,%.*s,%.*s,%.*s",
                          (uint32_t)packet->arg_size[0] -1, packet->arg[0], // Function
                          (uint32_t)packet->arg_size[1] -1, packet->arg[1], // unique
                          (uint32_t)packet->arg_size[2] -1, packet->arg[2]); // reducer
    }
    break;

  /* Client requests. */
  case GEARMAN_COMMAND_SUBMIT_JOB:
  case GEARMAN_COMMAND_SUBMIT_JOB_BG:
  case GEARMAN_COMMAND_SUBMIT_JOB_HIGH:
  case GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG:
  case GEARMAN_COMMAND_SUBMIT_JOB_LOW:
  case GEARMAN_COMMAND_SUBMIT_JOB_LOW_BG:
  case GEARMAN_COMMAND_SUBMIT_JOB_EPOCH:
    {
      gearman_job_priority_t priority;

      if (packet->command == GEARMAN_COMMAND_SUBMIT_JOB or
          packet->command == GEARMAN_COMMAND_SUBMIT_JOB_BG or
          packet->command == GEARMAN_COMMAND_SUBMIT_JOB_EPOCH)
      {
        priority= GEARMAN_JOB_PRIORITY_NORMAL;
      }
      else if (packet->command == GEARMAN_COMMAND_SUBMIT_JOB_HIGH or
               packet->command == GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG)
      {
        priority= GEARMAN_JOB_PRIORITY_HIGH;
      }
      else
      {
        priority= GEARMAN_JOB_PRIORITY_LOW;
      }

      if (packet->command == GEARMAN_COMMAND_SUBMIT_JOB_BG or
          packet->command == GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG or
          packet->command == GEARMAN_COMMAND_SUBMIT_JOB_LOW_BG or
          packet->command == GEARMAN_COMMAND_SUBMIT_JOB_EPOCH)
      {
        server_client= NULL;
      }
      else
      {
        server_client= gearman_server_client_add(server_con);
        if (server_client == NULL)
        {
          return GEARMAND_MEMORY_ALLOCATION_FAILURE;
        }
      }

      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "Received submission, function:%.*s unique:%.*s with %d arguments",
                         (uint32_t)packet->arg_size[0], packet->arg[0],
                         (uint32_t)packet->arg_size[1], packet->arg[1],
                         (int)packet->argc);
      int64_t when= 0;
      if (packet->command == GEARMAN_COMMAND_SUBMIT_JOB_EPOCH)
      {
        char *endptr;
        // @note strtoll will set errno if error, but it might also leave errno
        // alone if none happens (so a previous call that sets it might cause
        // an error.
        errno= 0;
        when= strtoll((char *)packet->arg[2], &endptr, 10);
        if (errno)
        {
          return gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "%s parsing epoch with strtoll(%ld)", strerror(errno), when);
        }
        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, 
                           "Received EPOCH job submission, function:%.*s unique:%.*s with data for %jd at %jd, args %d",
                           (uint32_t)packet->arg_size[0], packet->arg[0],
                           (uint32_t)packet->arg_size[1], packet->arg[1],
                           when, time(NULL),
                           (int)packet->argc);
      }

      if (packet->arg_size[1] -1 > GEARMAN_UNIQUE_SIZE)
      {
        gearmand_gerror("unique value too large", GEARMAND_ARGUMENT_TOO_LARGE);
        gearman_server_client_free(server_client);
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_ARGUMENT_TOO_LARGE, gearman_literal_param("Unique value too large"));
      }

      /* Schedule job. */
      gearman_server_job_st *server_job= gearman_server_job_add(Server,
                                                                (char *)(packet->arg[0]), packet->arg_size[0] -1, // Function
                                                                (char *)(packet->arg[1]), packet->arg_size[1] -1, // unique
                                                                packet->data, packet->data_size, priority,
                                                                server_client, &ret,
                                                                when);

      if (gearmand_success(ret))
      {
        packet->options.free_data= false;
      }
      else if (ret == GEARMAND_JOB_QUEUE_FULL)
      {
        gearman_server_client_free(server_client);
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_QUEUE_ERROR, gearman_literal_param("Job queue is full"));
      }
      else if (ret != GEARMAND_JOB_EXISTS)
      {
        gearman_server_client_free(server_client);
        gearmand_gerror("gearman_server_job_add", ret);
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_QUEUE_ERROR, gearmand_strerror(ret), strlen(gearmand_strerror(ret)));
      }

      /* Queue the job created packet. */
      ret= gearman_server_io_packet_add(server_con, false, GEARMAN_MAGIC_RESPONSE,
                                        GEARMAN_COMMAND_JOB_CREATED,
                                        server_job->job_handle,
                                        (size_t)strlen(server_job->job_handle),
                                        NULL);
      if (gearmand_failed(ret))
      {
        gearman_server_client_free(server_client);
        return gearmand_gerror("gearman_server_io_packet_add", ret);
      }

      gearmand_log_notice(GEARMAN_DEFAULT_LOG_PARAM,"accepted,%.*s,%.*s,%jd",
                          (uint32_t)packet->arg_size[0], packet->arg[0], // Function
                          (uint32_t)packet->arg_size[1], packet->arg[1], // Unique
                          when);
    }
    break;

  case GEARMAN_COMMAND_GET_STATUS_UNIQUE:
    {
      char unique_handle[GEARMAN_MAX_UNIQUE_SIZE];

      /* This may not be NULL terminated, so copy to make sure it is. */
      int unique_handle_length= snprintf(unique_handle, GEARMAN_MAX_UNIQUE_SIZE, "%.*s",
                                      (int)(packet->arg_size[0]), (char *)(packet->arg[0]));

      if (unique_handle_length >= GEARMAN_MAX_UNIQUE_SIZE || unique_handle_length < 0)
      {
        gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "snprintf(%d)", unique_handle_length);
        return GEARMAND_MEMORY_ALLOCATION_FAILURE;
      }

      gearman_server_job_st *server_job= gearman_server_job_get_by_unique(Server,
                                                                          unique_handle, (size_t)unique_handle_length,
                                                                          NULL);

      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Searching for unique job: \"%s\" found: %s clients:%d", unique_handle,
                         server_job ? "yes" : "no",
                         server_job ? server_job->client_count : 0);
      /* Queue status result packet. */
      if (server_job == NULL)
      {
        ret= gearman_server_io_packet_add(server_con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_STATUS_RES_UNIQUE,
                                          unique_handle, (size_t)(unique_handle_length +1), // Job Handle
                                          "0", (size_t)2, //
                                          "0", (size_t)2, //
                                          "0", (size_t)2, //
                                          "0", (size_t)2, //
                                          "0", (size_t)1, // client_count
                                          NULL);
      }
      else
      {
        char numerator_buffer[11]; /* Max string size to hold a uint32_t. */
        int numerator_buffer_length= snprintf(numerator_buffer, sizeof(numerator_buffer), "%u", server_job->numerator);
        if ((size_t)numerator_buffer_length >= sizeof(numerator_buffer) || numerator_buffer_length < 0)
        {
          gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "snprintf(%d)", numerator_buffer_length);
          return GEARMAND_MEMORY_ALLOCATION_FAILURE;
        }

        char denominator_buffer[11]; /* Max string size to hold a uint32_t. */
        int denominator_buffer_length= snprintf(denominator_buffer, sizeof(denominator_buffer), "%u", server_job->denominator);
        if ((size_t)denominator_buffer_length >= sizeof(denominator_buffer) || denominator_buffer_length < 0)
        {
          gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "snprintf(%d)", denominator_buffer_length);
          return GEARMAND_MEMORY_ALLOCATION_FAILURE;
        }

        char client_count_buffer[11]; /* Max string size to hold a uint32_t. */
        int client_count_buffer_length= snprintf(client_count_buffer, sizeof(client_count_buffer), "%u", server_job->client_count);
        if ((size_t)client_count_buffer_length >= sizeof(client_count_buffer) || client_count_buffer_length < 0)
        {
          gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "snprintf(%d)", client_count_buffer_length);
          return GEARMAND_MEMORY_ALLOCATION_FAILURE;
        }

        ret= gearman_server_io_packet_add(server_con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_STATUS_RES_UNIQUE,
                                          unique_handle, (size_t)(unique_handle_length +1), // unique_handle
                                          "1", (size_t)2, // is_known
                                          server_job->worker == NULL ? "0" : "1", (size_t)2, // is_running
                                          numerator_buffer, (size_t)(numerator_buffer_length +1), // numerator
                                          denominator_buffer, (size_t)(denominator_buffer_length +1), //denominator
                                          client_count_buffer, (size_t)(client_count_buffer_length), //client_count
                                          NULL);
      }

      if (gearmand_failed(ret))
      {
        return gearmand_gerror("gearman_server_io_packet_add", ret);
      }
    }
    break;

  case GEARMAN_COMMAND_GET_STATUS:
    {
      char job_handle[GEARMAND_JOB_HANDLE_SIZE];

      /* This may not be NULL terminated, so copy to make sure it is. */
      int job_handle_length= snprintf(job_handle, GEARMAND_JOB_HANDLE_SIZE, "%.*s",
                                      (int)(packet->arg_size[0]), (char *)(packet->arg[0]));

      if (job_handle_length >= GEARMAND_JOB_HANDLE_SIZE || job_handle_length < 0)
      {
        gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "snprintf(%d)", job_handle_length);
        return GEARMAND_MEMORY_ALLOCATION_FAILURE;
      }

      gearman_server_job_st *server_job= gearman_server_job_get(Server,
                                                                job_handle, (size_t)job_handle_length,
                                                                NULL);

      /* Queue status result packet. */
      if (server_job == NULL)
      {
        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,"status,%.*s,unknown,unknown,unknown,unknown",
                           int(job_handle_length), job_handle);

        ret= gearman_server_io_packet_add(server_con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_STATUS_RES,
                                          job_handle, (size_t)(job_handle_length +1), // Job Handle
                                          "0", (size_t)2, //
                                          "0", (size_t)2, //
                                          "0", (size_t)2, //
                                          "0", (size_t)1, //
                                          NULL);
      }
      else
      {
        char numerator_buffer[11]; /* Max string size to hold a uint32_t. */
        int numerator_buffer_length= snprintf(numerator_buffer, sizeof(numerator_buffer), "%u", server_job->numerator);
        if ((size_t)numerator_buffer_length >= sizeof(numerator_buffer) || numerator_buffer_length < 0)
        {
          gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "snprintf(%d)", numerator_buffer_length);
          return GEARMAND_MEMORY_ALLOCATION_FAILURE;
        }

        char denominator_buffer[11]; /* Max string size to hold a uint32_t. */
        int denominator_buffer_length= snprintf(denominator_buffer, sizeof(denominator_buffer), "%u", server_job->denominator);
        if ((size_t)denominator_buffer_length >= sizeof(denominator_buffer) || denominator_buffer_length < 0)
        {
          gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "snprintf(%d)", denominator_buffer_length);
          return GEARMAND_MEMORY_ALLOCATION_FAILURE;
        }

        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,"status,%.*s,known,%s,%.*s,%.*s",
                           int(job_handle_length), job_handle,
                           server_job->worker == NULL ? "quiet" : "running",
                           int(numerator_buffer_length), numerator_buffer,
                           int(denominator_buffer_length), denominator_buffer);


        ret= gearman_server_io_packet_add(server_con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_STATUS_RES,
                                          job_handle, (size_t)(job_handle_length +1),
                                          "1", (size_t)2,
                                          server_job->worker == NULL ? "0" : "1", (size_t)2,
                                          numerator_buffer, (size_t)(numerator_buffer_length +1),
                                          denominator_buffer, (size_t)(denominator_buffer_length),
                                          NULL);
      }

      if (gearmand_failed(ret))
      {
        return gearmand_gerror("gearman_server_io_packet_add", ret);
      }
    }

    break;

  case GEARMAN_COMMAND_OPTION_REQ:
    {
      char option[GEARMAND_OPTION_SIZE];

      /* This may not be NULL terminated, so copy to make sure it is. */
      int option_length= snprintf(option, sizeof(option), "%.*s",
                               (int)(packet->arg_size[0]), (char *)(packet->arg[0]));

      if (option_length >= GEARMAND_OPTION_SIZE || option_length < 0)
      {
        gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "snprintf(%d)", option_length);
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_UNKNOWN_OPTION,
                                    gearman_literal_param("Server does not recognize given option"));
      }

      if (strcasecmp(option, "exceptions") == 0)
      {
        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "'exceptions'");
        server_con->is_exceptions= true;
      }
      else
      {
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_UNKNOWN_OPTION,
                                    gearman_literal_param("Server does not recognize given option"));
      }

      ret= gearman_server_io_packet_add(server_con, false, GEARMAN_MAGIC_RESPONSE,
                                        GEARMAN_COMMAND_OPTION_RES,
                                        packet->arg[0], packet->arg_size[0],
                                        NULL);
      if (gearmand_failed(ret))
      {
        return gearmand_gerror("gearman_server_io_packet_add", ret);
      }
    }

    break;

  /* Worker requests. */
  case GEARMAN_COMMAND_CAN_DO:
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Registering function: %.*s",
                       (uint32_t)packet->arg_size[0], packet->arg[0]);
    if (gearman_server_worker_add(server_con, (char *)(packet->arg[0]),
                                  packet->arg_size[0], 0) == NULL)
    {
      return GEARMAND_MEMORY_ALLOCATION_FAILURE;
    }

    break;

  case GEARMAN_COMMAND_CAN_DO_TIMEOUT:
    {
      if (packet->arg_size[1] > GEARMAN_MAXIMUM_INTEGER_DISPLAY_LENGTH)
      {
        return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_INVALID_PACKET, "GEARMAND_INVALID_PACKET:strtol");
      }

      char strtol_buffer[GEARMAN_MAXIMUM_INTEGER_DISPLAY_LENGTH +1];
      memcpy(strtol_buffer, packet->arg[1], packet->arg_size[1]);
      strtol_buffer[packet->arg_size[1]]= 0;
      char *endptr;
      errno= 0;
      long timeout= strtol(strtol_buffer, &endptr, 10);
      if (timeout == LONG_MIN or timeout == LONG_MAX or errno != 0)
      {
        return gearmand_log_perror(GEARMAN_DEFAULT_LOG_PARAM, errno, "GEARMAN_COMMAND_CAN_DO_TIMEOUT:strtol: %s", strtol_buffer);
      }

      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Registering function: %.*s with timeout %ld",
                         (uint32_t)packet->arg_size[0], packet->arg[0], timeout);

      if (gearman_server_worker_add(server_con, (char *)(packet->arg[0]),
                                    packet->arg_size[0] - 1,
                                    timeout) == NULL)
      {
        return GEARMAND_MEMORY_ALLOCATION_FAILURE;
      }
    }

    break;

  case GEARMAN_COMMAND_CANT_DO:
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Removing function: %.*s", (uint32_t)packet->arg_size[0], packet->arg[0]);
    gearman_server_con_free_worker(server_con, (char *)(packet->arg[0]),
                                   packet->arg_size[0]);
    break;

  case GEARMAN_COMMAND_RESET_ABILITIES:
    gearman_server_con_free_workers(server_con);
    break;

  case GEARMAN_COMMAND_PRE_SLEEP:
    {
      gearman_server_job_st *server_job= gearman_server_job_peek(server_con);
      if (server_job == NULL)
      {
        server_con->is_sleeping= true;
        /* Remove any timeouts while sleeping */
        gearman_server_con_delete_timeout(server_con);
      }
      else
      {
        /* If there are jobs that could be run, queue a NOOP packet to wake the
          worker up. This could be the result of a race codition. */
        ret= gearman_server_io_packet_add(server_con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_NOOP, NULL);
        if (gearmand_failed(ret))
        {
          return gearmand_gerror("gearman_server_io_packet_add", ret);
        }
      }
    }

    break;

  case GEARMAN_COMMAND_GRAB_JOB:
  case GEARMAN_COMMAND_GRAB_JOB_UNIQ:
  case GEARMAN_COMMAND_GRAB_JOB_ALL:
    {
      server_con->is_sleeping= false;
      server_con->is_noop_sent= false;

      gearman_server_job_st *server_job= gearman_server_job_take(server_con);
      if (server_job == NULL)
      {
        /* No jobs found, queue no job packet. */
        ret= gearman_server_io_packet_add(server_con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_NO_JOB, NULL);
      }
      else if (packet->command == GEARMAN_COMMAND_GRAB_JOB_UNIQ)
      {
        /* 
          We found a runnable job, queue job assigned packet and take the job off the queue. 
        */
        ret= gearman_server_io_packet_add(server_con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_JOB_ASSIGN_UNIQ,
                                          server_job->job_handle, (size_t)(strlen(server_job->job_handle) + 1),
                                          server_job->function->function_name, server_job->function->function_name_size + 1,
                                          server_job->unique, (size_t)(server_job->unique_length + 1),
                                          server_job->data, server_job->data_size,
                                          NULL);
      }
      else if (packet->command == GEARMAN_COMMAND_GRAB_JOB_ALL and *server_job->reducer != '\0')
      {
        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                           "Sending reduce submission, Partitioner: %.*s(%lu) Reducer: %s(%lu) Unique: %.*s(%lu) with data sized (%lu)",
                           (uint32_t)server_job->function->function_name_size, server_job->function->function_name, (unsigned long)server_job->function->function_name_size,
                           server_job->reducer, (unsigned long)strlen(server_job->reducer),
                           (uint32_t)server_job->unique_length, server_job->unique, (unsigned long)server_job->unique_length,
                           (unsigned long)server_job->data_size);
        /* 
          We found a runnable job, queue job assigned packet and take the job off the queue. 
        */
        ret= gearman_server_io_packet_add(server_con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_JOB_ASSIGN_ALL,
                                          server_job->job_handle, (size_t)(strlen(server_job->job_handle) + 1),
                                          server_job->function->function_name, server_job->function->function_name_size + 1,
                                          server_job->unique, server_job->unique_length +1,
                                          server_job->reducer, (size_t)(strlen(server_job->reducer) +1),
                                          server_job->data, server_job->data_size,
                                          NULL);
      }
      else if (packet->command == GEARMAN_COMMAND_GRAB_JOB_ALL)
      {
        /* 
          We found a runnable job, queue job assigned packet and take the job off the queue. 
        */
        ret= gearman_server_io_packet_add(server_con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_JOB_ASSIGN_UNIQ,
                                          server_job->job_handle, (size_t)(strlen(server_job->job_handle) +1),
                                          server_job->function->function_name, server_job->function->function_name_size +1,
                                          server_job->unique, server_job->unique_length +1,
                                          server_job->data, server_job->data_size,
                                          NULL);
      }
      else
      {
        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                           "Sending GEARMAN_COMMAND_JOB_ASSIGN Function: %.*s(%lu) with data sized (%lu)" ,
                           (uint32_t)server_job->function->function_name_size, server_job->function->function_name, server_job->function->function_name_size,
                           (unsigned long)server_job->data_size);
        /* Same, but without unique ID. */
        ret= gearman_server_io_packet_add(server_con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_JOB_ASSIGN,
                                          server_job->job_handle, (size_t)(strlen(server_job->job_handle) + 1),
                                          server_job->function->function_name, server_job->function->function_name_size + 1,
                                          server_job->data, server_job->data_size,
                                          NULL);
      }

      if (gearmand_failed(ret))
      {
        gearmand_gerror("gearman_server_io_packet_add", ret);

        if (server_job)
        {
          return gearman_server_job_queue(server_job);
        }

        return ret;
      }

      /* Since job is assigned, we should respect function timeout */
      if (server_job != NULL)
      {
        gearman_server_con_add_job_timeout(server_con, server_job);
      }
    }

    break;

  case GEARMAN_COMMAND_WORK_DATA:
  case GEARMAN_COMMAND_WORK_WARNING:
    {
      gearman_server_job_st *server_job= gearman_server_job_get(Server,
                                                                (char *)(packet->arg[0]), (size_t)strlen(packet->arg[0]),
                                                                server_con);
      if (server_job == NULL)
      {
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_JOB_NOT_FOUND, gearman_literal_param("Job does not exist on server"));
      }

      /* Queue the data/warning packet for all clients. */
      ret= _server_queue_work_data(server_job, packet, packet->command);
      if (gearmand_failed(ret))
      {
        return gearmand_gerror("_server_queue_work_data", ret);
      }
    }

    break;

  case GEARMAN_COMMAND_WORK_STATUS:
    {
      gearman_server_job_st *server_job= gearman_server_job_get(Server,
                                                                (char *)(packet->arg[0]), (size_t)strlen(packet->arg[0]),
                                                                server_con);
      if (server_job == NULL)
      {
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_JOB_NOT_FOUND,
                                    gearman_literal_param("Job given in work result not found"));
      }

      /* Update job status. */
      server_job->numerator= (uint32_t)atoi((char *)(packet->arg[1]));

      /* This may not be NULL terminated, so copy to make sure it is. */
      char denominator_buffer[11]; /* Max string size to hold a uint32_t. */
      int denominator_buffer_length= snprintf(denominator_buffer, sizeof(denominator_buffer), "%.*s",
                                              (int)(packet->arg_size[2]),
                                              (char *)(packet->arg[2]));

      if ((size_t)denominator_buffer_length > sizeof(denominator_buffer) || denominator_buffer_length < 0)
      {
        gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "snprintf(%d)", denominator_buffer_length);
        return GEARMAND_MEMORY_ALLOCATION_FAILURE;
      }


      server_job->denominator= (uint32_t)atoi(denominator_buffer);

      /* Queue the status packet for all clients. */
      for (server_client= server_job->client_list; server_client;
           server_client= server_client->job_next)
      {
        ret= gearman_server_io_packet_add(server_client->con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_WORK_STATUS,
                                          packet->arg[0], packet->arg_size[0],
                                          packet->arg[1], packet->arg_size[1],
                                          packet->arg[2], packet->arg_size[2],
                                          NULL);
        if (gearmand_failed(ret))
        {
          gearmand_log_gerror_warn(GEARMAN_DEFAULT_LOG_PARAM, ret, "Failed to send WORK_STATUS packet to %s:%s", server_client->con->host(), server_client->con->port());
        }
      }
    }

    break;

  case GEARMAN_COMMAND_WORK_COMPLETE:
    {
      gearman_server_job_st *server_job= gearman_server_job_get(Server,
                                                                (char *)(packet->arg[0]), (size_t)strlen(packet->arg[0]),
                                                                server_con);
      if (server_job == NULL)
      {
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_JOB_NOT_FOUND, gearman_literal_param("Job given in work result not found"));
      }

      /* Queue the complete packet for all clients. */
      ret= _server_queue_work_data(server_job, packet,
                                   GEARMAN_COMMAND_WORK_COMPLETE);
      if (gearmand_failed(ret))
      {
        return gearmand_gerror("_server_queue_work_data", ret);
      }

      /* Remove from persistent queue if one exists. */
      if (server_job->job_queued)
      {
        ret= gearman_queue_done(Server,
                                server_job->unique,
                                server_job->unique_length,
                                server_job->function->function_name,
                                server_job->function->function_name_size);
        if (gearmand_failed(ret))
        {
          return gearmand_gerror("Remove from persistent queue", ret);
        }
      }

      /* Job is done, remove it. */
      gearman_server_job_free(server_job);
    }
    break;

  case GEARMAN_COMMAND_WORK_EXCEPTION:
    {
      gearman_server_job_st *server_job= gearman_server_job_get(Server,
                                                                (char *)(packet->arg[0]), (size_t)strlen(packet->arg[0]),
                                                                server_con);
      if (server_job == NULL)
      {
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_JOB_NOT_FOUND, 
                                    gearman_literal_param("An exception was received for a job that does not exist"));
      }

      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "Exception being sent for handle: %s",
                         server_job->job_handle);

      /* Queue the exception packet for all clients. */
      ret= _server_queue_work_data(server_job, packet, GEARMAN_COMMAND_WORK_EXCEPTION);
      if (gearmand_failed(ret))
      {
        return gearmand_gerror("_server_queue_work_data", ret);
      }

      /* Remove from persistent queue if one exists. */
      if (server_job->job_queued)
      {
        ret= gearman_queue_done(Server,
                                server_job->unique,
                                server_job->unique_length,
                                server_job->function->function_name,
                                server_job->function->function_name_size);
        if (gearmand_failed(ret))
        {
          return gearmand_gerror("Remove from persistent queue", ret);
        }
      }

      /* Job is done, remove it. */
      gearman_server_job_free(server_job);
    }

    break;

  case GEARMAN_COMMAND_WORK_FAIL:
    {
      char job_handle[GEARMAND_JOB_HANDLE_SIZE];

      /* This may not be NULL terminated, so copy to make sure it is. */
      int job_handle_length= snprintf(job_handle, GEARMAND_JOB_HANDLE_SIZE, "%.*s",
                                      (int)(packet->arg_size[0]), (char *)(packet->arg[0]));

      if (job_handle_length >= GEARMAND_JOB_HANDLE_SIZE || job_handle_length < 0)
      {
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_ARGUMENT_TOO_LARGE,
                                    gearman_literal_param("Error occurred due to GEARMAND_JOB_HANDLE_SIZE being too small from snprintf"));
      }

      gearman_server_job_st *server_job= gearman_server_job_get(Server, job_handle, (size_t)job_handle_length,
                                                                server_con);
      if (server_job == NULL)
      {
        return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_JOB_NOT_FOUND,
                                    gearman_literal_param("Job given in work result not found"));
      }

      /* Queue the fail packet for all clients. */
      for (server_client= server_job->client_list; server_client;
           server_client= server_client->job_next)
      {
        ret= gearman_server_io_packet_add(server_client->con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_WORK_FAIL,
                                          packet->arg[0], packet->arg_size[0],
                                          NULL);
        if (gearmand_failed(ret))
        {
          gearmand_log_gerror_warn(GEARMAN_DEFAULT_LOG_PARAM, ret, "Failed to send WORK_FAIL packet to %s:%s", server_client->con->host(), server_client->con->port());
        }
      }

      /* Remove from persistent queue if one exists. */
      if (server_job->job_queued)
      {
        ret= gearman_queue_done(Server,
                                server_job->unique,
                                server_job->unique_length,
                                server_job->function->function_name,
                                server_job->function->function_name_size);
        if (gearmand_failed(ret))
        {
          return gearmand_gerror("Remove from persistent queue", ret);
        }
      }

      /* Job is done, remove it. */
      gearman_server_job_free(server_job);
    }

    break;

  case GEARMAN_COMMAND_SET_CLIENT_ID:
    gearman_server_con_set_id(server_con, (char *)(packet->arg[0]),
                              packet->arg_size[0]);
    break;

  case GEARMAN_COMMAND_TEXT:
    return server_run_text(server_con, packet);

  case GEARMAN_COMMAND_UNUSED:
  case GEARMAN_COMMAND_NOOP:
  case GEARMAN_COMMAND_JOB_CREATED:
  case GEARMAN_COMMAND_NO_JOB:
  case GEARMAN_COMMAND_JOB_ASSIGN:
  case GEARMAN_COMMAND_ECHO_RES:
  case GEARMAN_COMMAND_ERROR:
  case GEARMAN_COMMAND_STATUS_RES:
  case GEARMAN_COMMAND_ALL_YOURS:
  case GEARMAN_COMMAND_OPTION_RES:
  case GEARMAN_COMMAND_SUBMIT_JOB_SCHED:
  case GEARMAN_COMMAND_JOB_ASSIGN_UNIQ:
  case GEARMAN_COMMAND_JOB_ASSIGN_ALL:
  case GEARMAN_COMMAND_MAX:
  case GEARMAN_COMMAND_STATUS_RES_UNIQUE:
  default:
    return _server_error_packet(GEARMAN_DEFAULT_LOG_PARAM, server_con, GEARMAN_INVALID_COMMAND, gearman_literal_param("Command not expected"));
  }

  return GEARMAND_SUCCESS;
}

gearmand_error_t gearman_server_shutdown_graceful(gearman_server_st *server)
{
  server->shutdown_graceful= true;

  if (server->job_count == 0)
  {
    return GEARMAND_SHUTDOWN;
  }

  return GEARMAND_SHUTDOWN_GRACEFUL;
}

static gearmand_error_t gearman_queue_replay(gearman_server_st& server)
{
  assert(server.state.queue_startup == true);
  if (server.queue_version == QUEUE_VERSION_FUNCTION)
  {
    assert(server.queue.functions->_replay_fn);
    return (*(server.queue.functions->_replay_fn))(&server,
                                                    (void *)server.queue.functions->_context,
                                                    gearmand::queue::Context::replay_add,
                                                    &server);
  }

  assert(server.queue.object);
  return server.queue.object->replay(&server);
}


gearmand_error_t gearman_server_queue_replay(gearman_server_st& server)
{
  server.state.queue_startup= true;

  gearmand_error_t ret= gearman_queue_replay(server);
  assert(ret != GEARMAND_UNKNOWN_STATE);

  server.state.queue_startup= false;

  return ret;
}

void *gearman_server_queue_context(const gearman_server_st *server)
{
  if (server->queue_version == QUEUE_VERSION_FUNCTION)
  {
    return (void *)server->queue.functions->_context;
  }

  return NULL;
}

namespace gearmand {
namespace queue {

gearmand_error_t Context::replay_add(gearman_server_st *server,
                                     void*, // context
                                     const char *unique, size_t unique_size,
                                     const char *function_name, size_t function_name_size,
                                     const void *data, size_t data_size,
                                     gearman_job_priority_t priority,
                                     int64_t when)
{
  assert(server->state.queue_startup == true);
  gearmand_error_t ret= GEARMAND_UNKNOWN_STATE;

  (void)gearman_server_job_add(server,
                               function_name, function_name_size,
                               unique, unique_size,
                               data, data_size, priority, NULL, &ret, when);

  if (gearmand_failed(ret))
  {
    gearmand_gerror("gearman_server_job_add", ret);
  }

  return ret;
}

} // namespace queue
} // namespace gearmand

/*
 * Private definitions
 */

static gearmand_error_t
_server_queue_work_data(gearman_server_job_st *server_job,
                        gearmand_packet_st *packet, const gearman_command_t command)
{
  for (gearman_server_client_st* server_client= server_job->client_list; server_client;
       server_client= server_client->job_next)
  {
    gearmand_error_t ret;

    if (command == GEARMAN_COMMAND_WORK_EXCEPTION and (server_client->con->is_exceptions == false))
    {
      ret= gearman_server_io_packet_add(server_client->con, true,
                                        GEARMAN_MAGIC_RESPONSE, GEARMAN_COMMAND_WORK_FAIL,
                                        packet->arg[0], packet->arg_size[0], NULL);
    }
    else
    {
      uint8_t *data;
      if (packet->data_size > 0)
      {
        if (packet->options.free_data and
            server_client->job_next == NULL)
        {
          data= (uint8_t *)(packet->data);
          packet->options.free_data= false;
        }
        else
        {
          data= (uint8_t *)realloc(NULL, packet->data_size);
          if (data == NULL)
          {
            return gearmand_perror(errno, "realloc");
          }

          memcpy(data, packet->data, packet->data_size);
        }
      }
      else
      {
        data= NULL;
      }

      ret= gearman_server_io_packet_add(server_client->con, true,
                                        GEARMAN_MAGIC_RESPONSE, command,
                                        packet->arg[0], packet->arg_size[0],
                                        data, packet->data_size, NULL);
    }

    if (gearmand_failed(ret))
    {
      gearmand_log_gerror_warn(GEARMAN_DEFAULT_LOG_PARAM, ret, "Failed to send WORK_FAIL packet to %s:%s", server_client->con->host(), server_client->con->port());
    }
  }

  return GEARMAND_SUCCESS;
}
