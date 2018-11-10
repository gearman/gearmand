/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Data Differential YATL (i.e. libtest)  library
 *
 *  Copyright (C) 2012 Data Differential, http://datadifferential.com/
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

#include <pthread.h>

namespace libtest
{
namespace thread
{

class Mutex
{
public:
  Mutex() :
    _err(0)
  {
    _err= pthread_mutex_init(&_mutex, NULL);
  }

  ~Mutex()
  {
    if ((_err= pthread_mutex_destroy(&_mutex)))
    {
      libtest::stream::make_cerr(LIBYATL_DEFAULT_PARAM) << "pthread_cond_destroy: " << strerror(_err);
      abort();
    }
  }

  pthread_mutex_t* handle()
  {
    if (_err != 0)
    {
      throw libtest::fatal(LIBYATL_DEFAULT_PARAM, "pthread_mutex_init: %s", strerror(_err));
    }

    return &_mutex;
  }

private:
  int _err;
  pthread_mutex_t _mutex;
};

class ScopedLock
{
public:
  ScopedLock(Mutex& mutex_) :
    _mutex(mutex_)
  {
    init();
  }

  ~ScopedLock()
  {
    int err;
    if ((err= pthread_mutex_unlock(_mutex.handle())))
    {
      libtest::stream::make_cerr(LIBYATL_DEFAULT_PARAM) << "pthread_mutex_unlock: " << strerror(err);
      abort();
    }
  }

  Mutex* handle()
  {
    return &_mutex;
  }

private:
  void init()
  {
    int err;
    if ((err= pthread_mutex_lock(_mutex.handle())))
    {
      throw libtest::fatal(LIBYATL_DEFAULT_PARAM, "pthread_mutex_lock: %s", strerror(err));
    }
  }

private:
  Mutex& _mutex;
};

class Condition
{
public:
  Condition()
  {
    int err;
    if ((err= pthread_cond_init(&_cond, NULL)))
    {
      throw libtest::fatal(LIBYATL_DEFAULT_PARAM, "pthread_mutex_init: %s", strerror(err));
    }
  }

  ~Condition()
  {
    int err;
    if ((err= pthread_cond_destroy(&_cond)))
    {
      libtest::stream::make_cerr(LIBYATL_DEFAULT_PARAM) << "pthread_cond_destroy: " << strerror(err);
      abort();
    }
  }

  void broadcast()
  {
    int err;
    if ((err= pthread_cond_broadcast(&_cond)))
    {
      throw libtest::fatal(LIBYATL_DEFAULT_PARAM, "pthread_cond_broadcast: %s", strerror(err));
    }
  }

  void signal()
  {
    int err;
    if ((err= pthread_cond_signal(&_cond)))
    {
      throw libtest::fatal(LIBYATL_DEFAULT_PARAM, "pthread_cond_broadcast: %s", strerror(err));
    }
  }

  void wait(ScopedLock& lock_)
  {
    int err;
    if ((err= pthread_cond_wait(&_cond, lock_.handle()->handle())))
    {
      throw libtest::fatal(LIBYATL_DEFAULT_PARAM, "pthread_cond_wait: %s", strerror(err));
    }
  }

private:
  pthread_cond_t _cond;
};

class Barrier
{
public:
  explicit Barrier(uint32_t count): 
    _threshold(count),
    _count(count),
    _generation(0)
  {   
    if (_count == 0)
    {
      fatal_assert("Zero is an invalid value");
    }
  }

  ~Barrier()
  {
  }

  bool wait()
  {   
    ScopedLock l(_mutex);
    uint32_t gen = _generation;

    if (--_count == 0)
    {   
      _generation++;
      _count = _threshold;
      _cond.broadcast();

      return true;
    }

    while (gen == _generation)
    {
      _cond.wait(l);
    }

    return false;
  }

private:
  Mutex _mutex;
  Condition _cond;
  uint32_t _threshold;
  uint32_t _count;
  uint32_t _generation;
};

class Thread 
{
private:
  typedef void *(*start_routine_fn) (void *);

public:
  template <class Function,class Arg1>
    Thread(Function func, Arg1 arg):
      _joined(false),
      _func((start_routine_fn)(void(*)())func),
      _context(arg)
    {
      int err;
      if ((err= pthread_create(&_thread, NULL, entry_func, (void*)this)))
      {
        throw libtest::fatal(LIBYATL_DEFAULT_PARAM, "pthread_create: %s", strerror(err));
      }
      _owner= pthread_self();
    }

  bool running() const
  {
    return (pthread_kill(_thread, 0) == 0);
  }

  bool detached()
  {
    if (EDEADLK == pthread_join(_thread, NULL))
    {
      return true;
    }

    /* Result of pthread_join was EINVAL == detached thread */
    return false;
  } 

  bool join()
  {
    if (_thread == pthread_self())
    {
      throw libtest::fatal(LIBYATL_DEFAULT_PARAM, "Thread cannot join on itself");
    }

    if (_owner != pthread_self())
    {
      throw libtest::fatal(LIBYATL_DEFAULT_PARAM, "Attempt made by a non-owner thead to join on thread");
    }

    bool ret= false;
    {
      ScopedLock l(_join_mutex);
      if (_joined == false)
      {
        int err;
        if ((err= pthread_join(_thread, NULL)))
        {
          switch(err)
          {
          case EINVAL:
            break;

          case ESRCH:
            ret= true;
            break;

          case EDEADLK:
          default:
            throw libtest::fatal(LIBYATL_DEFAULT_PARAM, "pthread_join: %s", strerror(err));
          }
        }
        else
        {
          ret= true;
        }

        _joined= true;
      }
    }

    return ret;
  }

  ~Thread()
  {
    join();
  }

protected:
  void run()
  {
    _func(_context);
  }

private:
  static void * entry_func(void* This)
  {
    ((Thread *)This)->run();
    return NULL;
  }

private:
  bool _joined;
  pthread_t _thread;
  pthread_t _owner;
  start_routine_fn _func;
  void* _context;
  Mutex _join_mutex;
};

} // namespace thread
} // namespace libtest
