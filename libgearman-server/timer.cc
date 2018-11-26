/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2012 Data Differential, http://datadifferential.com/ All
 *  rights reserved.
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

#include "libgearman-server/common.h"
#include <libgearman-server/gearmand.h>

#include <libgearman/pipe.h>
#include <libgearman-server/timer.h>

#include <cerrno>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <signal.h>

static pthread_once_t start_key_once= PTHREAD_ONCE_INIT;
static pthread_t thread_id;

static struct timeval current_epoch;

static int wakeup_fd[2];

static __attribute__((noreturn)) void* current_epoch_handler(void*)
{
  gearmand_debug("starting up Epoch thread");
  pollfd fds[2];
  while (true)
  {
    memset(fds, 0, sizeof(fds));
    fds[0].fd= -1; //STDIN_FILENO;
    fds[0].events= POLLIN;
    fds[0].revents= 0;
    fds[1].fd= wakeup_fd[0]; // wakeup fd
    fds[1].events= POLLIN;
    fds[1].revents= 0;

    int active_fd;
    if ((active_fd= poll(fds, 2, 1000)) == -1)
    {
      gearmand_perror(errno, "poll");
    }
    else
    {
      if (fds[1].revents)
      {
        pthread_exit(NULL);
      }
      gettimeofday(&current_epoch, NULL);
    }
  }

  pthread_exit(NULL);
}

namespace libgearman {
namespace server {

static void startup(void)
{
  wakeup_fd[0]= -1;
  wakeup_fd[1]= -1;

  if (setup_shutdown_pipe(wakeup_fd) == false)
  {
    fprintf(stderr, "Could not setup pipe\n");
    exit(1);
  }

  gettimeofday(&current_epoch, NULL);

  int error;
  if ((error= pthread_create(&thread_id, NULL, current_epoch_handler, NULL)))
  {
    fprintf(stderr, "pthread_create() failed: %s\n", strerror(error));
    exit(1);
  }
}


Epoch::Epoch()
{
  (void) pthread_once(&start_key_once, startup);
}

Epoch::~Epoch()
{
  gearmand_debug("shutting down Epoch thread");
  int count= 5;
  for (size_t x= 0; x < 2; x++)
  {
    while (--count)
    {
      if (close(wakeup_fd[x]) == -1)
      {
        switch(errno)
        {
        case EAGAIN:
          continue;

        default:
          break;
        }
      }

      break; // close()
    }
  }

#if 0
  int error;
  if ((error= pthread_kill(thread_id, SIGPIPE)) != 0)
  {
    gearmand_perror(error, "pthread_kill(thread_id, SIGTERM)");
  }
#endif
  pthread_join(thread_id, 0);

  gearmand_debug("shutdown of Epoch completed");
}

struct timeval Epoch::current()
{
  return current_epoch;
}

} // server
} // libgearman
