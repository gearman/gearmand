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
 * @brief Server connection definitions
 */

#include "gear_config.h"

#include "libgearman-server/common.h"

#include <string.h>
#include <errno.h>
#include <cassert>
#include <algorithm> 

static gearman_server_con_st * _server_con_create(gearman_server_thread_st *thread, gearmand_con_st *dcon,
                                                  gearmand_error_t& ret);

/*
 * Public definitions
 */

gearman_server_con_st *gearman_server_con_add(gearman_server_thread_st *thread, gearmand_con_st *dcon, gearmand_error_t& ret)
{
  gearman_server_con_st *con= _server_con_create(thread, dcon, ret);
  if (con)
  {
    if ((ret= gearman_io_set_fd(&(con->con), dcon->fd)) != GEARMAND_SUCCESS)
    {
      gearman_server_con_free(con);
      return NULL;
    }

    ret= gearmand_io_set_events(con, POLLIN);
    if (ret != GEARMAND_SUCCESS)
    {
      gearmand_gerror("gearmand_io_set_events", ret);
      gearman_server_con_free(con);
      return NULL;
    }
  }

  return con;
}

static gearman_server_con_st * _server_con_create(gearman_server_thread_st *thread,
                                                  gearmand_con_st *dcon,
                                                  gearmand_error_t& ret)
{
  gearman_server_con_st *con;

  if (thread->free_con_count > 0)
  {
    con= thread->free_con_list;
    GEARMAND_LIST__DEL(thread->free_con, con);
  }
  else
  {
    con= new (std::nothrow) gearman_server_con_st;
    if (con == NULL)
    {
      ret= gearmand_perror(errno, "new() build_gearman_server_con_st");
      return NULL;
    }
  }

  assert(con);
  if (con == NULL)
  {
    gearmand_error("Neigther an allocated gearman_server_con_st() or free listed could be found");
    ret= GEARMAND_MEMORY_ALLOCATION_FAILURE;
    return NULL;
  }

  gearmand_connection_options_t options[]= { GEARMAND_CON_MAX };
  gearmand_connection_init(thread->gearman, &(con->con), dcon, options);

  con->con.root= con;

  con->is_sleeping= false;
  con->is_exceptions= Gearmand()->_exceptions;
  con->is_dead= false;
  con->is_cleaned_up = false;
  con->is_noop_sent= false;

  con->ret= GEARMAND_SUCCESS;
  con->io_list= false;
  con->proc_list= false;
  con->to_be_freed_list= false;
  con->proc_removed= false;
  con->io_packet_count= 0;
  con->proc_packet_count= 0;
  con->worker_count= 0;
  con->client_count= 0;
  con->thread= thread;
  con->packet= NULL;
  con->io_packet_list= NULL;
  con->io_packet_end= NULL;
  con->proc_packet_list= NULL;
  con->proc_packet_end= NULL;
  con->io_next= NULL;
  con->io_prev= NULL;
  con->proc_next= NULL;
  con->proc_prev= NULL;
  con->to_be_freed_next= NULL;
  con->to_be_freed_prev= NULL;
  con->worker_list= NULL;
  con->client_list= NULL;
  con->_host= dcon->host;
  con->_port= dcon->port;
  strcpy(con->id, "-");
  con->timeout_event= NULL;

  con->protocol= NULL;
  con->_ssl= NULL;

  int error;
  if ((error= pthread_mutex_lock(&thread->lock)) == 0)
  {
    GEARMAND_LIST__ADD(thread->con, con);
    if ((error= pthread_mutex_unlock(&thread->lock)))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_lock");
      gearman_server_con_free(con);

      ret= GEARMAND_ERRNO;
      return NULL;
    }
  }
  else
  {
    assert(error);
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_lock");
    gearman_server_con_free(con);

    ret= GEARMAND_ERRNO;
    return NULL;
  }


  return con;
}

void gearman_server_con_attempt_free(gearman_server_con_st *con)
{
  con->_host= NULL;
  con->_port= NULL;

  if (Server->flags.threaded)
  {
    if (!(con->proc_removed) and !(Server->proc_shutdown))
    {
      gearman_server_con_delete_timeout(con);
      con->is_dead= true;
      con->is_sleeping= false;
      con->is_exceptions= Gearmand()->_exceptions;
      con->is_noop_sent= false;
      gearman_server_con_proc_add(con);
    }
  }
  else
  {
    gearman_server_con_free(con); 
  }
}

void gearman_server_con_free(gearman_server_con_st *con)
{
  gearman_server_thread_st *thread= con->thread;
  con->_host= NULL;
  con->_port= NULL;

  // Correct location?
#if defined(HAVE_SSL) && HAVE_SSL
  if (con->_ssl)
  {
    SSL_shutdown(con->_ssl);
    SSL_free(con->_ssl);
    con->_ssl= NULL;
  }
#endif // defined(HAVE_SSL)


  gearman_server_con_delete_timeout(con);

  if (con->is_cleaned_up)
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "con %p is already cleaned-up. returning", con);
    return;
  }
  
  gearmand_io_free(&(con->con));

  con->protocol_release();

  if (con->packet != NULL)
  {
    if (&(con->packet->packet) != con->con.recv_packet)
    {
      gearmand_packet_free(&(con->packet->packet));
    }

    gearman_server_packet_free(con->packet, con->thread, true);
  }

  while (con->io_packet_list != NULL)
  {
    gearman_server_io_packet_remove(con);
  }

  while (con->proc_packet_list != NULL)
  {
    gearman_server_packet_st* packet= gearman_server_proc_packet_remove(con);
    gearmand_packet_free(&(packet->packet));
    gearman_server_packet_free(packet, con->thread, true);
  }

  gearman_server_con_free_workers(con);

  while (con->client_list != NULL)
  {
    gearman_server_client_free(con->client_list);
  }

  if (con->timeout_event != NULL)
  {
    if (event_del(con->timeout_event) == -1)
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, errno, "calling event_del() on timeout_event failed");
    }
    free(con->timeout_event);
    con->timeout_event= NULL;
  }

  if (con->proc_list)
  {
    gearman_server_con_proc_remove(con);
  }

  if (con->io_list)
  {
    gearman_server_con_io_remove(con);
  }
  
  int lock_error;
  if ((lock_error= pthread_mutex_lock(&thread->lock)) == 0)
  {
    GEARMAND_LIST__DEL(con->thread->con, con);
    if ((lock_error= pthread_mutex_unlock(&thread->lock)))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, lock_error, "pthread_mutex_unlock");
    }
  }
  else
  {
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, lock_error, "pthread_mutex_unlock");
  }
  assert(lock_error == 0);

  if (thread->free_con_count < GEARMAND_MAX_FREE_SERVER_CON)
  {
    GEARMAND_LIST__ADD(thread->free_con, con);

    con->is_cleaned_up = true;
    return;
  }

  delete con;
}

gearmand_io_st *gearman_server_con_con(gearman_server_con_st *con)
{
  assert(con);
  return &con->con;
}

gearmand_con_st *gearman_server_con_data(gearman_server_con_st *con)
{
  assert(con);
  return gearman_io_context(&(con->con));
}

const char *gearman_server_con_id(gearman_server_con_st *con)
{
  assert(con);
  return con->id;
}

void gearman_server_con_set_id(gearman_server_con_st *con,
                               const char *id,
                               const size_t size)
{
  size_t min_size= std::min(size, size_t(GEARMAND_SERVER_CON_ID_SIZE -1));

  memcpy(con->id, id, min_size);
  con->id[min_size]= 0;

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                     "identifier set to %.*s", 
                     (uint32_t)min_size, con->id);
}

void gearman_server_con_free_worker(gearman_server_con_st *con,
                                    char *function_name,
                                    size_t function_name_size)
{
  gearman_server_worker_st *worker= con->worker_list;
  gearman_server_worker_st *prev_worker= NULL;

  while (worker != NULL)
  {
    if (worker->function->function_name_size == function_name_size &&
        !memcmp(worker->function->function_name, function_name,
                function_name_size))
    {
      gearman_server_worker_free(worker);

      /* Set worker to the last kept worker, or the beginning of the list. */
      if (prev_worker == NULL)
      {
        worker= con->worker_list;
      }
      else
      {
        worker= prev_worker;
      }
    }
    else
    {
      /* Save this so we don't need to scan the list again if one is removed. */
      prev_worker= worker;
      worker= worker->con_next;
    }
  }
}

void gearman_server_con_free_workers(gearman_server_con_st *con)
{
  while (con->worker_list != NULL)
  {
    gearman_server_worker_free(con->worker_list);
  }
}

void gearman_server_con_to_be_freed_add(gearman_server_con_st *con)
{
  int lock_error;
  if ((lock_error= pthread_mutex_lock(&con->thread->lock)) == 0)
  {
    if (con->to_be_freed_list)
    {
      if ((lock_error= pthread_mutex_unlock(&con->thread->lock)))
      {
        gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, lock_error, "pthread_mutex_unlock");
      }
      assert(lock_error == 0);
      return;
    }
  }
  else
  {
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, lock_error, "pthread_mutex_lock");
  }
  assert(lock_error == 0);

  GEARMAND_LIST_ADD(con->thread->to_be_freed, con, to_be_freed_);
  con->to_be_freed_list = true;

  /* Looks funny, but need to check to_be_freed_count locked, but call run unlocked. */
  if (con->thread->to_be_freed_count == 1 && con->thread->run_fn)
  {
    if ((lock_error= pthread_mutex_unlock(&con->thread->lock)))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, lock_error, "pthread_mutex_unlock");
    }
    assert(lock_error == 0);
    (*con->thread->run_fn)(con->thread, con->thread->run_fn_arg);
  }
  else
  {
    if ((lock_error= pthread_mutex_unlock(&con->thread->lock)))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, lock_error, "pthread_mutex_unlock");
    }
    assert(lock_error == 0);
  }
}

gearman_server_con_st * gearman_server_con_to_be_freed_next(gearman_server_thread_st *thread)
{
  gearman_server_con_st *con;

  if (thread->to_be_freed_list == NULL)
  {
    return NULL;
  }

  int lock_error;
  if ((lock_error= pthread_mutex_lock(&thread->lock)) == 0)
  {
    con= thread->to_be_freed_list;
    while (con != NULL)
    {
      GEARMAND_LIST_DEL(thread->to_be_freed, con, to_be_freed_);
        if (con->to_be_freed_list)
        {
          con->to_be_freed_list= false;
          break;
        }
      con= thread->to_be_freed_list;
    }

    if ((lock_error= pthread_mutex_unlock(&thread->lock)))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, lock_error, "pthread_mutex_unlock");
    }

    return con;
  }
  else
  {
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, lock_error, "pthread_mutex_lock");
  }
  assert(lock_error == 0);

  return NULL;
}

void gearman_server_con_io_add(gearman_server_con_st *con)
{
  if (con->io_list)
  {
    return;
  }

  int lock_error;
  if ((lock_error= pthread_mutex_lock(&con->thread->lock)) == 0)
  {
    GEARMAND_LIST_ADD(con->thread->io, con, io_);
    con->io_list= true;

    /* Looks funny, but need to check io_count locked, but call run unlocked. */
    if (con->thread->io_count == 1 && con->thread->run_fn)
    {
      if ((lock_error= pthread_mutex_unlock(&con->thread->lock)))
      {
        gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, lock_error, "pthread_mutex_unlock");
      }

      (*con->thread->run_fn)(con->thread, con->thread->run_fn_arg);
    }
    else
    {
      if ((lock_error= pthread_mutex_unlock(&con->thread->lock)))
      {
        gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, lock_error, "pthread_mutex_unlock");
      }
    }
  }
  else
  {
    gearmand_log_fatal(GEARMAN_DEFAULT_LOG_PARAM, "pthread_mutex_lock(%d), programming error, please report", lock_error);
  }

  assert(lock_error == 0);
}

void gearman_server_con_io_remove(gearman_server_con_st *con)
{
  int lock_error;
  if ((lock_error= pthread_mutex_lock(&con->thread->lock)) == 0)
  {
    if (con->io_list)
    {
      GEARMAND_LIST_DEL(con->thread->io, con, io_);
      con->io_list= false;
    }
    if ((lock_error= pthread_mutex_unlock(&con->thread->lock)))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, lock_error, "pthread_mutex_unlock");
    }
  }
  else
  {
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, lock_error, "pthread_mutex_lock");
  }

  assert(lock_error == 0);
}

gearman_server_con_st *
gearman_server_con_io_next(gearman_server_thread_st *thread)
{
  gearman_server_con_st *con= thread->io_list;

  if (con)
  {
    gearman_server_con_io_remove(con);
  }

  return con;
}

void gearman_server_con_proc_add(gearman_server_con_st *con)
{
  if (con->proc_list)
  {
    return;
  }

  int pthread_error;
  if ((pthread_error= pthread_mutex_lock(&con->thread->lock)) == 0)
  {
    GEARMAND_LIST_ADD(con->thread->proc, con, proc_);
    con->proc_list= true;
    if ((pthread_error= pthread_mutex_unlock(&con->thread->lock)))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_unlock");
    }

    if (! (Server->proc_shutdown) && !(Server->proc_wakeup))
    {
      if ((pthread_error= pthread_mutex_lock(&(Server->proc_lock))) == 0)
      {
        Server->proc_wakeup= true;
        if ((pthread_error= pthread_cond_signal(&(Server->proc_cond))) == 0)
        {
          if ((pthread_error= pthread_mutex_unlock(&(Server->proc_lock))))
          {
            gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_unlock");
          }
        }
        else
        {
          gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_cond_signal");
        }
      }
      else
      {
        gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_lock");
      }
    }
  }
  else
  {
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_lock");
  }
}

void gearman_server_con_proc_remove(gearman_server_con_st *con)
{
  int pthread_error;

  if ((pthread_error= pthread_mutex_lock(&con->thread->lock)) == 0)
  {
    if (con->proc_list)
    {
      GEARMAND_LIST_DEL(con->thread->proc, con, proc_);
      con->proc_list= false;
    }

    if ((pthread_error= pthread_mutex_unlock(&con->thread->lock)))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_unlock");
    }
  }
  else
  {
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_lock");
  }
}

gearman_server_con_st *
gearman_server_con_proc_next(gearman_server_thread_st *thread)
{
  if (thread->proc_list == NULL)
  {
    return NULL;
  }

  gearman_server_con_st *con= NULL;

  int pthread_error;
  if ((pthread_error= pthread_mutex_lock(&thread->lock)) == 0)
  {
    con= thread->proc_list;
    while (con != NULL)
    {
      GEARMAND_LIST_DEL(thread->proc, con, proc_);
      con->proc_list= false;
      if (!(con->proc_removed))
      {
        break;
      }
      con= thread->proc_list;
    }

    if ((pthread_error= pthread_mutex_unlock(&thread->lock)))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_unlock");
    }
  }
  else
  {
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_lock");
  }

  return con;
}

static void _server_job_timeout(int fd, short event, void *arg)
{
  (void)fd;
  (void)event;
  gearman_server_job_st *job= (gearman_server_job_st *)arg;

  /* A timeout has ocurred on a job, re-queue it */
  gearmand_log_warning(GEARMAN_DEFAULT_LOG_PARAM,
                       "Worker timeout reached on job, requeueing: %s %s",
                       job->job_handle, job->unique);

  gearmand_error_t ret= gearman_server_job_queue(job);
  if (ret != GEARMAND_SUCCESS)
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM,
                       "Failed trying to requeue job after timeout, job lost: %s %s",
                       job->job_handle, job->unique);
    gearman_server_job_free(job);
  }
}

gearmand_error_t gearman_server_con_add_job_timeout(gearman_server_con_st *con, gearman_server_job_st *job)
{
  if (job)
  {
    gearman_server_worker_st *worker;
    for (worker= con->worker_list; worker != NULL; worker= worker->con_next)
    {
      /* Assumes the functions are always fetched from the same server structure */
      if (worker->function == job->function)
      {
        break;
      }
    }

    /* It makes no sense to add a timeout to a connection that has no workers for a job */
    assert(worker);
    if (worker)
    {
      // We treat 0 and -1 as being the same (i.e. no timer)
      if (worker->timeout > 0)
      {
        if (worker->timeout < 1000)
        {
          worker->timeout= 1000;
        }

        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Adding timeout on %s for %s (%ld)",
                           job->function->function_name,
                           job->job_handle,
                           worker->timeout);
        if (con->timeout_event == NULL)
        {
          gearmand_con_st *dcon= con->con.context;
          con->timeout_event= (struct event *)malloc(sizeof(struct event)); // libevent POD
          if (con->timeout_event == NULL)
          {
            return gearmand_merror("malloc(sizeof(struct event)", struct event, 1);
          }
          timeout_set(con->timeout_event, _server_job_timeout, job);
          if (event_base_set(dcon->thread->base, con->timeout_event) == -1)
          {
            gearmand_perror(errno, "event_base_set");
          }
        }

        /* XXX Right now, if a worker has diff timeouts for functions I think
          this will overwrite any existing timeouts on that event. One
          solution to that would be to record the timeout from last time,
          and only set this one if it is longer than that one. */

        struct timeval timeout_tv = { 0 , 0 };
        time_t milliseconds= worker->timeout;
        timeout_tv.tv_sec= milliseconds / 1000;
        timeout_tv.tv_usec= (suseconds_t)((milliseconds % 1000) * 1000);
        timeout_add(con->timeout_event, &timeout_tv);
      }
      else if (con->timeout_event) // Delete the timer if it exists
      {
        gearman_server_con_delete_timeout(con);
      }
    }
  }

  return GEARMAND_SUCCESS;
}

void gearman_server_con_delete_timeout(gearman_server_con_st *con)
{
  if (con->timeout_event)
  {
    timeout_del(con->timeout_event);
    free(con->timeout_event);
    con->timeout_event= NULL;
  }
}

gearman_server_con_st *gearmand_ready(gearmand_connection_list_st *universal)
{
  if (universal->ready_con_list)
  {
    gearmand_io_st *con= universal->ready_con_list;
    con->options.ready= false;
    GEARMAND_LIST_DEL(universal->ready_con, con, ready_);
    return con->root;
  }

  return NULL;
}
