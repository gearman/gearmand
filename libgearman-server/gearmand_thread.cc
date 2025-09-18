/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2012 Data Differential, http://datadifferential.com/
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
 * @brief Gearmand Thread Definitions
 */

#include "gear_config.h"
#include "libgearman-server/common.h"
#include <libgearman-server/gearmand.h>

#include <cassert>
#include <cerrno>
#include <memory>
#include <csignal>

/*
 * Private declarations
 */

namespace
{

#if defined(HAVE_PTHREAD_TIMEDJOIN_NP) && HAVE_PTHREAD_TIMEDJOIN_NP
bool fill_timespec(struct timespec& ts)
{
#if defined(HAVE_CLOCK_GETTIME) && HAVE_CLOCK_GETTIME
  if (HAVE_CLOCK_GETTIME) // This won't be called on OSX, etc,...
  {
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) 
    {
      gearmand_perror(errno, "clock_gettime(CLOCK_REALTIME)");
      return false;
    }
  }
#else
  {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1) 
    {
      gearmand_perror(errno, "gettimeofday()");
      return false;
    }

    TIMEVAL_TO_TIMESPEC(&tv, &ts);
  }
#endif

  return true;
}
#endif // defined(HAVE_PTHREAD_TIMEDJOIN_NP) && HAVE_PTHREAD_TIMEDJOIN_NP

}

/**
 * @addtogroup gearmand_thread_private Private Gearmand Thread Functions
 * @ingroup gearmand_thread
 * @{
 */

static void *_thread(void *data);
static void _log(const char *line, gearmand_verbose_t verbose, gearmand_thread_st *dthread);
static void _run(gearman_server_thread_st *thread, void *fn_arg);

static gearmand_error_t _wakeup_init(gearmand_thread_st *thread);
static void _wakeup_close(gearmand_thread_st *thread);
static void _wakeup_clear(gearmand_thread_st *thread);
static void _wakeup_event(int fd, short events, void *arg);
static void _clear_events(gearmand_thread_st *thread);


namespace {

  gearmand_error_t gearmand_connection_watch(gearmand_io_st *con, short events, void *)
  {
    short set_events= 0;

    gearmand_con_st* dcon= gearman_io_context(con);

    if (events & POLLIN)
    {
      set_events|= EV_READ;
    }
    if (events & POLLOUT)
    {
      set_events|= EV_WRITE;
    }

    if (dcon->last_events != set_events)
    {
      if (dcon->last_events)
      {
        if (event_del(&(dcon->event)) == -1)
        {
          gearmand_perror(errno, "event_del");
          assert_msg(false, "event_del");
        }
      }
      event_set(&(dcon->event), dcon->fd, set_events | EV_PERSIST, _con_ready, dcon);
      if (event_base_set(dcon->thread->base, &(dcon->event)) == -1)
      {
        gearmand_perror(errno, "event_base_set");
        assert_msg(false, "event_del");
      }

      if (event_add(&(dcon->event), NULL) == -1)
      {
        gearmand_perror(errno, "event_add");
        return GEARMAND_EVENT;
      }

      dcon->last_events= set_events;
    }

    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                       "%15s:%5s Watching  %6s %s",
                       dcon->host, dcon->port,
                       events & POLLIN ? "POLLIN" : "",
                       events & POLLOUT ? "POLLOUT" : "");

    return GEARMAND_SUCCESS;
  }

}

gearmand_thread_st::gearmand_thread_st(gearmand_st& gearmand_):
  is_thread_lock{false},
  is_wakeup_event{false},
  count{0},
  dcon_count{0},
  dcon_add_count{0},
  free_dcon_count{0},
  wakeup_fd{},
#if __GNUC__ == 4 && __GNUC_MINOR__ < 9
  _gearmand(gearmand_),
#else
  _gearmand{gearmand_},
#endif
  next{nullptr},
  prev{nullptr},
  base{nullptr},
  dcon_list{nullptr},
  dcon_add_list{nullptr},
  free_dcon_list{nullptr},
  server_thread{},
  id{},
  lock{}
  //TODO uniform initialisation doesn't help with wakeup_event
{
}

/** @} */

/*
 * Public definitions
 */

gearmand_error_t gearmand_thread_create(gearmand_st& gearmand)
{
  gearmand_thread_st* thread= new (std::nothrow) gearmand_thread_st(gearmand);
  if (!thread)
  {
    return gearmand_merror("new", gearmand_thread_st, 1);
  }

  if (! gearman_server_thread_init(gearmand_server(&gearmand), &(thread->server_thread),
                                   _log, thread, gearmand_connection_watch))
  {
    delete thread;
    gearmand_fatal("gearman_server_thread_init(NULL)");
    return GEARMAND_MEMORY_ALLOCATION_FAILURE;
  }

  thread->is_thread_lock= false;
  thread->is_wakeup_event= false;
  thread->count= 0;
  thread->dcon_count= 0;
  thread->dcon_add_count= 0;
  thread->free_dcon_count= 0;
  thread->wakeup_fd[0]= -1;
  thread->wakeup_fd[1]= -1;

  GEARMAND_LIST__ADD(Gearmand()->thread, thread);

  thread->dcon_list= nullptr;
  thread->dcon_add_list= nullptr;
  thread->free_dcon_list= nullptr;

  /* If we have no threads, we still create a fake thread that uses the main
     libevent instance. Otherwise create a libevent instance for each thread. */
  if (gearmand.threads == 0)
  {
    thread->base= gearmand.base;
  }
  else
  {
    gearmand_debug("Initializing libevent for IO thread");

    assert(thread->base == nullptr);
    thread->base= event_base_new();
    if (!thread->base)
    {
      gearmand_thread_free(thread);
      gearmand_fatal("event_base_new()");
      return GEARMAND_EVENT;
    }
  }

  gearmand_error_t ret;
  if (gearmand_failed(ret= _wakeup_init(thread)))
  {
    gearmand_thread_free(thread);
    return ret;
  }

  /* If we are not running multi-threaded, just return the thread context. */
  if (gearmand.threads == 0)
  {
    return GEARMAND_SUCCESS;
  }

  thread->count= gearmand.thread_count;

  int pthread_ret= pthread_mutex_init(&(thread->lock), nullptr);
  if (pthread_ret != 0)
  {
    thread->count= 0;
    gearmand_thread_free(thread);

    return gearmand_fatal_perror(pthread_ret, "pthread_mutex_init");
  }

  thread->is_thread_lock= true;

  thread->server_thread.run(_run, thread);

  pthread_ret= pthread_create(&(thread->id), nullptr, _thread, thread);
  if (pthread_ret != 0)
  {
    thread->count= 0;
    gearmand_thread_free(thread);

    return gearmand_perror(pthread_ret, "pthread_create");
  }

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Thread %u created", thread->count);

  return GEARMAND_SUCCESS;
}

void gearmand_thread_free(gearmand_thread_st *thread)
{
  if (thread)
  {
    if (Gearmand()->threads and thread->count > 0)
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Shutting down thread %u", thread->count);

      gearmand_thread_wakeup(thread, GEARMAND_WAKEUP_SHUTDOWN);

      int pthread_error= -1;
#if defined(HAVE_PTHREAD_TIMEDJOIN_NP) && HAVE_PTHREAD_TIMEDJOIN_NP
      {
        struct timespec ts;
        if (fill_timespec(ts))
        {
          ts.tv_sec+= 300;
          pthread_error= pthread_timedjoin_np(thread->id, nullptr, &ts);
          if (pthread_error)
          {
            gearmand_perror(pthread_error, "pthread_timedjoin_np");
          }
        }

        if (pthread_error != 0)
        {
          pthread_error= pthread_kill(thread->id, SIGQUIT);
          if (pthread_error)
          {
            gearmand_perror(pthread_error, "pthread_kill(, SIGQUIT)");
          }
          pthread_error= pthread_join(thread->id, nullptr);
        }
      }
#else
      pthread_error= pthread_join(thread->id, nullptr);
#endif

      if (pthread_error)
      {
        gearmand_perror(pthread_error, "pthread_join");
      }
    }

    if (thread->is_thread_lock)
    {
      int pthread_error;
      if ((pthread_error= pthread_mutex_destroy(&(thread->lock))))
      {
        gearmand_perror(pthread_error, "pthread_mutex_destroy");
      }
    }

    _wakeup_close(thread);

    while (thread->dcon_list)
    {
      gearmand_con_free(thread->dcon_list);
    }

    while (thread->dcon_add_list)
    {
      gearmand_con_st* dcon= thread->dcon_add_list;
      thread->dcon_add_list= dcon->next;
      dcon->close_socket();
      delete dcon;
    }

    while (thread->free_dcon_list)
    {
      gearmand_con_st* dcon= thread->free_dcon_list;
      thread->free_dcon_list= dcon->next;
      delete dcon;
    }

    gearman_server_thread_free(&(thread->server_thread));

    GEARMAND_LIST__DEL(Gearmand()->thread, thread);

    if (Gearmand()->threads > 0)
    {
      if (thread->base)
      {
        if (Gearmand()->base == thread->base)
        {
          Gearmand()->base= nullptr;
        }
        event_base_free(thread->base);
        thread->base= nullptr;
      }

      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Thread %u shutdown complete", thread->count);
    }

    delete thread;
  }
}

void gearmand_thread_wakeup(gearmand_thread_st *thread,
                            gearmand_wakeup_t wakeup)
{
  uint8_t buffer= wakeup;

  /* If this fails, there is not much we can really do. This should never fail
     though if the main gearmand thread is still active. */
  int limit= 5;
  ssize_t written;
  while (--limit) 
  {
    if ((written= write(thread->wakeup_fd[1], &buffer, 1)) != 1)
    {
      if (written < 0)
      {
        switch (errno)
        {
        case EINTR:
          continue;

        default:
          break;
        }
        gearmand_perror(errno, gearmand_strwakeup(wakeup));
      }
      else
      {
        gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, 
                           "gearmand_wakeup() incorrectly wrote %lu bytes of data.", (unsigned long)written);
      }
    }

    break;
  }
}

void gearmand_thread_run(gearmand_thread_st *thread)
{
  while (1)
  {
    gearmand_error_t ret;
    gearmand_con_st *dcon= gearman_server_thread_run(&(thread->server_thread), &ret);

    if (ret == GEARMAND_SUCCESS or
        ret == GEARMAND_IO_WAIT or
        ret == GEARMAND_SHUTDOWN_GRACEFUL)
    {
      return;
    }

    if (!dcon)
    {
      /* We either got a GEARMAND_SHUTDOWN or some other fatal internal error.
         Either way, we want to shut the server down. */
      gearmand_wakeup(Gearmand(), GEARMAND_WAKEUP_SHUTDOWN);
      return;
    }

    gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Disconnected %s:%s", dcon->host, dcon->port);

    gearmand_con_free(dcon);
  }
}

#pragma GCC diagnostic push
#ifndef __INTEL_COMPILER
# pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

/*
 * Private definitions
 */

static void *_thread(void *data)
{
  gearmand_thread_st *thread= (gearmand_thread_st *)data;
  char buffer[BUFSIZ];

  int length= snprintf(buffer, sizeof(buffer), "[%6u ]", thread->count);
  if (length <= 0 or sizeof(length) >= sizeof(buffer))
  {
    assert(0);
    buffer[0]= 0;
  }

  (void)gearmand_initialize_thread_logging(buffer);

  gearmand_debug("Entering thread event loop");

  if (event_base_loop(thread->base, 0) == -1)
  {
    gearmand_fatal("event_base_loop(-1)");
    Gearmand()->ret= GEARMAND_EVENT;
  }

  gearmand_debug("Exiting thread event loop");

  return nullptr;
}

static void _log(const char *line, gearmand_verbose_t verbose, gearmand_thread_st*)
{
  if (Gearmand())
  {
    (*Gearmand()->log_fn)(line, verbose, (void *)Gearmand()->log_context);
  }
}

static void _run(gearman_server_thread_st*, void *fn_arg)
{
  if (fn_arg)
  {
    gearmand_thread_st *dthread= (gearmand_thread_st*)fn_arg;
    gearmand_thread_wakeup(dthread, GEARMAND_WAKEUP_RUN);
  }
}

static gearmand_error_t _wakeup_init(gearmand_thread_st *thread)
{
  gearmand_debug("Creating IO thread wakeup pipe");

#if defined(HAVE_PIPE2) && HAVE_PIPE2
  if (pipe2(thread->wakeup_fd, O_NONBLOCK) == -1)
  {
    return gearmand_perror(errno, "pipe");
  }
#else
  if (pipe(thread->wakeup_fd) == -1)
  {
    return gearmand_perror(errno, "pipe");
  }

  gearmand_error_t local_ret;
  if ((local_ret= gearmand_sockfd_nonblock(thread->wakeup_fd[0])))
  {
    return local_ret;
  }
#endif

  event_set(&(thread->wakeup_event), thread->wakeup_fd[0], EV_READ | EV_PERSIST,
            _wakeup_event, thread);
  if (event_base_set(thread->base, &(thread->wakeup_event)) == -1)
  {
    gearmand_perror(errno, "event_base_set");
  }

  if (event_add(&(thread->wakeup_event), nullptr) < 0)
  {
    gearmand_perror(errno, "event_add");
    return GEARMAND_EVENT;
  }

  thread->is_wakeup_event= true;

  return GEARMAND_SUCCESS;
}

static void _wakeup_close(gearmand_thread_st *thread)
{
  _wakeup_clear(thread);

  if (thread->wakeup_fd[0] >= 0)
  {
    gearmand_debug("Closing IO thread wakeup pipe");
    gearmand_pipe_close(thread->wakeup_fd[0]);
    thread->wakeup_fd[0]= -1;
    gearmand_pipe_close(thread->wakeup_fd[1]);
    thread->wakeup_fd[1]= -1;
  }
}

static void _wakeup_clear(gearmand_thread_st *thread)
{
  if (thread->is_wakeup_event)
  {
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Clearing event for IO thread wakeup pipe %u", thread->count);
    if (event_del(&(thread->wakeup_event)) < 0)
    {
      gearmand_perror(errno, "event_del() failure, shutdown may hang");
    }
    thread->is_wakeup_event= false;
  }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
static void _wakeup_event(int fd, short events __attribute__ ((unused)), void *arg)
{
  gearmand_thread_st *thread= (gearmand_thread_st *)arg;
  uint8_t buffer[GEARMAND_PIPE_BUFFER_SIZE];
  ssize_t ret;

  while (1)
  {
    ret= read(fd, buffer, GEARMAND_PIPE_BUFFER_SIZE);
    if (ret == 0)
    {
      _clear_events(thread);
      gearmand_fatal("read(EOF)");
      Gearmand()->ret= GEARMAND_PIPE_EOF;
      return;
    }
    else if (ret == -1)
    {
      int local_errno= errno;
      if (local_errno == EINTR)
      {
        continue;
      }

      if (local_errno == EAGAIN)
      {
        break;
      }

      _clear_events(thread);
      gearmand_perror(local_errno, "_wakeup_event:read");
      Gearmand()->ret= GEARMAND_ERRNO;
      return;
    }

    for (ssize_t x= 0; x < ret; x++)
    {
      switch ((gearmand_wakeup_t)buffer[x])
      {
      case GEARMAND_WAKEUP_PAUSE:
        gearmand_debug("Received PAUSE wakeup event");
        break;

      case GEARMAND_WAKEUP_SHUTDOWN_GRACEFUL:
        gearmand_debug("Received SHUTDOWN_GRACEFUL wakeup event");
        if (gearman_server_shutdown_graceful(&(Gearmand()->server)) == GEARMAND_SHUTDOWN)
        {
          gearmand_wakeup(Gearmand(), GEARMAND_WAKEUP_SHUTDOWN);
        }
        break;

      case GEARMAND_WAKEUP_SHUTDOWN:
        gearmand_debug("Received SHUTDOWN wakeup event");
        _clear_events(thread);
        break;

      case GEARMAND_WAKEUP_CON:
        gearmand_debug("Received CON wakeup event");
        gearmand_con_check_queue(thread);
        break;

      case GEARMAND_WAKEUP_RUN:
        gearmand_debug("Received RUN wakeup event");
        gearmand_thread_run(thread);
        break;

      default:
        gearmand_log_fatal(GEARMAN_DEFAULT_LOG_PARAM, "Received unknown wakeup event (%u)", buffer[x]);
        _clear_events(thread);
        Gearmand()->ret= GEARMAND_UNKNOWN_STATE;
        break;
      }
    }
  }
}
#pragma GCC diagnostic pop

static void _clear_events(gearmand_thread_st *thread)
{
  _wakeup_clear(thread);

  while (thread->dcon_list)
  {
    gearmand_con_free(thread->dcon_list);
  }
}
#pragma GCC diagnostic pop
