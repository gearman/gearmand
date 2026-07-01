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
 * @brief Task Definitions
 */

#include "gear_config.h"
#include <libgearman/common.h>

#include "libgearman/assert.hpp"

#include <cerrno>
#include <cstring>
#include <memory>

/*
 * Public Definitions
 */

gearman_task_st *gearman_task_internal_create(Client* client, gearman_task_st *task_shell)
{
  Task* task= new (std::nothrow) Task(client, task_shell);
  if (task)
  {
    return task->shell();
  }

  gearman_perror(client->universal, errno, "gearman_task_st new");
  gearman_task_free(task_shell);

  return NULL;
}

void gearman_task_free(Task* task)
{
  gearman_task_free(task->shell());
}

void gearman_task_free(gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    Task* task= task_shell->impl();
    assert(task_shell == task->shell());
    {
      assert(task->magic_ != TASK_ANTI_MAGIC);
      assert(task->magic_ == TASK_MAGIC);
      task->magic_= TASK_ANTI_MAGIC;
      if (task->client)
      {
        if (task->options.send_in_use)
        {
          gearman_packet_free(&(task->send));
        }

        if (task->type != GEARMAN_TASK_KIND_DO  and task->context and  task->client->task_context_free_fn)
        {
          task->client->task_context_free_fn(task_shell, static_cast<void *>(task->context));
        }

        if (task->client->task_list == task_shell)
        {
          task->client->task_list= task->next;
        }

        if (task->client->task_list_tail == task_shell)
        {
          task->client->task_list_tail= task->prev;
        }

        if (task->prev)
        {
          task->prev->impl()->next= task->next;
        }

        if (task->next)
        {
          task->next->impl()->prev= task->prev;
        }

        task->client->task_count--;

        // If the task we are removing is a current task, remove it from the client
        // structures.
        if (task->client->task == task_shell)
        {
          task->client->task= NULL;
        }
        task->client= NULL;
      }
      task->job_handle[0]= 0;

      task_shell->_impl= NULL;

      delete task;
    }
  }
  else if (task_shell)
  {
    task_shell->_impl= NULL;
  }
}

bool gearman_task_is_active(const gearman_task_st *task_shell)
{
  assert(task_shell);
  assert(task_shell->impl());
  if (task_shell and task_shell->impl())
  {
    switch (task_shell->impl()->state)
    {
    case GEARMAN_TASK_STATE_NEW:
    case GEARMAN_TASK_STATE_SUBMIT:
    case GEARMAN_TASK_STATE_WORKLOAD:
    case GEARMAN_TASK_STATE_WORK:
    case GEARMAN_TASK_STATE_CREATED:
    case GEARMAN_TASK_STATE_DATA:
    case GEARMAN_TASK_STATE_WARNING:
    case GEARMAN_TASK_STATE_STATUS:
      return true;

    case GEARMAN_TASK_STATE_COMPLETE:
    case GEARMAN_TASK_STATE_EXCEPTION:
    case GEARMAN_TASK_STATE_FAIL:
    case GEARMAN_TASK_STATE_FINISHED:
      break;
    }
  }

  return false;
}

const char *gearman_task_strstate(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    switch (task_shell->impl()->state)
    {
    case GEARMAN_TASK_STATE_NEW: return "GEARMAN_TASK_STATE_NEW";
    case GEARMAN_TASK_STATE_SUBMIT: return "GEARMAN_TASK_STATE_SUBMIT";
    case GEARMAN_TASK_STATE_WORKLOAD: return "GEARMAN_TASK_STATE_WORKLOAD";
    case GEARMAN_TASK_STATE_WORK: return "GEARMAN_TASK_STATE_WORK";
    case GEARMAN_TASK_STATE_CREATED: return "GEARMAN_TASK_STATE_CREATED";
    case GEARMAN_TASK_STATE_DATA: return "GEARMAN_TASK_STATE_DATA";
    case GEARMAN_TASK_STATE_WARNING: return "GEARMAN_TASK_STATE_WARNING";
    case GEARMAN_TASK_STATE_STATUS: return "GEARMAN_TASK_STATE_STATUS";
    case GEARMAN_TASK_STATE_COMPLETE: return "GEARMAN_TASK_STATE_COMPLETE";
    case GEARMAN_TASK_STATE_EXCEPTION: return "GEARMAN_TASK_STATE_EXCEPTION";
    case GEARMAN_TASK_STATE_FAIL: return "GEARMAN_TASK_STATE_FAIL";
    case GEARMAN_TASK_STATE_FINISHED: return "GEARMAN_TASK_STATE_FINISHED";
    }

    assert_msg(false, "Invalid result");
    return "";
  }

  return NULL;
}

void gearman_task_clear_fn(gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    task_shell->impl()->func= gearman_actions_default();
  }
}

void *gearman_task_context(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    return const_cast<void *>(task_shell->impl()->context);
  }

  return NULL;
}

void gearman_task_set_context(gearman_task_st *task_shell, void *context)
{
  if (task_shell and task_shell->impl())
  {
    task_shell->impl()->context= context;
  }
}

const char *gearman_task_function_name(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    return task_shell->impl()->send.arg[0];
  }

  return NULL;
}

const char *gearman_task_unique(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    return task_shell->impl()->unique;
  }

  return 0;
}

const char *gearman_task_job_handle(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    return task_shell->impl()->job_handle;
  }

  return 0;
}

bool gearman_task_is_known(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    return task_shell->impl()->options.is_known;
  }

  return false;
}

bool gearman_task_is_running(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    return task_shell->impl()->options.is_running;
  }

  return false;
}

uint32_t gearman_task_numerator(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    return task_shell->impl()->numerator;
  }

  return 0;
}

uint32_t gearman_task_denominator(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    return task_shell->impl()->denominator;
  }

  return 0;
}

void gearman_task_give_workload(gearman_task_st *task_shell, const void *workload, size_t workload_size)
{
  if (task_shell and task_shell->impl())
  {
    gearman_packet_give_data(task_shell->impl()->send, workload, workload_size);
  }
}

size_t gearman_task_send_workload(gearman_task_st *task_shell,
                                  const void *workload, size_t workload_size,
                                  gearman_return_t *ret_ptr)
{
  gearman_return_t unused;
  if (ret_ptr == NULL)
  {
    ret_ptr= &unused;
  }

  if (task_shell and task_shell->impl())
  {
    return task_shell->impl()->con->send_and_flush(workload, workload_size, ret_ptr);
  }

  *ret_ptr= GEARMAN_INVALID_ARGUMENT;

  return 0;
}

gearman_result_st *gearman_task_result(gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    return task_shell->impl()->result();
  }

  return NULL;
}

gearman_result_st *gearman_task_mutable_result(gearman_task_st* task_shell)
{
  if (task_shell)
  {
    Task* task= task_shell->impl();
    if (task)
    {
      if (task->result() == NULL)
      {
        task->create_result(0);
        assert(task->result());
      }

      return task->result();
    }
  }
  
  return NULL;
}

const void *gearman_task_data(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl() and task_shell->impl()->recv and task_shell->impl()->recv->data)
  {
    return task_shell->impl()->recv->data;
  }

  return NULL;
}

size_t gearman_task_data_size(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    if (task_shell->impl()->recv and task_shell->impl()->recv->data_size)
    {
      return task_shell->impl()->recv->data_size;
    }
  }

  return 0;
}

void *gearman_task_take_data(gearman_task_st *task_shell, size_t *data_size)
{
  if (task_shell and task_shell->impl())
  {
    return gearman_packet_take_data(*(task_shell->impl())->recv, data_size);
  }

  return NULL;
}

size_t gearman_task_recv_data(gearman_task_st *task_shell, void *data,
                                  size_t data_size,
                                  gearman_return_t *ret_ptr)
{
  gearman_return_t unused;
  if (ret_ptr == NULL)
  {
    ret_ptr= &unused;
  }

  if (task_shell and task_shell->impl())
  {
    return task_shell->impl()->con->receive_data(data, data_size, *ret_ptr);
  }

  *ret_ptr= GEARMAN_INVALID_ARGUMENT;

  return 0;
}

const char *gearman_task_error(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    if (task_shell->impl()->error_code() == GEARMAN_UNKNOWN_STATE or 
        task_shell->impl()->error_code() == GEARMAN_SUCCESS)
    {
      return NULL;
    }

    return gearman_strerror(task_shell->impl()->error_code());
  }

  return NULL;
}

gearman_return_t gearman_task_return(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    return task_shell->impl()->error_code();
  }

  return GEARMAN_INVALID_ARGUMENT;
}

Task::~Task()
{
  free_result();

  if (_shell)
  {
    if (_shell != &_owned_shell)
    {
      gearman_set_allocated(_shell, false);
    }
    _shell->_impl= NULL;
    _shell= NULL;
  }
}

void Task::result(gearman_result_st* result_)
{
  delete _result_ptr;
  _result_ptr= result_;
}

bool Task::create_result(size_t initial_size)
{
  assert(_result_ptr == NULL);
  if (_result_ptr)
  {
    _result_ptr->clear();
    return _result_ptr;
  }

  _result_ptr= new (std::nothrow) gearman_result_st(initial_size);
  return bool(_result_ptr);
}

bool gearman_task_has_exception(const gearman_task_st* task_shell)
{
  if (task_shell and task_shell->impl())
  {
    if (task_shell->impl()->exception.empty() == false)
    {
      return true;
    }
  }

  return false;
}

gearman_string_t gearman_task_exception(const gearman_task_st* task_shell)
{
  if (task_shell and task_shell->impl())
  {
    if (task_shell->impl()->exception.empty() == false)
    {
      gearman_string_t ret= { task_shell->impl()->exception.value(), task_shell->impl()->exception.size() };
      return ret;
    }
  }

  static gearman_string_t ret= {0, 0};
  return ret;
}

bool gearman_task_is_finished(const gearman_task_st *task_shell)
{
  if (task_shell and task_shell->impl())
  {
    return task_shell->impl()->is_finished();
  }

  return false;
}
