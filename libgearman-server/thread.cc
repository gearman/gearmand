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
 * @brief Server Thread Definitions
 */

#include "gear_config.h"
#include "libgearman-server/common.h"

#include <libgearman/command.h>
#include "libgearman/strcommand.h"

#ifdef __cplusplus
# include <cassert>
# include <cerrno>
#else
# include <assert.h>
# include <errno.h>
#endif

/*
 * Private declarations
 */

/**
 * @addtogroup gearman_server_private Private Server Functions
 * @ingroup gearman_server
 * @{
 */

/**
 * Try reading packets for a connection.
 */
static gearmand_error_t _thread_packet_read(gearman_server_con_st *con);

/**
 * Flush outgoing packets for a connection.
 */
static gearmand_error_t _thread_packet_flush(gearman_server_con_st *con);

/**
 * Start processing thread for the server.
 */
static gearmand_error_t _proc_thread_start(gearman_server_st *server);

/**
 * Kill processing thread for the server.
 */
static void _proc_thread_kill(gearman_server_st *server);

/** @} */

/*
 * Public definitions
 */

bool gearman_server_thread_init(gearman_server_st *server,
                                gearman_server_thread_st *thread,
                                gearman_log_server_fn *log_function,
                                gearmand_thread_st *context,
                                gearmand_event_watch_fn *event_watch)
{
  assert(server);
  assert(thread);
  if (server->thread_count == 1)
  {
    /* The server is going to be multi-threaded, start processing thread. */
    if (_proc_thread_start(server) != GEARMAND_SUCCESS)
    {
      return false;
    }
  }

  thread->con_count= 0;
  thread->io_count= 0;
  thread->proc_count= 0;
  thread->to_be_freed_count= 0;
  thread->free_con_count= 0;
  thread->free_packet_count= 0;
  thread->log_fn= log_function;
  thread->log_context= context;
  thread->run_fn= NULL;
  thread->run_fn_arg= NULL;
  thread->con_list= NULL;
  thread->io_list= NULL;
  thread->proc_list= NULL;
  thread->free_con_list= NULL;
  thread->free_packet_list= NULL;
  thread->to_be_freed_list= NULL;

  int error;
  if ((error= pthread_mutex_init(&(thread->lock), NULL)))
  {
    gearmand_perror(error, "pthread_mutex_init");
    return false;
  }

  GEARMAND_LIST__ADD(server->thread, thread);

  thread->gearman= &(thread->gearmand_connection_list_static);
  thread->gearman->init(event_watch, NULL);

  return true;
}

void gearman_server_thread_free(gearman_server_thread_st *thread)
{
  _proc_thread_kill(Server);
  
  while (thread->con_list != NULL)
  {
    gearman_server_con_free(thread->con_list);
  }

  while (thread->free_con_list != NULL)
  {
    gearman_server_con_st *con= thread->free_con_list;
    thread->free_con_list= con->next;
    delete con;
  }

  while (thread->free_packet_list != NULL)
  {
    gearman_server_packet_st *packet= thread->free_packet_list;
    thread->free_packet_list= packet->next;
    destroy_gearman_server_packet_st(packet);
  }

  if (thread->gearman != NULL)
  {
    thread->gearman->list_free();
  }

  pthread_mutex_destroy(&(thread->lock));

  GEARMAND_LIST__DEL(Server->thread, thread);
}

gearmand_con_st *
gearman_server_thread_run(gearman_server_thread_st *thread,
                          gearmand_error_t *ret_ptr)
{
  /* If we are multi-threaded, we may have packets to flush or connections that
     should start reading again. */
  if (Server->flags.threaded)
  {
    gearman_server_con_st *server_con;

    while ((server_con= gearman_server_con_to_be_freed_next(thread)) != NULL)
    {
      if (server_con->is_dead && server_con->proc_removed)
        gearman_server_con_free(server_con);
      else
        gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "con %p isn't dead %d or proc removed %d, but is in to_be_freed_list",
                           server_con, server_con->is_dead, server_con->proc_removed);
    }

    while ((server_con= gearman_server_con_io_next(thread)) != NULL)
    {
      if (server_con->is_dead)
      {
        gearman_server_con_attempt_free(server_con);
        continue;
      }

      if (server_con->ret != GEARMAND_SUCCESS)
      {
        *ret_ptr= server_con->ret;
        return gearman_server_con_data(server_con);
      }

      /* See if any outgoing packets were queued. */
      *ret_ptr= _thread_packet_flush(server_con);
      if (*ret_ptr != GEARMAND_SUCCESS && *ret_ptr != GEARMAND_IO_WAIT)
      {
        return gearman_server_con_data(server_con);
      }
    }
  }

  /* Check for new activity on connections. */
  {
    gearman_server_con_st *server_con;

    while ((server_con= gearmand_ready(thread->gearman)))
    {
      /* Try to read new packets. */
      if (server_con->con.revents & POLLIN)
      {
        *ret_ptr= _thread_packet_read(server_con);
        if (*ret_ptr != GEARMAND_SUCCESS && *ret_ptr != GEARMAND_IO_WAIT)
          return gearman_server_con_data(server_con);
      }

      /* Flush existing outgoing packets. */
      if (server_con->con.revents & POLLOUT)
      {
        *ret_ptr= _thread_packet_flush(server_con);
        if (*ret_ptr != GEARMAND_SUCCESS && *ret_ptr != GEARMAND_IO_WAIT)
        {
          return gearman_server_con_data(server_con);
        }
      }
    }
  }

  /* Start flushing new outgoing packets if we are single threaded. */
  if (! (Server->flags.threaded))
  {
    gearman_server_con_st *server_con;
    while ((server_con= gearman_server_con_io_next(thread)))
    {
      *ret_ptr= _thread_packet_flush(server_con);
      if (*ret_ptr != GEARMAND_SUCCESS && *ret_ptr != GEARMAND_IO_WAIT)
      {
        return gearman_server_con_data(server_con);
      }
    }
  }

  /* Check for the two shutdown modes. */
  if (Server->shutdown)
  {
    *ret_ptr= GEARMAND_SHUTDOWN;
  }
  else if (Server->shutdown_graceful)
  {
    if (Server->job_count == 0)
    {
      *ret_ptr= GEARMAND_SHUTDOWN;
    }
    else
    {
      *ret_ptr= GEARMAND_SHUTDOWN_GRACEFUL;
    }
  }
  else
  {
    *ret_ptr= GEARMAND_SUCCESS;
  }

  return NULL;
}

/*
 * Private definitions
 */

static gearmand_error_t _thread_packet_read(gearman_server_con_st *con)
{
  while (1)
  {
    if (con->packet == NULL)
    {
      if (! (con->packet= gearman_server_packet_create(con->thread, true)))
      {
        return GEARMAND_MEMORY_ALLOCATION_FAILURE;
      }
    }

    gearmand_error_t ret;
    if (gearmand_failed(ret= gearman_io_recv(con, true)))
    {
      if (ret == GEARMAND_IO_WAIT)
      {
        break;
      }

      gearman_server_packet_free(con->packet, con->thread, true);
      con->packet= NULL;
      return ret;
    }

    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                       "Received %s",
                       gearmand_strcommand(&con->packet->packet));

    /* We read a complete packet. */
    if (Server->flags.threaded)
    {
      /* Multi-threaded, queue for the processing thread to run. */
      gearman_server_proc_packet_add(con, con->packet);
      con->packet= NULL;
    }
    else
    {
      /* Single threaded, run the command here. */
      gearmand_error_t rc= gearman_server_run_command(con, &(con->packet->packet));
      gearmand_packet_free(&(con->packet->packet));
      gearman_server_packet_free(con->packet, con->thread, true);
      con->packet= NULL;
      if (gearmand_failed(rc))
      {
        return rc;
      }
    }
  }

  return GEARMAND_SUCCESS;
}

static gearmand_error_t _thread_packet_flush(gearman_server_con_st *con)
{
  /* Check to see if we've already tried to avoid excessive system calls. */
  if (con->con.events & POLLOUT)
  {
    return GEARMAND_IO_WAIT;
  }

  while (con->io_packet_list)
  {
    gearmand_error_t ret= gearman_io_send(con, &(con->io_packet_list->packet),
                                          con->io_packet_list->next == NULL ? true : false);
    if (gearmand_failed(ret))
    {
      return ret;
    }

    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, 
                       "Sent %s",
                       gearman_strcommand(con->io_packet_list->packet.command));

    gearman_server_io_packet_remove(con);
  }

  /* Clear the POLLOUT flag. */
  return gearmand_io_set_events(con, POLLIN);
}

static gearmand_error_t _proc_thread_start(gearman_server_st *server)
{
  int error;
  if ((error= pthread_mutex_init(&(server->proc_lock), NULL)))
  {
    return gearmand_perror(error, "pthread_mutex_init");
  }

  if ((error= pthread_cond_init(&(server->proc_cond), NULL)))
  {
    return gearmand_perror(error, "pthread_cond_init");
  }

  pthread_attr_t attr;
  if ((error= pthread_attr_init(&attr)))
  {
    return gearmand_perror(error, "pthread_attr_init");
  }

  if ((error= pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM)))
  {
    (void) pthread_attr_destroy(&attr);
    return gearmand_perror(error, "pthread_attr_setscope");
  }

  if ((error= pthread_create(&(server->proc_id), &attr, _proc, server)))
  {
    (void) pthread_attr_destroy(&attr);
    return gearmand_perror(error, "pthread_create");
  }

  if ((error= pthread_attr_destroy(&attr)))
  {
    gearmand_perror(error, "pthread_create");
  }

  server->flags.threaded= true;

  return GEARMAND_SUCCESS;
}

static void _proc_thread_kill(gearman_server_st *server)
{
  if (! (server->flags.threaded) || server->proc_shutdown)
  {
    return;
  }

  server->proc_shutdown= true;

  /* Signal proc thread to shutdown. */
  int error;
  if ((error= pthread_mutex_lock(&(server->proc_lock))) == 0)
  {
    if ((error= pthread_cond_signal(&(server->proc_cond))))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_cond_signal");
    }

    if ((error= pthread_mutex_unlock(&(server->proc_lock))))
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_unlock");
    }
  }
  else
  {
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_lock");
  }

  /* Wait for the proc thread to exit and then cleanup. */
  if ((error= pthread_join(server->proc_id, NULL)))
  {
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_join");
  }

  if ((error= pthread_cond_destroy(&(server->proc_cond))))
  {
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_cond_destroy");
  }

  if ((error= pthread_mutex_destroy(&(server->proc_lock))))
  {
    gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, error, "pthread_mutex_destroy");
  }
}
