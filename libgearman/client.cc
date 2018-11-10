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

#include "gear_config.h"

#include <libgearman/common.h>
#include <libgearman/log.hpp>

#include "libgearman/assert.hpp"
#include "libgearman/interface/push.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

/*
  Allocate a client structure.
 */
static gearman_client_st *_client_allocate(gearman_client_st *client_shell, bool is_clone)
{
  Client *client= new (std::nothrow) Client(client_shell);
  if (client)
  {
    if (is_clone == false)
    {
      if (getenv("GEARMAN_SERVERS"))
      {
        if (gearman_client_add_servers(client->shell(), getenv("GEARMAN_SERVERS")))
        {
          gearman_client_free(client->shell());
          return NULL;
        }
      }
    }

    return client->shell();
  }

  return NULL;
}

/**
 * Callback function used when parsing server lists.
 */
static gearman_return_t _client_add_server(const char *host, in_port_t port,
                                           void *context)
{
  return gearman_client_add_server(static_cast<gearman_client_st *>(context), host, port);
}


/**
 * Real do function.
 */
static void *_client_do(gearman_client_st *client_shell, gearman_command_t command,
                        const char *function_name,
                        const char *unique,
                        const void *workload_str, size_t workload_size,
                        size_t *result_size, gearman_return_t *ret_ptr)
{
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

  client->universal.reset_error();

  size_t unused_size;
  if (result_size == NULL)
  {
    result_size= &unused_size;
  }
  *result_size= 0;

  gearman_string_t function= { gearman_string_param_cstr(function_name) };
  gearman_unique_t local_unique= gearman_unique_make(unique, unique ? strlen(unique) : 0);
  gearman_string_t workload= { static_cast<const char*>(workload_str), workload_size };

  client->universal.options.no_new_data= true;
  gearman_task_st* do_task= add_task(*client, NULL, NULL, command,
                                     function,
                                     local_unique,
                                     workload,
                                     time_t(0),
                                     gearman_actions_do_default());
  client->universal.options.no_new_data= false;
  if (do_task == NULL)
  {
    *ret_ptr= client->universal.error_code();
    gearman_task_free(do_task);

    return NULL;
  }
  assert_msg(do_task->impl(), "Bad return by add_task()");
  do_task->impl()->type= GEARMAN_TASK_KIND_DO;

  gearman_return_t ret= gearman_client_run_block_tasks(client, do_task);

  // gearman_client_run_tasks failed
  assert(client->task_list); // Programmer error, we should always have the task that we used for do

  char *returnable= NULL;
  if (gearman_failed(ret))
  {
    // We only record the error if it is different then the one we saved.
    if (client->universal.error_code() != ret)
    {
      gearman_error(client->universal, ret, "occurred during gearman_client_run_tasks()");
    }

    *ret_ptr= ret;
    *result_size= 0;
  }
  else if (gearman_success(ret) and do_task->impl()->error_code() == GEARMAN_SUCCESS)
  {
    *ret_ptr= do_task->impl()->error_code();
    if (gearman_task_result(do_task))
    {
      if (gearman_has_allocator(client->universal))
      {
        gearman_string_t result= gearman_result_string(do_task->impl()->result());
        returnable= static_cast<char *>(gearman_malloc(client->universal, gearman_size(result) +1));
        if (returnable == NULL)
        {
          gearman_error(client->universal, GEARMAN_MEMORY_ALLOCATION_FAILURE, "custom workload_fn failed to allocate memory");
          *result_size= 0;
        }
        else // NULL terminate
        {
          memcpy(returnable, gearman_c_str(result), gearman_size(result));
          returnable[gearman_size(result)]= 0;
          *result_size= gearman_size(result);
        }
      }
      else
      {
        gearman_string_t result= gearman_result_take_string(do_task->impl()->result());
        *result_size= gearman_size(result);
        returnable= const_cast<char *>(gearman_c_str(result));
      }
    }
    else // NULL job
    {
      *result_size= 0;
    }
  }
  else // gearman_client_run_tasks() was successful, but the task was not
  {
    gearman_error(client->universal, do_task->impl()->error_code(), "occurred during gearman_client_run_tasks()");

    *ret_ptr= do_task->impl()->error_code();
    *result_size= 0;
  }

  gearman_task_free(do_task);
  client->new_tasks= 0;
  client->running_tasks= 0;

  return returnable;
}

/*
  Real background do function.
*/
static gearman_return_t _client_do_background(gearman_client_st* client_shell,
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
  client->universal.reset_error();

  if (gearman_size(function) == 0)
  {
    return gearman_error(client->universal, GEARMAN_INVALID_ARGUMENT, "function argument was empty");
  }

  client->_do_handle[0]= 0; // Reset the job_handle we store in client

  client->universal.options.no_new_data= true;
  gearman_task_st* do_task= add_task(*client, NULL, 
                                     client, 
                                     command,
                                     function,
                                     unique,
                                     workload,
                                     time_t(0),
                                     gearman_actions_do_default());
  client->universal.options.no_new_data= false;

  if (do_task == NULL)
  {
    gearman_task_free(do_task);
    return client->universal.error_code();
  }
  assert(do_task);
  do_task->impl()->type= GEARMAN_TASK_KIND_DO;

  gearman_return_t ret= gearman_client_run_block_tasks(client, do_task);

  if (job_handle)
  {
    strncpy(job_handle, do_task->impl()->job_handle, GEARMAN_JOB_HANDLE_SIZE);
  }
  strncpy(client->_do_handle, do_task->impl()->job_handle, GEARMAN_JOB_HANDLE_SIZE);
  client->new_tasks= 0;
  client->running_tasks= 0;
  gearman_task_free(do_task);

  return ret;
}


/*
 * Public Definitions
 */

gearman_client_st *gearman_client_create(gearman_client_st *client)
{
  return _client_allocate(client, false);
}

gearman_client_st *gearman_client_clone(gearman_client_st *destination,
                                        const gearman_client_st *source)
{
  if (source == NULL or source->impl() == NULL)
  {
    return _client_allocate(destination, false);
  }
  destination= _client_allocate(destination, true);

  if (destination == NULL or destination->impl() == NULL)
  {
    return NULL;
  }

  destination->impl()->options.non_blocking= source->impl()->options.non_blocking;
  destination->impl()->options.unbuffered_result= source->impl()->options.unbuffered_result;
  destination->impl()->options.no_new= source->impl()->options.no_new;
  destination->impl()->options.free_tasks= source->impl()->options.free_tasks;
  destination->impl()->options.generate_unique= source->impl()->options.generate_unique;
  destination->impl()->ssl(source->impl()->ssl());
  destination->impl()->actions= source->impl()->actions;
  destination->impl()->_do_handle[0]= 0;

  gearman_universal_clone(destination->impl()->universal, source->impl()->universal);

  if (gearman_failed(destination->impl()->universal.error_code()))
  {
    gearman_client_free(destination);
    return NULL;
  }

  return destination;
}

void gearman_client_free(gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl())
  {
    gearman_client_task_free_all(client_shell);

    gearman_universal_free(client_shell->impl()->universal);

    delete client_shell->impl();
  }
}

const char *gearman_client_error(const gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl())
  {
    return client_shell->impl()->universal.error();
  }

  return NULL;
}

gearman_return_t gearman_client_error_code(const gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl())
  {
    return client_shell->impl()->universal.error_code();
  }

  return GEARMAN_INVALID_ARGUMENT;
}

int gearman_client_errno(const gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl())
  {
    return client_shell->impl()->universal.last_errno();
  }

  return EINVAL;
}

gearman_client_options_t gearman_client_options(const gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl())
  {
    Client* client= client_shell->impl();
    int32_t options;
    memset(&options, 0, sizeof(int32_t));

    if (gearman_is_allocated(client_shell))
      options|= int(GEARMAN_CLIENT_ALLOCATED);

    if (client->options.non_blocking)
      options|= int(GEARMAN_CLIENT_NON_BLOCKING);

    if (client->options.unbuffered_result)
      options|= int(GEARMAN_CLIENT_UNBUFFERED_RESULT);

    if (client->options.no_new)
      options|= int(GEARMAN_CLIENT_NO_NEW);

    if (client->options.free_tasks)
      options|= int(GEARMAN_CLIENT_FREE_TASKS);

    if (client->options.generate_unique)
      options|= int(GEARMAN_CLIENT_GENERATE_UNIQUE);

    if (client->ssl())
      options|= int(GEARMAN_CLIENT_SSL);

    return gearman_client_options_t(options);
  }

  return gearman_client_options_t(GEARMAN_WORKER_MAX);
}

bool gearman_client_has_option(gearman_client_st *client_shell,
                                gearman_client_options_t option)
{
  if (client_shell and client_shell->impl())
  {
    Client* client= client_shell->impl();

    switch (option)
    {
    case GEARMAN_CLIENT_ALLOCATED:
      return gearman_is_allocated(client_shell);

    case GEARMAN_CLIENT_NON_BLOCKING:
      return client->options.non_blocking;

    case GEARMAN_CLIENT_UNBUFFERED_RESULT:
      return client->options.unbuffered_result;

    case GEARMAN_CLIENT_NO_NEW:
      return client->options.no_new;

    case GEARMAN_CLIENT_FREE_TASKS:
      return client->options.free_tasks;

    case GEARMAN_CLIENT_GENERATE_UNIQUE:
      return client->options.generate_unique;

    case GEARMAN_CLIENT_EXCEPTION:
      return client->options.exceptions;

    case GEARMAN_CLIENT_SSL:
      return client->ssl();

    default:
    case GEARMAN_CLIENT_TASK_IN_USE:
    case GEARMAN_CLIENT_MAX:
      break; // Let these fall through to false
    }
  }

  return false;
}

void gearman_client_set_options(gearman_client_st *client_shell,
                                gearman_client_options_t options)
{
  if (client_shell)
  {
    gearman_client_options_t usable_options[]= {
      GEARMAN_CLIENT_NON_BLOCKING,
      GEARMAN_CLIENT_UNBUFFERED_RESULT,
      GEARMAN_CLIENT_FREE_TASKS,
      GEARMAN_CLIENT_GENERATE_UNIQUE,
      GEARMAN_CLIENT_EXCEPTION,
      GEARMAN_CLIENT_SSL,
      GEARMAN_CLIENT_MAX
    };

    for (gearman_client_options_t* ptr= usable_options; *ptr != GEARMAN_CLIENT_MAX ; ptr++)
    {
      if (options & *ptr)
      {
        gearman_client_add_options(client_shell, *ptr);
      }
      else
      {
        gearman_client_remove_options(client_shell, *ptr);
      }
    }
  }
}

void gearman_client_add_options(gearman_client_st *client_shell,
                                gearman_client_options_t options)
{
  if (client_shell and client_shell->impl())
  {
    Client* client= client_shell->impl();
    if (options & GEARMAN_CLIENT_NON_BLOCKING)
    {
      gearman_universal_add_options(client->universal, GEARMAN_UNIVERSAL_NON_BLOCKING);
      client->options.non_blocking= true;
    }

    if (options & GEARMAN_CLIENT_UNBUFFERED_RESULT)
    {
      client->options.unbuffered_result= true;
    }

    if (options & GEARMAN_CLIENT_FREE_TASKS)
    {
      client->options.free_tasks= true;
    }

    if (options & GEARMAN_CLIENT_GENERATE_UNIQUE)
    {
      client->options.generate_unique= true;
    }

    if (options & GEARMAN_CLIENT_EXCEPTION)
    {
      client->options.exceptions= gearman_client_set_server_option(client_shell, gearman_literal_param("exceptions"));
    }

    if (options & GEARMAN_CLIENT_SSL)
    {
      client->ssl(true);
    }
  }
}

void gearman_client_remove_options(gearman_client_st *client_shell,
                                   gearman_client_options_t options)
{
  if (client_shell and client_shell->impl())
  {
    Client* client= client_shell->impl();

    if (options & GEARMAN_CLIENT_NON_BLOCKING)
    {
      gearman_universal_remove_options(client->universal, GEARMAN_UNIVERSAL_NON_BLOCKING);
      client->options.non_blocking= false;
    }

    if (options & GEARMAN_CLIENT_UNBUFFERED_RESULT)
    {
      client->options.unbuffered_result= false;
    }

    if (options & GEARMAN_CLIENT_FREE_TASKS)
    {
      client->options.free_tasks= false;
    }

    if (options & GEARMAN_CLIENT_GENERATE_UNIQUE)
    {
      client->options.generate_unique= false;
    }
  }
}

int gearman_client_timeout(gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl())
  {
    return gearman_universal_timeout(client_shell->impl()->universal);
  }

  return -1;
}

void gearman_client_set_timeout(gearman_client_st *client_shell, int timeout)
{
  if (client_shell and client_shell->impl())
  {
    gearman_universal_set_timeout(client_shell->impl()->universal, timeout);
  }
}

void *gearman_client_context(const gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl())
  {
    return const_cast<void *>(client_shell->impl()->context);
  }

  return NULL;
}

void gearman_client_set_context(gearman_client_st *client_shell, void *context)
{
  if (client_shell and client_shell->impl())
  {
    client_shell->impl()->context= context;
  }
}

void gearman_client_set_log_fn(gearman_client_st *client_shell,
                               gearman_log_fn *function, void *context,
                               gearman_verbose_t verbose)
{
  if (client_shell and client_shell->impl())
  {
    gearman_set_log_fn(client_shell->impl()->universal, function, context, verbose);
  }
}

void gearman_client_set_workload_malloc_fn(gearman_client_st *client_shell,
                                           gearman_malloc_fn *function,
                                           void *context)
{
  if (client_shell and client_shell->impl())
  {
    gearman_set_workload_malloc_fn(client_shell->impl()->universal, function, context);
  }
}

void gearman_client_set_workload_free_fn(gearman_client_st *client_shell, gearman_free_fn *function, void *context)
{
  if (client_shell and client_shell->impl())
  {
    gearman_set_workload_free_fn(client_shell->impl()->universal, function, context);
  }
}

gearman_return_t gearman_client_add_server(gearman_client_st *client_shell,
                                           const char *host, in_port_t port)
{
  if (client_shell and client_shell->impl())
  {
    Client* client= client_shell->impl();

    if (gearman_connection_create(client->universal, host, port) == NULL)
    {
      assert(client->error_code() != GEARMAN_SUCCESS);
      return client->error_code();
    }
    assert(client->universal.has_connections());

    return GEARMAN_SUCCESS;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t Client::add_server(const char *host, const char* service_)
{
  if (gearman_connection_create(universal, host, service_) == NULL)
  {
    assert(error_code() != GEARMAN_SUCCESS);
    return error_code();
  }

  return GEARMAN_SUCCESS;
}

gearman_return_t gearman_client_add_servers(gearman_client_st *client_shell,
                                            const char *servers)
{
  return gearman_parse_servers(servers, _client_add_server, client_shell);
}

void gearman_client_remove_servers(gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl())
  {
    gearman_free_all_cons(client_shell->impl()->universal);
  }
}

gearman_return_t gearman_client_wait(gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl())
  {
    return gearman_wait(client_shell->impl()->universal);
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_client_cancel_job(gearman_client_st *client_shell,
                                           gearman_job_handle_t job_handle)
{
  if (client_shell and client_shell->impl())
  {
    client_shell->impl()->universal.reset_error();

    return cancel_job(client_shell->impl()->universal, job_handle);
  }

  return GEARMAN_INVALID_ARGUMENT;
}

void *gearman_client_do(gearman_client_st *client_shell,
                        const char *function,
                        const char *unique,
                        const void *workload,
                        size_t workload_size, size_t *result_size,
                        gearman_return_t *ret_ptr)
{
  return _client_do(client_shell, GEARMAN_COMMAND_SUBMIT_JOB,
                    function,
                    unique,
                    workload, workload_size,
                    result_size, ret_ptr);
}

void *gearman_client_do_high(gearman_client_st *client_shell,
                             const char *function,
                             const char *unique,
                             const void *workload, size_t workload_size,
                             size_t *result_size, gearman_return_t *ret_ptr)
{
  return _client_do(client_shell, GEARMAN_COMMAND_SUBMIT_JOB_HIGH,
                    function,
                    unique,
                    workload, workload_size,
                    result_size, ret_ptr);
}

void *gearman_client_do_low(gearman_client_st *client_shell,
                            const char *function,
                            const char *unique,
                            const void *workload, size_t workload_size,
                            size_t *result_size, gearman_return_t *ret_ptr)
{
  return _client_do(client_shell, GEARMAN_COMMAND_SUBMIT_JOB_LOW,
                    function,
                    unique,
                    workload, workload_size,
                    result_size, ret_ptr);
}

size_t gearman_client_count_tasks(gearman_client_st *client_shell)
{
  if (client_shell == NULL or client_shell->impl() == NULL)
  {
    return 0;
  }

  size_t count= 1;
  gearman_task_st *search= client_shell->impl()->task_list;

  while ((search= search->impl()->next))
  {
    count++;
  }

  return count;
}

bool gearman_client_has_active_tasks(gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl())
  {
    Client* client= client_shell->impl();

    if (client->task_list and client->task_list->impl())
    {
      gearman_task_st* search= client->task_list;

      do
      {
        if (gearman_task_is_active(search))
        {
          return true;
        }
      } while ((search= search->impl()->next));
    }
  }

  return false;
}

const char *gearman_client_do_job_handle(gearman_client_st *self)
{
  if (self)
  {
    return self->impl()->_do_handle;
  }

  errno= EINVAL;
  return NULL;
}

void gearman_client_do_status(gearman_client_st *, uint32_t *numerator, uint32_t *denominator)
{
  if (numerator)
  {
    *numerator= 0;
  }

  if (denominator)
  {
    *denominator= 0;
  }
}

gearman_return_t gearman_client_do_background(gearman_client_st *client_shell,
                                              const char *function_name,
                                              const char *unique,
                                              const void *workload_str,
                                              size_t workload_size,
                                              gearman_job_handle_t job_handle)
{
  gearman_string_t function= { gearman_string_param_cstr(function_name) };
  gearman_unique_t local_unique= gearman_unique_make(unique, unique ? strlen(unique) : 0);
  gearman_string_t workload= { static_cast<const char*>(workload_str), workload_size };

  return _client_do_background(client_shell, GEARMAN_COMMAND_SUBMIT_JOB_BG,
                               function,
                               local_unique,
                               workload,
                               job_handle);
}

gearman_return_t gearman_client_do_high_background(gearman_client_st *client_shell,
                                                   const char *function_name,
                                                   const char *unique,
                                                   const void *workload_str,
                                                   size_t workload_size,
                                                   gearman_job_handle_t job_handle)
{
  gearman_string_t function= { gearman_string_param_cstr(function_name) };
  gearman_unique_t local_unique= gearman_unique_make(unique, unique ? strlen(unique) : 0);
  gearman_string_t workload= { static_cast<const char*>(workload_str), workload_size };

  return _client_do_background(client_shell, GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG,
                               function,
                               local_unique,
                               workload,
                               job_handle);
}

gearman_return_t gearman_client_do_low_background(gearman_client_st *client_shell,
                                                  const char *function_name,
                                                  const char *unique,
                                                  const void *workload_str,
                                                  size_t workload_size,
                                                  gearman_job_handle_t job_handle)
{
  gearman_string_t function= { gearman_string_param_cstr(function_name) };
  gearman_unique_t local_unique= gearman_unique_make(unique, unique ? strlen(unique) : 0);
  gearman_string_t workload= { static_cast<const char*>(workload_str), workload_size };

  return _client_do_background(client_shell, GEARMAN_COMMAND_SUBMIT_JOB_LOW_BG,
                               function,
                               local_unique,
                               workload,
                               job_handle);
}

gearman_status_t gearman_client_unique_status(gearman_client_st *client_shell,
                                              const char *unique, size_t unique_length)
{
  (void)unique_length;
  gearman_status_t status;
  gearman_init(status);

  if (client_shell == NULL or client_shell->impl() == NULL)
  {
    gearman_status_set_return(status, GEARMAN_INVALID_ARGUMENT);
    return status;
  }
  Client* client= client_shell->impl();

  client->universal.reset_error();

  gearman_return_t ret;
  gearman_task_st* do_task= gearman_client_add_task_status_by_unique(client_shell,
                                                                     NULL,
                                                                     unique, &ret);
  if (do_task == NULL)
  {
    gearman_status_set_return(status, client->universal.error_code());
    return status;
  }

  if (gearman_failed(ret))
  {
    gearman_status_set_return(status, ret);
    return status;
  }

  Task* task= do_task->impl();
  task->type= GEARMAN_TASK_KIND_DO;

  gearman_task_clear_fn(do_task);

  ret= gearman_client_run_block_tasks(client, do_task);

  // @note we don't know if our task was run or not, we just know something
  // happened.

  if (gearman_success(ret))
  {
    gearman_status_set(status,
                       task->options.is_known,
                       task->options.is_running,
                       task->numerator,
                       task->denominator,
                       task->client_count);

    if (gearman_status_is_known(status) == false and gearman_status_is_running(status) == false)
    {
      if (task->options.is_running) 
      {
        ret= GEARMAN_IN_PROGRESS;
      }
      else if (task->options.is_known)
      {
        ret= GEARMAN_JOB_EXISTS;
      }
    }
  }

  gearman_task_free(do_task);

  gearman_status_set_return(status, ret);

  return status;
}

gearman_return_t gearman_client_job_status(gearman_client_st *client_shell,
                                           const gearman_job_handle_t job_handle,
                                           bool *is_known, bool *is_running,
                                           uint32_t *numerator,
                                           uint32_t *denominator)
{
  gearman_return_t ret;

  if (client_shell == NULL or client_shell->impl() == NULL)
  {
    return GEARMAN_INVALID_ARGUMENT;
  }

  Client* client= client_shell->impl();

  client->universal.reset_error();

  gearman_task_st *do_task= gearman_client_add_task_status(client_shell, NULL, client,
                                                           job_handle, &ret);
  if (gearman_failed(ret))
  {
    gearman_task_free(do_task);
    return ret;
  }
  assert_msg(do_task, "Programming error, a NULL return happend from gearman_client_add_task_status() without an error");
  do_task->impl()->type= GEARMAN_TASK_KIND_DO;

  gearman_task_clear_fn(do_task);

  ret= gearman_client_run_block_tasks(client, do_task);

  // @note we don't know if our task was run or not, we just know something
  // happened.

  if (gearman_success(ret))
  {
    if (is_known)
    {
      *is_known= do_task->impl()->options.is_known;
    }

    if (is_running)
    {
      *is_running= do_task->impl()->options.is_running;
    }

    if (numerator)
    {
      *numerator= do_task->impl()->numerator;
    }

    if (denominator)
    {
      *denominator= do_task->impl()->denominator;
    }

    if (!is_known and !is_running)
    {
      if (do_task->impl()->options.is_running) 
      {
        ret= GEARMAN_IN_PROGRESS;
      }
      else if (do_task->impl()->options.is_known)
      {
        ret= GEARMAN_JOB_EXISTS;
      }
    }
  }
  else
  {
    if (is_known)
    {
      *is_known= false;
    }

    if (is_running)
    {
      *is_running= false;
    }

    if (numerator)
    {
      *numerator= 0;
    }

    if (denominator)
    {
      *denominator= 0;
    }
  }
  gearman_task_free(do_task);

  return ret;
}

gearman_return_t gearman_client_echo(gearman_client_st *client,
                                     const void *workload,
                                     size_t workload_size)
{
  if (client == NULL or client->impl() == NULL)
  {
    return GEARMAN_INVALID_ARGUMENT;
  }

  return gearman_echo(client->impl()->universal, workload, workload_size);
}

void gearman_client_task_free_all(gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl() and client_shell->impl()->task_list)
  {
    Client* client= client_shell->impl();
    while (client->task_list)
    {
      assert(client == client->task_list->impl()->client);
      gearman_task_free(client->task_list);
    }
  }
}


void gearman_client_set_task_context_free_fn(gearman_client_st *client,
                                             gearman_task_context_free_fn *function)
{
  if (client and client->impl())
  {
    client->impl()->task_context_free_fn= function;
  }
}

gearman_return_t gearman_client_set_memory_allocators(gearman_client_st *client,
                                                      gearman_malloc_fn *malloc_fn,
                                                      gearman_free_fn *free_fn,
                                                      gearman_realloc_fn *realloc_fn,
                                                      gearman_calloc_fn *calloc_fn,
                                                      void *context)
{
  if (client and client->impl())
  {
    return gearman_set_memory_allocator(client->impl()->universal.allocator, malloc_fn, free_fn, realloc_fn, calloc_fn, context);
  }

  return GEARMAN_INVALID_ARGUMENT;
}



gearman_task_st *gearman_client_add_task(gearman_client_st *client,
                                         gearman_task_st *task,
                                         void *context,
                                         const char *function,
                                         const char *unique,
                                         const void *workload, size_t workload_size,
                                         gearman_return_t *ret_ptr)
{
  gearman_return_t unused;
  if (ret_ptr == NULL)
  {
    ret_ptr= &unused;
  }

  if (client and client->impl())
  {
    return add_task_ptr(*(client->impl()), task,
                        context, GEARMAN_COMMAND_SUBMIT_JOB,
                        function,
                        unique,
                        workload, workload_size,
                        time_t(0),
                        *ret_ptr,
                        client->impl()->actions);
  }

  *ret_ptr= GEARMAN_INVALID_ARGUMENT;
  return NULL;
}

gearman_task_st *gearman_client_add_task_high(gearman_client_st *client,
                                              gearman_task_st *task,
                                              void *context,
                                              const char *function,
                                              const char *unique,
                                              const void *workload, size_t workload_size,
                                              gearman_return_t *ret_ptr)
{
  gearman_return_t unused;
  if (ret_ptr == NULL)
  {
    ret_ptr= &unused;
  }

  if (client and client->impl())
  {
    return add_task_ptr(*(client->impl()), task, context,
                        GEARMAN_COMMAND_SUBMIT_JOB_HIGH,
                        function,
                        unique,
                        workload, workload_size,
                        time_t(0),
                        *ret_ptr,
                        client->impl()->actions);
  }

  *ret_ptr= GEARMAN_INVALID_ARGUMENT;
  return NULL;
}

gearman_task_st *gearman_client_add_task_low(gearman_client_st *client,
                                             gearman_task_st *task,
                                             void *context,
                                             const char *function,
                                             const char *unique,
                                             const void *workload, size_t workload_size,
                                             gearman_return_t *ret_ptr)
{
  gearman_return_t unused;
  if (ret_ptr == NULL)
  {
    ret_ptr= &unused;
  }

  if (client and client->impl())
  {
    return add_task_ptr(*(client->impl()), task, context, GEARMAN_COMMAND_SUBMIT_JOB_LOW,
                        function,
                        unique,
                        workload, workload_size,
                        time_t(0),
                        *ret_ptr,
                        client->impl()->actions);
  }

  *ret_ptr= GEARMAN_INVALID_ARGUMENT;
  return NULL;
}

gearman_task_st *gearman_client_add_task_background(gearman_client_st *client,
                                                    gearman_task_st *task,
                                                    void *context,
                                                    const char *function,
                                                    const char *unique,
                                                    const void *workload, size_t workload_size,
                                                    gearman_return_t *ret_ptr)
{
  gearman_return_t unused;
  if (ret_ptr == NULL)
  {
    ret_ptr= &unused;
  }

  if (client == NULL or client->impl() == NULL)
  {
    *ret_ptr= GEARMAN_INVALID_ARGUMENT;
    return NULL;
  }

  return add_task_ptr(*(client->impl()), task, context, GEARMAN_COMMAND_SUBMIT_JOB_BG,
                      function,
                      unique,
                      workload, workload_size,
                      time_t(0),
                      *ret_ptr,
                      client->impl()->actions);
}

gearman_task_st *
gearman_client_add_task_high_background(gearman_client_st *client,
                                        gearman_task_st *task,
                                        void *context,
                                        const char *function,
                                        const char *unique,
                                        const void *workload, size_t workload_size,
                                        gearman_return_t *ret_ptr)
{
  gearman_return_t unused;
  if (ret_ptr == NULL)
  {
    ret_ptr= &unused;
  }

  if (client == NULL or client->impl() == NULL)
  {
    *ret_ptr= GEARMAN_INVALID_ARGUMENT;
    return NULL;
  }

  return add_task_ptr(*(client->impl()), task, context,
                      GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG,
                      function,
                      unique,
                      workload, workload_size,
                      time_t(0),
                      *ret_ptr,
                      client->impl()->actions);
}

gearman_task_st* gearman_client_add_task_low_background(gearman_client_st *client,
                                                        gearman_task_st *task,
                                                        void *context,
                                                        const char *function,
                                                        const char *unique,
                                                        const void *workload, size_t workload_size,
                                                        gearman_return_t *ret_ptr)
{
  gearman_return_t unused;
  if (ret_ptr == NULL)
  {
    ret_ptr= &unused;
  }

  if (client == NULL or client->impl() == NULL)
  {
    *ret_ptr= GEARMAN_INVALID_ARGUMENT;
    return NULL;
  }

  return add_task_ptr(*(client->impl()), task, context,
                      GEARMAN_COMMAND_SUBMIT_JOB_LOW_BG,
                      function,
                      unique,
                      workload, workload_size,
                      time_t(0),
                      *ret_ptr,
                      client->impl()->actions);

}

gearman_task_st *gearman_client_add_task_status(gearman_client_st *client_shell,
                                                gearman_task_st *task_shell,
                                                void *context,
                                                const gearman_job_handle_t job_handle,
                                                gearman_return_t *ret_ptr)
{
  const void *args[1];
  size_t args_size[1];

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

  if ((task_shell= gearman_task_internal_create(client, task_shell)) == NULL)
  {
    *ret_ptr= gearman_client_error_code(client_shell);
    return NULL;
  }

  Task* task= task_shell->impl();

  task->context= context;
  snprintf(task->job_handle, GEARMAN_JOB_HANDLE_SIZE, "%s", job_handle);

  args[0]= job_handle;
  args_size[0]= strlen(job_handle);
  gearman_return_t rc= gearman_packet_create_args(client->universal, task->send,
                                                  GEARMAN_MAGIC_REQUEST,
                                                  GEARMAN_COMMAND_GET_STATUS,
                                                  args, args_size, 1);
  if (gearman_success(rc))
  {
    client->new_tasks++;
    client->running_tasks++;
    task->options.send_in_use= true;
  }
  *ret_ptr= rc;

  return task_shell;
}

gearman_task_st *gearman_client_add_task_status_by_unique(gearman_client_st *client_shell,
                                                          gearman_task_st *task_shell,
                                                          const char *unique_handle,
                                                          gearman_return_t *ret_ptr)
{
  const void *args[1];
  size_t args_size[1];

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

  if (unique_handle == NULL)
  {
    *ret_ptr= GEARMAN_INVALID_ARGUMENT;
    return NULL;
  }

  size_t unique_length= strlen(unique_handle);
  if (unique_length > GEARMAN_MAX_UNIQUE_SIZE)
  {
    *ret_ptr= GEARMAN_INVALID_ARGUMENT;
    return NULL;
  }

  if ((task_shell= gearman_task_internal_create(client, task_shell)) == NULL)
  {
    *ret_ptr= gearman_client_error_code(client_shell);
    return NULL;
  }

  Task* task= task_shell->impl();

  task->unique_length= unique_length;
  memcpy(task->unique, unique_handle, unique_length);
  task->unique[task->unique_length]= 0;

  args[0]= task->unique;
  args_size[0]= task->unique_length;
  gearman_return_t rc= gearman_packet_create_args(client->universal, task->send,
                                                  GEARMAN_MAGIC_REQUEST,
                                                  GEARMAN_COMMAND_GET_STATUS_UNIQUE,
                                                  args, args_size, 1);
  if (gearman_success(rc))
  {
    client->new_tasks++;
    client->running_tasks++;
    task->options.send_in_use= true;
  }
  *ret_ptr= rc;

  return task_shell;
}

void gearman_client_set_workload_fn(gearman_client_st *client,
                                    gearman_workload_fn *function)
{
  if (client and client->impl())
  {
    client->impl()->actions.workload_fn= function;
  }
}

void gearman_client_set_created_fn(gearman_client_st *client,
                                   gearman_created_fn *function)
{
  if (client and client->impl())
  {
    client->impl()->actions.created_fn= function;
  }
}

void gearman_client_set_data_fn(gearman_client_st *client,
                                gearman_data_fn *function)
{
  if (client and client->impl())
  {
    client->impl()->actions.data_fn= function;
  }
}

void gearman_client_set_warning_fn(gearman_client_st *client,
                                   gearman_warning_fn *function)
{
  if (client and client->impl())
  {
    client->impl()->actions.warning_fn= function;
  }
}

void gearman_client_set_status_fn(gearman_client_st *client,
                                  gearman_universal_status_fn *function)
{
  if (client and client->impl())
  {
    client->impl()->actions.status_fn= function;
  }
}

void gearman_client_set_complete_fn(gearman_client_st *client,
                                    gearman_complete_fn *function)
{
  if (client and client->impl())
  {
    client->impl()->actions.complete_fn= function;
  }
}

void gearman_client_set_exception_fn(gearman_client_st *client,
                                     gearman_exception_fn *function)
{
  if (client and client->impl())
  {
    client->impl()->actions.exception_fn= function;
  }
}

void gearman_client_set_fail_fn(gearman_client_st* client,
                                gearman_fail_fn *function)
{
  if (client and client->impl())
  {
    client->impl()->actions.fail_fn= function;
  }
}

void gearman_client_clear_fn(gearman_client_st* client)
{
  if (client and client->impl())
  {
    client->impl()->actions= gearman_actions_default();
  }
}

static inline gearman_return_t _client_run_tasks(gearman_client_st *client_shell, gearman_task_st* exit_task)
{
  gearman_return_t ret= GEARMAN_MAX_RETURN;

  Client* client= client_shell->impl();

  switch(client->state)
  {
  case GEARMAN_CLIENT_STATE_IDLE:
    while (1)
    {
      /* Start any new tasks. */
      if (client->new_tasks > 0 && ! (client->options.no_new))
      {
        for (client->task= client->task_list; client->task;
             client->task= client->task->impl()->next)
        {
          if (client->task->impl()->state != GEARMAN_TASK_STATE_NEW)
          {
            continue;
          }

  case GEARMAN_CLIENT_STATE_NEW:
          if (client->task == NULL)
          {
            client->state= GEARMAN_CLIENT_STATE_IDLE;
            break;
          }

          gearman_return_t local_ret= _client_run_task(client->task->impl());
          if (gearman_failed(local_ret) and local_ret != GEARMAN_IO_WAIT)
          {
            client->state= GEARMAN_CLIENT_STATE_NEW;

            return local_ret;
          }
        }

        if (client->new_tasks == 0)
        {
          client->universal.flush();
        }
      }

      /* See if there are any connections ready for I/O. */
      while ((client->con= gearman_ready(client->universal)))
      {
        if (client->con->is_revents(POLLOUT | POLLERR | POLLHUP | POLLNVAL))
        {
          /* Socket is ready for writing, continue submitting jobs. */
          for (client->task= client->task_list; client->task;
               client->task= client->task->impl()->next)
          {
            if (client->task->impl()->con != client->con or
                (client->task->impl()->state != GEARMAN_TASK_STATE_SUBMIT and
                 client->task->impl()->state != GEARMAN_TASK_STATE_WORKLOAD))
            {
              continue;
            }

  case GEARMAN_CLIENT_STATE_SUBMIT:
            if (client->task == NULL)
            {
              client->state= GEARMAN_CLIENT_STATE_IDLE;
              break;
            }
            gearman_return_t local_ret= _client_run_task(client->task->impl());
            if (local_ret == GEARMAN_COULD_NOT_CONNECT)
            {
              client->state= GEARMAN_CLIENT_STATE_IDLE;
              return local_ret;
            }
            else if (gearman_failed(local_ret) and local_ret != GEARMAN_IO_WAIT)
            {
              client->state= GEARMAN_CLIENT_STATE_SUBMIT;
              return local_ret;
            }
          }

          /* Connection errors are fatal. */
          if (client->con->is_revents(POLLERR | POLLHUP | POLLNVAL))
          {
            gearman_error(client->universal, GEARMAN_LOST_CONNECTION, "detected lost connection in _client_run_tasks()");
            client->con->close_socket();
            client->state= GEARMAN_CLIENT_STATE_IDLE;
            return GEARMAN_LOST_CONNECTION;
          }
        }

        if ((client->con->is_revents(POLLIN)) == false)
        {
          continue;
        }

        /* Socket is ready for reading. */
        while (1)
        {
          /* Read packet on connection and find which task it belongs to. */
          if (client->options.unbuffered_result)
          {
            /* If client is handling the data read, make sure it's complete. */
            if (client->con->recv_state == GEARMAN_CON_RECV_STATE_READ_DATA)
            {
              for (client->task= client->task_list; client->task;
                   client->task= client->task->impl()->next)
              {
                if (client->task->impl()->con == client->con &&
                    (client->task->impl()->state == GEARMAN_TASK_STATE_DATA or
                     client->task->impl()->state == GEARMAN_TASK_STATE_COMPLETE))
                {
                  break;
                }
              }

              /*
                Someone has set GEARMAN_CLIENT_UNBUFFERED_RESULT but hasn't setup the client to fetch data correctly.
                Fatal error :(
              */
              return gearman_universal_set_error(client->universal, GEARMAN_INVALID_ARGUMENT, GEARMAN_AT,
                                                 "client created with GEARMAN_CLIENT_UNBUFFERED_RESULT, but was not setup to use it. %s", __func__);
            }
            else
            {
              /* Read the next packet, without buffering the data part. */
              client->task= NULL;
              (void)client->con->receiving(client->con->_packet, ret, false);
            }
          }
          else
          {
            /* Read the next packet, buffering the data part. */
            client->task= NULL;
            (void)client->con->receiving(client->con->_packet, ret, true);
          }

          if (client->task == NULL)
          {
            assert(ret != GEARMAN_MAX_RETURN);

            /* Check the return of the gearman_connection_recv() calls above. */
            if (gearman_failed(ret))
            {
              if (ret == GEARMAN_IO_WAIT)
              {
                break;
              }

              client->state= GEARMAN_CLIENT_STATE_IDLE;
              return ret;
            }

            client->con->options.packet_in_use= true;

            /* We have a packet, see which task it belongs to. */
            for (client->task= client->task_list; client->task;
                 client->task= client->task->impl()->next)
            {
              if (client->task->impl()->con != client->con)
              {
                continue;
              }

              if (client->con->_packet.command == GEARMAN_COMMAND_JOB_CREATED)
              {
                if (client->task->impl()->created_id != client->con->created_id)
                {
                  continue;
                }

                /* New job created, drop through below and notify task. */
                client->con->created_id++;
              }
              else if (client->con->_packet.command == GEARMAN_COMMAND_ERROR)
              {
                gearman_return_t maybe_server_error= string2return_code(static_cast<char *>(client->con->_packet.arg[0]), int(client->con->_packet.arg_size[0]));

                if (maybe_server_error == GEARMAN_MAX_RETURN)
                {
                  maybe_server_error= GEARMAN_SERVER_ERROR;
                }

                gearman_universal_set_error(client->universal, maybe_server_error, GEARMAN_AT,
                                            "%s:%.*s",
                                            static_cast<char *>(client->con->_packet.arg[0]),
                                            int(client->con->_packet.arg_size[1]),
                                            static_cast<char *>(client->con->_packet.arg[1]));

                /* 
                  Packet cleanup copied from "Clean up the packet" below, and must
                  remain in sync with its reference.
                */
                gearman_packet_free(&(client->con->_packet));
                client->con->options.packet_in_use= false;

                /* This step copied from _client_run_tasks() above: */
                /* Increment this value because new job created then failed. */
                client->con->created_id++;

                return maybe_server_error;
              }
              else if (client->con->_packet.command == GEARMAN_COMMAND_STATUS_RES_UNIQUE and
                       (strncmp(gearman_task_unique(client->task),
                               static_cast<char *>(client->con->_packet.arg[0]),
                               client->con->_packet.arg_size[0]) == 0))
              { }
              else if (strncmp(client->task->impl()->job_handle,
                               static_cast<char *>(client->con->_packet.arg[0]),
                               client->con->_packet.arg_size[0]) ||
                       (client->con->_packet.failed() == false &&
                        strlen(client->task->impl()->job_handle) != client->con->_packet.arg_size[0] - 1) ||
                       (client->con->_packet.failed() &&
                        strlen(client->task->impl()->job_handle) != client->con->_packet.arg_size[0]))
              {
                continue;
              }

              /* Else, we have a matching result packet of some kind. */

              break;
            }

            if (client->task == NULL)
            {
              /* The client has stopped waiting for the response, ignore it. */
              client->con->free_private_packet();
              continue;
            }

            client->task->impl()->recv= &(client->con->_packet);
          }

  case GEARMAN_CLIENT_STATE_PACKET:
          /* Let task process job created or result packet. */
          gearman_return_t local_ret= _client_run_task(client->task->impl());
          if (local_ret == GEARMAN_IO_WAIT)
          {
            break;
          }

          if (gearman_failed(local_ret))
          {
            client->state= GEARMAN_CLIENT_STATE_PACKET;
            return local_ret;
          }

          /* Clean up the packet. */
          client->con->free_private_packet();

          /* If exit task is set and matched, exit */
          if (exit_task)
          {
            if (exit_task->impl()->error_code() != GEARMAN_UNKNOWN_STATE)
            {
              client->state= GEARMAN_CLIENT_STATE_IDLE;
              return GEARMAN_SUCCESS;
            }
          }

          /* If all tasks are done, return. */
          if (client->running_tasks == 0)
          {
            client->state= GEARMAN_CLIENT_STATE_IDLE;
            return GEARMAN_SUCCESS;
          }
        }
      }

      /* If all tasks are done, return. */
      if (client->running_tasks == 0)
      {
        break;
      }

      if (client->new_tasks > 0 and ! (client->options.no_new))
      {
        continue;
      }

      if (client->options.non_blocking)
      {
        /* Let the caller wait for activity. */
        client->state= GEARMAN_CLIENT_STATE_IDLE;

        return gearman_gerror(client->universal, GEARMAN_IO_WAIT);
      }

      /* Wait for activity on one of the connections. */
      gearman_return_t local_ret= gearman_wait(client->universal);
      if (gearman_failed(local_ret) and local_ret != GEARMAN_IO_WAIT)
      {
        client->state= GEARMAN_CLIENT_STATE_IDLE;

        return local_ret;
      }
    }

    break;
  }

  client->state= GEARMAN_CLIENT_STATE_IDLE;

  return GEARMAN_SUCCESS;
}

gearman_return_t gearman_client_run_tasks(gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl())
  {
    Client* client= client_shell->impl();

    if (client->task_list == NULL) // We are immediately successful if all tasks are completed
    {
      return GEARMAN_SUCCESS;
    }

    gearman_return_t rc;
    {
      PUSH_NON_BLOCKING(client->universal);

      rc= _client_run_tasks(client_shell, NULL);
    }

    if (rc == GEARMAN_COULD_NOT_CONNECT)
    {
      client->universal.reset();
    }

    return rc;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_client_run_block_tasks(Client* client, gearman_task_st* exit_task)
{
  if (client->task_list == NULL) // We are immediately successful if all tasks are completed
  {
    return GEARMAN_SUCCESS;
  }

  gearman_return_t rc;
  {
    PUSH_BLOCKING(client->universal);

    rc= _client_run_tasks(client->shell(), exit_task);
  }

  if (gearman_failed(rc))
  {
    if (rc == GEARMAN_COULD_NOT_CONNECT)
    {
      client->universal.reset();
    }

    if (client->universal.error_code() != rc and rc != GEARMAN_COULD_NOT_CONNECT)
    {
      assert(client->universal.error_code() == rc);
    }
  }

  return rc;
}

/*
 * Static Definitions
 */

bool gearman_client_compare(const gearman_client_st *first_shell, const gearman_client_st *second_shell)
{
  if (first_shell and second_shell)
  {
    Client* first= first_shell->impl();
    Client* second= second_shell->impl();

    if (first and second)
    {
      if (strcmp(first->universal.con_list->_host, second->universal.con_list->_host) == 0)
      {
        if (strcmp(first->universal.con_list->_service, second->universal.con_list->_service) == 0)
        {
          return true;
        }
      }
    }
  }

  return false;
}

bool gearman_client_set_server_option(gearman_client_st *client_shell, const char *option_arg, size_t option_arg_size)
{
  if (client_shell and client_shell->impl())
  {
    Client* client= client_shell->impl();
    gearman_string_t option= { option_arg, option_arg_size };

    if (gearman_success(gearman_server_option(client->universal, option)))
    {
      if (gearman_request_option(client->universal, option))
      {
        if (strncmp("exceptions", option_arg, sizeof("exceptions")) == 0)
        {
          client->options.exceptions= true;
        }

        return true;
      }
    }
  }

  return false;
}

void gearman_client_set_namespace(gearman_client_st *client_shell, const char *namespace_key, size_t namespace_key_size)
{
  if (client_shell and client_shell->impl())
  {
    gearman_universal_set_namespace(client_shell->impl()->universal, namespace_key, namespace_key_size);
  }
}

gearman_return_t gearman_client_set_identifier(gearman_client_st *client,
                                               const char *id, size_t id_size)
{
  if (client and client->impl())
  {
    return gearman_set_identifier(client->impl()->universal, id, id_size);
  }

  return GEARMAN_INVALID_ARGUMENT;
}

const char *gearman_client_namespace(gearman_client_st *self)
{
  return gearman_univeral_namespace(self->impl()->universal);
}

bool gearman_client_has_tasks(const gearman_client_st *client_shell)
{
  if (client_shell and client_shell->impl())
  {
#ifndef NDEBUG
    if (client_shell->impl()->task_list)
    {
      assert(client_shell->impl()->task_count);
    }
    else
    {
      assert(client_shell->impl()->task_count == 0);
    }
#endif

    return bool(client_shell->impl()->task_list);
  }

  return false;
}
