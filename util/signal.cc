/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Data Differential Utility library
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

#include "gear_config.h"
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <util/signal.hpp>

namespace datadifferential {
namespace util {

#define MAGIC_MEMORY 123569

bool SignalThread::is_shutdown()
{
  bool ret;
  pthread_mutex_lock(&shutdown_mutex);
  ret= bool(__shutdown != SHUTDOWN_RUNNING);
  pthread_mutex_unlock(&shutdown_mutex);

  return ret;
}

void SignalThread::set_shutdown(shutdown_t arg)
{
  pthread_mutex_lock(&shutdown_mutex);
  __shutdown= arg;
  pthread_mutex_unlock(&shutdown_mutex);

  if (arg == SHUTDOWN_GRACEFUL)
  {
    if (pthread_kill(thread, SIGUSR2) == 0)
    {
      void *retval;
      pthread_join(thread, &retval);
    }
  }
}

shutdown_t SignalThread::get_shutdown()
{
  shutdown_t local;
  pthread_mutex_lock(&shutdown_mutex);
  local= __shutdown;
  pthread_mutex_unlock(&shutdown_mutex);

  return local;
}

void SignalThread::post()
{
  sem_post(lock);
}

void SignalThread::test()
{
  (void)magic_memory;
  assert(magic_memory == MAGIC_MEMORY);
  assert(sigismember(&set, SIGABRT));
  assert(sigismember(&set, SIGINT));
  assert(sigismember(&set, SIGQUIT));
  assert(sigismember(&set, SIGTERM));
  assert(sigismember(&set, SIGUSR2));
}

void SignalThread::sighup(signal_callback_fn* arg)
{
  _sighup= arg;
}

std::string SignalThread::random_lock_name(std::string::size_type len = 10)
{
  static auto& chrs = "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  thread_local static std::mt19937 rg{std::random_device{}()};
  thread_local static std::uniform_int_distribution<std::string::size_type> pick(0, sizeof(chrs) - 2);

  // convenient to named semaphore definition
  // http://man7.org/linux/man-pages/man7/sem_overview.7.html
  std::string s{"/"};
  s.reserve(len--);

  while (len--)
  {
    s += chrs[pick(rg)];
  }
  return s;
}
void SignalThread::sighup()
{
  if (_sighup)
  {
    _sighup();
  }
}

SignalThread::~SignalThread()
{
  if (not is_shutdown())
  {
    set_shutdown(SHUTDOWN_GRACEFUL);
  }

#if 0
  if (pthread_equal(thread, pthread_self()) != 0 and (pthread_kill(thread, 0) == ESRCH) == true)
  {
    void *retval;
    pthread_join(thread, &retval);
  }
#endif
  sem_close(lock);
}

extern "C" {

static void *sig_thread(void *arg)
{   
  SignalThread *context= (SignalThread*)arg;

  context->test();
  context->post();

  while (context->get_shutdown() == SHUTDOWN_RUNNING)
  {
    int sig;

    if (context->wait(sig) == -1)
    {
      std::cerr << "sigwait() returned errno:" << strerror(errno) << std::endl;
      continue;
    }

    switch (sig)
    {
    case SIGUSR2:
      break;

    case SIGHUP:
      context->sighup();
      break;

    case SIGABRT:
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
      if (context->is_shutdown() == false)
      {
        context->set_shutdown(SHUTDOWN_FORCED);
      }

      if (context->exit_on_signal())
      {
        exit(EXIT_SUCCESS);
      }

      break;

    default:
      std::cerr << "Signal handling thread got unexpected signal " <<  strsignal(sig) << std::endl;
      break;
    }
  }

  return NULL;
}

}

SignalThread::SignalThread(bool exit_on_signal_arg) :
  _exit_on_signal(exit_on_signal_arg),
  magic_memory(MAGIC_MEMORY),
  __shutdown(SHUTDOWN_RUNNING),
  thread(pthread_self()),
  _sighup(NULL)
{
  pthread_mutex_init(&shutdown_mutex, NULL);
  sigemptyset(&set);

  sigaddset(&set, SIGABRT);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGQUIT);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGUSR2);
}


bool SignalThread::setup()
{
  set_shutdown(SHUTDOWN_RUNNING);

  int error;
  if ((error= pthread_sigmask(SIG_BLOCK, &set, NULL)) != 0)
  {
    std::cerr << "pthread_sigmask() died during pthread_sigmask(" << strerror(error) << ")" << std::endl;
    return false;
  }

  if ((error= pthread_create(&thread, NULL, &sig_thread, this)) != 0)
  {
    std::cerr << "pthread_create() died during pthread_create(" << strerror(error) << ")" << std::endl;
    return false;
  }

  const char * lock_name = random_lock_name().c_str();
  lock = sem_open(lock_name, O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 0);
  if (lock == SEM_FAILED) {
    std::cerr << "WARNING: sem_open failed(" << strerror(errno) << ")"
              << " when opening lock '" << lock_name << "'." << std::endl;
  } else {
    sem_wait(lock);
  }

  return true;
}

} /* namespace util */
} /* namespace datadifferential */
