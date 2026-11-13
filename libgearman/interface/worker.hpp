/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2012 Data Differential, http://datadifferential.com/
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

#include "libgearman/interface/universal.hpp"
#include "libgearman/interface/packet.hpp"

struct Worker
{
  struct Options {
    bool non_blocking;
    bool packet_init;
    bool change;
    bool grab_uniq;
    bool grab_all;
    bool timeout_return;
    bool _in_work;

    Options() :
      non_blocking(false),
      packet_init(false),
      change(false),
      grab_uniq(true),
      grab_all(true),
      timeout_return(false),
      _in_work(false)
    { }
  } options;
  enum gearman_worker_state_t state;
  enum gearman_worker_universal_t work_state;
  uint32_t function_count;
  uint32_t job_count;
  size_t work_result_size;
  void *context;
  gearman_connection_st *con;
  Job *_job;
  Job *job_list;
  struct _worker_function_st *function;
  struct _worker_function_st *function_list;
  struct _worker_function_st *work_function;
  void *work_result;
  struct gearman_universal_st universal;
  gearman_packet_st grab_job;
  gearman_packet_st pre_sleep;
  Job *_work_job;

  Worker(gearman_worker_st* shell_) :
    state(GEARMAN_WORKER_STATE_START),
    work_state(GEARMAN_WORKER_WORK_UNIVERSAL_GRAB_JOB),
    function_count(0),
    job_count(0),
    work_result_size(0),
    context(NULL),
    con(NULL),
    _job(NULL),
    job_list(NULL),
    function(NULL),
    function_list(NULL),
    work_function(NULL),
    work_result(NULL),
    _work_job(NULL),
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

  ~Worker()
  {
  }

  gearman_worker_st* shell()
  {
    return _shell;
  }

  gearman_job_st* job();

  gearman_job_st* take_job();

  void job(gearman_job_st* job_);

  gearman_job_st* work_job();

  bool has_work_job() const
  {
    return bool(_work_job);
  }

  void work_job(gearman_job_st* work_job_);

  bool ssl() const
  {
    return universal.ssl();
  }

  void ssl(bool ssl_)
  {
    universal.ssl(ssl_);
  }

  bool in_work() const
  {
    return options._in_work;
  }

  void in_work(bool in_work_)
  {
    options._in_work= in_work_;
  }

  bool has_identifier() const
  {
    return universal.has_identifier();
  }

  const char* error() const
  {
    return universal.error();
  }

  gearman_return_t error_code() const
  {
    return universal.error_code();
  }

private:
  gearman_worker_st* _shell;
  gearman_worker_st _owned_shell;
};

