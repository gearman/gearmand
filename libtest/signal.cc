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

#include "libtest/yatlcon.h"
#include <libtest/common.h>

#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <unistd.h>

#include <csignal>

#include <libtest/signal.h>

using namespace libtest;

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
  fatal_assert(magic_memory == MAGIC_MEMORY);

  if (!getenv("LIBTEST_IN_GDB"))
  {
    assert(sigismember(&set, SIGALRM));
    assert(sigismember(&set, SIGABRT));
    assert(sigismember(&set, SIGQUIT));
    assert(sigismember(&set, SIGINT));
    assert(sigismember(&set, SIGVTALRM));
  }
  assert(sigismember(&set, SIGUSR2));
}

bool SignalThread::unblock()
{
  int error;
  if ((error= pthread_sigmask(SIG_UNBLOCK, &set, NULL)) != 0)
  {
    Error << "While trying to reset signal mask to original set, pthread_sigmask() died during pthread_sigmask(" << strerror(error) << ")";
    return false;
  }

  return true;
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

SignalThread::~SignalThread()
{
  if (is_shutdown() == false)
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

  unblock();
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
      Error << "sigwait() returned errno:" << strerror(errno);
      continue;
    }

    switch (sig)
    {
    case SIGALRM:
    case SIGVTALRM:
      Error << strsignal(sig);
      if (gdb_is_caller())
      {
        abort();
      }
      exit(EXIT_FAILURE);

    case SIGABRT:
    case SIGUSR2:
    case SIGINT:
    case SIGQUIT:
      if (context->is_shutdown() == false)
      {
        context->set_shutdown(SHUTDOWN_FORCED);
      }
      break;
    case SIGPIPE:
      {
        Error << "Ignoring SIGPIPE";
      }
      break;

    case 0:
      Error << "Inside of gdb";
      break;

    default:
      Error << "Signal handling thread got unexpected signal " <<  strsignal(sig);
      break;
    }
  }

  return NULL;
}

}

SignalThread::SignalThread() :
  magic_memory(MAGIC_MEMORY),
  __shutdown{},
  thread(pthread_self())
{
  pthread_mutex_init(&shutdown_mutex, NULL);
  sigemptyset(&set);
  if (!getenv("LIBTEST_IN_GDB"))
  {
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGABRT);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGVTALRM);
  }
  sigaddset(&set, SIGPIPE);
  sigaddset(&set, SIGUSR2);
  sigemptyset(&original_set);
  pthread_sigmask(SIG_BLOCK, NULL, &original_set);
}


bool SignalThread::setup()
{
  set_shutdown(SHUTDOWN_RUNNING);

  const char * lock_name = random_lock_name().c_str();
  lock = sem_open(lock_name, O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 0);
  if (lock == SEM_FAILED)
  {
    Error << errno << ": " << strerror(errno)
          << " when opening lock '" << lock_name << "'.";
  }

  if (sigismember(&original_set, SIGQUIT))
  {
    Error << strsignal(SIGQUIT) << " has been previously set.";
  }

  if (sigismember(&original_set, SIGINT))
  {
    Error << strsignal(SIGINT) << " has been previously set.";
  }

  if (sigismember(&original_set, SIGVTALRM))
  {
    Error << strsignal(SIGVTALRM) << " has been previously set.";
  }

  if (sigismember(&original_set, SIGUSR2))
  {
    Error << strsignal(SIGUSR2) << " has been previously set.";
  }

  int error;
  if ((error= pthread_sigmask(SIG_BLOCK, &set, NULL)) != 0)
  {
    Error << "pthread_sigmask() died during pthread_sigmask(" << strerror(error) << ")";
    return false;
  }

  if ((error= pthread_create(&thread, NULL, &sig_thread, this)) != 0)
  {
    Error << "pthread_create() died during pthread_create(" << strerror(error) << ")";
    return false;
  }

  sem_wait(lock);

  return true;
}
