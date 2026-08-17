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

#pragma once

#include "libgearman-server/struct/server.h"
#include "libgearman/ssl.h"

#include "libgearman-server/struct/port.h"

#include <vector>
#include <cstring>

struct gearmand_st
{
  class SocketOpt {
  public:
    SocketOpt():
      _keepalive(false),
      _keepalive_idle(-1),
      _keepalive_interval(-1),
      _keepalive_count(-1)
    {
    }

    bool keepalive()
    {
      return _keepalive;
    }

    void keepalive(int keepalive_)
    {
      _keepalive= keepalive_;
    }

    int keepalive_idle()
    {
      return _keepalive_idle;
    }

    void keepalive_idle(int keepalive_idle_)
    {
      _keepalive= true;
      _keepalive_idle= keepalive_idle_;
    }

    int keepalive_interval()
    {
      return _keepalive_interval;
    }

    void keepalive_interval(int keepalive_interval_)
    {
      _keepalive= true;
      _keepalive_interval= keepalive_interval_;
    }

    int keepalive_count()
    {
      return _keepalive_count;
    }

    void keepalive_count(int keepalive_count_)
    {
      _keepalive= true;
      _keepalive_count= keepalive_count_;
    }

  private:
    bool _keepalive;
    int _keepalive_idle;
    int _keepalive_interval;
    int _keepalive_count;
  } _socketopt;

  SocketOpt& socketopt()
  {
    return _socketopt;
  }
  gearmand_verbose_t verbose;
  gearmand_error_t ret;
  int backlog; // Set socket backlog for listening connection
  bool is_listen_event;
  bool is_wakeup_event;
  bool _exceptions;
  int timeout;
  uint32_t threads;
  uint32_t thread_count;
  uint32_t free_dcon_count;
  int wakeup_fd[2];
  char *host;
  gearmand_log_fn *log_fn;
  void *log_context;
  struct event_base *base;
  gearmand_thread_st *thread_list;
  gearmand_thread_st *thread_add_next;
  gearmand_con_st *free_dcon_list;
  gearman_server_st server;
  struct event wakeup_event;
  std::vector<gearmand_port_st> _port_list;
  private:
  SSL_CTX* _ctx_ssl;
  public:

  gearmand_st(const char *host_,
              uint32_t threads_,
              int backlog_,
              const gearmand_verbose_t verbose_,
              bool exceptions_) :
    verbose(verbose_),
    ret(GEARMAND_SUCCESS),
    backlog(backlog_),
    is_listen_event(false),
    is_wakeup_event(false),
    _exceptions(exceptions_),
    timeout(-1),
    threads(threads_),
    thread_count(0),
    free_dcon_count(0),
    host(NULL),
    log_fn(NULL),
    log_context(NULL),
    base(NULL),
    thread_list(NULL),
    thread_add_next(NULL),
    free_dcon_list(NULL),
    _ctx_ssl(NULL)
  {
    if (host_)
    {
      host= strdup(host_);
    }
    wakeup_fd[0]= -1;
    wakeup_fd[1]= -1;
  }

  void init_ssl()
  {
#if defined(HAVE_SSL) && HAVE_SSL
    SSL_load_error_strings();
    SSL_library_init();

    _ctx_ssl= SSL_CTX_new(SSLv23_server_method());
#endif
  }

  SSL_CTX* ctx_ssl()
  {
    return _ctx_ssl;
  }

  ~gearmand_st()
  {
    if (host)
    {
      free(host);
    }

#if defined(HAVE_SSL) && HAVE_SSL
    SSL_CTX_free(_ctx_ssl);
#endif
  }

  bool exceptions() const
  {
    return _exceptions;
  }
};
