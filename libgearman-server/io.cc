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
#include "libgearman-server/common.h"
#include <libgearman-server/plugins/base.h>

#include <cstring>
#include <cerrno>
#include <cassert>

#ifndef SOCK_NONBLOCK 
# define SOCK_NONBLOCK 0
#endif

#ifndef MSG_DONTWAIT
# define MSG_DONTWAIT 0
#endif

static void _connection_close(gearmand_io_st *connection)
{
  if (connection->has_fd())
  {
    assert(connection->options.external_fd);
    if (connection->options.external_fd)
    {
      connection->options.external_fd= false;
    }
    else
    {
      gearmand_log_fatal(GEARMAN_DEFAULT_LOG_PARAM, "We should never have an internal fd");
    }

    connection->clear();
    connection->_state= gearmand_io_st::GEARMAND_CON_UNIVERSAL_INVALID;

    connection->send_state= gearmand_io_st::GEARMAND_CON_SEND_STATE_NONE;
    connection->send_buffer_ptr= connection->send_buffer;
    connection->send_buffer_size= 0;
    connection->send_data_size= 0;
    connection->send_data_offset= 0;

    connection->recv_state= gearmand_io_st::GEARMAND_CON_RECV_UNIVERSAL_NONE;
    if (connection->recv_packet != NULL)
    {
      gearmand_packet_free(connection->recv_packet);
      connection->recv_packet= NULL;
    }

    connection->recv_buffer_ptr= connection->recv_buffer;
    connection->recv_buffer_size= 0;
  }
}


const char* gearmand_io_st::host() const
{
  if (context)
  {
    return context->host;
  }

  return "-";
}

const char* gearmand_io_st::port() const
{
  if (context)
  {
    return context->port;
  }

  return "-";
}

static size_t _connection_read(gearman_server_con_st *con, void *data, size_t data_size, gearmand_error_t &ret)
{
  ssize_t read_size;
  gearmand_io_st *connection= &con->con;

  while (1)
  {
#if defined(HAVE_SSL) && HAVE_SSL
    if (con->_ssl)
    {
# if defined(HAVE_WOLFSSL) && HAVE_WOLFSSL
      read_size= wolfSSL_recv(con->_ssl, data, int(data_size), MSG_DONTWAIT);
# else
      read_size= SSL_read(con->_ssl, data, int(data_size));
# endif
      assert(HAVE_SSL); // Just to make sure if macro is aligned.
      int ssl_error;
      switch ((ssl_error= SSL_get_error(con->_ssl, int(read_size))))
      {
        case SSL_ERROR_NONE:
          break;

        case SSL_ERROR_ZERO_RETURN:
          read_size= 0;
          break;

        case SSL_ERROR_WANT_CONNECT:
        case SSL_ERROR_WANT_ACCEPT:
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
        case SSL_ERROR_WANT_X509_LOOKUP:
          read_size= SOCKET_ERROR;
          errno= EAGAIN;
          break;

        case SSL_ERROR_SYSCALL:

          if (errno) // If errno is really set, then let our normal error logic handle.
          {
            read_size= SOCKET_ERROR;
            break;
          }

        case SSL_ERROR_SSL:
        default:
          { // All other errors
            char errorString[SSL_ERROR_SIZE];
            ERR_error_string_n(ssl_error, errorString, sizeof(errorString));
            ret= GEARMAND_LOST_CONNECTION;
            gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "SSL failure(%s) errno:%d", errorString);
            _connection_close(connection);

            return 0;
          }
      }
    }
    else
#endif
    {
      read_size= recv(connection->fd(), data, data_size, MSG_DONTWAIT);
    }

    if (read_size == 0)
    {
      ret= GEARMAND_LOST_CONNECTION;
      gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Peer connection has called close()");
      _connection_close(connection);
      return 0;
    }
    else if (read_size == SOCKET_ERROR)
    {
      int local_errno= errno;
      switch (local_errno)
      {
      case EAGAIN:
        ret= gearmand_io_set_events(con, POLLIN);
        if (gearmand_failed(ret))
        {
          gearmand_perror(local_errno, "recv");
          return 0;
        }

        ret= GEARMAND_IO_WAIT;
        return 0;

      case EINTR:
        continue;

      case EPIPE:
      case ECONNRESET:
      case EHOSTDOWN:
        {
          ret= GEARMAND_LOST_CONNECTION;
          gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Peer connection has called close()");
          _connection_close(connection);
          return 0;
        }

      default:
        ret= GEARMAND_ERRNO;
      }

      gearmand_perror(local_errno, "closing connection due to previous errno error");
      _connection_close(connection);
      return 0;
    }

    break;
  }

  ret= GEARMAND_SUCCESS;
  return size_t(read_size);
}

static gearmand_error_t gearmand_connection_recv_data(gearman_server_con_st *con, void *data, size_t data_size)
{
  gearmand_io_st *connection= &con->con;

  if (connection->recv_data_size == 0)
  {
    return GEARMAND_SUCCESS;
  }

  if ((connection->recv_data_size - connection->recv_data_offset) < data_size)
  {
    data_size= connection->recv_data_size - connection->recv_data_offset;
  }

  size_t recv_size= 0;
  if (connection->recv_buffer_size > 0)
  {
    if (connection->recv_buffer_size < data_size)
    {
      recv_size= connection->recv_buffer_size;
    }
    else
    {
      recv_size= data_size;
    }

    memcpy(data, connection->recv_buffer_ptr, recv_size);
    connection->recv_buffer_ptr+= recv_size;
    connection->recv_buffer_size-= recv_size;
  }

  gearmand_error_t ret;
  if (data_size != recv_size)
  {
    recv_size+= _connection_read(con, ((uint8_t *)data) + recv_size, data_size - recv_size, ret);
    connection->recv_data_offset+= recv_size;
  }
  else
  {
    connection->recv_data_offset+= recv_size;
    ret= GEARMAND_SUCCESS;
  }

  if (connection->recv_data_size == connection->recv_data_offset)
  {
    connection->recv_data_size= 0;
    connection->recv_data_offset= 0;
    connection->recv_state= gearmand_io_st::GEARMAND_CON_RECV_UNIVERSAL_NONE;
  }

  return ret;
}

static gearmand_error_t _connection_flush(gearman_server_con_st *con)
{
  gearmand_io_st *connection= &con->con;

  assert(connection->_state == gearmand_io_st::GEARMAND_CON_UNIVERSAL_CONNECTED);
  while (1)
  {
    switch (connection->_state)
    {
    case gearmand_io_st::GEARMAND_CON_UNIVERSAL_INVALID:
      assert(0);
      return GEARMAND_ERRNO;

    case gearmand_io_st::GEARMAND_CON_UNIVERSAL_CONNECTED:
      uint32_t loop_counter= 0;
      while (connection->send_buffer_size)
      {
        ssize_t write_size;
#if defined(HAVE_SSL) && HAVE_SSL
        if (con->_ssl)
        {
#if defined(HAVE_WOLFSSL) && HAVE_WOLFSSL
          write_size= wolfSSL_send(con->_ssl, connection->send_buffer_ptr, int(connection->send_buffer_size), MSG_NOSIGNAL|MSG_DONTWAIT);
#elif defined(HAVE_OPENSSL) && HAVE_OPENSSL
          write_size= SSL_write(con->_ssl, connection->send_buffer_ptr, int(connection->send_buffer_size));
#endif
          assert(HAVE_SSL); // Just to make sure if macro is aligned.

          int ssl_error;
          switch ((ssl_error= SSL_get_error(con->_ssl, int(write_size))))
          {
            case SSL_ERROR_NONE:
              break;

            case SSL_ERROR_ZERO_RETURN:
              errno= ECONNRESET;
              write_size= SOCKET_ERROR;
              break;

            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_CONNECT:
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_X509_LOOKUP:
              write_size= SOCKET_ERROR;
              errno= EAGAIN;
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
                char errorString[SSL_ERROR_SIZE];
                ERR_error_string_n(ssl_error, errorString, sizeof(errorString));
                _connection_close(connection);
                return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_LOST_CONNECTION, "SSL failure(%s)",
                                           errorString);
              }
          }
        }
        else
#endif
        {
          write_size= send(connection->fd(), connection->send_buffer_ptr, connection->send_buffer_size, MSG_NOSIGNAL|MSG_DONTWAIT);
        }

        if (write_size == 0) // detect infinite loop?
        {
          ++loop_counter;
          gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "send() sent zero bytes of %u",
                             uint32_t(connection->send_buffer_size));

          if (loop_counter > 5)
          {
            _connection_close(connection);
            return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_LOST_CONNECTION, "send() failed to send data");
          }
          continue;
        }
        else if (write_size == SOCKET_ERROR)
        {
          int local_errno= errno;
          switch (local_errno)
          {
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
          case EWOULDBLOCK:
#endif
          case EAGAIN:
            {
              gearmand_error_t gret= gearmand_io_set_events(con, POLLOUT);
              if (gret != GEARMAND_SUCCESS)
              {
                return gret;
              }
              return GEARMAND_IO_WAIT;
            }

          case EINTR:
            continue;

          case EPIPE:
          case ECONNRESET:
          case EHOSTDOWN:
            _connection_close(connection);
            return gearmand_perror(local_errno, "lost connection to client during send(EPIPE || ECONNRESET || EHOSTDOWN)");

          default:
            break;
          }

          _connection_close(connection);
          return gearmand_perror(local_errno, "send() failed, closing connection");
        }

        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "send() %u bytes to peer",
                           uint32_t(write_size));

        connection->send_buffer_size-= static_cast<size_t>(write_size);
        if (connection->send_state == gearmand_io_st::GEARMAND_CON_SEND_UNIVERSAL_FLUSH_DATA)
        {
          connection->send_data_offset+= static_cast<size_t>(write_size);
          if (connection->send_data_offset == connection->send_data_size)
          {
            connection->send_data_size= 0;
            connection->send_data_offset= 0;
            break;
          }

          if (connection->send_buffer_size == 0)
          {
            return GEARMAND_SUCCESS;
          }
        }
        else if (connection->send_buffer_size == 0)
        {
          break;
        }

        connection->send_buffer_ptr+= write_size;
      }

      connection->send_state= gearmand_io_st::GEARMAND_CON_SEND_STATE_NONE;
      connection->send_buffer_ptr= connection->send_buffer;
      return GEARMAND_SUCCESS;
    }
  }
}

/**
 * @addtogroup gearmand_io_static Static Connection Declarations
 * @ingroup gearman_connection
 * @{
 */


void gearmand_connection_init(gearmand_connection_list_st *gearman,
                              gearmand_io_st *connection,
                              gearmand_con_st *dcon,
                              gearmand_connection_options_t *options)
{
  assert(gearman);
  assert(connection);

  connection->options.ready= false;
  connection->options.packet_in_use= false;
  connection->options.external_fd= false;
  connection->options.close_after_flush= false;

  if (options)
  {
    while (*options != GEARMAND_CON_MAX)
    {
      gearman_io_set_option(connection, *options, true);
      options++;
    }
  }

  connection->_state= gearmand_io_st::GEARMAND_CON_UNIVERSAL_INVALID;
  connection->send_state= gearmand_io_st::GEARMAND_CON_SEND_STATE_NONE;
  connection->recv_state= gearmand_io_st::GEARMAND_CON_RECV_UNIVERSAL_NONE;
  connection->clear();
  connection->created_id= 0;
  connection->created_id_next= 0;
  connection->send_buffer_size= 0;
  connection->send_data_size= 0;
  connection->send_data_offset= 0;
  connection->recv_buffer_size= 0;
  connection->recv_data_size= 0;
  connection->recv_data_offset= 0;
  connection->universal= gearman;

  GEARMAND_LIST__ADD(gearman->con, connection);

  connection->context= dcon;

  connection->send_buffer_ptr= connection->send_buffer;
  connection->recv_packet= NULL;
  connection->recv_buffer_ptr= connection->recv_buffer;
}

void gearmand_connection_list_st::list_free()
{
  while (con_list)
  {
    gearmand_io_free(con_list);
  }
}

gearmand_connection_list_st::gearmand_connection_list_st() :
  con_count(0),
  con_list(NULL),
  event_watch_fn(NULL),
  event_watch_context(NULL)
{
}

void gearmand_connection_list_st::init(gearmand_event_watch_fn *watch_fn, void *watch_context)
{
  ready_con_count= 0;
  ready_con_list= NULL;
  con_count= 0;
  con_list= NULL;
  event_watch_fn= watch_fn;
  event_watch_context= watch_context;
}

void gearmand_io_free(gearmand_io_st *connection)
{
  if (connection->has_fd())
  {
    _connection_close(connection);
  }

  if (connection->options.ready)
  {
    connection->options.ready= false;
    GEARMAND_LIST_DEL(connection->universal->ready_con, connection, ready_);
  }

  GEARMAND_LIST__DEL(connection->universal->con, connection);

  if (connection->options.packet_in_use)
  {
    gearmand_packet_free(&(connection->packet));
  }
}

gearmand_error_t gearman_io_set_option(gearmand_io_st *connection,
                                       gearmand_connection_options_t options,
                                       bool value)
{
  switch (options)
  {
  case GEARMAND_CON_PACKET_IN_USE:
    connection->options.packet_in_use= value;
    break;
  case GEARMAND_CON_EXTERNAL_FD:
    connection->options.external_fd= value;
    break;
  case GEARMAND_CON_CLOSE_AFTER_FLUSH:
    connection->options.close_after_flush= value;
    break;
  case GEARMAND_CON_MAX:
    return GEARMAND_INVALID_COMMAND;
  }

  return GEARMAND_SUCCESS;
}

/** @} */

/*
 * Public Definitions
 */

gearmand_error_t gearman_io_set_fd(gearmand_io_st *connection, int fd_)
{
  assert(connection);
  return connection->set_fd(fd_);
}

gearmand_con_st *gearman_io_context(const gearmand_io_st *connection)
{
  return connection->context;
}

#if __GNUC__ >= 7
  #pragma GCC diagnostic warning "-Wimplicit-fallthrough"
#endif
gearmand_error_t gearman_io_send(gearman_server_con_st *con,
                                 const gearmand_packet_st *packet, bool flush)
{
  size_t send_size;

  gearmand_io_st *connection= &con->con;

  switch (connection->send_state)
  {
  case gearmand_io_st::GEARMAND_CON_SEND_STATE_NONE:
    if (! (packet->options.complete))
    {
      gearmand_error("packet not complete");
      return GEARMAND_INVALID_PACKET;
    }

    /* Pack first part of packet, which is everything but the payload. */
    while (1)
    {
      gearmand_error_t ret;
      send_size= con->protocol->pack(packet,
                                     con,
                                     connection->send_buffer +connection->send_buffer_size,
                                     GEARMAND_SEND_BUFFER_SIZE -connection->send_buffer_size,
                                     ret);
      if (ret == GEARMAND_SUCCESS)
      {
        connection->send_buffer_size+= send_size;
        break;
      }
      else if (ret == GEARMAND_IGNORE_PACKET)
      {
        return GEARMAND_SUCCESS;
      }
      else if (ret != GEARMAND_FLUSH_DATA)
      {
        return ret;
      }

      /* We were asked to flush when the buffer is already flushed! */
      if (connection->send_buffer_size == 0)
      {
        gearmand_error("send buffer too small");

        return GEARMAND_SEND_BUFFER_TOO_SMALL;
      }

      /* Flush buffer now if first part of packet won't fit in. */
      connection->send_state= gearmand_io_st::GEARMAND_CON_SEND_UNIVERSAL_PRE_FLUSH;

    case gearmand_io_st::GEARMAND_CON_SEND_UNIVERSAL_PRE_FLUSH:
      {
        gearmand_error_t local_ret;
        if ((local_ret= _connection_flush(con)) != GEARMAND_SUCCESS)
        {
          return local_ret;
        }
      }
    }

    /* Return here if we have no data to send. */
    if (packet->data_size == 0)
    {
      break;
    }

    /* If there is any room in the buffer, copy in data. */
    if (packet->data and (GEARMAND_SEND_BUFFER_SIZE - connection->send_buffer_size) > 0)
    {
      connection->send_data_offset= GEARMAND_SEND_BUFFER_SIZE - connection->send_buffer_size;
      if (connection->send_data_offset > packet->data_size)
      {
        connection->send_data_offset= packet->data_size;
      }

      memcpy(connection->send_buffer +connection->send_buffer_size,
             packet->data,
             connection->send_data_offset);
      connection->send_buffer_size+= connection->send_data_offset;

      /* Return if all data fit in the send buffer. */
      if (connection->send_data_offset == packet->data_size)
      {
        connection->send_data_offset= 0;
        break;
      }
    }

    /* Flush buffer now so we can start writing directly from data buffer. */
    connection->send_state= gearmand_io_st::GEARMAND_CON_SEND_UNIVERSAL_FORCE_FLUSH;

  case gearmand_io_st::GEARMAND_CON_SEND_UNIVERSAL_FORCE_FLUSH:
    {
      gearmand_error_t local_ret;
      if ((local_ret= _connection_flush(con)) != GEARMAND_SUCCESS)
      {
        return local_ret;
      }
    }

    connection->send_data_size= packet->data_size;

    /* If this is NULL, then ?? function will be used. */
    if (packet->data == NULL)
    {
      connection->send_state= gearmand_io_st::GEARMAND_CON_SEND_UNIVERSAL_FLUSH_DATA;
      return GEARMAND_SUCCESS;
    }

    /* Copy into the buffer if it fits, otherwise flush from packet buffer. */
    connection->send_buffer_size= packet->data_size - connection->send_data_offset;
    if (connection->send_buffer_size < GEARMAND_SEND_BUFFER_SIZE)
    {
      memcpy(connection->send_buffer,
             packet->data + connection->send_data_offset,
             connection->send_buffer_size);
      connection->send_data_size= 0;
      connection->send_data_offset= 0;
      break;
    }

    connection->send_buffer_ptr= const_cast<char *>(packet->data) + connection->send_data_offset;
    connection->send_state= gearmand_io_st::GEARMAND_CON_SEND_UNIVERSAL_FLUSH_DATA;

  case gearmand_io_st::GEARMAND_CON_SEND_UNIVERSAL_FLUSH:
  case gearmand_io_st::GEARMAND_CON_SEND_UNIVERSAL_FLUSH_DATA:
    {
      gearmand_error_t local_ret= _connection_flush(con);
      if (local_ret == GEARMAND_SUCCESS and
          connection->options.close_after_flush)
      {
        _connection_close(connection);
        local_ret= GEARMAND_LOST_CONNECTION;
        gearmand_debug("closing connection after flush by request");
      }
      return local_ret;
    }
  }

  if (flush)
  {
    connection->send_state= gearmand_io_st::GEARMAND_CON_SEND_UNIVERSAL_FLUSH;
    gearmand_error_t local_ret= _connection_flush(con);
    if (local_ret == GEARMAND_SUCCESS and connection->options.close_after_flush)
    {
      _connection_close(connection);
      local_ret= GEARMAND_LOST_CONNECTION;
      gearmand_debug("closing connection after flush by request");
    }
    return local_ret;
  }

  connection->send_state= gearmand_io_st::GEARMAND_CON_SEND_STATE_NONE;
  return GEARMAND_SUCCESS;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
gearmand_error_t gearman_io_recv(gearman_server_con_st *con, bool recv_data)
{
  gearmand_io_st *connection= &con->con;
  gearmand_packet_st *packet= &(con->packet->packet);

  switch (connection->recv_state)
  {
  case gearmand_io_st::GEARMAND_CON_RECV_UNIVERSAL_NONE:
    if (connection->_state != gearmand_io_st::GEARMAND_CON_UNIVERSAL_CONNECTED)
    {
      gearmand_error("not connected");
      return GEARMAND_NOT_CONNECTED;
    }

    connection->recv_packet= packet;
    // The options being passed in are just defaults.
    connection->recv_packet->reset(GEARMAN_MAGIC_TEXT, GEARMAN_COMMAND_TEXT);

    connection->recv_state= gearmand_io_st::GEARMAND_CON_RECV_UNIVERSAL_READ;

  case gearmand_io_st::GEARMAND_CON_RECV_UNIVERSAL_READ:
    while (1)
    {
      gearmand_error_t ret;

      if (connection->recv_buffer_size > 0)
      {
        assert(con->protocol);
        size_t recv_size= con->protocol->unpack(connection->recv_packet,
                                                con,
                                                connection->recv_buffer_ptr,
                                                connection->recv_buffer_size, ret);
        connection->recv_buffer_ptr+= recv_size;
        connection->recv_buffer_size-= recv_size;
        if (gearmand_success(ret))
        {
          break;
        }
        else if (ret != GEARMAND_IO_WAIT)
        {
          gearmand_gerror_warn("protocol failure, closing connection", ret);
          _connection_close(connection);
          return ret;
        }
      }

      /* Shift buffer contents if needed. */
      if (connection->recv_buffer_size > 0)
      {
        memmove(connection->recv_buffer, connection->recv_buffer_ptr, connection->recv_buffer_size);
      }
      connection->recv_buffer_ptr= connection->recv_buffer;

      size_t recv_size= _connection_read(con, connection->recv_buffer + connection->recv_buffer_size,
					 GEARMAND_RECV_BUFFER_SIZE - connection->recv_buffer_size, ret);
      if (gearmand_failed(ret))
      {
        // GEARMAND_LOST_CONNECTION is not worth a warning, clients/workers just
        // drop connections for close.
        if (ret != GEARMAND_LOST_CONNECTION)
        {
          gearmand_gerror_warn("Failed while in _connection_read()", ret);
        }
        return ret;
      }
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "read %lu bytes",
                         (unsigned long)recv_size);

      connection->recv_buffer_size+= recv_size;
    }

    if (packet->data_size == 0)
    {
      connection->recv_state= gearmand_io_st::GEARMAND_CON_RECV_UNIVERSAL_NONE;
      break;
    }

    connection->recv_data_size= packet->data_size;

    if (not recv_data)
    {
      connection->recv_state= gearmand_io_st::GEARMAND_CON_RECV_STATE_READ_DATA;
      break;
    }

    packet->data= static_cast<char *>(realloc(NULL, packet->data_size));
    if (not packet->data)
    {
      // Server up the memory error first, in case _connection_close()
      // creates any.
      gearmand_merror("realloc", char, packet->data_size);
      _connection_close(connection);
      return GEARMAND_MEMORY_ALLOCATION_FAILURE;
    }

    packet->options.free_data= true;
    connection->recv_state= gearmand_io_st::GEARMAND_CON_RECV_STATE_READ_DATA;

  case gearmand_io_st::GEARMAND_CON_RECV_STATE_READ_DATA:
    while (connection->recv_data_size)
    {
      gearmand_error_t ret;
      ret= gearmand_connection_recv_data(con,
                                         ((uint8_t *)(packet->data)) +
                                         connection->recv_data_offset,
                                         packet->data_size -
                                         connection->recv_data_offset);
      if (gearmand_failed(ret))
      {
        return ret;
      }
    }

    connection->recv_state= gearmand_io_st::GEARMAND_CON_RECV_UNIVERSAL_NONE;
    break;
  }

  packet= connection->recv_packet;
  connection->recv_packet= NULL;

  return GEARMAND_SUCCESS;
}

gearmand_error_t gearmand_io_set_events(gearman_server_con_st *con, short events)
{
  gearmand_io_st *connection= &con->con;

  if ((connection->events | events) == connection->events)
  {
    return GEARMAND_SUCCESS;
  }

  connection->events|= events;

  if (connection->universal->event_watch_fn)
  {
    gearmand_error_t ret= connection->universal->event_watch_fn(connection, connection->events,
                                                                (void *)connection->universal->event_watch_context);
    if (gearmand_failed(ret))
    {
      gearmand_gerror_warn("event watch failed, closing connection", ret);
      _connection_close(connection);
      return ret;
    }
  }

  return GEARMAND_SUCCESS;
}

gearmand_error_t gearmand_io_set_revents(gearman_server_con_st *con, short revents)
{
  gearmand_io_st *connection= &con->con;

  if (revents != 0)
  {
    connection->options.ready= true;
    GEARMAND_LIST_ADD(connection->universal->ready_con, connection, ready_);
  }

  connection->revents= revents;

  /* Remove external POLLOUT watch if we didn't ask for it. Otherwise we spin
    forever until another POLLIN state change. This is much more efficient
    than removing POLLOUT on every state change since some external polling
    mechanisms need to use a system call to change flags (like Linux epoll). */
  if (revents & POLLOUT && !(connection->events & POLLOUT) &&
      connection->universal->event_watch_fn != NULL)
  {
    gearmand_error_t ret= connection->universal->event_watch_fn(connection, connection->events,
                                                                (void *)connection->universal->event_watch_context);
    if (gearmand_failed(ret))
    {
      gearmand_gerror_warn("event watch failed, closing connection", ret);
      _connection_close(connection);
      return ret;
    }
  }

  connection->events&= (short)~revents;

  return GEARMAND_SUCCESS;
}

/*
 * Static Definitions
 */

gearmand_error_t gearmand_io_st::_io_setsockopt()
{
  {
    int setting= 1;
    if (setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, &setting, (socklen_t)sizeof(int)) and errno != EOPNOTSUPP)
    {
      return gearmand_perror(errno, "setsockopt(TCP_NODELAY)");
    }
  }

  {
    struct linger linger;
    linger.l_onoff= 1;
    linger.l_linger= GEARMAND_DEFAULT_SOCKET_TIMEOUT;
    if (setsockopt(_fd, SOL_SOCKET, SO_LINGER, &linger, (socklen_t)sizeof(struct linger)))
    {
      return gearmand_perror(errno, "setsockopt(SO_LINGER)");
    }
  }

#if defined(__MACH__) && defined(__APPLE__) || defined(__FreeBSD__)
  {
    int setting= 1;

    // This is not considered a fatal error 
    if (setsockopt(_fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&setting, sizeof(int)))
    {
      gearmand_perror(errno, "setsockopt(SO_NOSIGPIPE)");
    }
  }
#endif

#if 0
  if (0)
  {
    struct timeval waittime;
    waittime.tv_sec= GEARMAND_DEFAULT_SOCKET_TIMEOUT;
    waittime.tv_usec= 0;
    if (setsockopt(_fd, SOL_SOCKET, SO_SNDTIMEO, &waittime, (socklen_t)sizeof(struct timeval)) and errno != ENOPROTOOPT)
    {
      return gearmand_perror(errno, "setsockopt(SO_SNDTIMEO)");
    }

    if (setsockopt(_fd, SOL_SOCKET, SO_RCVTIMEO, &waittime, (socklen_t)sizeof(struct timeval)) and errno != ENOPROTOOPT)
    {
      return gearmand_perror(errno, "setsockopt(SO_RCVTIMEO)");
    }
  }
#endif

#if 0
  if (0)
  {
    int setting= GEARMAND_DEFAULT_SOCKET_SEND_SIZE;
    if (setsockopt(_fd, SOL_SOCKET, SO_SNDBUF, &setting, (socklen_t)sizeof(int)))
    {
      return gearmand_perror(errno, "setsockopr(SO_SNDBUF)");
    }

    setting= GEARMAND_DEFAULT_SOCKET_RECV_SIZE;
    if (setsockopt(_fd, SOL_SOCKET, SO_RCVBUF, &setting, (socklen_t)sizeof(int)))
    {
      return gearmand_perror(errno, "setsockopt(SO_RCVBUF)");
    }
  }
#endif

  if (SOCK_NONBLOCK == 0)
  {
    gearmand_error_t local_ret;
    if ((local_ret= gearmand_sockfd_nonblock(_fd)))
    {
      return local_ret;
    }
  }

  return GEARMAND_SUCCESS;
}

gearmand_error_t gearmand_sockfd_nonblock(const int& sockfd)
{
  int flags;
  do
  {
    flags= fcntl(sockfd, F_GETFL, 0);
    if (flags == -1)
    {
      gearmand_log_perror_warn(GEARMAN_DEFAULT_LOG_PARAM, flags, "gearmand_sockfd_nonblock");
    }
  } while (flags == -1 and (errno == EINTR or errno == EAGAIN));

  if (flags == -1)
  {
    return gearmand_perror(errno, "fcntl(F_GETFL)");
  }
  else if ((flags & O_NONBLOCK) == 0)
  {
    int retval;
    do
    {
      retval= fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
      if (retval == -1)
      {
        gearmand_log_perror_warn(GEARMAN_DEFAULT_LOG_PARAM, retval, "gearmand_sockfd_nonblock");
      }
    } while (retval == -1 and (errno == EINTR or errno == EAGAIN));

    if (retval == -1)
    {
      return gearmand_perror(errno, "fcntl(F_SETFL)");
    }
  }

  return GEARMAND_SUCCESS;
}

void gearmand_sockfd_close(int& sockfd)
{
  if (sockfd != INVALID_SOCKET)
  {
    /* in case of death shutdown to avoid blocking at close() */
    if (shutdown(sockfd, SHUT_RDWR) == SOCKET_ERROR && get_socket_errno() != ENOTCONN)
    {
      gearmand_perror(errno, "shutdown");
      assert(errno != ENOTSOCK);
    }
    else if (closesocket(sockfd) == SOCKET_ERROR)
    {
      gearmand_perror(errno, "close");
    }

    sockfd= INVALID_SOCKET;
  }
  else
  {
    gearmand_debug("gearmand_sockfd_close() called with an invalid socket, this was probably ok");
  }
}

void gearmand_pipe_close(int& pipefd)
{
  if (pipefd == INVALID_SOCKET)
  {
    gearmand_error("gearmand_pipe_close() called with an invalid socket");
    return;
  }

  if (closesocket(pipefd) == SOCKET_ERROR)
  {
    gearmand_perror(errno, "close");
  }

  pipefd= -1;
}
#pragma GCC diagnostic pop
