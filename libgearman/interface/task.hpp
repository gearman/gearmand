/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2012-2013 Data Differential, http://datadifferential.com/
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

#pragma once

#include "libgearman/interface/client.hpp"
#include "libgearman/interface/packet.hpp"

#define TASK_MAGIC 134
#define TASK_ANTI_MAGIC 157

struct Task
{
  struct Options {
    bool send_in_use;
    bool is_known;
    bool is_running;
    bool was_reduced;
    bool is_paused;

    Options() :
      send_in_use(false),
      is_known(false),
      is_running(false),
      was_reduced(false),
      is_paused(false)
    { }

    ~Options()
    {
    }
  } options;
  enum gearman_task_kind_t type;
  enum gearman_task_state_t state;
  uint32_t magic_;
  uint32_t created_id;
  uint32_t numerator;
  uint32_t denominator;
  uint32_t client_count;
  Client *client;
  gearman_task_st *next;
  gearman_task_st *prev;
  void *context;
  gearman_connection_st *con;
  gearman_packet_st *recv;
  gearman_packet_st send;
  struct gearman_actions_t func;
  struct error_st _error;
  struct gearman_result_st *_result_ptr;
  char job_handle[GEARMAN_JOB_HANDLE_SIZE];
  gearman_vector_st exception;
  size_t unique_length;
  char unique[GEARMAN_MAX_UNIQUE_SIZE];

  gearman_task_st* shell()
  {
    return _shell;
  }

  Task(Client* client_, gearman_task_st* shell_) :
    type(GEARMAN_TASK_KIND_ADD_TASK),
    state(GEARMAN_TASK_STATE_NEW),
    magic_(TASK_MAGIC),
    created_id(0),
    numerator(0),
    denominator(0),
    client_count(0),
    client(client_),
    next(NULL),
    prev(NULL),
    context(NULL),
    con(NULL),
    recv(NULL),
    func(client_->actions),
    _error(GEARMAN_UNKNOWN_STATE),
    _result_ptr(NULL),
    unique_length(0),
    _shell(shell_)
  {
    job_handle[0]= 0;
    unique[0]= 0;

    if (_shell)
    {
      gearman_set_allocated(_shell, false);
    }
    else
    {
      _shell= &_owned_shell;
      gearman_set_allocated(_shell, true);
    }

    // Add the task to the client
    {
      // New task always has no next
      // next has been initialized to NULL in the member-initializer list
      if (client_->task_list_tail != NULL)
      {
        // List is not empty: append to tail
        prev= client_->task_list_tail;
        client_->task_list_tail->impl()->next= _shell;
      }
      else
      {
        // List is empty: new task becomes both head and tail
        // prev has been initialized to NULL in the member-initializer list
        client_->task_list= _shell;
      }
      client_->task_list_tail= _shell;
      client_->task_count++;
    }

    _shell->impl(this);
  }

  ~Task();

  gearman_result_st* result()
  {
    return _result_ptr;
  }

  void result(gearman_result_st* result_);

  void free_result()
  {
    result(NULL);
  }

  bool create_result(size_t initial_size);

  void set_state(const enum gearman_task_state_t state_)
  {
    state= state_;
  }

  gearman_return_t error_code(const gearman_return_t rc_)
  {
    return _error.error_code(rc_);
  }

  gearman_return_t error_code(const gearman_return_t rc_,  const char*)
  {
    return _error.error_code(rc_);
  }

  gearman_return_t error_code() const
  {
    return _error.error_code();
  }

  const char* error() const
  {
    return _error.error();
  }

  bool is_finished() const
  {
    switch (state)
    {
      case GEARMAN_TASK_STATE_NEW:
      case GEARMAN_TASK_STATE_SUBMIT:
      case GEARMAN_TASK_STATE_WORKLOAD:
      case GEARMAN_TASK_STATE_WORK:
      case GEARMAN_TASK_STATE_CREATED:
      case GEARMAN_TASK_STATE_DATA:
      case GEARMAN_TASK_STATE_WARNING:
      case GEARMAN_TASK_STATE_STATUS:
        return false;

      case GEARMAN_TASK_STATE_COMPLETE:
      case GEARMAN_TASK_STATE_EXCEPTION:
      case GEARMAN_TASK_STATE_FAIL:
      case GEARMAN_TASK_STATE_FINISHED:
        return true;
    }

    return false;
  }

private:
  gearman_task_st* _shell;
  gearman_task_st _owned_shell;
};
