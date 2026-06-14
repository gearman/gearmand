/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2013 Data Differential, http://datadifferential.com/
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

#include <libtest/test.hpp>
#include <libhostile/hostile.h>

#include <cassert>
#include <cstring>
#include <memory>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>


#include <cstdio>

#include <tests/start_worker.h>
#include <util/instance.hpp>

using namespace libtest;
using namespace datadifferential;

#include "libgearman/worker.hpp"
using namespace org::gearmand;

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#define CONTEXT_MAGIC_MARKER 45

struct context_st {
  worker_handle_st *handle;
  private:
  libgearman::Worker _worker;

  public:
  int magic;
  libtest::thread::Barrier* _sync_point;

  context_st(worker_handle_st* handle_,
             const libgearman::Worker& worker_) :
    handle(handle_),
    _worker(worker_),
    magic(CONTEXT_MAGIC_MARKER),
    _sync_point(handle_->sync_point())
  {
  }

  const libgearman::Worker& worker() const
  {
    return _worker;
  }

  void wait(void)
  {
    if (_sync_point)
    {
      _sync_point->wait();
      _sync_point= NULL;
    }
  }

  void fail(void)
  {
    handle->failed_startup= true;
    wait();
  }

  ~context_st()
  {
  }

};

static void thread_runner(context_st* con)
{
  std::unique_ptr<context_st> context(con);

  assert(context.get());
  assert(context.get() == con);
  if (context.get() == NULL)
  {
    Error << "context_st passed to function was NULL";
    context->fail();
    return;
  }

  assert (context->magic == CONTEXT_MAGIC_MARKER);
  if (context->magic != CONTEXT_MAGIC_MARKER)
  {
    Error << "context_st had bad magic";
    context->fail();
    return;
  }

  libgearman::Worker worker(context->worker());
  if (&worker == NULL)
  {
    Error << "Failed to create Worker";
    context->fail();
    return;
  }

  assert(context->handle);
  if (context->handle == NULL)
  {
    Error << "Progammer error, no handle found";
    context->fail();
    return;
  }
  context->handle->set_worker_id(&worker);

  // Set worker id
  {
    if (gearman_failed(gearman_worker_set_identifier(&worker, gearman_literal_param("start_worker"))))
    {
      context->fail();
      return;
    }
  }

  // Check for a working server by pinging it with echo
  if (gearman_failed(gearman_worker_echo(&worker, gearman_literal_param("start_worker"))))
  {
    context->fail();
    return;
  }

  context->wait();

  gearman_return_t ret= GEARMAN_SUCCESS;
  while (context->handle->is_shutdown() == false)
  {
    ret= gearman_worker_work(&worker);

    if (ret == GEARMAN_NO_REGISTERED_FUNCTIONS)
    {
      context->handle->set_shutdown();
      continue;
    }

    if (ret == GEARMAN_SHUTDOWN or ret == GEARMAN_SHUTDOWN_GRACEFUL)
    {
      continue;
    }

    if (ret != GEARMAN_SUCCESS and ret != GEARMAN_INVALID_ARGUMENT and ret != GEARMAN_WORK_FAIL)
    {
      context->handle->error();
#if 0
      Error <<  context->function_name << ": " << gearman_strerror(ret) << ": " << gearman_worker_error(&worker);
#endif
    }
  }
}

worker_handle_st *worker_run(const libgearman::Worker& worker_)
{
  worker_handle_st *handle= new worker_handle_st();
  fatal_assert(handle);

  context_st *context= new context_st(handle, worker_);
  fatal_assert(context);

  handle->_thread= boost::shared_ptr<libtest::thread::Thread>(new libtest::thread::Thread(thread_runner, context));
  if (bool(handle->_thread) == false)
  {
    delete context;
    delete handle;

    FATAL("Could not allocate worker");
  }

  handle->wait();

  return handle;
}


worker_handle_st *test_worker_start(in_port_t port, 
                                    const char *namespace_key,
                                    const char *function_name,
                                    const gearman_function_t &worker_fn,
                                    void *context_,
                                    gearman_worker_options_t options,
                                    int timeout)
{
  worker_handle_st *handle= new worker_handle_st();
  fatal_assert(handle);

  libgearman::Worker worker(port);

  if (namespace_key)
  {
    gearman_worker_set_namespace(&worker, namespace_key, strlen(namespace_key));
  }

  // Set worker id
  {
    if (gearman_failed(gearman_worker_set_identifier(&worker, gearman_literal_param("start_worker"))))
    {
      delete handle;
      FATAL("gearman_worker_set_identifier() failed");
    }
  }

  // Check for a working server by pinging it with echo
  if (gearman_failed(gearman_worker_echo(&worker, gearman_literal_param("start_worker"))))
  {
    delete handle;
    FATAL("gearman_worker_echo() failed");
  }

  if (gearman_failed(gearman_worker_define_function(&worker,
                                                    function_name, strlen(function_name),
                                                    worker_fn,
                                                    timeout, 
                                                    context_)))
  {
    delete handle;
    FATAL("gearman_worker_define_function() failed");
  }

  if (options != gearman_worker_options_t())
  {
    gearman_worker_add_options(&worker, options);
  }

  context_st *context= new context_st(handle, worker);
  fatal_assert(context);

  handle->_thread= boost::shared_ptr<libtest::thread::Thread>(new libtest::thread::Thread(thread_runner, context));
  if (bool(handle->_thread) == false)
  {
    delete context;
    delete handle;

    FATAL("Could not allocate worker");
  }

  handle->wait();

  return handle;
}

libtest::thread::Barrier* worker_handle_st::sync_point()
{
  return &_sync_point;
}

void worker_handle_st::set_worker_id(gearman_worker_st* worker)
{
  _worker_id= gearman_worker_id(worker);
}

worker_handle_st::worker_handle_st() :
  failed_startup(false),
  _shutdown(false),
  _error_count(0),
  _worker_id(gearman_id_t()),
  _sync_point(2)
{
}

worker_handle_st::~worker_handle_st()
{
  shutdown();
}

void worker_handle_st::wait()
{
  _sync_point.wait();
}

void worker_handle_st::set_shutdown()
{
  libtest::thread::ScopedLock l(_shutdown_lock);
  _shutdown= true;
}

bool worker_handle_st::is_shutdown()
{
  libtest::thread::ScopedLock l(_shutdown_lock);
  return _shutdown;
}

bool worker_handle_st::shutdown()
{
  if (is_shutdown())
  {
    return true;
  }

  set_shutdown();

  // This block issues an error, but the error is not fatal
  gearman_return_t rc;
  if (gearman_failed(rc=  gearman_kill(_worker_id, GEARMAN_KILL)))
  {
    Error << "failed to shutdown " << rc;
  }

  _thread->join();

  return true;
}

bool worker_handle_st::check()
{
  gearman_return_t rc;
  if (gearman_failed(rc=  gearman_kill(_worker_id, GEARMAN_SIGNAL_CHECK)))
  {
    return false;
  }

  return true;
}



worker_handles_st::worker_handles_st()
{
}

worker_handles_st::~worker_handles_st()
{
  reset();
}

// Warning, this will not clean up memory
void worker_handles_st::kill_all()
{
  assert(valgrind_is_caller() == false);
  _workers.clear();
}

void worker_handles_st::reset()
{
  for (std::vector<worker_handle_st *>::iterator iter= _workers.begin(); 
       iter != _workers.end();
       ++iter)
  {
    delete *iter;
  }
  _workers.clear();
}

void worker_handles_st::push(worker_handle_st *arg)
{
  _workers.push_back(arg);
}
