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
 * @brief Connection Definitions
 */

#include "gear_config.h"
#include "libgearman/common.h"

#include "libgearman/assert.hpp"

#include "libgearman/interface/push.hpp"
#include "libgearman/log.hpp"

#include "libgearman/protocol/option.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unistd.h>

#if HAVE_NETINET_TCP_H
# include <netinet/tcp.h>    /* for TCP_NODELAY */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifndef SOCK_CLOEXEC 
# define SOCK_CLOEXEC 0
#endif

#ifndef SOCK_NONBLOCK 
# define SOCK_NONBLOCK 0
#endif

#ifndef FD_CLOEXEC
# define FD_CLOEXEC 0
#endif

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif


/**
 * @addtogroup gearman_connection_static Static Connection Declarations
 * @ingroup gearman_connection
 * @{
 */

gearman_connection_st::gearman_connection_st(gearman_universal_st &universal_arg, const char* host_, const char* service_):
  state(GEARMAN_CON_UNIVERSAL_ADDRINFO),
  send_state(GEARMAN_CON_SEND_STATE_NONE),
  recv_state(GEARMAN_CON_RECV_UNIVERSAL_NONE),
  _events(0),
  _revents(0),
  fd(INVALID_SOCKET),
  _ssl(NULL),
  cached_errno(0),
  created_id(0),
  created_id_next(0),
  send_buffer_size(0),
  send_data_size(0),
  send_data_offset(0),
  recv_buffer_size(0),
  recv_data_size(0),
  recv_data_offset(0),
  universal(universal_arg),
  next(NULL),
  prev(NULL),
  context(NULL),
  _addrinfo(NULL),
  addrinfo_next(NULL),
  send_buffer_ptr(NULL),
  _recv_packet(NULL)
{
  set_host(host_, service_);

  if (universal.con_list)
  {
    universal.con_list->prev= this;
  }
  next= universal.con_list;
  universal.con_list= this;
  universal.con_count++;

  send_buffer_ptr= send_buffer;
  recv_buffer_ptr= recv_buffer;
}

gearman_connection_st *gearman_connection_create(gearman_universal_st& universal,
                                                 const char *host_, const char* service_)
{
  gearman_connection_st *connection= new (std::nothrow) gearman_connection_st(universal, host_, service_);
  if (connection)
  {
    if (gearman_failed(connection->lookup()))
    {
      delete connection;
      return NULL;
    }
  }
  else
  {
    gearman_perror(universal, errno, "Failed at new() gearman_connection_st new");
  }

  return connection;
}

gearman_connection_st *gearman_connection_create(gearman_universal_st& universal,
                                                 const char *host, const in_port_t& port_)
{
  const char *service_ptr= NULL;
  char service[GEARMAN_NI_MAXSERV]= { 0 };

  if (port_ < 1)
  {
    service_ptr= GEARMAN_DEFAULT_TCP_PORT_STRING;
  }
  else
  {
    snprintf(service, sizeof(service), "%hu", uint16_t(port_));
    service[GEARMAN_NI_MAXSERV -1]= 0;

    service_ptr= service;
  }

  return gearman_connection_create(universal, host, service_ptr);
}

gearman_connection_st *gearman_connection_copy(gearman_universal_st& universal,
                                               const gearman_connection_st& from)
{
  gearman_connection_st *connection= new (std::nothrow) gearman_connection_st(universal, from.host(), from.service());
  if (connection)
  {
    connection->options.ready= from.options.ready;
    // @todo Is this right?
    connection->options.packet_in_use= from.options.packet_in_use;

    strcpy(connection->_host, from._host);
    strcpy(connection->_service, from._service);
  }
  else
  {
    gearman_perror(universal, errno, "Failed at new() gearman_connection_st new");
  }

  return connection;
}

gearman_connection_st::~gearman_connection_st()
{
  close_socket();

  reset_addrinfo();

  { // Remove from universal list
    if (universal.con_list == this)
    {
      universal.con_list= next;
    }

    if (prev)
    {
      prev->next= next;
    }

    if (next)
    {
      next->prev= prev;
    }

    universal.con_count--;
  }

  free_private_packet();
}

void gearman_connection_st::free_private_packet()
{
  if (options.packet_in_use)
  {
    gearman_packet_free(&_packet);
    options.packet_in_use= false;
  }
}

/** @} */

/*
 * Public Definitions
 */

void gearman_connection_st::set_host(const char *host_, const in_port_t port_)
{
  if (port_ < 1)
  {
    set_host(host_, GEARMAN_DEFAULT_TCP_PORT_STRING);
  }
  else
  {
    snprintf(_service, sizeof(_service), "%hu", uint16_t(port_));
    _service[GEARMAN_NI_MAXSERV -1]= 0;

    set_host(host_, _service);
  }
}

void gearman_connection_st::set_host(const char *host_, const char* service_)
{
  reset_addrinfo();

  if (host_ and host_[0])
  { }
  else
  {
    host_= GEARMAN_DEFAULT_TCP_HOST;
  }
  strncpy(_host, host_, GEARMAN_NI_MAXHOST);
  _host[GEARMAN_NI_MAXHOST -1]= 0;

  if (service_ and service_[0])
  { }
  else
  {
    service_= GEARMAN_DEFAULT_TCP_PORT_STRING;
  }
  strncpy(_service, service_, GEARMAN_NI_MAXSERV);
  _service[GEARMAN_NI_MAXSERV -1]= 0;
}

/*
  Do not call error within this function.
*/
void gearman_connection_st::close_socket()
{
  if (_ssl)
  {
#if defined(HAVE_SSL) && HAVE_SSL
    SSL_shutdown(_ssl);
    SSL_free(_ssl);
    _ssl= NULL;
#endif
  }

  if (fd != INVALID_SOCKET)
  {
    /* in case of death shutdown to avoid blocking at close_socket() */
    if (shutdown(fd, SHUT_RDWR) == SOCKET_ERROR && get_socket_errno() != ENOTCONN)
    { }
    else
    {
      if (closesocket(fd) == SOCKET_ERROR)
      { }
    }

    state= GEARMAN_CON_UNIVERSAL_CONNECT;
    fd= INVALID_SOCKET;
    _events= 0;
    _revents= 0;

    send_state= GEARMAN_CON_SEND_STATE_NONE;
    send_buffer_ptr= send_buffer;
    send_buffer_size= 0;
    send_data_size= 0;
    send_data_offset= 0;

    recv_state= GEARMAN_CON_RECV_UNIVERSAL_NONE;
    free_recv_packet();

    recv_buffer_ptr= recv_buffer;
    recv_buffer_size= 0;

    options.server_options_sent= false;

    // created_id_next is incremented for every outbound packet (except status).
    // created_id is incremented for every response packet received, and also when
    // no packets are received due to an error. There are lots of such error paths
    // and it seems simpler to just reset these both to zero when a connection is
    // 'closed'.
    created_id= 0;
    created_id_next= 0;
  }
}

void gearman_connection_st::free_recv_packet()
{
  if (_recv_packet)
  {
    gearman_packet_free(recv_packet());
    _recv_packet= NULL;
  }
}

void gearman_connection_st::reset_addrinfo()
{
  if (_addrinfo)
  {
    freeaddrinfo(_addrinfo);
    _addrinfo= NULL;
  }

  addrinfo_next= NULL;
}

gearman_return_t gearman_connection_st::send_identifier(void)
{
  if (universal._identifier)
  {
    options.identifier_sent= false;
    const void* id= (void*)universal._identifier->value();
    size_t id_size= universal._identifier->size();

    gearman_packet_st packet;
    gearman_return_t ret= gearman_packet_create_args(universal, packet, GEARMAN_MAGIC_REQUEST, GEARMAN_COMMAND_SET_CLIENT_ID, (const void**)&id, &id_size, 1);

    if (gearman_success(ret))
    {
      PUSH_BLOCKING(universal);

      options.identifier_sent= true;
      gearman_return_t local_ret= send_packet(packet, true);
      if (gearman_failed(local_ret))
      {
        options.identifier_sent= false;
        ret= local_ret;
      }
      else
      {
        options.identifier_sent= true;
      }
    }

    gearman_packet_free(&packet);

    return ret;
  }

  return GEARMAN_SUCCESS;
}


/*
 * The send_packet() method does not only send the passed-in packet_arg. If there are any server options
 * established, and they haven't yet been sent over, then these options are sent over first.
 * Only if that succeeds is the packet_arg sent.
 * The reason for this is server options are only set once by the client/worker. In the older code, this
 * resulted in them being sent over exactly once. If the connection was dropped and rebuilt, then the options
 * were not sent over again, rendering them moot. This way, we're guaranteed that the options are always sent
 * at least once to a connected server.
 */
gearman_return_t gearman_connection_st::send_packet(const gearman_packet_st& packet_arg, const bool flush_buffer)
{
  if (options.identifier_sent == false)
  {
    gearman_return_t ret= send_identifier();
    if (gearman_failed(ret))
    {
      return ret;
    }
    options.identifier_sent= true;
  }

  if (options.server_options_sent == false)
  {
    for (gearman_server_options_st* head= universal.server_options_list;
         head;
         head= head->next)
    {
      gearman_packet_st message;
      gearman_string_t option= { (const char*)head->value(), head->size() };
      gearman_return_t ret=  libgearman::protocol::option(universal, message, option);
      if (gearman_failed(ret))
      {
        assert(universal.error_code());
        assert(universal.error());
        gearman_packet_free(&message);
        return universal.error_code();
      }

      PUSH_BLOCKING(universal);
      OptionCheck check(universal, option);
      ret= _send_packet(message, true);
      if (gearman_failed(ret))
      {
        assert(universal.error_code());
        assert(universal.error());
        gearman_packet_free(&message);
        return universal.error_code();
      }

      options.packet_in_use= true;
      gearman_packet_st *packet_ptr= receiving(_packet, ret, true);
      if (packet_ptr == NULL)
      {
        gearman_packet_free(&message);
        options.packet_in_use= false;
        return gearman_error(universal, ret, "Failed in receiving()");
      }

      if (gearman_failed(ret) ||
          gearman_failed(ret= check.success(this)))
      {
        gearman_packet_free(&message);
        free_private_packet();
        reset_recv_packet();
        return ret;
      }

      free_private_packet();
      reset_recv_packet();
      gearman_packet_free(&message);
    }

    options.server_options_sent= true;
  }

  return _send_packet(packet_arg, flush_buffer);
}

/*
 * This is the real implementation that actually sends a packet. Read the comments for send_packet() for why
 * that is. Note that this is a private method. External callers should only call send_packet().
 */
#if __GNUC__ >= 7
  #pragma GCC diagnostic warning "-Wimplicit-fallthrough"
#endif
gearman_return_t gearman_connection_st::_send_packet(const gearman_packet_st& packet_arg, const bool flush_buffer)
{
  switch (send_state)
  {
  case GEARMAN_CON_SEND_STATE_NONE:
    assert_msg(packet_arg.universal, "send_packet() was to execute against a packet with no universal");
    universal_reset_error(*(packet_arg.universal));
    if (packet_arg.options.complete == false)
    {
      return gearman_error(universal, GEARMAN_INVALID_PACKET, "packet not complete");
    }

    /* Pack first part of packet, which is everything but the payload. */
    while (1)
    {
      { // Scoping to shut compiler up about switch/case jump
        gearman_return_t rc;
        size_t send_size= gearman_packet_pack(packet_arg,
                                              send_buffer +send_buffer_size,
                                              GEARMAN_SEND_BUFFER_SIZE -send_buffer_size, rc);

        if (gearman_success(rc))
        {
          send_buffer_size+= send_size;
          break;
        }
        else if (rc != GEARMAN_FLUSH_DATA)
        {
          return rc;
        }
      }

      /* We were asked to flush when the buffer is already flushed! */
      if (send_buffer_size == 0)
      {
        return gearman_universal_set_error(universal, GEARMAN_SEND_BUFFER_TOO_SMALL, GEARMAN_AT,
                                           "send buffer too small (%u)", GEARMAN_SEND_BUFFER_SIZE);
      }

      /* Flush buffer now if first part of packet won't fit in. */
      send_state= GEARMAN_CON_SEND_UNIVERSAL_PRE_FLUSH;

    case GEARMAN_CON_SEND_UNIVERSAL_PRE_FLUSH:
      {
        gearman_return_t ret= flush();
        if (gearman_failed(ret))
        {
          return ret;
        }
      }
    }

    /* Return here if we have no data to send. */
    if (packet_arg.data_size == 0)
    {
      break;
    }

    /* If there is any room in the buffer, copy in data. */
    if (packet_arg.data and (GEARMAN_SEND_BUFFER_SIZE - send_buffer_size) > 0)
    {
      send_data_offset= GEARMAN_SEND_BUFFER_SIZE - send_buffer_size;
      if (send_data_offset > packet_arg.data_size)
      {
        send_data_offset= packet_arg.data_size;
      }

      memcpy(send_buffer + send_buffer_size, packet_arg.data, send_data_offset);
      send_buffer_size+= send_data_offset;

      /* Return if all data fit in the send buffer. */
      if (send_data_offset == packet_arg.data_size)
      {
        send_data_offset= 0;
        break;
      }
    }

    /* Flush buffer now so we can start writing directly from data buffer. */
    send_state= GEARMAN_CON_SEND_UNIVERSAL_FORCE_FLUSH;

  case GEARMAN_CON_SEND_UNIVERSAL_FORCE_FLUSH:
    {
      gearman_return_t ret= flush();
      if (gearman_failed(ret))
      {
        return ret;
      }
    }

    send_data_size= packet_arg.data_size;

    /* If this is NULL, then gearman_connection_send_data function will be used. */
    if (packet_arg.data == NULL)
    {
      send_state= GEARMAN_CON_SEND_UNIVERSAL_FLUSH_DATA;
      return GEARMAN_SUCCESS;
    }

    /* Copy into the buffer if it fits, otherwise flush from packet buffer. */
    send_buffer_size= packet_arg.data_size - send_data_offset;
    if (send_buffer_size < GEARMAN_SEND_BUFFER_SIZE)
    {
      memcpy(send_buffer,
             (const char*)(packet_arg.data) +send_data_offset,
             send_buffer_size);
      send_data_size= 0;
      send_data_offset= 0;
      break;
    }

    send_buffer_ptr= (const char*)(size_t(packet_arg.data) +send_data_offset);
    send_state= GEARMAN_CON_SEND_UNIVERSAL_FLUSH_DATA;

  case GEARMAN_CON_SEND_UNIVERSAL_FLUSH:
  case GEARMAN_CON_SEND_UNIVERSAL_FLUSH_DATA:
    return flush();
  }

  if (flush_buffer)
  {
    send_state= GEARMAN_CON_SEND_UNIVERSAL_FLUSH;
    return flush();
  }

  send_state= GEARMAN_CON_SEND_STATE_NONE;

  return GEARMAN_SUCCESS;
}

size_t gearman_connection_st::send_and_flush(const void *data, size_t data_size, gearman_return_t *ret_ptr)
{
  if (send_state != GEARMAN_CON_SEND_UNIVERSAL_FLUSH_DATA)
  {
    return gearman_error(universal, GEARMAN_NOT_FLUSHING, "not flushing");
  }

  if (data_size > (send_data_size - send_data_offset))
  {
    return gearman_error(universal, GEARMAN_DATA_TOO_LARGE, "data too large");
  }

  send_buffer_ptr= (const char*)data;
  send_buffer_size= data_size;

  *ret_ptr= flush();

  return data_size -send_buffer_size;
}

gearman_return_t gearman_connection_st::lookup()
{
  reset_addrinfo();

  struct addrinfo ai;
  memset(&ai, 0, sizeof(struct addrinfo));
  ai.ai_socktype= SOCK_STREAM;
  ai.ai_protocol= IPPROTO_TCP;

  assert_msg(_addrinfo == NULL, "Programmer error, reset_addrinfo() is either broke, or was not called.");
  int ret;
  if ((ret= getaddrinfo(_host, _service, &ai, &(_addrinfo))))
  {
    int local_errno= errno;
    reset_addrinfo();
    switch (ret)
    {
      case EAI_AGAIN:
        return gearman_universal_set_error(universal, GEARMAN_TIMEOUT, GEARMAN_AT, "Nameserver timed out while looking up %s:%s", host(), service());

      case EAI_MEMORY:
        return gearman_universal_set_error(universal, GEARMAN_MEMORY_ALLOCATION_FAILURE, GEARMAN_AT,
                                           "A memory allocation failed while calling getaddrinfo() for %s:%s", host(), service());

      case EAI_SYSTEM:
        if (local_errno == ENOENT)
        {
          return gearman_universal_set_error(universal, GEARMAN_GETADDRINFO, GEARMAN_AT, "DNS lookup failed for %s:%s", host(), service());
        }
        return gearman_universal_set_perror(universal, GEARMAN_ERRNO, local_errno, GEARMAN_AT, "System error happened during a call to getaddrinfo() for %s:%s", host(), service());

      default:
        break;
    }

    return gearman_universal_set_error(universal, GEARMAN_GETADDRINFO, GEARMAN_AT, "%s:%s getaddrinfo:%s", host(), service(), gai_strerror(ret));
  }

  addrinfo_next= _addrinfo;
  assert(addrinfo_next);
  state= GEARMAN_CON_UNIVERSAL_CONNECT;

  return GEARMAN_SUCCESS;
}

gearman_return_t gearman_connection_st::enable_ssl()
{
#if defined(HAVE_SSL) && HAVE_SSL
  if (universal.ssl())
  {
    _ssl= SSL_new(universal.ctx_ssl());
    if (_ssl == NULL)
    {
      close_socket();
      return gearman_error(universal, GEARMAN_COULD_NOT_CONNECT, "wolfSSL_new() failed to return a valid object");
    }

    if (SSL_set_fd(_ssl, fd) != SSL_SUCCESS)
    {
      close_socket();
      char errorString[SSL_ERROR_SIZE];
      ERR_error_string_n(SSL_get_error(_ssl, 0), errorString, sizeof(errorString));
      return gearman_error(universal, GEARMAN_COULD_NOT_CONNECT, errorString);
    }
  }
#endif

  return GEARMAN_SUCCESS;
}

#if __GNUC__ >= 7
  #pragma GCC diagnostic warning "-Wimplicit-fallthrough"
#endif
gearman_return_t gearman_connection_st::flush()
{
  while (1)
  {
    switch (state)
    {
    case GEARMAN_CON_UNIVERSAL_ADDRINFO:
      {
        gearman_return_t ret= lookup();

        if (gearman_failed(ret))
        {
          return ret;
        }
      }

    case GEARMAN_CON_UNIVERSAL_CONNECT:
      if (fd != INVALID_SOCKET)
      {
        close_socket();
      }

      if (addrinfo_next == NULL)
      {
        state= GEARMAN_CON_UNIVERSAL_ADDRINFO;
        return gearman_universal_set_error(universal, GEARMAN_COULD_NOT_CONNECT, GEARMAN_AT, "Connection to %s:%s failed", _host, _service);
      }

      // rewrite tye if HAVE_SOCK_CLOEXEC
      fd= socket(addrinfo_next->ai_family, addrinfo_next->ai_socktype|SOCK_CLOEXEC|SOCK_NONBLOCK, addrinfo_next->ai_protocol);

      if (fd == INVALID_SOCKET)
      {
        state= GEARMAN_CON_UNIVERSAL_ADDRINFO;
        return gearman_perror(universal, errno, "socket");
      }

      {
        gearman_return_t gret= set_socket_options();
        if (gearman_failed(gret))
        {
          close_socket();
          return gret;
        }
      }

      while (1)
      {
        if (connect(fd, addrinfo_next->ai_addr, addrinfo_next->ai_addrlen) == 0)
        {
          state= GEARMAN_CON_UNIVERSAL_CONNECTED;
#if 0
          addrinfo_next= NULL;
#endif

          break;
        }

        switch (errno)
        {
          // Treat as an async connect
        case EINTR:
        case EINPROGRESS:
            state= GEARMAN_CON_UNIVERSAL_CONNECTING;
            break;

        case ECONNREFUSED:
        case ENETUNREACH:
        case ETIMEDOUT:
          addrinfo_next= addrinfo_next->ai_next;

          // We will treat this as an error but retry the address
        case EAGAIN:
          state= GEARMAN_CON_UNIVERSAL_CONNECT;
          close_socket();
          break;

        default:
          gearman_perror(universal, errno, "connect");
          close_socket();
          return gearman_universal_set_error(universal, GEARMAN_COULD_NOT_CONNECT, GEARMAN_AT, "%s:%s", _host, _service);
        }

        break;
      }

      if (state != GEARMAN_CON_UNIVERSAL_CONNECTING)
      {
        break;
      }

    case GEARMAN_CON_UNIVERSAL_CONNECTING:
      while (1)
      {
        if (_revents & (POLLERR | POLLHUP | POLLNVAL))
        {
          state= GEARMAN_CON_UNIVERSAL_CONNECT;
          addrinfo_next= addrinfo_next->ai_next;
          break;
        }
        else if (_revents & POLLOUT)
        {
          state= GEARMAN_CON_UNIVERSAL_CONNECTED;
          gearman_return_t ssl_ret;
          if ((ssl_ret= enable_ssl()) != GEARMAN_SUCCESS)
          {
            return ssl_ret;
          }

          break;
        }

        set_events(POLLOUT);

        if (universal.is_non_blocking())
        {
          state= GEARMAN_CON_UNIVERSAL_CONNECTING;
          return gearman_gerror(universal, GEARMAN_IO_WAIT);
        }

        gearman_return_t gret= gearman_wait(universal);
        if (gearman_failed(gret))
        {
          return gret;
        }
      }

      if (state != GEARMAN_CON_UNIVERSAL_CONNECTED)
      {
        break;
      }

    case GEARMAN_CON_UNIVERSAL_CONNECTED:
      while (send_buffer_size != 0)
      {
        ssize_t write_size;
#if defined(HAVE_SSL) && HAVE_SSL
        if (_ssl)
        {
#if defined(HAVE_WOLFSSL) && HAVE_WOLFSSL
          write_size= wolfSSL_send(_ssl, send_buffer_ptr, int(send_buffer_size), MSG_NOSIGNAL);
#elif defined(HAVE_OPENSSL) && HAVE_OPENSSL
          write_size= SSL_write(_ssl, send_buffer_ptr, int(send_buffer_size));
#endif
          int ssl_error;
          switch ((ssl_error= SSL_get_error(_ssl, int(write_size))))
          {
            case SSL_ERROR_NONE:
              break;

            case SSL_ERROR_ZERO_RETURN:
              errno= ECONNRESET;
              write_size= SOCKET_ERROR;
              break;

            case SSL_ERROR_WANT_CONNECT:
            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_X509_LOOKUP:
              errno= EAGAIN;
              write_size= SOCKET_ERROR;
              break;

            case SSL_ERROR_SYSCALL:
              if (errno) // If errno is really set, then let our normal error logic handle.
              {
                write_size= SOCKET_ERROR;
                break;
              }

            case SSL_ERROR_SSL:
            default:
              {
                char errorString[80];
                ERR_error_string_n(ssl_error, errorString, sizeof(errorString));
                close_socket();
                return gearman_universal_set_error(universal, GEARMAN_LOST_CONNECTION, GEARMAN_AT, "SSL failure(%s)", errorString);
              }
          }
        }
        else
#endif // define(HAVE_SSL)
        {
          write_size= ::send(fd, send_buffer_ptr, send_buffer_size, MSG_NOSIGNAL);
        }

        if (write_size == 0) // Zero value on send()
        { }
        else if (write_size == SOCKET_ERROR)
        {
          switch (errno)
          {
            case ENOTCONN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
            case EWOULDBLOCK:
#endif
            case EAGAIN:
              {
                set_events(POLLOUT);

                if (gearman_universal_is_non_blocking(universal))
                {
                  return gearman_gerror(universal, GEARMAN_IO_WAIT);
                }

                gearman_return_t gret= gearman_wait(universal);
                if (gearman_failed(gret))
                {
                  return gret;
                }

                continue;
              }
            case EPIPE:
              {
                gearman_return_t ret= gearman_perror(universal, errno, "lost connection to server during send");
                close_socket();
                return ret;
              }
            case ECONNRESET:
            case EHOSTDOWN:
              {
                gearman_return_t ret= gearman_perror(universal, errno, "lost connection to server during send");
                close_socket();
                return ret;
              }
            default:
              break;
          }

          gearman_return_t ret= gearman_perror(universal, errno, "send");
          close_socket();

          return ret;
        }
        gearman_log_debug(universal, "connection sent %u bytes of data", uint32_t(write_size));

        send_buffer_size-= size_t(write_size);
        if (send_state == GEARMAN_CON_SEND_UNIVERSAL_FLUSH_DATA)
        {
          send_data_offset+= size_t(write_size);
          if (send_data_offset == send_data_size)
          {
            send_data_size= 0;
            send_data_offset= 0;
            break;
          }

          if (send_buffer_size == 0)
          {
            return GEARMAN_SUCCESS;
          }
        }
        else if (send_buffer_size == 0)
        {
          break;
        }

        send_buffer_ptr+= write_size;
      }

      send_state= GEARMAN_CON_SEND_STATE_NONE;
      send_buffer_ptr= send_buffer;

      return GEARMAN_SUCCESS;
    }
  }
}

#if __GNUC__ >= 7
  #pragma GCC diagnostic warning "-Wimplicit-fallthrough"
#endif
gearman_packet_st *gearman_connection_st::receiving(gearman_packet_st& packet_arg,
                                                    gearman_return_t& ret,
                                                    const bool recv_data)
{
  switch (recv_state)
  {
  case GEARMAN_CON_RECV_UNIVERSAL_NONE:
    if (state != GEARMAN_CON_UNIVERSAL_CONNECTED)
    {
      ret= gearman_error(universal, GEARMAN_NOT_CONNECTED, "not connected");
      return NULL;
    }

    // This should not fail
    _recv_packet= gearman_packet_create(universal, packet_arg);
    assert(_recv_packet == &packet_arg);
    if (_recv_packet == NULL)
    {
      ret= gearman_error(universal, GEARMAN_MEMORY_ALLOCATION_FAILURE, "Programmer error, gearman_packet_create() failed which should not be possible");
      return NULL;
    }

    recv_state= GEARMAN_CON_RECV_UNIVERSAL_READ;

  case GEARMAN_CON_RECV_UNIVERSAL_READ:
    while (1)
    {
      // If we have data, see if it is a complete packet
      if (recv_buffer_size > 0)
      {
        assert(recv_packet());
        size_t recv_size= gearman_packet_unpack(*(recv_packet()),
                                                recv_buffer_ptr,
                                                recv_buffer_size, ret);
        recv_buffer_ptr+= recv_size;
        recv_buffer_size-= recv_size;

        if (gearman_success(ret))
        {
          break;
        }
        else if (ret != GEARMAN_IO_WAIT)
        {
          assert(universal.error_code());
          close_socket();
          return NULL;
        }
      }

      /* Shift buffer contents if needed. */
      if (recv_buffer_size > 0)
      {
        memmove(recv_buffer, recv_buffer_ptr, recv_buffer_size);
      }
      recv_buffer_ptr= recv_buffer;

      size_t recv_size= recv_socket(recv_buffer +recv_buffer_size, GEARMAN_RECV_BUFFER_SIZE -recv_buffer_size, ret);
      if (gearman_failed(ret))
      {
        if (ret != GEARMAN_IO_WAIT) {
          recv_state= GEARMAN_CON_RECV_UNIVERSAL_NONE;
        }
        return NULL;
      }

      recv_buffer_size+= recv_size;
    }

    if (packet_arg.data_size == 0)
    {
      recv_state= GEARMAN_CON_RECV_UNIVERSAL_NONE;
      break;
    }

    recv_data_size= packet_arg.data_size;

    if (recv_data == false )
    {
      recv_state= GEARMAN_CON_RECV_STATE_READ_DATA;
      break;
    }

    assert(packet_arg.universal);
    packet_arg.data= gearman_malloc((*packet_arg.universal), packet_arg.data_size);
    if (packet_arg.data == NULL)
    {
      ret= gearman_error(universal, GEARMAN_MEMORY_ALLOCATION_FAILURE, "gearman_malloc((*packet_arg.universal), packet_arg.data_size)");
      close_socket();
      return NULL;
    }

    packet_arg.options.free_data= true;
    recv_state= GEARMAN_CON_RECV_STATE_READ_DATA;

  case GEARMAN_CON_RECV_STATE_READ_DATA:
    while (recv_data_size)
    {
      (void)receive_data(static_cast<uint8_t *>(const_cast<void *>(packet_arg.data)) +
                         recv_data_offset,
                         packet_arg.data_size -recv_data_offset, ret);
      if (gearman_failed(ret))
      {
        return NULL;
      }
    }

    recv_state= GEARMAN_CON_RECV_UNIVERSAL_NONE;
    break;
  }

  gearman_packet_st *tmp_packet_arg= recv_packet();
  reset_recv_packet();

  return tmp_packet_arg;
}

size_t gearman_connection_st::receive_data(void *data, size_t data_size, gearman_return_t& ret)
{
  size_t recv_size= 0;

  if (recv_data_size == 0)
  {
    ret= GEARMAN_SUCCESS;
    return 0;
  }

  if ((recv_data_size - recv_data_offset) < data_size)
  {
    data_size= recv_data_size - recv_data_offset;
  }

  if (recv_buffer_size > 0)
  {
    if (recv_buffer_size < data_size)
    {
      recv_size= recv_buffer_size;
    }
    else
    {
      recv_size= data_size;
    }

    memcpy(data, recv_buffer_ptr, recv_size);
    recv_buffer_ptr+= recv_size;
    recv_buffer_size-= recv_size;
  }

  if (data_size != recv_size)
  {
    // @note fix this to test for error before blindly doing this opperation
    recv_size+= recv_socket(static_cast<uint8_t *>(const_cast<void *>(data)) + recv_size, data_size - recv_size, ret);
    recv_data_offset+= recv_size;
  }
  else
  {
    recv_data_offset+= recv_size;
    ret= GEARMAN_SUCCESS;
  }

  if (recv_data_size == recv_data_offset)
  {
    recv_data_size= 0;
    recv_data_offset= 0;
    recv_state= GEARMAN_CON_RECV_UNIVERSAL_NONE;
  }

  return recv_size;
}

size_t gearman_connection_st::recv_socket(void *data, size_t data_size, gearman_return_t& ret)
{
  ssize_t read_size;

  while (1)
  {
#if defined(HAVE_SSL) && HAVE_SSL
    if (_ssl)
    {
# if defined(HAVE_WOLFSSL) && HAVE_WOLFSSL
      read_size= wolfSSL_recv(_ssl, data, int(data_size), MSG_NOSIGNAL);
# elif defined(HAVE_OPENSSL) && HAVE_OPENSSL
      read_size= SSL_read(_ssl, data, int(data_size));
# endif
      int ssl_error;
      switch ((ssl_error= SSL_get_error(_ssl, int(read_size))))
      {
        case SSL_ERROR_NONE:
          break;

        case SSL_ERROR_ZERO_RETURN:
          read_size= 0;
          break;

        case SSL_ERROR_WANT_CONNECT:
        case SSL_ERROR_WANT_ACCEPT:
        case SSL_ERROR_WANT_WRITE:
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_X509_LOOKUP:
          errno= EAGAIN;
          read_size= SOCKET_ERROR;
          break;

        case SSL_ERROR_SYSCALL:
          if (errno) // If errno is really set, then let our normal error logic handle.
          {
            read_size= SOCKET_ERROR;
            break;
          }

        case SSL_ERROR_SSL:
        default:
          {
            char errorString[80];
            ERR_error_string_n(ssl_error, errorString, sizeof(errorString));
            close_socket();
            return gearman_universal_set_error(universal, GEARMAN_LOST_CONNECTION, GEARMAN_AT, "SSL failure(%s)", errorString);
          }
      }
    }
    else
#endif // defined(HAVE_SSL)
    {
      read_size= ::recv(fd, data, data_size, MSG_NOSIGNAL);
    }

    if (read_size == 0)
    {
      ret= gearman_error(universal, GEARMAN_LOST_CONNECTION, "lost connection to server (EOF)");
      close_socket();

      return 0;
    }
    else if (read_size == SOCKET_ERROR)
    {
      if (errno == EAGAIN or errno == ENOTCONN)
      {
        set_events(POLLIN);

        if (universal.is_non_blocking())
        {
          ret= gearman_gerror(universal, GEARMAN_IO_WAIT);
          return 0;
        }

        ret= gearman_wait(universal);

        if (gearman_failed(ret))
        {
          if (ret == GEARMAN_SHUTDOWN)
          {
            close_socket();
          }

          return 0;
        }

        continue;
      }
      else if (errno == EINTR)
      {
        continue;
      }
      else if (errno == EPIPE or errno == ECONNRESET or errno == EHOSTDOWN)
      {
        ret= gearman_perror(universal, errno, "lost connection to server during read");
      }
      else
      {
        ret= gearman_perror(universal, errno, "recv");
      }

      close_socket();
      return 0;
    }

    break;
  }

  ret= GEARMAN_SUCCESS;
  return size_t(read_size);
}

void gearman_connection_st::set_events(short arg)
{
  if ((_events | arg) == _events)
  {
    return;
  }

  _events|= arg;
}

void gearman_connection_st::set_revents(short arg)
{
  if (arg)
  {
    options.ready= true;
  }

  _revents= arg;
  _events&= short(~arg);
}

/*
 * Static Definitions
 */

gearman_return_t gearman_connection_st::set_socket_options()
{
  if (SOCK_CLOEXEC == 0)
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
          rval= fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
        } while (rval == -1 && (errno == EINTR or errno == EAGAIN));

        if (rval == -1)
        {
          gearman_perror(universal, errno, "fcntl (fd, F_SETFD, flags | FD_CLOEXEC)");
        }
      }
      else
      {
        gearman_perror(universal, errno, "fcntl(fd, F_GETFD, 0)");
      }
    }
  }

  {
    int ret= 1;
    ret= setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &ret,
                    socklen_t(sizeof(int)));
    if (ret == -1 && errno != EOPNOTSUPP)
    {
      return gearman_perror(universal, errno, "setsockopt(TCP_NODELAY)");
    }
  }

  {
    struct linger linger;
    linger.l_onoff= 1;
    linger.l_linger= GEARMAN_DEFAULT_SOCKET_TIMEOUT;
    int ret= setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger,
                        socklen_t(sizeof(struct linger)));
    if (ret == -1)
    {
      return gearman_perror(universal, errno, "setsockopt(SO_LINGER)");
    }
  }

#if 0
  if (0)
  {
    struct timeval waittime;
    waittime.tv_sec= GEARMAN_DEFAULT_SOCKET_TIMEOUT;
    waittime.tv_usec= 0;
    int ret= setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &waittime,
                        socklen_t(sizeof(struct timeval)));
    if (ret == -1 && errno != ENOPROTOOPT)
    {
      gearman_perror(universal, errno, "setsockopt(SO_SNDTIMEO)");
      return GEARMAN_ERRNO;
    }

    ret= setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &waittime,
                    socklen_t(sizeof(struct timeval)));
    if (ret == -1 && errno != ENOPROTOOPT)
    {
      gearman_perror(universal, errno, "setsockopt(SO_RCVTIMEO)");
      return GEARMAN_ERRNO;
    }
  }
#endif

  {
    int optval= 1;
    int ret= setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
    if (ret == -1 && errno != ENOPROTOOPT)
    {
      return gearman_perror(universal, errno, "setsockopt(SO_KEEPALIVE)");
    }
  }

  {
    int ret= GEARMAN_DEFAULT_SOCKET_SEND_SIZE;
    ret= setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &ret, socklen_t(sizeof(int)));
    if (ret == -1)
    {
      return gearman_perror(universal, errno, "setsockopt(SO_SNDBUF)");
    }
  }

#if defined(SO_NOSIGPIPE)
  if (SO_NOSIGPIPE)
  {
    int ret= 1;
    ret= setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, static_cast<void *>(&ret), sizeof(int));

    // This is not considered a fatal error 
    if (ret == -1)
    {
      gearman_perror(universal, errno, "setsockopt(SO_NOSIGPIPE)");
    }
  }
#endif

  {
    int ret= GEARMAN_DEFAULT_SOCKET_RECV_SIZE;
    ret= setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &ret, socklen_t(sizeof(int)));
    if (ret == -1)
    {
      return gearman_perror(universal, errno, "setsockopt(SO_RCVBUF)");
    }
  }

  // If SOCK_NONBLOCK doesn't work, just enable non_block via fcntl
  if (SOCK_NONBLOCK == 0)
  {
    int flags;
    do
    {
      flags= fcntl(fd, F_GETFL, 0);
    } while (flags == -1 and (errno == EINTR or errno == EAGAIN));

    if (flags == -1)
    {
      return gearman_perror(universal, errno, "fcntl(F_GETFL)");
    }
    else if ((flags & O_NONBLOCK) == 0)
    {
      int retval;
      do
      {
        retval= fcntl(fd, F_SETFL, flags | O_NONBLOCK);
      } while (retval == -1 and (errno == EINTR or errno == EAGAIN));

      if (retval == -1)
      {
        return gearman_perror(universal, errno, "fcntl(F_SETFL)");
      }
    }
  }

  return GEARMAN_SUCCESS;
}
