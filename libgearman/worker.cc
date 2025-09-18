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
#include "libgearman/uuid.hpp"
#include <libgearman/function/base.hpp>
#include <libgearman/function/make.hpp>

#include "libgearman/assert.hpp"

#include "libgearman/log.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

/**
 * @addtogroup gearman_worker_static Static Worker Declarations
 * @ingroup gearman_worker
 * @{
 */

static inline struct _worker_function_st *_function_exist(Worker* worker, const char *function_name, const size_t function_length)
{
  struct _worker_function_st *function;

  for (function= worker->function_list; function;
       function= function->next)
  {
    if (function_length == function->function_length())
    {
      if (memcmp(function_name, function->function_name(), function_length) == 0)
      {
        break;
      }
    }
  }

  return function;
}

/**
 * Allocate a worker structure.
 */
static gearman_worker_st *_worker_allocate(gearman_worker_st *worker, bool is_clone);

/**
 * Initialize common packets for later use.
 */
static gearman_return_t _worker_packet_init(Worker*);

/**
 * Callback function used when parsing server lists.
 */
static gearman_return_t _worker_add_server(const char *host, in_port_t port, void *context);

/**
 * Allocate and add a function to the register list.
 */
static gearman_return_t _worker_function_create(Worker *worker,
                                                const char *function_name, const size_t function_length,
                                                const gearman_function_t &function,
                                                uint32_t timeout,
                                                void *context);

/**
 * Free a function.
 */
static void _worker_function_free(Worker* worker,
                                  struct _worker_function_st *function);


/** @} */

/*
 * Public Definitions
 */

gearman_worker_st *gearman_worker_create(gearman_worker_st *worker_shell)
{
  worker_shell= _worker_allocate(worker_shell, false);

  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();

    gearman_return_t ret;
    if (gearman_failed((ret= _worker_packet_init(worker))))
    {
      gearman_worker_free(worker_shell);
      return NULL;
    }
  }

  return worker_shell;
}

gearman_worker_st *gearman_worker_clone(gearman_worker_st *worker_shell,
                                        const gearman_worker_st *source_shell)
{
  if (source_shell == NULL)
  {
    return gearman_worker_create(worker_shell);
  }

  worker_shell= _worker_allocate(worker_shell, true);

  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();
    Worker* source= source_shell->impl();

    worker->options.change= source->options.change;
    worker->options.grab_uniq= source->options.grab_uniq;
    worker->options.grab_all= source->options.grab_all;
    worker->options.timeout_return= source->options.timeout_return;
    worker->ssl(source->ssl());

    gearman_universal_clone(worker->universal, source->universal);

    if (gearman_failed(_worker_packet_init(worker)))
    {
      gearman_worker_free(worker_shell);
      return NULL;
    }

    for (struct _worker_function_st* function= source->function_list;
         function;
         function= function->next)
    {
      _worker_function_create(worker,
                              function->function_name(), function->function_length(),
                              function->function(),
                              function->timeout(),
                              function->context());
    }
  }

  return worker_shell;
}

void gearman_worker_free(gearman_worker_st *worker_shell)
{
#ifndef NDEBUG
  if (worker_shell)
  {
    assert(worker_shell->impl());
  }
#endif

  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();
    gearman_worker_unregister_all(worker_shell);

    if (worker->options.packet_init)
    {
      gearman_packet_free(&worker->grab_job);
      gearman_packet_free(&worker->pre_sleep);
    }

    worker->job(NULL);

    if (worker->work_result)
    {
      gearman_free(worker->universal, worker->work_result);
    }

    while (worker->function_list)
    {
      _worker_function_free(worker, worker->function_list);
    }

    gearman_job_free_all(worker_shell);

    gearman_universal_free(worker->universal);

    delete worker;
  }
}

const char *gearman_worker_error(const gearman_worker_st *worker_shell)
{
  if (worker_shell and worker_shell->impl())
  {
    return worker_shell->impl()->universal.error();
  }

  return NULL;
}

int gearman_worker_errno(gearman_worker_st *worker_shell)
{
  if (worker_shell and worker_shell->impl())
  {
    return worker_shell->impl()->universal.last_errno();
  }

  return EINVAL;
}

gearman_worker_options_t gearman_worker_options(const gearman_worker_st *worker_shell)
{
  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();
    int options;
    memset(&options, 0, sizeof(gearman_worker_options_t));

    if (gearman_is_allocated(worker_shell))
    {
      options|= int(GEARMAN_WORKER_ALLOCATED);
    }

    if (worker->options.non_blocking)
      options|= int(GEARMAN_WORKER_NON_BLOCKING);
    if (worker->options.packet_init)
      options|= int(GEARMAN_WORKER_PACKET_INIT);
    if (worker->options.change)
      options|= int(GEARMAN_WORKER_CHANGE);
    if (worker->options.grab_uniq)
      options|= int(GEARMAN_WORKER_GRAB_UNIQ);
    if (worker->options.grab_all)
      options|= int(GEARMAN_WORKER_GRAB_ALL);
    if (worker->options.timeout_return)
      options|= int(GEARMAN_WORKER_TIMEOUT_RETURN);
    if (worker->ssl())
      options|= int(GEARMAN_WORKER_SSL);
    if (worker->has_identifier())
      options|= int(GEARMAN_WORKER_IDENTIFIER);

    return gearman_worker_options_t(options);
  }

  return gearman_worker_options_t();
}

void gearman_worker_set_options(gearman_worker_st *worker,
                                gearman_worker_options_t options)
{
  if (worker and worker->impl())
  {
    gearman_worker_options_t usable_options[]= {
      GEARMAN_WORKER_NON_BLOCKING,
      GEARMAN_WORKER_GRAB_UNIQ,
      GEARMAN_WORKER_GRAB_ALL,
      GEARMAN_WORKER_TIMEOUT_RETURN,
      GEARMAN_WORKER_SSL,
      GEARMAN_WORKER_IDENTIFIER,
      GEARMAN_WORKER_MAX
    };

    for (gearman_worker_options_t* ptr= usable_options; *ptr != GEARMAN_WORKER_MAX ; ++ptr)
    {
      if (options & *ptr)
      {
        gearman_worker_add_options(worker, *ptr);
      }
      else
      {
        gearman_worker_remove_options(worker, *ptr);
      }
    }
  }
}

void gearman_worker_add_options(gearman_worker_st *worker_shell,
                                gearman_worker_options_t options)
{
  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();

    if (options & GEARMAN_WORKER_NON_BLOCKING)
    {
      gearman_universal_add_options(worker->universal, GEARMAN_UNIVERSAL_NON_BLOCKING);
      worker->options.non_blocking= true;
    }

    if (options & GEARMAN_WORKER_GRAB_UNIQ)
    {
      worker->grab_job.command= GEARMAN_COMMAND_GRAB_JOB_UNIQ;
      gearman_return_t rc= gearman_packet_pack_header(&(worker->grab_job));
      (void)(rc);
      assert(gearman_success(rc));
      worker->options.grab_uniq= true;
    }

    if (options & GEARMAN_WORKER_GRAB_ALL)
    {
      worker->grab_job.command= GEARMAN_COMMAND_GRAB_JOB_ALL;
      gearman_return_t rc= gearman_packet_pack_header(&(worker->grab_job));
      (void)(rc);
      assert(gearman_success(rc));
      worker->options.grab_all= true;
    }

    if (options & GEARMAN_WORKER_TIMEOUT_RETURN)
    {
      worker->options.timeout_return= true;
    }

    if (options & GEARMAN_WORKER_SSL)
    {
      worker->ssl(true);
    }

    if (options & GEARMAN_WORKER_IDENTIFIER)
    {
      char uuid_buffer[GEARMAN_MAX_IDENTIFIER];
      size_t length= GEARMAN_MAX_IDENTIFIER;
      safe_uuid_generate(uuid_buffer, length);
      worker->universal.identifier(uuid_buffer, length);
    }
  }
}

void gearman_worker_remove_options(gearman_worker_st *worker_shell,
                                   gearman_worker_options_t options)
{
  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();

    if (options & GEARMAN_WORKER_NON_BLOCKING)
    {
      gearman_universal_remove_options(worker->universal, GEARMAN_UNIVERSAL_NON_BLOCKING);
      worker->options.non_blocking= false;
    }

    if (options & GEARMAN_WORKER_TIMEOUT_RETURN)
    {
      worker->options.timeout_return= false;
      gearman_universal_set_timeout(worker->universal, GEARMAN_WORKER_WAIT_TIMEOUT);
    }

    if (options & GEARMAN_WORKER_GRAB_UNIQ)
    {
      worker->grab_job.command= GEARMAN_COMMAND_GRAB_JOB;
      (void)gearman_packet_pack_header(&(worker->grab_job));
      worker->options.grab_uniq= false;
    }

    if (options & GEARMAN_WORKER_GRAB_ALL)
    {
      worker->grab_job.command= GEARMAN_COMMAND_GRAB_JOB;
      (void)gearman_packet_pack_header(&(worker->grab_job));
      worker->options.grab_all= false;
    }

    if (options & GEARMAN_WORKER_IDENTIFIER)
    {
      worker->universal.identifier(NULL, 0);
    }
  }
}

int gearman_worker_timeout(gearman_worker_st *worker)
{
  if (worker and worker->impl())
  {
    return gearman_universal_timeout(worker->impl()->universal);
  }

  return 0;
}

void gearman_worker_set_timeout(gearman_worker_st *worker, int timeout)
{
  if (worker and worker->impl())
  {
    gearman_worker_add_options(worker, GEARMAN_WORKER_TIMEOUT_RETURN);
    gearman_universal_set_timeout(worker->impl()->universal, timeout);
  }
}

void *gearman_worker_context(const gearman_worker_st *worker)
{
  if (worker and worker->impl())
  {
    return worker->impl()->context;
  }

  return NULL;
}

void gearman_worker_set_context(gearman_worker_st *worker, void *context)
{
  if (worker and worker->impl())
  {
    worker->impl()->context= context;
  }
}

void gearman_worker_set_log_fn(gearman_worker_st *worker,
                               gearman_log_fn *function, void *context,
                               gearman_verbose_t verbose)
{
  if (worker and worker->impl())
  {
    gearman_set_log_fn(worker->impl()->universal, function, context, verbose);
  }
}

void gearman_worker_set_workload_malloc_fn(gearman_worker_st *worker,
                                           gearman_malloc_fn *function,
                                           void *context)
{
  if (worker and worker->impl())
  {
    gearman_set_workload_malloc_fn(worker->impl()->universal, function, context);
  }
}

void gearman_worker_set_workload_free_fn(gearman_worker_st *worker,
                                         gearman_free_fn *function,
                                         void *context)
{
  if (worker and worker->impl())
  {
    gearman_set_workload_free_fn(worker->impl()->universal, function, context);
  }
}

gearman_return_t gearman_worker_add_server(gearman_worker_st *worker,
                                           const char *host, in_port_t port)
{
  if (worker and worker->impl())
  {
    if (gearman_connection_create(worker->impl()->universal, host, port) == NULL)
    {
      return gearman_universal_error_code(worker->impl()->universal);
    }

    return GEARMAN_SUCCESS;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_worker_add_servers(gearman_worker_st *worker, const char *servers)
{
  if (worker and worker->impl())
  {
    return gearman_parse_servers(servers, _worker_add_server, worker);
  }

  return GEARMAN_INVALID_ARGUMENT;
}

void gearman_worker_remove_servers(gearman_worker_st *worker)
{
  if (worker and worker->impl())
  {
    gearman_free_all_cons(worker->impl()->universal);
  }
}

gearman_return_t gearman_worker_wait(gearman_worker_st *worker)
{
  if (worker and worker->impl())
  {
    return gearman_wait(worker->impl()->universal);
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_worker_register(gearman_worker_st *worker_shell,
                                         const char *function_name,
                                         uint32_t timeout)
{
  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();

    gearman_function_t null_func= gearman_function_create_null();
    return _worker_function_create(worker, function_name, strlen(function_name), null_func, timeout, NULL);
  }

  return GEARMAN_INVALID_ARGUMENT;
}

bool gearman_worker_function_exist(gearman_worker_st *worker_shell,
                                   const char *function_name,
                                   size_t function_length)
{
  if (worker_shell and worker_shell->impl())
  {
    struct _worker_function_st *function;
    Worker* worker= worker_shell->impl();

    function= _function_exist(worker, function_name, function_length);

    return (function && function->options.remove == false) ? true : false;
  }

  return false;
}

static inline gearman_return_t _worker_unregister(gearman_worker_st *worker_shell,
                                                  const char *function_name, size_t function_length)
{
  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();

    _worker_function_st *function= _function_exist(worker, function_name, function_length);

    if (function == NULL or function->options.remove)
    {
      return GEARMAN_NO_REGISTERED_FUNCTION;
    }

    if (function->options.packet_in_use)
    {
      gearman_packet_free(&(function->packet()));
      function->options.packet_in_use= false;
    }

    const void *args[1];
    size_t args_size[1];
    args[0]= function->name();
    args_size[0]= function->length();
    gearman_return_t ret= gearman_packet_create_args(worker->universal, function->packet(),
                                                     GEARMAN_MAGIC_REQUEST, GEARMAN_COMMAND_CANT_DO,
                                                     args, args_size, 1);
    if (gearman_failed(ret))
    {
      function->options.packet_in_use= false;
      return ret;
    }
    function->options.packet_in_use= true;

    function->options.change= true;
    function->options.remove= true;

    worker->options.change= true;

    return GEARMAN_SUCCESS;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_worker_unregister(gearman_worker_st *worker,
                                           const char *function_name)
{
  return _worker_unregister(worker, function_name, strlen(function_name));
}

gearman_return_t gearman_worker_unregister_all(gearman_worker_st *worker_shell)
{
  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();

    struct _worker_function_st *function;
    uint32_t count= 0;

    if (worker->function_list == NULL)
    {
      return GEARMAN_NO_REGISTERED_FUNCTIONS;
    }

    /* Lets find out if we have any functions left that are valid */
    for (function= worker->function_list; function;
         function= function->next)
    {
      if (function->options.remove == false)
      {
        count++;
      }
    }

    if (count == 0)
    {
      return GEARMAN_NO_REGISTERED_FUNCTIONS;
    }

    gearman_packet_free(&(worker->function_list->packet()));

    gearman_return_t ret= gearman_packet_create_args(worker->universal,
                                                     worker->function_list->packet(),
                                                     GEARMAN_MAGIC_REQUEST,
                                                     GEARMAN_COMMAND_RESET_ABILITIES,
                                                     NULL, NULL, 0);
    if (gearman_failed(ret))
    {
      worker->function_list->options.packet_in_use= false;

      return ret;
    }

    while (worker->function_list->next)
    {
      _worker_function_free(worker, worker->function_list->next);
    }

    worker->function_list->options.change= true;
    worker->function_list->options.remove= true;

    worker->options.change= true;

    return GEARMAN_SUCCESS;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

#if __GNUC__ >= 7
  #pragma GCC diagnostic warning "-Wimplicit-fallthrough"
#endif
gearman_job_st *gearman_worker_grab_job(gearman_worker_st *worker_shell,
                                        gearman_job_st *job,
                                        gearman_return_t *ret_ptr)
{
  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();
    struct _worker_function_st *function;
    uint32_t active;
    bool no_job= false;

    if (worker->in_work() == false)
    {
      worker->universal.reset_error();
    }

    gearman_return_t unused;
    if (ret_ptr == NULL)
    {
      ret_ptr= &unused;
    }

    *ret_ptr= GEARMAN_MAX_RETURN;

    if (worker->universal.con_list == NULL)
    {
      *ret_ptr= GEARMAN_NO_SERVERS;
      return NULL;
    }

    while (1)
    {
      switch (worker->state)
      {
        case GEARMAN_WORKER_STATE_START:
          /* If there are any new functions changes, send them now. */
          if (worker->options.change)
          {
            worker->function= worker->function_list;
            while (worker->function)
            {
              if (not (worker->function->options.change))
              {
                worker->function= worker->function->next;
                continue;
              }

              for (worker->con= (&worker->universal)->con_list; worker->con;
                   worker->con= worker->con->next_connection())
              {
                if (worker->con->socket_descriptor_is_valid() == false)
                {
                  continue;
                }

                case GEARMAN_WORKER_STATE_FUNCTION_SEND:
                *ret_ptr= worker->con->send_packet(worker->function->packet(), true);
                if (gearman_failed(*ret_ptr))
                {
                  if (*ret_ptr == GEARMAN_IO_WAIT)
                  {
                    worker->state= GEARMAN_WORKER_STATE_FUNCTION_SEND;
                  }
                  else if (*ret_ptr == GEARMAN_LOST_CONNECTION)
                  {
                    continue;
                  }

                  assert(*ret_ptr != GEARMAN_MAX_RETURN);
                  return NULL;
                }
              }

              if (worker->function->options.remove)
              {
                function= worker->function->prev;
                _worker_function_free(worker, worker->function);
                if (function == NULL)
                {
                  worker->function= worker->function_list;
                }
                else
                {
                  worker->function= function;
                }
              }
              else
              {
                worker->function->options.change= false;
                worker->function= worker->function->next;
              }
            }

            worker->options.change= false;
          }

          if (not worker->function_list)
          {
            gearman_error(worker->universal, GEARMAN_NO_REGISTERED_FUNCTIONS, "no functions have been registered");
            *ret_ptr= GEARMAN_NO_REGISTERED_FUNCTIONS;
            return NULL;
          }

          for (worker->con= (&worker->universal)->con_list; worker->con;
               worker->con= worker->con->next_connection())
          {
            /* If the connection to the job server is not active, start it. */
            if (worker->con->socket_descriptor_is_valid() == false)
            {
              for (worker->function= worker->function_list;
                   worker->function;
                   worker->function= worker->function->next)
              {
                case GEARMAN_WORKER_STATE_CONNECT:
                  *ret_ptr= worker->con->send_packet(worker->function->packet(), true);
                  if (gearman_failed(*ret_ptr))
                  {
                    if (*ret_ptr == GEARMAN_IO_WAIT)
                    {
                      worker->state= GEARMAN_WORKER_STATE_CONNECT;
                    }
                    else if (*ret_ptr == GEARMAN_COULD_NOT_CONNECT or *ret_ptr == GEARMAN_LOST_CONNECTION)
                    {
                      break;
                    }

                    assert(*ret_ptr != GEARMAN_MAX_RETURN);
                    return NULL;
                  }
              }

              if (*ret_ptr == GEARMAN_COULD_NOT_CONNECT)
              {
                continue;
              }
            }
            /* fall-thru */

            case GEARMAN_WORKER_STATE_GRAB_JOB_SEND:
            if (worker->con->socket_descriptor_is_valid() == false)
            {
              continue;
            }

            *ret_ptr= worker->con->send_packet(worker->grab_job, true);
            if (gearman_failed(*ret_ptr))
            {
              if (*ret_ptr == GEARMAN_IO_WAIT)
              {
                worker->state= GEARMAN_WORKER_STATE_GRAB_JOB_SEND;
              }
              else if (*ret_ptr == GEARMAN_LOST_CONNECTION)
              {
                continue;
              }

              assert(*ret_ptr != GEARMAN_MAX_RETURN);
              return NULL;
            }

            if (worker->job() == NULL)
            {
              assert(job == NULL);
              worker->job(gearman_job_create(worker, job));
              if (worker->job() == NULL)
              {
                *ret_ptr= GEARMAN_MEMORY_ALLOCATION_FAILURE;
                return NULL;
              }
              assert(worker->job()->impl());
            }

            while (1)
            {
              /* fall-thru */
              case GEARMAN_WORKER_STATE_GRAB_JOB_RECV:
                assert(worker);
                assert(worker->job());
                assert(worker->job()->impl());
                (void)worker->con->receiving(worker->job()->impl()->assigned, *ret_ptr, true);

                if (gearman_failed(*ret_ptr))
                {
                  if (*ret_ptr == GEARMAN_IO_WAIT)
                  {
                    worker->state= GEARMAN_WORKER_STATE_GRAB_JOB_RECV;
                  }
                  else
                  {
                    worker->job(NULL);

                    if (*ret_ptr == GEARMAN_LOST_CONNECTION)
                    {
                      break;
                    }
                  }

                  assert(*ret_ptr != GEARMAN_MAX_RETURN);
                  return NULL;
                }

                if (worker->job()->impl()->assigned.command == GEARMAN_COMMAND_NOOP)
                {
                  gearman_log_debug(worker->universal, "Received NOOP");
                }

                if (worker->job()->impl()->assigned.command == GEARMAN_COMMAND_JOB_ASSIGN or
                    worker->job()->impl()->assigned.command == GEARMAN_COMMAND_JOB_ASSIGN_ALL or
                    worker->job()->impl()->assigned.command == GEARMAN_COMMAND_JOB_ASSIGN_UNIQ)
                {
                  worker->job()->impl()->options.assigned_in_use= true;
                  worker->job()->impl()->con= worker->con;
                  worker->state= GEARMAN_WORKER_STATE_GRAB_JOB_SEND;
                  job= worker->take_job();

                  assert(*ret_ptr != GEARMAN_MAX_RETURN);
                  return job;
                }

                if (worker->job()->impl()->assigned.command == GEARMAN_COMMAND_NO_JOB or
                    worker->job()->impl()->assigned.command == GEARMAN_COMMAND_OPTION_RES)
                {
                  no_job= true;
                  gearman_packet_free(&(worker->job()->impl()->assigned));
                  break;
                }

                if (worker->job()->impl()->assigned.command != GEARMAN_COMMAND_NOOP)
                {
                  gearman_universal_set_error(worker->universal, GEARMAN_UNEXPECTED_PACKET, GEARMAN_AT,
                                              "unexpected packet:%s",
                                              gearman_command_info(worker->job()->impl()->assigned.command)->name);
                  gearman_packet_free(&(worker->job()->impl()->assigned));
                  worker->job(NULL);
                  *ret_ptr= GEARMAN_UNEXPECTED_PACKET;
                  return NULL;
                }

                gearman_packet_free(&(worker->job()->impl()->assigned));
            }
          }

          if (worker->in_work() == false and no_job)
          {
            *ret_ptr= GEARMAN_NO_JOBS;
            break;
          }
          /* fall-thru */

        case GEARMAN_WORKER_STATE_PRE_SLEEP:
          for (worker->con= (&worker->universal)->con_list; worker->con;
               worker->con= worker->con->next_connection())
          {
            if (worker->con->socket_descriptor_is_valid() == false)
            {
              continue;
            }

            *ret_ptr= worker->con->send_packet(worker->pre_sleep, true);
            if (gearman_failed(*ret_ptr))
            {
              if (*ret_ptr == GEARMAN_IO_WAIT)
              {
                worker->state= GEARMAN_WORKER_STATE_PRE_SLEEP;
              }
              else if (*ret_ptr == GEARMAN_LOST_CONNECTION)
              {
                continue;
              }

              assert(*ret_ptr != GEARMAN_MAX_RETURN);
              return NULL;
            }
          }

          worker->state= GEARMAN_WORKER_STATE_START;

          /* Set a watch on all active connections that we sent a PRE_SLEEP to. */
          active= 0;
          for (worker->con= worker->universal.con_list; worker->con; worker->con= worker->con->next_connection())
          {
            if (worker->con->socket_descriptor_is_valid())
            {
              worker->con->set_events(POLLIN);
              active++;
            }
          }

          if ((&worker->universal)->options.non_blocking)
          {
            *ret_ptr= GEARMAN_NO_JOBS;
            return NULL;
          }

          if (active == 0)
          {
            if (*ret_ptr == GEARMAN_COULD_NOT_CONNECT)
            {
              return NULL;
            }
            else if (worker->universal.timeout < 0)
            {
              gearman_nap(GEARMAN_WORKER_WAIT_TIMEOUT);
            }
            else
            {
              if (worker->universal.timeout > 0)
              {
                gearman_nap(worker->universal);
              }

              if (worker->options.timeout_return)
              {
                *ret_ptr= gearman_error(worker->universal, GEARMAN_TIMEOUT, "Option timeout return reached");

                return NULL;
              }
            }
          }
          else
          {
            *ret_ptr= gearman_wait(worker->universal);
            if (gearman_failed(*ret_ptr) and (*ret_ptr != GEARMAN_TIMEOUT or worker->options.timeout_return))
            {
              assert(*ret_ptr != GEARMAN_MAX_RETURN);
              return NULL;
            }
          }

          break;
      }

      if (*ret_ptr == GEARMAN_NO_JOBS)
      {
        break;
      }
    }
  }

  assert(*ret_ptr != GEARMAN_MAX_RETURN);
  return NULL;
}

void gearman_job_free_all(gearman_worker_st *worker_shell)
{
  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();

    while (worker->job_list)
    {
      gearman_job_free(worker->job_list->shell());
    }
  }
}

gearman_return_t gearman_worker_add_function(gearman_worker_st *worker_shell,
                                             const char *function_name,
                                             uint32_t timeout,
                                             gearman_worker_fn *worker_fn,
                                             void *context)
{
  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();

    if (function_name == NULL)
    {
      return gearman_error(worker->universal, GEARMAN_INVALID_ARGUMENT, "function name not given");
    }

    if (worker_fn == NULL)
    {
      return gearman_error(worker->universal, GEARMAN_INVALID_ARGUMENT, "function not given");
    }
    gearman_function_t local= gearman_function_create_v1(worker_fn);

    return _worker_function_create(worker,
                                   function_name, strlen(function_name),
                                   local,
                                   timeout,
                                   context);
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_worker_define_function(gearman_worker_st *worker_shell,
                                                const char *function_name, const size_t function_name_length,
                                                const gearman_function_t function,
                                                const uint32_t timeout,
                                                void *context)
{
  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();

    if (function_name == NULL or function_name_length == 0)
    {
      return gearman_error(worker->universal, GEARMAN_INVALID_ARGUMENT, "function name not given");
    }

    return _worker_function_create(worker,
                                   function_name, function_name_length,
                                   function,
                                   timeout,
                                   context);
  }

  return GEARMAN_INVALID_ARGUMENT;
}

void gearman_worker_reset_error(Worker& worker)
{
  universal_reset_error(worker.universal);
}

void gearman_worker_reset_error(gearman_worker_st *worker)
{
  if (worker and worker->impl())
  {
    universal_reset_error(worker->impl()->universal);
  }
}

#if __GNUC__ >= 7
  #pragma GCC diagnostic warning "-Wimplicit-fallthrough"
#endif
gearman_return_t gearman_worker_work(gearman_worker_st *worker_shell)
{
  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();
    bool shutdown= false;

    universal_reset_error(worker->universal);

    switch (worker->work_state)
    {
      case GEARMAN_WORKER_WORK_UNIVERSAL_GRAB_JOB:
        {
          worker->in_work(true);
          gearman_return_t ret;
          worker->work_job(gearman_worker_grab_job(worker->shell(), NULL, &ret));
          worker->in_work(false);

          if (gearman_failed(ret))
          {
            if (ret == GEARMAN_COULD_NOT_CONNECT)
            {
              worker->universal.reset();
            }
            return ret;
          }
          assert(worker->has_work_job());

          for (worker->work_function= worker->function_list;
               worker->work_function;
               worker->work_function= worker->work_function->next)
          {
            if (strcmp(gearman_job_function_name(worker->work_job()), worker->work_function->name()) == 0)
            {
              break;
            }
          }

          if (not worker->work_function)
          {
            worker->work_job(NULL);
            return gearman_error(worker->universal, GEARMAN_INVALID_FUNCTION_NAME, "Function not found");
          }

          if (not worker->work_function->has_callback())
          {
            worker->work_job(NULL);
            return gearman_error(worker->universal, GEARMAN_INVALID_FUNCTION_NAME, "Neither a gearman_worker_fn, or gearman_function_fn callback was supplied");
          }

          worker->work_result_size= 0;
        }
        /* fall-thru */

      case GEARMAN_WORKER_WORK_UNIVERSAL_FUNCTION:
        {
          switch (worker->work_function->callback(worker->work_job(),
                                                  static_cast<void *>(worker->work_function->context())))
          {
            case GEARMAN_FUNCTION_FATAL:
              if (gearman_job_send_fail_fin(worker->work_job()->impl()) == GEARMAN_LOST_CONNECTION) // If we fail this, we have no connection, @note this causes us to lose the current error
              {
                worker->work_job()->impl()->_error_code= GEARMAN_LOST_CONNECTION;
                break;
              }
              worker->work_state= GEARMAN_WORKER_WORK_UNIVERSAL_FAIL;
              return worker->work_job()->impl()->_error_code;

            case GEARMAN_FUNCTION_ERROR: // retry 
              worker->universal.reset();
              worker->work_job()->impl()->_error_code= GEARMAN_LOST_CONNECTION;
              break;

            case GEARMAN_FUNCTION_SHUTDOWN:
              shutdown= true;

            case GEARMAN_FUNCTION_SUCCESS:
              break;
          }

          if (worker->work_job()->impl()->_error_code == GEARMAN_LOST_CONNECTION)
          {
            break;
          }
        }
        /* fall-thru */

      case GEARMAN_WORKER_WORK_UNIVERSAL_COMPLETE:
        {
          worker->work_job()->impl()->_error_code= gearman_job_send_complete_fin(worker->work_job()->impl(),
                                                                              worker->work_result, worker->work_result_size);
          if (worker->work_job()->impl()->_error_code == GEARMAN_IO_WAIT)
          {
            worker->work_state= GEARMAN_WORKER_WORK_UNIVERSAL_COMPLETE;
            return gearman_error(worker->universal, worker->work_job()->impl()->_error_code,
                                 "A failure occurred after worker had successful complete, unless gearman_job_send_complete() was called directly by worker, client has not been informed of success.");
          }

          if (worker->work_result)
          {
            gearman_free(worker->universal, worker->work_result);
            worker->work_result= NULL;
          }

          // If we lost the connection, we retry the work, otherwise we error
          if (worker->work_job()->impl()->_error_code == GEARMAN_LOST_CONNECTION)
          {
            break;
          }
          else if (worker->work_job()->impl()->_error_code == GEARMAN_SHUTDOWN)
          { }
          else if (gearman_failed(worker->work_job()->impl()->_error_code))
          {
            worker->work_state= GEARMAN_WORKER_WORK_UNIVERSAL_FAIL;

            return worker->work_job()->impl()->_error_code;
          }
        }
        break;

      case GEARMAN_WORKER_WORK_UNIVERSAL_FAIL:
        {
          if (gearman_failed(worker->work_job()->impl()->_error_code= gearman_job_send_fail_fin(worker->work_job()->impl())))
          {
            if (worker->work_job()->impl()->_error_code == GEARMAN_LOST_CONNECTION)
            {
              break;
            }

            return worker->work_job()->impl()->_error_code;
          }
        }
        break;
    }

    worker->work_job(NULL);

    worker->work_state= GEARMAN_WORKER_WORK_UNIVERSAL_GRAB_JOB;

    if (shutdown)
    {
      return GEARMAN_SHUTDOWN;
    }

    return GEARMAN_SUCCESS;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_worker_echo(gearman_worker_st *worker,
                                     const void *workload,
                                     size_t workload_size)
{
  if (worker and worker->impl())
  {
    return gearman_echo(worker->impl()->universal, workload, workload_size);
  }

  return GEARMAN_INVALID_ARGUMENT;
}

/*
 * Static Definitions
 */

static gearman_worker_st *_worker_allocate(gearman_worker_st *worker_shell, bool is_clone)
{
  Worker *worker= new (std::nothrow) Worker(worker_shell);
  if (worker)
  {
    if (is_clone == false)
    {
      if (getenv("GEARMAN_SERVERS"))
      {
        if (gearman_worker_add_servers(worker->shell(), getenv("GEARMAN_SERVERS")))
        {
          gearman_worker_free(worker->shell());
          return NULL;
        }
      }
    }

    if (worker->universal.wakeup(true) == false)
    {
      delete worker;
      return NULL;
    }

    return worker->shell();
  }
#if defined(DEBUG) && DEBUG
  perror("new Worker");
#endif

  return NULL;
}

static gearman_return_t _worker_packet_init(Worker* worker)
{
  gearman_return_t ret= gearman_packet_create_args(worker->universal, worker->grab_job,
                                                   GEARMAN_MAGIC_REQUEST, GEARMAN_COMMAND_GRAB_JOB_ALL,
                                                   NULL, NULL, 0);
  if (gearman_failed(ret))
  {
    return ret;
  }

  ret= gearman_packet_create_args(worker->universal, worker->pre_sleep,
                                  GEARMAN_MAGIC_REQUEST, GEARMAN_COMMAND_PRE_SLEEP,
                                  NULL, NULL, 0);
  if (gearman_failed(ret))
  {
    gearman_packet_free(&(worker->grab_job));
    return ret;
  }

  worker->options.packet_init= true;

  return GEARMAN_SUCCESS;
}

static gearman_return_t _worker_add_server(const char *host, in_port_t port, void *context)
{
  return gearman_worker_add_server(static_cast<gearman_worker_st *>(context), host, port);
}

static gearman_return_t _worker_function_create(Worker *worker,
                                                const char *function_name_, const size_t function_length_,
                                                const gearman_function_t &function_arg,
                                                uint32_t timeout,
                                                void *context)
{
  const void *args[2];
  size_t args_size[2];

  if (function_length_ == 0 or function_name_ == NULL or function_length_ > GEARMAN_FUNCTION_MAX_SIZE)
  {
    if (function_length_ > GEARMAN_FUNCTION_MAX_SIZE)
    {
      gearman_error(worker->universal, GEARMAN_INVALID_ARGUMENT, "function name longer then GEARMAN_MAX_FUNCTION_SIZE");
    } 
    else
    {
      gearman_error(worker->universal, GEARMAN_INVALID_ARGUMENT, "invalid function");
    }

    return GEARMAN_INVALID_ARGUMENT;
  }

  _worker_function_st *function= make(worker->universal._namespace,
                                      function_name_, function_length_,
                                      function_arg, context, timeout);
  if (function == NULL)
  {
    gearman_perror(worker->universal, errno, "_worker_function_st::new()");
    return GEARMAN_MEMORY_ALLOCATION_FAILURE;
  }

  gearman_return_t ret;
  if (timeout > 0)
  {
    char timeout_buffer[GEARMAN_MAXIMUM_INTEGER_DISPLAY_LENGTH +1];
    snprintf(timeout_buffer, sizeof(timeout_buffer), "%u", timeout);
    timeout_buffer[GEARMAN_MAXIMUM_INTEGER_DISPLAY_LENGTH]= 0;
    args[0]= function->name();
    args_size[0]= function->length() + 1;
    args[1]= timeout_buffer;
    args_size[1]= strlen(timeout_buffer);
    ret= gearman_packet_create_args(worker->universal, function->packet(),
                                    GEARMAN_MAGIC_REQUEST,
                                    GEARMAN_COMMAND_CAN_DO_TIMEOUT,
                                    args, args_size, 2);
  }
  else
  {
    args[0]= function->name();
    args_size[0]= function->length();
    ret= gearman_packet_create_args(worker->universal, function->packet(),
                                    GEARMAN_MAGIC_REQUEST, GEARMAN_COMMAND_CAN_DO,
                                    args, args_size, 1);
  }

  if (gearman_failed(ret))
  {
    delete function;

    return ret;
  }

  if (worker->function_list)
  {
    worker->function_list->prev= function;
  }

  function->next= worker->function_list;
  function->prev= NULL;
  worker->function_list= function;
  worker->function_count++;

  worker->options.change= true;

  return GEARMAN_SUCCESS;
}

static void _worker_function_free(Worker* worker,
                                  struct _worker_function_st *function)
{
  if (worker->function_list == function)
  {
    worker->function_list= function->next;
  }

  if (function->prev)
  {
    function->prev->next= function->next;
  }

  if (function->next)
  {
    function->next->prev= function->prev;
  }
  worker->function_count--;

  delete function;
}

gearman_return_t gearman_worker_set_memory_allocators(gearman_worker_st *worker,
                                                      gearman_malloc_fn *malloc_fn,
                                                      gearman_free_fn *free_fn,
                                                      gearman_realloc_fn *realloc_fn,
                                                      gearman_calloc_fn *calloc_fn,
                                                      void *context)
{
  if (worker and worker->impl())
  {
    return gearman_set_memory_allocator(worker->impl()->universal.allocator, malloc_fn, free_fn, realloc_fn, calloc_fn, context);
  }

  return GEARMAN_INVALID_ARGUMENT;
}

bool gearman_worker_set_server_option(gearman_worker_st *worker_shell, const char *option_arg, size_t option_arg_size)
{
  if (worker_shell and worker_shell->impl())
  {
    Worker* worker= worker_shell->impl();
    gearman_string_t option= { option_arg, option_arg_size };

    if (gearman_success(gearman_server_option(worker->universal, option)))
    {
      if (gearman_request_option(worker->universal, option))
      {
        return true;
      }
    }
  }

  return false;
}

void gearman_worker_set_namespace(gearman_worker_st *self, const char *namespace_key, size_t namespace_key_size)
{
  if (self and self->impl())
  {
    gearman_universal_set_namespace(self->impl()->universal, namespace_key, namespace_key_size);
  }
}

gearman_id_t gearman_worker_id(gearman_worker_st *self)
{
  if (self == NULL)
  {
    gearman_id_t handle= { INVALID_SOCKET, INVALID_SOCKET };
    return handle;
  }

  return gearman_universal_id(self->impl()->universal);
}

gearman_return_t gearman_worker_set_identifier(gearman_worker_st *worker,
                                               const char *id, size_t id_size)
{
  if (worker and worker->impl())
  {
    return gearman_set_identifier(worker->impl()->universal, id, id_size);
  }

  return GEARMAN_INVALID_ARGUMENT;
}

const char *gearman_worker_namespace(gearman_worker_st* worker)
{
  if (worker and worker->impl())
  {
    return gearman_univeral_namespace(worker->impl()->universal);
  }

  return NULL;
}

gearman_job_st* Worker::take_job()
{
  gearman_job_st* tmp= _job->shell();
  _job= NULL;
  return tmp;
}

gearman_job_st* Worker::job()
{
  if (_job)
  {
    return _job->shell();
  }

  return NULL;
}

void Worker::job(gearman_job_st* job_)
{
  if (job_)
  {
    assert(job_->impl());
    assert(_job == NULL);
    _job= job_->impl();
  }
  else if (_job)
  {
    gearman_job_free(_job->shell());
    _job= NULL;
  }
}

gearman_job_st* Worker::work_job()
{
  assert(_work_job);
  return _work_job->shell();
}

void Worker::work_job(gearman_job_st* work_job_)
{
  if (work_job_)
  {
    assert(_work_job == NULL);
    _work_job= work_job_->impl();
  }
  else if (_work_job)
  {
    gearman_job_free(_work_job->shell());
    _work_job= NULL;
  }
}
