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
#include "libgearman/vector.h"
#include "libgearman/protocol/work_exception.h"

#include <cstdio>
#include <cstring>
#include <memory>

struct gearman_job_reducer_st {
  gearman_universal_st &universal;
  gearman_client_st *client;
  gearman_result_st result;
  gearman_vector_st *reducer_function;
  gearman_aggregator_fn *aggregator_fn;

  gearman_job_reducer_st(gearman_universal_st &universal_arg,
                         const gearman_string_t &reducer_function_name,
                         gearman_aggregator_fn *aggregator_fn_arg):
    universal(universal_arg),
    client(NULL),
    reducer_function(NULL),
    aggregator_fn(aggregator_fn_arg)
  {
    assert_msg(gearman_size(reducer_function_name), "Trying to creat a function with zero length");
    reducer_function= gearman_string_create(NULL, gearman_size(reducer_function_name));
    gearman_string_append(reducer_function, gearman_string_param(reducer_function_name));
  }

  const char* name() const
  {
    if (reducer_function)
    {
      return reducer_function->c_str();
    }

    return "__UNKNOWN";
  }

  ~gearman_job_reducer_st() 
  {
    gearman_client_free(client);
    gearman_string_free(reducer_function);
  }

  bool init()
  {
    client= gearman_client_create(NULL);
    if (client)
    {
      gearman_universal_clone(client->impl()->universal, universal);
#if 0
      if (universal._namespace)
      {
        gearman_client_set_namespace(client, 
                                     gearman_string_value(universal._namespace),
                                     gearman_string_length(universal._namespace));
      }

      for (gearman_connection_st *con= universal.con_list; con; con= con->next_connection())
      {
        if (gearman_failed(client->impl()->add_server(con->_host, con->_service)))
        {
          return false;
        }
      }
#endif

      return true;
    }

    return false;
  }

  bool add(gearman_argument_t &arguments)
  {
    gearman_string_t function= gearman_string(reducer_function);
    gearman_unique_t unique= gearman_unique_make(0, 0);
    gearman_task_st *task= add_task(*(client->impl()),
                                    NULL,
                                    GEARMAN_COMMAND_SUBMIT_JOB,
                                    function,
                                    unique,
                                    arguments.value,
                                    time_t(0),
                                    gearman_actions_execute_defaults());
    if (task == NULL)
    {
      gearman_universal_error_code(client->impl()->universal);

      return false;
    }

    return true;
  }

  gearman_return_t complete()
  {
    gearman_return_t rc;
    if (gearman_failed(rc= gearman_client_run_tasks(client)))
    {
      return rc;
    }

    gearman_task_st *check_task= client->impl()->task_list;

    if (check_task)
    {
      do
      {
        if (gearman_failed(check_task->impl()->error_code()))
        {
          return check_task->impl()->error_code();
        }
      } while ((check_task= gearman_next(check_task)));

      if (aggregator_fn)
      {
        gearman_aggregator_st aggregator(client->impl()->context);
        aggregator_fn(&aggregator, client->impl()->task_list, &result);
      }
    }

    return GEARMAN_SUCCESS;
  }
};

/**
 * @addtogroup gearman_job_static Static Job Declarations
 * @ingroup gearman_job
 * @{
 */

/**
 * Send a packet for a job.
 */
static gearman_return_t _job_send(Job* job);

/*
 * Public Definitions
 */

gearman_job_st *gearman_job_create(Worker* worker, gearman_job_st *job_shell)
{
  assert(worker);
  if (worker)
  {
    Job* job;
    assert(job_shell == NULL);
    if (job_shell)
    {
      job= job_shell->impl();
      assert(job);
    }
    else
    {
      job= new (std::nothrow) Job(job_shell, *(worker));

      if (job == NULL)
      {
        gearman_error(worker->universal, GEARMAN_MEMORY_ALLOCATION_FAILURE, "new failed for Job");
        return NULL;
      }
    }

    job->reducer= NULL;
    job->_error_code= GEARMAN_UNKNOWN_STATE;

    if (job->_worker.job_list)
    {
      job->_worker.job_list->prev= job;
    }
    job->next= job->_worker.job_list;
    job->prev= NULL;
    job->_worker.job_list= job;
    job->_worker.job_count++;

    job->con= NULL;

    return job->shell();
  }

  return NULL;
}

Job::Job(gearman_job_st* shell_, Worker& worker_):
  _worker(worker_),
  _client(NULL),
  next(NULL),
  prev(NULL),
  con(NULL),
  reducer(NULL),
  _error_code(GEARMAN_UNKNOWN_STATE),
  _shell(shell_)
{
  if (shell_)
  {
    gearman_set_allocated(_shell, false);
  }
  else
  {
    _shell= &_owned_shell;
    gearman_set_allocated(_shell, true);
  }

  _shell->impl(this);
  gearman_set_initialized(_shell, true);
}

Job::~Job()
{
  if (_client)
  {
    gearman_client_free(_client);
  }

  delete reducer;
}

gearman_client_st* Job::client()
{
  if (_client == NULL)
  {
    _client= gearman_client_create(NULL);
    if (_client)
    {
      gearman_universal_clone(_client->impl()->universal, _worker.universal);
    }
  }

  return _client;
}

bool gearman_job_build_reducer(Job* job, gearman_aggregator_fn *aggregator_fn)
{
  if (job->reducer)
  {
    return true;
  }

  gearman_string_t reducer_func= gearman_job_reducer_string(job);

  job->reducer= new (std::nothrow) gearman_job_reducer_st(job->universal(), reducer_func, aggregator_fn);
  if (job->reducer == NULL)
  {
    gearman_job_free(job->shell());
    return false;
  }

  if (job->reducer->init() == false)
  {
    gearman_job_free(job->shell());
    return false;
  }

  return true;
}


gearman_worker_st *gearman_job_clone_worker(gearman_job_st *job_shell)
{
  if (job_shell and job_shell->impl())
  {
    return gearman_worker_clone(NULL, job_shell->impl()->_worker.shell());
  }

  return NULL;
}

gearman_client_st *gearman_job_use_client(gearman_job_st *job_shell)
{
  if (job_shell and job_shell->impl())
  {
    return job_shell->impl()->client();
  }

  return NULL;
}

gearman_return_t gearman_job_send_data(gearman_job_st *job_shell, const void *data, size_t data_size)
{
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    if (job->finished() == false)
    {
      const void *args[2];
      size_t args_size[2];

      if (job->reducer)
      {
        gearman_argument_t value= gearman_argument_make(NULL, 0, static_cast<const char *>(data), data_size);
        job->reducer->add(value);

        return GEARMAN_SUCCESS;
      }

      if ((job->options.work_in_use) == false)
      {
        args[0]= job->assigned.arg[0];
        args_size[0]= job->assigned.arg_size[0];
        args[1]= data;
        args_size[1]= data_size;
        gearman_return_t ret= gearman_packet_create_args(job->universal(), job->work,
                                                         GEARMAN_MAGIC_REQUEST,
                                                         GEARMAN_COMMAND_WORK_DATA,
                                                         args, args_size, 2);
        if (gearman_failed(ret))
        {
          return ret;
        }

        job->options.work_in_use= true;
      }

      return _job_send(job);
    }

    return GEARMAN_SUCCESS;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_job_send_warning(gearman_job_st *job_shell,
                                          const void *warning,
                                          size_t warning_size)
{
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    if (job->finished() == false)
    {
      const void *args[2];
      size_t args_size[2];

      if ((job->options.work_in_use) == false)
      {
        args[0]= job->assigned.arg[0];
        args_size[0]= job->assigned.arg_size[0];
        args[1]= warning;
        args_size[1]= warning_size;

        gearman_return_t ret;
        ret= gearman_packet_create_args(job->universal(), job->work,
                                        GEARMAN_MAGIC_REQUEST,
                                        GEARMAN_COMMAND_WORK_WARNING,
                                        args, args_size, 2);
        if (gearman_failed(ret))
        {
          return ret;
        }

        job->options.work_in_use= true;
      }

      return _job_send(job);
    }

    return GEARMAN_SUCCESS;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_job_send_status(gearman_job_st *job_shell,
                                         uint32_t numerator,
                                         uint32_t denominator)
{
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    if (job->finished() == false)
    {
      char numerator_string[12];
      char denominator_string[12];
      const void *args[3];
      size_t args_size[3];

      if (not (job->options.work_in_use))
      {
        snprintf(numerator_string, 12, "%u", numerator);
        snprintf(denominator_string, 12, "%u", denominator);

        args[0]= job->assigned.arg[0];
        args_size[0]= job->assigned.arg_size[0];
        args[1]= numerator_string;
        args_size[1]= strlen(numerator_string) + 1;
        args[2]= denominator_string;
        args_size[2]= strlen(denominator_string);

        gearman_return_t ret;
        ret= gearman_packet_create_args(job->universal(), job->work,
                                        GEARMAN_MAGIC_REQUEST,
                                        GEARMAN_COMMAND_WORK_STATUS,
                                        args, args_size, 3);
        if (gearman_failed(ret))
        {
          return ret;
        }

        job->options.work_in_use= true;
      }

      return _job_send(job);
    }

    return GEARMAN_SUCCESS;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_job_send_complete(gearman_job_st *job_shell,
                                           const void *result,
                                           size_t result_size)
{
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    if (job->finished() == false)
    {
      if (job->reducer)
      {
        return GEARMAN_INVALID_ARGUMENT;
      }

      return gearman_job_send_complete_fin(job, result, result_size);
    }

    return GEARMAN_SUCCESS;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_job_send_complete_fin(Job* job,
                                               const void *result, size_t result_size)
{
  if (job->finished() == false)
  {
    if (job->reducer)
    {
      if (result_size)
      {
        gearman_argument_t value= gearman_argument_make(NULL, 0, static_cast<const char *>(result), result_size);
        job->reducer->add(value);
      }

      gearman_return_t rc= job->reducer->complete();
      if (gearman_failed(rc))
      {
        return gearman_universal_set_error(job->universal(), rc, GEARMAN_AT, "%s couldn't call complete()", job->reducer->name());
      }

      const gearman_vector_st *reduced_value= job->reducer->result.string();
      if (reduced_value)
      {
        result= gearman_string_value(reduced_value);
        result_size= gearman_string_length(reduced_value);
      }
      else
      {
        result= NULL;
        result_size= 0;
      }
    } 

    const void *args[2];
    size_t args_size[2];

    if (not (job->options.work_in_use))
    {
      args[0]= job->assigned.arg[0];
      args_size[0]= job->assigned.arg_size[0];

      args[1]= result;
      args_size[1]= result_size;
      gearman_return_t ret= gearman_packet_create_args(job->_worker.universal, job->work,
                                                       GEARMAN_MAGIC_REQUEST,
                                                       GEARMAN_COMMAND_WORK_COMPLETE,
                                                       args, args_size, 2);
      if (gearman_failed(ret))
      {
        return ret;
      }
      job->options.work_in_use= true;
    }

    gearman_return_t ret= _job_send(job);
    if (gearman_failed(ret))
    {
      return ret;
    }
    job->finished(true);
  }

  return GEARMAN_SUCCESS;
}

gearman_return_t gearman_job_send_exception(gearman_job_st *job_shell,
                                            const void *exception,
                                            size_t exception_size)
{
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    if (exception == NULL or exception_size == 0)
    {
      return gearman_error(job->universal(), GEARMAN_INVALID_ARGUMENT, "No exception was provided");
    }

    if (job->finished() == false)
    {
      if (job->options.work_in_use == false)
      {
        gearman_string_t handle_string= { static_cast<const char *>(job->assigned.arg[0]), job->assigned.arg_size[0] };
        gearman_string_t exception_string= { static_cast<const char *>(exception), exception_size };

        gearman_return_t ret= libgearman::protocol::work_exception(job->_worker.universal, job->work, handle_string, exception_string);
        if (gearman_failed(ret))
        {
          return ret;
        }

        job->options.work_in_use= true;
      }

      if (gearman_failed(_job_send(job)))
      {
        return job->error_code();
      }
      job->finished(true);
    }

    return GEARMAN_SUCCESS;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_job_send_fail(gearman_job_st *job_shell)
{
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    if (job->finished() == false)
    {
      if (job->reducer)
      {
        return gearman_error(job->universal(), GEARMAN_INVALID_ARGUMENT, "Job has a reducer");
      }

      return gearman_job_send_fail_fin(job);
    }

    return GEARMAN_SUCCESS;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

gearman_return_t gearman_job_send_fail_fin(Job* job)
{
  assert(job);
  if (job)
  {
    const void *args[1];
    size_t args_size[1];

    if (job->finished() == false)
    {
      if (not (job->options.work_in_use))
      {
        args[0]= job->assigned.arg[0];
        args_size[0]= job->assigned.arg_size[0] - 1;
        gearman_return_t ret= gearman_packet_create_args(job->_worker.universal, job->work,
                                                         GEARMAN_MAGIC_REQUEST,
                                                         GEARMAN_COMMAND_WORK_FAIL,
                                                         args, args_size, 1);
        if (gearman_failed(ret))
        {
          return ret;
        }

        job->options.work_in_use= true;
      }

      gearman_return_t ret= _job_send(job);
      if (gearman_failed(ret))
      {
        return ret;
      }

      job->finished(true);
    }

    return GEARMAN_SUCCESS;
  }

  return GEARMAN_INVALID_ARGUMENT;
}

const char *gearman_job_handle(const gearman_job_st *job_shell)
{
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    return static_cast<const char *>(job->assigned.arg[0]);
  }

  return NULL;
}

const char *gearman_job_function_name(const gearman_job_st *job_shell)
{
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    return static_cast<char *>(job->assigned.arg[1]);
  }
  return NULL;
}

gearman_string_t gearman_job_function_name_string(const Job* job)
{
  assert(job);
  if (job)
  {
    gearman_string_t temp= { job->assigned.arg[1], job->assigned.arg_size[1] };
    return temp;
  }

  static gearman_string_t ret= {0, 0};
  return ret;
}

const char *gearman_job_unique(const gearman_job_st *job_shell)
{
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    if (job->assigned.command == GEARMAN_COMMAND_JOB_ASSIGN_UNIQ or
        job->assigned.command == GEARMAN_COMMAND_JOB_ASSIGN_ALL)
    {
      return static_cast<const char *>(job->assigned.arg[2]);
    }

    return "";
  }

  return NULL;
}

bool gearman_job_is_map(const Job* job)
{
  assert(job);
  return bool(job->assigned.command == GEARMAN_COMMAND_JOB_ASSIGN_ALL) and job->assigned.arg_size[3] > 1;
}

gearman_string_t gearman_job_reducer_string(const Job* job)
{
  if (job)
  {
    if (job->assigned.command == GEARMAN_COMMAND_JOB_ASSIGN_ALL and job->assigned.arg_size[3] > 1)
    {
      gearman_string_t temp= { job->assigned.arg[3], job->assigned.arg_size[3] -1 };
      return temp;
    }

    static gearman_string_t null_temp= { gearman_literal_param("") };

    return null_temp;
  }

  static gearman_string_t ret= {0, 0};
  return ret;
}

const char *gearman_job_reducer(const gearman_job_st *job_shell)
{
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    if (job->assigned.command == GEARMAN_COMMAND_JOB_ASSIGN_ALL)
    {
      return static_cast<const char *>(job->assigned.arg[3]);
    }

    return "";
  }

  return NULL;
}

const void *gearman_job_workload(const gearman_job_st *job_shell)
{
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    return job->assigned.data;
  }

  return NULL;
}

size_t gearman_job_workload_size(const gearman_job_st *job_shell)
{
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    return job->assigned.data_size;
  }

  return 0;
}

void *gearman_job_take_workload(gearman_job_st *job_shell, size_t *data_size)
{
  if (job_shell and job_shell->impl())
  {
    return gearman_packet_take_data(job_shell->impl()->assigned, data_size);
  }

  return NULL;
}

void gearman_job_free(gearman_job_st *job_shell)
{
#ifndef NDEBUG
  if (job_shell)
  {
    assert(job_shell->impl());
  }
#endif
  if (job_shell and job_shell->impl())
  {
    Job* job= job_shell->impl();

    if (job->options.assigned_in_use)
    {
      gearman_packet_free(&(job->assigned));
    }

    if (job->options.work_in_use)
    {
      gearman_packet_free(&(job->work));
    }

    if (job->_worker.job_list == job)
    {
      job->_worker.job_list= job->next;
    }

    if (job->prev)
    {
      job->prev->next= job->next;
    }

    if (job->next)
    {
      job->next->prev= job->prev;
    }
    job->_worker.job_count--;

    delete job;
  }
  else if (job_shell)
  {
    assert(job_shell->impl());
  }
}

/*
 * Static Definitions
 */

static gearman_return_t _job_send(Job *job)
{
  gearman_return_t ret= job->con->send_packet(job->work, true);

  while ((ret == GEARMAN_IO_WAIT) or (ret == GEARMAN_TIMEOUT))
  {
    ret= gearman_wait(job->universal());
    if (ret == GEARMAN_SUCCESS)
    {
      ret= job->con->send_packet(job->work, true);
    }
  }

  if (gearman_failed(ret))
  {
    return ret;
  }

  gearman_packet_free(&(job->work));
  job->options.work_in_use= false;

  return GEARMAN_SUCCESS;
}

const char *gearman_job_error(gearman_job_st *job_shell)
{
  if (job_shell and job_shell->impl())
  {
    return job_shell->impl()->_worker.error();
  }

  return NULL;
}
