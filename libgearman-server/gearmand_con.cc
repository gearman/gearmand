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
 * @brief Gearmand Connection Definitions
 */

#include "gear_config.h"
#include "libgearman-server/common.h"
#include <libgearman-server/gearmand.h>
#include <libgearman-server/queue.h>
#include <cstring>

#include <cerrno>
#include <cassert>

/*
 * Private declarations
 */

void gearmand_con_st::close_socket()
{
  gearmand_sockfd_close(fd);
}

/**
 * @addtogroup gearmand_con_private Private Gearmand Connection Functions
 * @ingroup gearmand_con
 * @{
 */

static gearmand_error_t _con_add(gearmand_thread_st *thread,
                                 gearmand_con_st *dcon)
{
  gearmand_error_t ret= GEARMAND_SUCCESS;
  dcon->server_con= gearman_server_con_add(&(thread->server_thread), dcon, ret);

  assert(dcon->server_con || ret != GEARMAND_SUCCESS);
  assert(! dcon->server_con || ret == GEARMAND_SUCCESS);

  if (dcon->server_con == NULL)
  {
    dcon->close_socket();
    return ret;
  }

  if (dcon->port_st())
  {
    ret= dcon->add_fn(dcon->server_con);
    if (gearmand_failed(ret))
    {
      gearman_server_con_free(dcon->server_con);

      dcon->close_socket();

      return ret;
    }
  }

  GEARMAND_LIST__ADD(thread->dcon, dcon);

  return GEARMAND_SUCCESS;
}

gearmand_error_t gearmand_con_st::add_fn(gearman_server_con_st* con_st_)
{
  assert(_port_st);
  assert(con_st_);
  assert(con_st_ == server_con);
  return _port_st->add_fn(con_st_);
}

gearmand_error_t gearmand_con_st::remove_fn(gearman_server_con_st* con_st_)
{
  assert(_port_st);
  assert(con_st_);
  return _port_st->remove_fn(con_st_);
}

void _con_ready(int, short events, void *arg)
{
  gearmand_con_st *dcon= (gearmand_con_st *)(arg);
  short revents= 0;

  if (events & EV_READ)
  {
    revents|= POLLIN;
  }
  if (events & EV_WRITE)
  {
    revents|= POLLOUT;
  }

  gearmand_error_t ret= gearmand_io_set_revents(dcon->server_con, revents);
  if (gearmand_failed(ret))
  {
    gearmand_gerror("gearmand_io_set_revents", ret);
    gearmand_con_free(dcon);
    return;
  }

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, 
                     "%s:%s Ready     %6s %s",
                     dcon->host, dcon->port,
                     revents & POLLIN ? "POLLIN" : "",
                     revents & POLLOUT ? "POLLOUT" : "");

  gearmand_thread_run(dcon->thread);
}

/** @} */

/*
 * Public definitions
 */

/**
 * Generate hash key for job handles and unique IDs.
 */
uint32_t _server_job_hash(const char *key, size_t key_size)
{
  const char *ptr= key;
  int32_t value= 0;

  while (key_size--)
  {
    value += (int32_t)*ptr++;
    value += (value << 10);
    value ^= (value >> 6);
  }

  value += (value << 3);
  value ^= (value >> 11);
  value += (value << 15);

  return (uint32_t)(value == 0 ? 1 : value);
}

void _server_con_worker_list_append(gearman_server_worker_st *list,
                                    gearman_server_worker_st *worker)
{
  worker->con_prev= NULL;
  worker->con_next= list;
  while (worker->con_next != NULL)
  {
    worker->con_prev= worker->con_next;
    worker->con_next= worker->con_next->con_next;
  }

  if (worker->con_prev)
  {
    worker->con_prev->con_next= worker;
  }
}

void destroy_gearman_server_job_st(gearman_server_job_st* arg)
{
  gearmand_debug("delete gearman_server_con_st");
  delete arg;
}

gearman_server_job_st *gearman_server_job_get_by_unique(gearman_server_st *server,
                                                        const char *unique,
                                                        const size_t unique_length,
                                                        gearman_server_con_st *worker_con)
{
  uint32_t key= _server_job_hash(unique, unique_length);
  gearman_server_job_st *server_job;

  for (server_job= server->unique_hash[key % server->hashtable_buckets];
       server_job != NULL; server_job= server_job->unique_next)
  {
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "COMPARE unique \"%s\"(%u) == \"%s\"(%u)",
                       bool(server_job->unique[0]) ? server_job->unique :  "<null>", uint32_t(strlen(server_job->unique)),
                       unique, uint32_t(unique_length));

    if (bool(server_job->unique[0]) and
        (strcmp(server_job->unique, unique) == 0))
    {
      /* Check to make sure the worker asking for the job still owns the job. */
      if (worker_con != NULL and
          (server_job->worker == NULL or server_job->worker->con != worker_con))
      {
        return NULL;
      }

      return server_job;
    }
  }

  return NULL;
}

gearman_server_job_st *gearman_server_job_get(gearman_server_st *server,
                                              const char *job_handle,
                                              const size_t job_handle_length,
                                              gearman_server_con_st *worker_con)
{
  uint32_t key= _server_job_hash(job_handle, job_handle_length);

  for (gearman_server_job_st *server_job= server->job_hash[key % server->hashtable_buckets];
       server_job != NULL; server_job= server_job->next)
  {
    if (server_job->job_handle_key == key and
        strncmp(server_job->job_handle, job_handle, GEARMAND_JOB_HANDLE_SIZE) == 0)
    {
      /* Check to make sure the worker asking for the job still owns the job. */
      if (worker_con != NULL and
          (server_job->worker == NULL or server_job->worker->con != worker_con))
      {
        return NULL;
      }

      return server_job;
    }
  }

  return NULL;
}

gearmand_error_t gearman_server_job_cancel(gearman_server_st& server,
                                           const char *job_handle,
                                           const size_t job_handle_length)
{
  gearmand_error_t ret= GEARMAND_NO_JOBS;
  uint32_t key= _server_job_hash(job_handle, job_handle_length);

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "cancel: %.*s", int(job_handle_length), job_handle);

  for (gearman_server_job_st *server_job= server.job_hash[key % server.hashtable_buckets];
       server_job != NULL;
       server_job= server_job->next)
  {
    if (server_job->job_handle_key == key and
        strncmp(server_job->job_handle, job_handle, GEARMAND_JOB_HANDLE_SIZE) == 0)
    {
      /* Queue the fail packet for all clients. */
      for (gearman_server_client_st* client= server_job->client_list; client != NULL; client= client->job_next)
      {
        ret= gearman_server_io_packet_add(client->con, false,
                                          GEARMAN_MAGIC_RESPONSE,
                                          GEARMAN_COMMAND_WORK_FAIL,
                                          server_job->job_handle,
                                          (size_t)strlen(server_job->job_handle),
                                          NULL);
        if (gearmand_failed(ret))
        {
          gearmand_log_gerror_warn(GEARMAN_DEFAULT_LOG_PARAM, ret, "Failed to send WORK_FAIL packet to %s:%s", client->con->host(), client->con->port());
        }
      }

      /* Remove from persistent queue if one exists. */
      if (server_job->job_queued)
      {
        ret= gearman_queue_done(Server,
                                server_job->unique,
                                server_job->unique_length,
                                server_job->function->function_name,
                                server_job->function->function_name_size);
        if (gearmand_failed(ret))
        {
          return gearmand_gerror("Remove from persistent queue", ret);
        }
      }

      server_job->ignore_job= true;
      server_job->job_queued= false;

      return GEARMAND_SUCCESS;
    }
  }

  return ret;
}

gearman_server_job_st * gearman_server_job_peek(gearman_server_con_st *server_con)
{
  for (gearman_server_worker_st *server_worker= server_con->worker_list;
       server_worker != NULL;
       server_worker= server_worker->con_next)
  {
    if (server_worker->function->job_count != 0)
    {
      for (gearman_job_priority_t priority= GEARMAN_JOB_PRIORITY_HIGH;
           priority != GEARMAN_JOB_PRIORITY_MAX;
           priority= gearman_job_priority_t(int(priority) +1))
      {
        gearman_server_job_st *server_job;
        server_job= server_worker->function->job_list[priority];

        int64_t current_time= (int64_t)time(NULL);

        while(server_job && 
              server_job->when != 0 && 
              server_job->when > current_time)
        {
          server_job= server_job->function_next;  
        }
        
        if (server_job != NULL)
        {

          if (server_job->ignore_job)
          {
            /* This is only happens when a client disconnects from a foreground
              job. We do this because we don't want to run the job anymore. */
            server_job->ignore_job= false;

            gearman_server_job_free(gearman_server_job_take(server_con));

            return gearman_server_job_peek(server_con);
          }
        
          return server_job;
        }
      }
    }
  }
  

  return NULL;
}

gearman_server_job_st *gearman_server_job_take(gearman_server_con_st *server_con)
{
  for (gearman_server_worker_st *server_worker= server_con->worker_list; server_worker; server_worker= server_worker->con_next)
  {
    if (server_worker->function and server_worker->function->job_count)
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Jobs available for %.*s: %lu",
                         (int)server_worker->function->function_name_size, server_worker->function->function_name,
                         (unsigned long)(server_worker->function->job_count));

      if (Server->flags.round_robin)
      {
        GEARMAND_LIST_DEL(server_con->worker, server_worker, con_)
        _server_con_worker_list_append(server_con->worker_list, server_worker);
        ++server_con->worker_count;
        if (server_con->worker_list == NULL)
        {
          server_con->worker_list= server_worker;
        }
      }

      gearman_job_priority_t priority;
      for (priority= GEARMAN_JOB_PRIORITY_HIGH; priority < GEARMAN_JOB_PRIORITY_LOW;
           priority= gearman_job_priority_t(int(priority) +1))
      {
        if (server_worker->function->job_list[priority])
        {
          break;
        }
      }

      gearman_server_job_st *server_job= server_worker->function->job_list[priority];
      gearman_server_job_st *previous_job= server_job;
  
      int64_t current_time= (int64_t)time(NULL);
  
      while (server_job and server_job->when != 0 and server_job->when > current_time)
      {
        previous_job= server_job;
        server_job= server_job->function_next;  
      }
  
      if (server_job)
      { 
        if (server_job->function->job_list[priority] == server_job)
        {
          // If it's the head of the list, advance it
          server_job->function->job_list[priority]= server_job->function_next;
        }
        else
        {
          // Otherwise, just remove the item from the list
          previous_job->function_next= server_job->function_next;
        }
        
        // If it's the tail of the list, move the tail back
        if (server_job->function->job_end[priority] == server_job)
        {
          server_job->function->job_end[priority]= previous_job;
        }
        server_job->function->job_count--;

        server_job->worker= server_worker;
        GEARMAND_LIST_ADD(server_worker->job, server_job, worker_);
        server_job->function->job_running++;

        if (server_job->ignore_job)
        {
          gearman_server_job_free(server_job);
          return gearman_server_job_take(server_con);
        }
        
        return server_job;
      }
    }
  }
  
  return NULL;
}

void *_proc(void *data)
{
  gearman_server_st *server= (gearman_server_st *)data;

  (void)gearmand_initialize_thread_logging("[  proc ]");

  while (1)
  {
    int pthread_error;
    if ((pthread_error= pthread_mutex_lock(&(server->proc_lock))))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_lock");
      return NULL;
    }

    while (server->proc_wakeup == false)
    {
      if (server->proc_shutdown)
      {
        if ((pthread_error= pthread_mutex_unlock(&(server->proc_lock))))
        {
          gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_unlock");
        }
        return NULL;
      }

      (void) pthread_cond_wait(&(server->proc_cond), &(server->proc_lock));
    }
    server->proc_wakeup= false;

    {
      if ((pthread_error= pthread_mutex_unlock(&(server->proc_lock))))
      {
        gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_unlock");
      }
    }

    for (gearman_server_thread_st *thread= server->thread_list; thread != NULL; thread= thread->next)
    {
      gearman_server_con_st *con;
      while ((con= gearman_server_con_proc_next(thread)) != NULL)
      {
        bool packet_sent = false;
        while (1)
        {
          gearman_server_packet_st *packet= gearman_server_proc_packet_remove(con);
          if (packet == NULL)
          {
            break;
          }

          con->ret= gearman_server_run_command(con, &(packet->packet));
          packet_sent = true;
          gearmand_packet_free(&(packet->packet));
          gearman_server_packet_free(packet, con->thread, false);
        }

        // if a packet was sent in above block, and connection is dead,
        // queue up into io thread so it comes back to the PROC queue for
        // marking proc_removed. this prevents leaking any connection objects
        if (packet_sent)
        {
          if (con->is_dead)
          {
            gearman_server_con_io_add(con);
          }
        }
        else if (con->is_dead)
        {
          gearman_server_con_free_workers(con);

          while (con->client_list != NULL)
            gearman_server_client_free(con->client_list);

          con->proc_removed= true;
          gearman_server_con_to_be_freed_add(con);
        }
      }
    }
  }
}

gearman_server_job_st * gearman_server_job_create(gearman_server_st *server)
{
  gearman_server_job_st *server_job;

  if (server->free_job_count > 0)
  {
    server_job= server->free_job_list;
    GEARMAND_LIST__DEL(server->free_job, server_job);
  }
  else
  {
    server_job= new (std::nothrow) gearman_server_job_st;
    if (server_job == NULL)
    {
      return NULL;
    }
  }

  server_job->ignore_job= false;
  server_job->job_queued= false;
  server_job->retries= 0;
  server_job->priority= GEARMAN_JOB_PRIORITY_NORMAL;
  server_job->job_handle_key= 0;
  server_job->unique_key= 0;
  server_job->client_count= 0;
  server_job->numerator= 0;
  server_job->denominator= 0;
  server_job->data_size= 0;
  server_job->next= NULL;
  server_job->prev= NULL;
  server_job->unique_next= NULL;
  server_job->unique_prev= NULL;
  server_job->worker_next= NULL;
  server_job->worker_prev= NULL;
  server_job->function= NULL;
  server_job->function_next= NULL;
  server_job->data= NULL;
  server_job->client_list= NULL;
  server_job->worker= NULL;
  server_job->job_handle[0]= 0;
  server_job->unique[0]= 0;
  server_job->unique_length= 0;

  return server_job;
}

gearmand_error_t gearmand_con_create(gearmand_st *gearmand, int& fd,
                                     const char *host, const char *port,
                                     struct gearmand_port_st* port_st_)
{
  gearmand_con_st *dcon;

  if (gearmand->free_dcon_count > 0)
  {
    dcon= gearmand->free_dcon_list;
    GEARMAND_LIST__DEL(gearmand->free_dcon, dcon);
  }
  else
  {
    dcon= new (std::nothrow) gearmand_con_st;
    if (dcon == NULL)
    {
      gearmand_perror(errno, "new build_gearmand_con_st");
      gearmand_sockfd_close(fd);

      return GEARMAND_MEMORY_ALLOCATION_FAILURE;
    }

    memset(&dcon->event, 0, sizeof(struct event));
  }

  dcon->last_events= 0;
  dcon->fd= fd;
  dcon->next= NULL;
  dcon->prev= NULL;
  dcon->server_con= NULL;
  strncpy(dcon->host, host, NI_MAXHOST);
  dcon->host[NI_MAXHOST -1]= 0;
  strncpy(dcon->port, port, NI_MAXSERV);
  dcon->port[NI_MAXSERV -1]= 0;
  dcon->_port_st= port_st_;

  /* If we are not threaded, just add the connection now. */
  if (gearmand->threads == 0)
  {
    dcon->thread= gearmand->thread_list;
    return _con_add(gearmand->thread_list, dcon);
  }

  /* We do a simple round-robin connection queue algorithm here. */
  if (gearmand->thread_add_next == NULL)
  {
    gearmand->thread_add_next= gearmand->thread_list;
  }

  dcon->thread= gearmand->thread_add_next;

  /* We don't need to lock if the list is empty. */
  if (dcon->thread->dcon_add_count == 0 &&
      dcon->thread->free_dcon_count < gearmand->max_thread_free_dcon_count)
  {
    GEARMAND_LIST__ADD(dcon->thread->dcon_add, dcon);
    gearmand_thread_wakeup(dcon->thread, GEARMAND_WAKEUP_CON);
  }
  else
  {
    uint32_t free_dcon_count;
    gearmand_con_st *free_dcon_list= NULL;

    int pthread_error;
    if ((pthread_error= pthread_mutex_lock(&(dcon->thread->lock))) == 0)
    {
      GEARMAND_LIST__ADD(dcon->thread->dcon_add, dcon);

      /* Take the free connection structures back to reuse. */
      free_dcon_list= dcon->thread->free_dcon_list;
      free_dcon_count= dcon->thread->free_dcon_count;
      dcon->thread->free_dcon_list= NULL;
      dcon->thread->free_dcon_count= 0;

      if ((pthread_error= pthread_mutex_unlock(&(dcon->thread->lock))))
      {
        gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_unlock");
      }
    }
    else
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, pthread_error, "pthread_mutex_lock");
      gearmand_wakeup(Gearmand(), GEARMAND_WAKEUP_SHUTDOWN);
    }

    /* Only wakeup the thread if this is the first in the queue. We don't need
       to lock around the count check, worst case it was already picked up and
       we send an extra byte. */
    if (dcon->thread->dcon_add_count == 1)
    {
      gearmand_thread_wakeup(dcon->thread, GEARMAND_WAKEUP_CON);
    }

    /* Put the free connection structures we grabbed on the main list. */
    while (free_dcon_list != NULL)
    {
      dcon= free_dcon_list;
      GEARMAND_LIST__DEL(free_dcon, dcon);
      GEARMAND_LIST__ADD(gearmand->free_dcon, dcon);
    }
  }

  gearmand->thread_add_next= gearmand->thread_add_next->next;

  return GEARMAND_SUCCESS;
}

void gearmand_con_free(gearmand_con_st *dcon)
{
  if (event_initialized(&(dcon->event)))
  {
    if (event_del(&(dcon->event)) == -1)
    {
      gearmand_perror(errno, "event_del");
    }
    else
    {
      /* This gets around a libevent bug when both POLLIN and POLLOUT are set. */
      event_set(&(dcon->event), dcon->fd, EV_READ, _con_ready, dcon);
      if (event_base_set(dcon->thread->base, &(dcon->event)) == -1)
      {
        gearmand_perror(errno, "event_base_set");
      }

      if (event_add(&(dcon->event), NULL) == -1)
      {
        gearmand_perror(errno, "event_add");
      }
      else
      {
        if (event_del(&(dcon->event)) == -1)
        {
          gearmand_perror(errno, "event_del");
        }
      }
    }
  }

  // @note server_con could be null if we failed to complete the initial
  // connection.
  if (dcon->server_con)
  {
    gearman_server_con_attempt_free(dcon->server_con);
  }

  GEARMAND_LIST__DEL(dcon->thread->dcon, dcon);

  dcon->close_socket();

  if (Gearmand()->free_dcon_count < GEARMAND_MAX_FREE_SERVER_CON)
  {
    if (Gearmand()->threads == 0)
    {
      GEARMAND_LIST__ADD(Gearmand()->free_dcon, dcon);
    }
    else
    {
      /* Lock here because the main thread may be emptying this. */
      int error;
      if ((error=  pthread_mutex_lock(&(dcon->thread->lock))) == 0)
      {
        GEARMAND_LIST__ADD(dcon->thread->free_dcon, dcon);
        if ((error= pthread_mutex_unlock(&(dcon->thread->lock))))
        {
          gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_unlock");
        }
      }
      else
      {
        gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_lock");
      }
    }
  }
  else
  {
    delete dcon;
  }
}

void gearmand_con_check_queue(gearmand_thread_st *thread)
{
  /* Dirty check is fine here, wakeup is always sent after add completes. */
  if (thread->dcon_add_count == 0)
  {
    return;
  }

  /* We want to add new connections inside the lock because other threads may
     walk the thread's dcon_list while holding the lock. */
  while (thread->dcon_add_list != NULL)
  {
    int error;
    if ((error= pthread_mutex_lock(&(thread->lock))) == 0)
    {
      gearmand_con_st *dcon= thread->dcon_add_list;
      GEARMAND_LIST__DEL(thread->dcon_add, dcon);

      if ((error= pthread_mutex_unlock(&(thread->lock))))
      {
        gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_unlock");
        gearmand_wakeup(Gearmand(), GEARMAND_WAKEUP_SHUTDOWN);
      }

      gearmand_error_t rc;
      if ((rc= _con_add(thread, dcon)) != GEARMAND_SUCCESS)
      {
        gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, rc, "%s:%s _con_add() has failed, please report any crashes that occur immediately after this.",
                            dcon->host,
                            dcon->port);
        gearmand_con_free(dcon);
      }
    }
    else
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_lock");
      gearmand_wakeup(Gearmand(), GEARMAND_WAKEUP_SHUTDOWN);
    }
  }
}
