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

#pragma once

#include "libgearman/allocator.hpp" 
#include "libgearman/server_options.hpp"
#include "libgearman/interface/packet.hpp"
#include "libgearman/interface/error.hpp"
#include "libgearman/vector.h" 
#include "libgearman/assert.hpp" 
#include "libgearman/ssl.h"

#include <cstring>

enum universal_options_t
{
  GEARMAN_UNIVERSAL_NON_BLOCKING,
  GEARMAN_UNIVERSAL_DONT_TRACK_PACKETS,
  GEARMAN_UNIVERSAL_IDENTIFY,
  GEARMAN_UNIVERSAL_MAX,
  GEARMAN_UNIVERSAL_STOP_WAIT_ON_SIGNAL
};

/**
  @todo this is only used by the server and should be made private.
 */
typedef struct gearman_connection_st gearman_connection_st;
typedef gearman_return_t (gearman_event_watch_fn)(gearman_connection_st *con,
                                                  short events, void *context);

struct gearman_universal_st : public error_st
{
  struct Options {
    bool dont_track_packets;
    bool non_blocking;
    bool stop_wait_on_signal;
    bool no_new_data;
    bool _ssl;
    struct gearman_vector_st *_ssl_ca_file;
    struct gearman_vector_st *_ssl_certificate;
    struct gearman_vector_st *_ssl_key;

    Options() :
      dont_track_packets{false},
      non_blocking{false},
      stop_wait_on_signal{false},
      no_new_data{false},
      _ssl{false},
      _ssl_ca_file{NULL},
      _ssl_certificate{NULL},
      _ssl_key{NULL}
    { }
  } options;
  gearman_verbose_t verbose;
  uint32_t con_count;
  uint32_t packet_count;
  uint32_t pfds_size;
  uint32_t sending;
  int timeout; // Connection timeout.
  gearman_connection_st *con_list;
  gearman_server_options_st *server_options_list;
  gearman_packet_st *packet_list;
  struct pollfd *pfds;
  gearman_log_fn *log_fn;
  void *log_context;
  gearman_allocator_t allocator;
  struct gearman_vector_st *_identifier;
  struct gearman_vector_st *_namespace;
  SSL_CTX* _ctx_ssl;
  struct error_st _error;
  int wakeup_fd[2];

  bool ssl() const
  {
    return options._ssl;
  }

  void ssl(bool ssl_)
  {
    options._ssl= ssl_;
  }

  private:
  void close_wakeup();

  public:
  bool wakeup(bool);
  bool has_wakeup() const
  {
    return wakeup_fd[0] != INVALID_SOCKET;
  }

  bool is_non_blocking() const
  {
    return options.non_blocking;
  }

  void non_blocking(bool arg_)
  {
    options.non_blocking= arg_;
  }

  bool is_stop_wait_on_signal() const
  {
      return options.stop_wait_on_signal;
  }
  void stop_wait_on_signal(bool arg_)
  {
    options.stop_wait_on_signal = arg_;
  }

  const char *error() const
  {
    if (_error.error() == nullptr)
    {
      if (_error.error_code() != GEARMAN_SUCCESS)
      {
        return gearman_strerror(_error.error_code());
      }

      return nullptr;
    }

    return _error.error();
  }

  gearman_return_t error_code() const
  {
    return _error.error_code();
  }

  gearman_return_t error_code(const gearman_return_t rc)
  {
    return _error.error_code(rc);
  }

  int last_errno() const
  {
    return _error.system_error();
  }

  void last_errno(const int last_errno_)
  {
    _error.system_error(last_errno_);
  }

  bool has_connections() const
  {
    return con_count;
  }

  void reset_error()
  {
    _error.clear();
  }

  gearman_return_t option(const universal_options_t& option_, bool value);

  gearman_universal_st(const universal_options_t *options_= nullptr) :
    verbose(GEARMAN_VERBOSE_NEVER),
    con_count(0),
    packet_count(0),
    pfds_size(0),
    sending(0),
    timeout{-1},
    con_list(nullptr),
    server_options_list(nullptr),
    packet_list(nullptr),
    pfds(nullptr),
    log_fn(nullptr),
    log_context(nullptr),
    allocator(gearman_default_allocator()),
    _identifier(nullptr),
    _namespace(nullptr),
    _ctx_ssl(nullptr)
  {
    wakeup_fd[0]= INVALID_SOCKET;
    wakeup_fd[1]= INVALID_SOCKET;

    if (options_)
    {
      while (*options_ != GEARMAN_UNIVERSAL_MAX)
      {
        /**
          @note Check for bad value, refactor gearman_add_options().
        */
        (void)option(*options_, true);
        options_++;
      }
    }
  }

  const char* ssl_ca_file() const
  {
    if (options._ssl_ca_file && options._ssl_ca_file->size())
    {
      return options._ssl_ca_file->c_str();
    }

    if (getenv("GEARMAND_CA_CERTIFICATE"))
    {
      return getenv("GEARMAND_CA_CERTIFICATE");
    }

    return GEARMAND_CA_CERTIFICATE;
  }

  void ssl_ca_file(const char* ssl_ca_file_)
  {
    gearman_string_free(options._ssl_ca_file);
    size_t ssl_ca_file_size_ = 0;
    if (ssl_ca_file_ && (ssl_ca_file_size_ = strlen(ssl_ca_file_)))
    {
      options._ssl_ca_file = gearman_string_create(NULL, ssl_ca_file_, ssl_ca_file_size_);
    }
    else
    {
      options._ssl_ca_file = NULL;
    }
  }

  const char* ssl_certificate() const
  {
    if (options._ssl_certificate && options._ssl_certificate->size())
    {
      return options._ssl_certificate->c_str();
    }

    if (getenv("GEARMAN_CLIENT_PEM"))
    {
      return getenv("GEARMAN_CLIENT_PEM");
    }

    return GEARMAN_CLIENT_PEM;
  }

  void ssl_certificate(const char *ssl_certificate_)
  {
    gearman_string_free(options._ssl_certificate);
    size_t ssl_certificate_size_ = 0;
    if (ssl_certificate_ && (ssl_certificate_size_ = strlen(ssl_certificate_)))
    {
      options._ssl_certificate = gearman_string_create(NULL, ssl_certificate_, ssl_certificate_size_);
    }
    else
    {
      options._ssl_certificate = NULL;
    }
  }

  const char* ssl_key() const
  {
    if (options._ssl_key && options._ssl_key->size())
    {
      return options._ssl_key->c_str();
    }

    if (getenv("GEARMAN_CLIENT_KEY"))
    {
      return getenv("GEARMAN_CLIENT_KEY");
    }

    return GEARMAN_CLIENT_KEY;
  }

  void ssl_key(const char *ssl_key_)
  {
    gearman_string_free(options._ssl_key);
    size_t ssl_key_size_ = 0;
    if (ssl_key_ && (ssl_key_size_ = strlen(ssl_key_)))
    {
      options._ssl_key = gearman_string_create(NULL, ssl_key_, ssl_key_size_);
    }
    else
    {
      options._ssl_key = NULL;
    }
  }

private:
  bool init_ssl();

public:
  SSL_CTX* ctx_ssl() 
  {
    if (ssl())
    {
      if (!_ctx_ssl)
      {
        if (init_ssl() == false)
        {
          abort();
        }
      }
      assert(_ctx_ssl);

      return _ctx_ssl;
    }

    return nullptr;
  }

  ~gearman_universal_st();

  void identifier(const char *identifier_, const size_t identifier_size_);
  bool has_identifier() const
  {
    return _identifier;
  }

  void flush();
  void reset();
};

static inline bool gearman_universal_is_non_blocking(gearman_universal_st &self)
{
  return self.is_non_blocking();
}

static inline const char *gearman_universal_error(const gearman_universal_st &self)
{
  return self.error();
}

static inline gearman_return_t gearman_universal_error_code(const gearman_universal_st &self)
{
  return self.error_code();
}

static inline int gearman_universal_errno(const gearman_universal_st &self)
{
  return self.last_errno();
}

static inline void universal_reset_error(gearman_universal_st &self)
{
  self.reset_error();
}
