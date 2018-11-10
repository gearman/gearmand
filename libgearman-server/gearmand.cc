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
 * @brief Gearmand Definitions
 */

#include "gear_config.h"
#include "libgearman-server/common.h"

#include <cerrno>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include <libgearman-server/gearmand.h>

#include "libgearman-server/struct/port.h"
#include "libgearman-server/plugins.h"
#include "libgearman-server/timer.h"
#include "libgearman-server/queue.h"

#include "util/memory.h"
using namespace org::tangent;

using namespace gearmand;

#ifndef SOCK_NONBLOCK 
# define SOCK_NONBLOCK 0
#endif

#ifndef SOL_TCP 
# define SOL_TCP 0
#endif

#ifndef TCP_KEEPIDLE 
# define TCP_KEEPIDLE 0
#endif

#ifndef TCP_KEEPINTVL 
# define TCP_KEEPINTVL 0
#endif

#ifndef TCP_KEEPCNT 
# define TCP_KEEPCNT 0
#endif

/*
 * Private declarations
 */

/**
 * @addtogroup gearmand_private Private Gearman Daemon Functions
 * @ingroup gearmand
 * @{
 */

static gearmand_error_t _listen_init(gearmand_st *gearmand);
static void _listen_close(gearmand_st *gearmand);
static gearmand_error_t _listen_watch(gearmand_st *gearmand);
static void _listen_clear(gearmand_st *gearmand);
static void _listen_event(int fd, short events, void *arg);

static gearmand_error_t _wakeup_init(gearmand_st *gearmand);
static void _wakeup_close(gearmand_st *gearmand);
static gearmand_error_t _wakeup_watch(gearmand_st *gearmand);
static void _wakeup_clear(gearmand_st *gearmand);
static void _wakeup_event(int fd, short events, void *arg);

static gearmand_error_t _watch_events(gearmand_st *gearmand);
static void _clear_events(gearmand_st *gearmand);
static void _close_events(gearmand_st *gearmand);

static bool gearman_server_create(gearman_server_st& server,
                                  const uint32_t job_retries,
                                  const char *job_handle_prefix,
                                  uint8_t worker_wakeup,
                                  bool round_robin,
                                  uint32_t hashtable_buckets);
static void gearmand_set_log_fn(gearmand_st *gearmand, gearmand_log_fn *function,
                                void *context, const gearmand_verbose_t verbose);


static void gearman_server_free(gearman_server_st& server)
{
  /* All threads should be cleaned up before calling this. */
  assert(server.thread_list == NULL);

  for (uint32_t key= 0; key < server.hashtable_buckets; key++)
  {
    while (server.job_hash[key] != NULL)
    {
      gearman_server_save_job(server, server.job_hash[key]);
      gearman_server_job_free(server.job_hash[key]);
    }
  }
  gearman_queue_flush(&server);

  for (uint32_t function_key= 0; function_key < GEARMAND_DEFAULT_HASH_SIZE;
       function_key++)
  {
    while(server.function_hash[function_key] != NULL)
    {
      gearman_server_function_free(&server, server.function_hash[function_key]);
    }
  }

  while (server.free_packet_list != NULL)
  {
    gearman_server_packet_st *packet= server.free_packet_list;
    server.free_packet_list= packet->next;
    delete packet;
  }

  while (server.free_job_list != NULL)
  {
    gearman_server_job_st* job= server.free_job_list;
    server.free_job_list= job->next;
    delete job;
  }

  while (server.free_client_list != NULL)
  {
    gearman_server_client_st* client= server.free_client_list;
    server.free_client_list= client->con_next;
    delete client;
  }

  while (server.free_worker_list != NULL)
  {
    gearman_server_worker_st* worker= server.free_worker_list;
    server.free_worker_list= worker->con_next;
    delete worker;
  }

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "removing queue: %s", (server.queue_version == QUEUE_VERSION_CLASS) ? "CLASS" : "FUNCTION");
  if (server.queue_version == QUEUE_VERSION_CLASS)
  {
    delete server.queue.object;
    assert(server.queue.functions == NULL);
  }
  else if (server.queue_version == QUEUE_VERSION_FUNCTION)
  {
    delete server.queue.functions;
    assert(server.queue.object == NULL);
  }
  else
  {
    gearmand_debug("Unknown queue type in removal");
  }

  free(server.job_hash);
  free(server.unique_hash);
  free(server.function_hash);
}

/** @} */

#pragma GCC diagnostic push
#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

/*
 * Public definitions
 */

static gearmand_st *_global_gearmand= NULL;

gearmand_st *Gearmand(void)
{
  if (_global_gearmand == NULL)
  {
    assert_msg(false, "Gearmand() was called before it was allocated");
    gearmand_error("Gearmand() was called before it was allocated");
  }
  assert(_global_gearmand);
  return _global_gearmand;
}

gearmand_st *gearmand_create(gearmand_config_st *config,
                             const char *host_arg,
                             uint32_t threads_arg,
                             int backlog_arg,
                             const uint32_t job_retries,
                             const char *job_handle_prefix,
                             uint8_t worker_wakeup,
                             gearmand_log_fn *log_function, void *log_context, const gearmand_verbose_t verbose_arg,
                             bool round_robin,
                             bool exceptions_,
                             uint32_t hashtable_buckets)
{
  assert(_global_gearmand == NULL);
  if (_global_gearmand)
  {
    gearmand_error("You have called gearmand_create() twice within your application.");
    _exit(EXIT_FAILURE);
  }

  gearmand_st* gearmand= new (std::nothrow) gearmand_st(host_arg, threads_arg, backlog_arg, verbose_arg, exceptions_);
  if (gearmand == NULL)
  {
    gearmand_perror(errno, "Failed to new() gearmand_st");
    return NULL;
  }
  _global_gearmand= gearmand;

  gearmand->socketopt()= config->config.sockopt();

  if (gearman_server_create(gearmand->server, job_retries,
                            job_handle_prefix, worker_wakeup,
                            round_robin, hashtable_buckets) == false)
  {
    delete gearmand;
    _global_gearmand= NULL;
    return NULL;
  }

  gearmand_set_log_fn(gearmand, log_function, log_context, verbose_arg);

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "THREADS: %u", threads_arg);

  return gearmand;
}

void gearmand_free(gearmand_st *gearmand)
{
  if (gearmand)
  {
    _close_events(gearmand);

    if (gearmand->threads > 0)
    {
      gearmand_debug("Shutting down all threads");
    }

    while (gearmand->thread_list != NULL)
    {
      gearmand_thread_free(gearmand->thread_list);
    }

    while (gearmand->free_dcon_list != NULL)
    {
      gearmand_con_st* dcon= gearmand->free_dcon_list;
      gearmand->free_dcon_list= dcon->next;
      delete dcon;
    }

    if (gearmand->base != NULL)
    {
      event_base_free(gearmand->base);
      gearmand->base= NULL;
    }

    gearman_server_free(gearmand->server);

    gearmand_info("Shutdown complete");

    delete gearmand;
  }
}

static void gearmand_set_log_fn(gearmand_st *gearmand, gearmand_log_fn *function,
                                void *context, const gearmand_verbose_t verbose)
{
  gearmand->log_fn= function;
  gearmand->log_context= context;
  gearmand->verbose= verbose;
}

bool gearmand_exceptions(gearmand_st *gearmand)
{
  return gearmand->exceptions();
}

gearmand_error_t gearmand_port_add(gearmand_st *gearmand, const char *port,
                                   gearmand_connection_add_fn *function,
                                   gearmand_connection_remove_fn* remove_)
{
  assert(gearmand);
  gearmand->_port_list.resize(gearmand->_port_list.size() +1);

  memcpy(gearmand->_port_list.back().port, port, NI_MAXSERV);
  gearmand->_port_list.back().port[NI_MAXSERV -1]= '\0';
  gearmand->_port_list.back().add_fn(function);
  gearmand->_port_list.back().remove_fn(remove_);

  return GEARMAND_SUCCESS;
}

gearman_server_st *gearmand_server(gearmand_st *gearmand)
{
  return &gearmand->server;
}

gearmand_error_t gearmand_run(gearmand_st *gearmand)
{
  libgearman::server::Epoch epoch;

  /* Initialize server components. */
  if (gearmand->base == NULL)
  {
    gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Starting up with pid %lu, verbose is set to %s", 
                      (unsigned long)(getpid()),
                      gearmand_verbose_name(gearmand->verbose));

    if (gearmand->threads > 0)
    {
      /* Set the number of free connection structures each thread should keep
         around before the main thread is forced to take them. We compute this
         here so we don't need to on every new connection. */
      gearmand->max_thread_free_dcon_count= ((GEARMAND_MAX_FREE_SERVER_CON /
                                              gearmand->threads) / 2);
    }

    gearmand->base= static_cast<struct event_base *>(event_base_new());
    if (gearmand->base == NULL)
    {
      gearmand_fatal("event_base_new(NULL)");
      return GEARMAND_EVENT;
    }

    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Method for libevent: %s", event_base_get_method(gearmand->base));

    gearmand->ret= _listen_init(gearmand);
    if (gearmand->ret != GEARMAND_SUCCESS)
    {
      return gearmand->ret;
    }

    gearmand->ret= _wakeup_init(gearmand);
    if (gearmand->ret != GEARMAND_SUCCESS)
    {
      return gearmand->ret;
    }

    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Creating %u threads", gearmand->threads);

    /* If we have 0 threads we still need to create a fake one for context. */
    uint32_t x= 0;
    do
    {
      gearmand->ret= gearmand_thread_create(*gearmand);
      if (gearmand->ret != GEARMAND_SUCCESS)
        return gearmand->ret;
      x++;
    }
    while (x < gearmand->threads);

    gearmand_debug("replaying queue: begin");
    gearmand->ret= gearman_server_queue_replay(gearmand->server);
    if (gearmand_failed(gearmand->ret))
    {
      return gearmand_gerror("failed to reload queue", gearmand->ret);
    }
    gearmand_debug("replaying queue: end");
  }

  gearmand->ret= _watch_events(gearmand);
  if (gearmand_failed(gearmand->ret))
  {
    return gearmand->ret;
  }

  gearmand_debug("Entering main event loop");

  if (event_base_loop(gearmand->base, 0) == -1)
  {
    gearmand_fatal("event_base_loop(-1)");
    return GEARMAND_EVENT;
  }

  gearmand_debug("Exited main event loop");

  return gearmand->ret;
}

void gearmand_wakeup(gearmand_st *gearmand, gearmand_wakeup_t wakeup)
{
  /* 
    If this fails, there is not much we can really do. This should never fail though if the main gearmand thread is still active.
  */
  if (gearmand->wakeup_fd[1] != -1)
  {
    int limit= 5;
    while (--limit)  // limit is for EINTR
    {
      ssize_t written;
      uint8_t buffer= wakeup;
      if ((written= write(gearmand->wakeup_fd[1], &buffer, 1)) != 1)
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

      return;
    }
  }
}


/*
 * Private definitions
 */

gearmand_error_t set_socket(gearmand_st* gearmand, int& fd, struct addrinfo *addrinfo_next)
{
  /* Call to socket() can fail for some getaddrinfo results, try another. */
  fd= socket(addrinfo_next->ai_family, addrinfo_next->ai_socktype,
             addrinfo_next->ai_protocol);
  if (fd == -1)
  {
    return gearmand_perror(errno, "socket()");
  }

#ifdef IPV6_V6ONLY
  {
    int flags= 1;
    if (addrinfo_next->ai_family == AF_INET6)
    {
      flags= 1;
      if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &flags, sizeof(flags)) == -1)
      {
        return gearmand_perror(errno, "setsockopt(IPV6_V6ONLY)");
      }
    }
  }
#endif

  if (0) // Add in when we have server working as a library again.
  {
    if (FD_CLOEXEC)
    {
      int flags;
      do 
      {
        flags= fcntl(fd, F_GETFD, 0);
      } while (flags == -1 and (errno == EINTR or errno == EAGAIN));

      if (flags != -1)
      {
        int rval;
        do
        { 
          rval= fcntl (fd, F_SETFD, flags | FD_CLOEXEC);
        } while (rval == -1 && (errno == EINTR or errno == EAGAIN));
        // we currently ignore the case where rval is -1
      }
    }
  }

  {
    int flags= 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) == -1)
    {
      return gearmand_perror(errno, "setsockopt(SO_REUSEADDR)");
    }
  }

  if (gearmand->socketopt().keepalive())
  {
    int flags= 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof(flags)) == -1)
    {
      return gearmand_perror(errno, "setsockopt(SO_KEEPALIVE)");
    }

    if (SOL_TCP)
    {
#if defined(TCP_KEEPIDLE) && TCP_KEEPIDLE
      if (TCP_KEEPIDLE and gearmand->socketopt().keepalive_idle() != -1)
      {
        int optval= gearmand->socketopt().keepalive_idle();
        if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &optval, sizeof(optval)) == -1)
        {
          return gearmand_perror(errno, "setsockopt(TCP_KEEPIDLE)");
        }
      }
#endif

#if defined(TCP_KEEPINTVL) && TCP_KEEPINTVL
      if (TCP_KEEPINTVL and gearmand->socketopt().keepalive_interval() != -1)
      {
        int optval= gearmand->socketopt().keepalive_interval();
        if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &optval, sizeof(optval)) == -1)
        {
          return gearmand_perror(errno, "setsockopt(TCP_KEEPINTVL)");
        }
      }
#endif

#if defined(TCP_KEEPCNT) && TCP_KEEPCNT
      if (TCP_KEEPCNT and gearmand->socketopt().keepalive_count() != -1)
      {
        int optval= gearmand->socketopt().keepalive_count();
        if (setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &optval, sizeof(optval)) == -1)
        {
          return gearmand_perror(errno, "setsockopt(TCP_KEEPCNT)");
        }
      }
#endif
    }
  }

  {
    struct linger ling= {0, 0};
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) == -1)
    {
      return gearmand_perror(errno, "setsockopt(SO_LINGER)");
    }
  }

  {
    int flags= 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags)) == -1)
    {
      return gearmand_perror(errno, "setsockopt(TCP_NODELAY)");
    }
  }

  return GEARMAND_SUCCESS;
}

static const uint32_t bind_timeout= 20; // Number is not special, but look at INFO messages if you decide to change it.

typedef std::pair<std::string, std::string> host_port_t;

static gearmand_error_t _listen_init(gearmand_st *gearmand)
{
  for (uint32_t x= 0; x < gearmand->_port_list.size(); ++x)
  {
    struct addrinfo hints;
    struct addrinfo *addrinfo= NULL;

    gearmand_port_st *port= &gearmand->_port_list[x];

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags= AI_PASSIVE;
    hints.ai_socktype= SOCK_STREAM;

    {
      int ret= getaddrinfo(gearmand->host, port->port, &hints, &addrinfo);
      if (ret != 0)
      {
        char buffer[1024];

        int length= snprintf(buffer, sizeof(buffer), "%s:%s", gearmand->host ? gearmand->host : "<any>", port->port);
        if (length <= 0 or size_t(length) >= sizeof(buffer))
        {
          buffer[0]= 0;
        }

        if (addrinfo)
        {
          freeaddrinfo(addrinfo);
        }
        return gearmand_gai_error(buffer, ret);
      }
    }

    std::set<host_port_t> unique_hosts;
    for (struct addrinfo *addrinfo_next= addrinfo; addrinfo_next != NULL;
         addrinfo_next= addrinfo_next->ai_next)
    {
      char host[NI_MAXHOST];

      {
        int ret= getnameinfo(addrinfo_next->ai_addr, addrinfo_next->ai_addrlen, host,
                             NI_MAXHOST, port->port, NI_MAXSERV,
                             NI_NUMERICHOST | NI_NUMERICSERV);
        if (ret != 0)
        {
          gearmand_gai_error("getaddrinfo", ret);
          strncpy(host, "-", sizeof(host));
          strncpy(port->port, "-", sizeof(port->port));
        }
      }

      std::string host_string(host);
      std::string port_string(port->port);
      host_port_t check= std::make_pair(host_string, port_string);
      if (unique_hosts.find(check) != unique_hosts.end())
      {
        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Already listening on %s:%s", host, port->port);
        continue;
      }
      unique_hosts.insert(check);

      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Trying to listen on %s:%s", host, port->port);

      /*
        @note logic for this pulled from Drizzle.

        Sometimes the port is not released fast enough when stopping and
        restarting the server. This happens quite often with the test suite
        on busy Linux systems. Retry to bind the address at these intervals:
        Sleep intervals: 1, 2, 4,  6,  9, 13, 17, 22, ...
        Retry at second: 1, 3, 7, 13, 22, 35, 52, 74, ...
        Limit the sequence by drizzled_bind_timeout.
      */
      uint32_t waited;
      uint32_t this_wait;
      uint32_t retry;
      int ret= -1;
      int fd;
      for (waited= 0, retry= 1; ; retry++, waited+= this_wait)
      {
        { 
          gearmand_error_t socket_ret;
          if (gearmand_failed(socket_ret= set_socket(gearmand, fd, addrinfo_next)))
          {
            gearmand_sockfd_close(fd);
            freeaddrinfo(addrinfo);
            return socket_ret;
          }
        }

        errno= 0;
        if ((ret= bind(fd, addrinfo_next->ai_addr, addrinfo_next->ai_addrlen)) == 0)
        {
          // Success
          break;
        }
        // Protect our error
        ret= errno;
        gearmand_sockfd_close(fd);
        
        if (waited >= bind_timeout)
        {
          freeaddrinfo(addrinfo);
          return gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "Timeout occurred when calling bind() for %s:%s", host, port->port);
        }

        if (ret != EADDRINUSE)
        {
          freeaddrinfo(addrinfo);
          return gearmand_perror(ret, "bind");
        }

        this_wait= retry * retry / 3 + 1;

        // We are in single user threads, so strerror() is fine.
        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Retrying bind(%s) on %s:%s %u + %u >= %u", 
                           strerror(ret), host, port->port,
                           waited, this_wait, bind_timeout);

        struct timespec requested;
        requested.tv_sec= this_wait;
        requested.tv_nsec= 0;

        nanosleep(&requested, NULL);
      }

      if (listen(fd, gearmand->backlog) == -1)
      {
        gearmand_perror(errno, "listen");

        gearmand_sockfd_close(fd);

        freeaddrinfo(addrinfo);
        return GEARMAND_ERRNO;
      }

      // Scoping note for eventual transformation
      {
        int* fd_list= (int *)realloc(port->listen_fd, sizeof(int) * (port->listen_count + 1));
        if (fd_list == NULL)
        {
          gearmand_perror(errno, "realloc");

          gearmand_sockfd_close(fd);

          freeaddrinfo(addrinfo);
          return GEARMAND_ERRNO;
        }

        port->listen_fd= fd_list;
      }

      port->listen_fd[port->listen_count]= fd;
      port->listen_count++;

      gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Listening on %s:%s (%d)", host, port->port, fd);
    }

    freeaddrinfo(addrinfo);

    /* Report last socket() error if we couldn't find an address to bind. */
    if (port->listen_fd == NULL)
    {
      return gearmand_log_fatal(GEARMAN_DEFAULT_LOG_PARAM, "Could not bind/listen to any addresses");
    }

    assert(port->listen_event == NULL);
    port->listen_event= (struct event *)malloc(sizeof(struct event) * port->listen_count); // libevent POD
    if (port->listen_event == NULL)
    {
      return gearmand_merror("malloc(sizeof(struct event) * port->listen_count)", struct event, port->listen_count);
    }

    for (uint32_t y= 0; y < port->listen_count; ++y)
    {
      event_set(&(port->listen_event[y]), port->listen_fd[y], EV_READ | EV_PERSIST, _listen_event, port);

      if (event_base_set(gearmand->base, &(port->listen_event[y])) == -1)
      {
        return gearmand_perror(errno, "event_base_set()");
      }
    }
  }

  return GEARMAND_SUCCESS;
}

static void _listen_close(gearmand_st *gearmand)
{
  _listen_clear(gearmand);

  for (uint32_t x= 0; x < gearmand->_port_list.size(); ++x)
  {
    for (uint32_t y= 0; y < gearmand->_port_list[x].listen_count; ++y)
    {
      if (gearmand->_port_list[x].listen_fd[y] >= 0)
      {
        gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Closing listening socket (%d)", gearmand->_port_list[x].listen_fd[y]);
        gearmand_sockfd_close(gearmand->_port_list[x].listen_fd[y]);
        gearmand->_port_list[x].listen_fd[y]= -1;
      }
    }
  }
}

static gearmand_error_t _listen_watch(gearmand_st *gearmand)
{
  if (gearmand->is_listen_event)
  {
    return GEARMAND_SUCCESS;
  }

  for (uint32_t x= 0; x < gearmand->_port_list.size(); ++x)
  {
    for (uint32_t y= 0; y < gearmand->_port_list[x].listen_count; y++)
    {
      gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Adding event for listening socket (%d)",
                        gearmand->_port_list[x].listen_fd[y]);

      if (event_add(&(gearmand->_port_list[x].listen_event[y]), NULL) < 0)
      {
        gearmand_perror(errno, "event_add");
        return GEARMAND_EVENT;
      }
    }
  }

  gearmand->is_listen_event= true;
  return GEARMAND_SUCCESS;
}

static void _listen_clear(gearmand_st *gearmand)
{
  if (gearmand->is_listen_event)
  {
    for (uint32_t x= 0; x < gearmand->_port_list.size(); ++x)
    {
      for (uint32_t y= 0; y < gearmand->_port_list[x].listen_count; y++)
      {
        gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, 
                          "Clearing event for listening socket (%d)",
                          gearmand->_port_list[x].listen_fd[y]);

        if (event_del(&(gearmand->_port_list[x].listen_event[y])) == -1)
        {
          gearmand_perror(errno, "We tried to event_del() an event which no longer existed");
          assert_msg(false, "We tried to event_del() an event which no longer existed");
        }
      }
    }

    gearmand->is_listen_event= false;
  }
}

static void _listen_event(int event_fd, short events __attribute__ ((unused)), void *arg)
{
  gearmand_port_st *port= (gearmand_port_st *)arg;
  struct sockaddr sa;

  socklen_t sa_len= sizeof(sa);
#if defined(HAVE_ACCEPT4) && HAVE_ACCEPT4
  int fd= accept4(event_fd, &sa, &sa_len, SOCK_NONBLOCK); //  SOCK_NONBLOCK);
#else
  int fd= accept(event_fd, &sa, &sa_len);
#endif

  if (fd == -1)
  {
    int local_error= errno;

    switch (local_error)
    {
    case EINTR:
    case EMFILE:
      return;

    case ECONNABORTED:
      gearmand_perror(local_error, "accept");
      return;

    default:
      break;
    }

    _clear_events(Gearmand());
    Gearmand()->ret= gearmand_perror(local_error, "accept");
    return;
  }
  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "accept() fd:%d", fd);

  /* 
    Since this is numeric, it should never fail. Even if it did we don't want to really error from it.
  */
  char host[NI_MAXHOST];
  char port_str[NI_MAXSERV];
  int error= getnameinfo(&sa, sa_len, host, NI_MAXHOST, port_str, NI_MAXSERV,
                         NI_NUMERICHOST | NI_NUMERICSERV);
  if (error != 0)
  {
    gearmand_gai_error("getnameinfo", error);
    strncpy(host, "-", sizeof(host));
    strncpy(port_str, "-", sizeof(port_str));
  }

  gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Accepted connection from %s:%s", host, port_str);

  {
    int flags= 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof(flags)) == -1)
    {
      gearmand_log_perror(GEARMAN_DEFAULT_LOG_PARAM, errno, "%s:%s setsockopt(SO_KEEPALIVE)", host, port_str);
    }
  }

  gearmand_error_t ret= gearmand_con_create(Gearmand(), fd, host, port_str, port);
  if (ret == GEARMAND_MEMORY_ALLOCATION_FAILURE)
  {
    gearmand_sockfd_close(fd);
    return;
  }
  else if (ret != GEARMAND_SUCCESS)
  {
    Gearmand()->ret= ret;
    _clear_events(Gearmand());
  }
}

static gearmand_error_t _wakeup_init(gearmand_st *gearmand)
{
  gearmand_debug("Creating wakeup pipe");

#if defined(HAVE_PIPE2) && HAVE_PIPE2
  if (pipe2(gearmand->wakeup_fd, O_NONBLOCK) == -1)
  {
    return gearmand_fatal_perror(errno, "pipe2(gearmand->wakeup_fd)");
  }
#else
  if (pipe(gearmand->wakeup_fd) == -1)
  {
    return gearmand_fatal_perror(errno, "pipe(gearmand->wakeup_fd)");
  }

  gearmand_error_t local_ret;
  if ((local_ret= gearmand_sockfd_nonblock(gearmand->wakeup_fd[0])))
  {
    return local_ret;
  }
#endif

  event_set(&(gearmand->wakeup_event), gearmand->wakeup_fd[0],
            EV_READ | EV_PERSIST, _wakeup_event, gearmand);
  if (event_base_set(gearmand->base, &(gearmand->wakeup_event)) == -1)
  {
    gearmand_perror(errno, "event_base_set");
  }

  return GEARMAND_SUCCESS;
}

static void _wakeup_close(gearmand_st *gearmand)
{
  _wakeup_clear(gearmand);

  if (gearmand->wakeup_fd[0] >= 0)
  {
    gearmand_debug("Closing wakeup pipe");
    gearmand_pipe_close(gearmand->wakeup_fd[0]);
    gearmand->wakeup_fd[0]= -1;
    gearmand_pipe_close(gearmand->wakeup_fd[1]);
    gearmand->wakeup_fd[1]= -1;
  }
}

static gearmand_error_t _wakeup_watch(gearmand_st *gearmand)
{
  if (gearmand->is_wakeup_event)
  {
    return GEARMAND_SUCCESS;
  }

  gearmand_debug("Adding event for wakeup pipe");

  if (event_add(&(gearmand->wakeup_event), NULL) < 0)
  {
    gearmand_perror(errno, "event_add");
    return GEARMAND_EVENT;
  }

  gearmand->is_wakeup_event= true;
  return GEARMAND_SUCCESS;
}

static void _wakeup_clear(gearmand_st *gearmand)
{
  if (gearmand->is_wakeup_event)
  {
    gearmand_debug("Clearing event for wakeup pipe");
    if (event_del(&(gearmand->wakeup_event)) < 0)
    {
      gearmand_perror(errno, "We tried to event_del() an event which no longer existed");
      assert_msg(false, "We tried to event_del() an event which no longer existed");
    }
    gearmand->is_wakeup_event= false;
  }
}

static void _wakeup_event(int fd, short, void *arg)
{
  gearmand_st *gearmand= (gearmand_st *)arg;

  while (1)
  {
    uint8_t buffer[GEARMAND_PIPE_BUFFER_SIZE];
    ssize_t ret= read(fd, buffer, GEARMAND_PIPE_BUFFER_SIZE);
    if (ret == 0)
    {
      _clear_events(gearmand);
      gearmand_fatal("read(EOF)");
      gearmand->ret= GEARMAND_PIPE_EOF;
      return;
    }
    else if (ret == -1)
    {
      int local_error= errno;
      if (local_error == EINTR)
      {
        continue;
      }

      if (local_error == EAGAIN)
      {
        break;
      }

      _clear_events(gearmand);
      gearmand->ret= gearmand_perror(local_error, "_wakeup_event:read");
      return;
    }

    for (ssize_t x= 0; x < ret; ++x)
    {
      switch ((gearmand_wakeup_t)buffer[x])
      {
      case GEARMAND_WAKEUP_PAUSE:
        gearmand_debug("Received PAUSE wakeup event");
        _clear_events(gearmand);
        gearmand->ret= GEARMAND_PAUSE;
        break;

      case GEARMAND_WAKEUP_SHUTDOWN_GRACEFUL:
        gearmand_debug("Received SHUTDOWN_GRACEFUL wakeup event");
        _listen_close(gearmand);

        for (gearmand_thread_st* thread= gearmand->thread_list; 
             thread != NULL;
             thread= thread->next)
        {
          gearmand_thread_wakeup(thread, GEARMAND_WAKEUP_SHUTDOWN_GRACEFUL);
        }

        gearmand->ret= GEARMAND_SHUTDOWN_GRACEFUL;
        break;

      case GEARMAND_WAKEUP_SHUTDOWN:
        gearmand_debug("Received SHUTDOWN wakeup event");
        _clear_events(gearmand);
        gearmand->ret= GEARMAND_SHUTDOWN;
        break;

      case GEARMAND_WAKEUP_CON:
      case GEARMAND_WAKEUP_RUN:
        gearmand_log_fatal(GEARMAN_DEFAULT_LOG_PARAM, "Received unknown wakeup event (%u)", buffer[x]);
        _clear_events(gearmand);
        gearmand->ret= GEARMAND_UNKNOWN_STATE;
        break;
      }
    }
  }
}

static gearmand_error_t _watch_events(gearmand_st *gearmand)
{
  gearmand_error_t ret;
  if (gearmand_failed(ret= _listen_watch(gearmand)))
  {
    return ret;
  }

  if (gearmand_failed(ret= _wakeup_watch(gearmand)))
  {
    return ret;
  }

  return GEARMAND_SUCCESS;
}

static void _clear_events(gearmand_st *gearmand)
{
  _listen_clear(gearmand);
  _wakeup_clear(gearmand);

  /*
    If we are not threaded, tell the fake thread to shutdown now to clear
    connections. Otherwise we will never exit the libevent loop.
  */
  if (gearmand->threads == 0 && gearmand->thread_list != NULL)
  {
    gearmand_thread_wakeup(gearmand->thread_list, GEARMAND_WAKEUP_SHUTDOWN);
  }
}

static void _close_events(gearmand_st *gearmand)
{
  _listen_close(gearmand);
  _wakeup_close(gearmand);
}

/** @} */

/*
 * Public Definitions
 */

const char *gearmand_version(void)
{
    return PACKAGE_VERSION;
}

const char *gearmand_bugreport(void)
{
    return PACKAGE_BUGREPORT;
}

const char *gearmand_verbose_name(gearmand_verbose_t verbose)
{
  switch (verbose)
  {
  case GEARMAND_VERBOSE_FATAL:
    return "FATAL";

  case GEARMAND_VERBOSE_ALERT:
    return "ALERT";

  case GEARMAND_VERBOSE_CRITICAL:
    return "CRITICAL";

  case GEARMAND_VERBOSE_ERROR:
    return "ERROR";

  case GEARMAND_VERBOSE_WARN:
    return "WARNING";

  case GEARMAND_VERBOSE_NOTICE:
    return "NOTICE";

  case GEARMAND_VERBOSE_INFO:
    return "INFO";

  case GEARMAND_VERBOSE_DEBUG:
    return "DEBUG";
  }

  assert_msg(false, "Invalid result");
  gearmand_fatal("Invalid gearmand_verbose_t used.");

  return "UNKNOWN";
}

bool gearmand_verbose_check(const char *name, gearmand_verbose_t& level)
{
  bool success= true;
  if (strcmp("FATAL", name) == 0)
  {
    level= GEARMAND_VERBOSE_FATAL;
  }
  else if (strcmp("ALERT", name) == 0)
  {
    level= GEARMAND_VERBOSE_ALERT;
  }
  else if (strcmp("CRITICAL", name) == 0)
  {
    level= GEARMAND_VERBOSE_CRITICAL;
  }
  else if (strcmp("ERROR", name) == 0)
  {
    level= GEARMAND_VERBOSE_ERROR;
  }
  else if (strcmp("WARNING", name) == 0)
  {
    level= GEARMAND_VERBOSE_WARN;
  }
  else if (strcmp("NOTICE", name) == 0)
  {
    level= GEARMAND_VERBOSE_NOTICE;
  }
  else if (strcmp("INFO", name) == 0)
  {
    level= GEARMAND_VERBOSE_INFO;
  }
  else if (strcmp("DEBUG", name) == 0)
  {
    level= GEARMAND_VERBOSE_DEBUG;
  }
  else
  {
    success= false;
  }

  return success;
}

static bool gearman_server_create(gearman_server_st& server, 
                                  const uint32_t job_retries_arg,
                                  const char *job_handle_prefix,
                                  uint8_t worker_wakeup_arg,
                                  bool round_robin_arg,
                                  uint32_t hashtable_buckets)
{
  server.state.queue_startup= false;
  server.flags.round_robin= round_robin_arg;
  server.flags.threaded= false;
  server.shutdown= false;
  server.shutdown_graceful= false;
  server.proc_wakeup= false;
  server.proc_shutdown= false;
  server.job_retries= job_retries_arg;
  server.worker_wakeup= worker_wakeup_arg;
  server.thread_count= 0;
  server.free_packet_count= 0;
  server.function_count= 0;
  server.job_count= 0;
  server.unique_count= 0;
  server.free_job_count= 0;
  server.free_client_count= 0;
  server.free_worker_count= 0;
  server.thread_list= NULL;
  server.free_packet_list= NULL;
  server.free_job_list= NULL;
  server.free_client_list= NULL;
  server.free_worker_list= NULL;

  server.queue_version= QUEUE_VERSION_NONE;
  server.queue.object= NULL;
  server.queue.functions= NULL;

  server.function_hash= (gearman_server_function_st **) calloc(GEARMAND_DEFAULT_HASH_SIZE, sizeof(gearman_server_function_st *));
  if (server.function_hash == NULL)
  {
    gearmand_merror("calloc", server.function_hash, GEARMAND_DEFAULT_HASH_SIZE);
    return false;
  }

  server.hashtable_buckets= hashtable_buckets;
  server.job_hash= (gearman_server_job_st **) calloc(hashtable_buckets, sizeof(gearman_server_job_st *));
  if (server.job_hash == NULL)
  {
    gearmand_merror("calloc", server.job_hash, hashtable_buckets);
    return false;
  }

  server.unique_hash= (gearman_server_job_st **) calloc(hashtable_buckets, sizeof(gearman_server_job_st *));
  if (server.unique_hash == NULL)
  {
    gearmand_merror("calloc", server.unique_hash, hashtable_buckets);
    return false;
  }

  int checked_length= -1;
  if (job_handle_prefix)
  {
    checked_length= snprintf(server.job_handle_prefix, GEARMAND_JOB_HANDLE_SIZE, "%s", job_handle_prefix);
  }
  else
  {
    struct utsname un;
    if (uname(&un) == -1)
    {
      gearmand_log_fatal_perror(GEARMAN_DEFAULT_LOG_PARAM, errno, "uname(&un) failed");
      gearman_server_free(server);
      return false;
    }
    checked_length= snprintf(server.job_handle_prefix, GEARMAND_JOB_HANDLE_SIZE, "H:%s", un.nodename);
  }

  if (checked_length >= GEARMAND_JOB_HANDLE_SIZE || checked_length < 0)
  {
    gearmand_log_fatal(GEARMAN_DEFAULT_LOG_PARAM, "Available length %d not enough to store job handle prefix %s",
                       GEARMAND_JOB_HANDLE_SIZE, server.job_handle_prefix);
    gearman_server_free(server);
    return false;
  }

  server.job_handle_count= 1;

  return true;
}

gearmand_error_t gearmand_set_socket_keepalive(gearmand_st *gearmand, bool keepalive_)
{
  if (gearmand)
  {
    gearmand->socketopt().keepalive(keepalive_);
    return GEARMAND_SUCCESS;
  }

  return GEARMAND_INVALID_ARGUMENT;
}

gearmand_error_t gearmand_set_socket_keepalive_idle(gearmand_st *gearmand, int keepalive_idle_)
{
  if (gearmand)
  {
    gearmand->socketopt().keepalive_idle(keepalive_idle_);
    return GEARMAND_SUCCESS;
  }

  return GEARMAND_INVALID_ARGUMENT;
}

gearmand_error_t gearmand_set_socket_keepalive_interval(gearmand_st *gearmand, int keepalive_interval_)
{
  if (gearmand)
  {
    gearmand->socketopt().keepalive_interval(keepalive_interval_);
    return GEARMAND_SUCCESS;
  }

  return GEARMAND_INVALID_ARGUMENT;
}

gearmand_error_t gearmand_set_socket_keepalive_count(gearmand_st *gearmand, int keepalive_count_)
{
  if (gearmand)
  {
    gearmand->socketopt().keepalive_count(keepalive_count_);
    return GEARMAND_SUCCESS;
  }

  return GEARMAND_INVALID_ARGUMENT;
}
#pragma GCC diagnostic pop
