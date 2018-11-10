/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2012-2013 Data Differential, http://datadifferential.com/
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
 * @brief Gear Protocol Definitions
 */

#include "gear_config.h"
#include "configmake.h"

#include <libgearman-server/common.h>
#include <libgearman/strcommand.h>
#include <libgearman-server/packet.h>
#include "libgearman/strcommand.h"

#include <cstdio>
#include <cstdlib>
#include <cerrno>

#include "libgearman/ssl.h"

#include <libgearman-server/plugins/protocol/gear/protocol.h>
#include "libgearman/command.h"

static gearmand_error_t gearmand_packet_unpack_header(gearmand_packet_st *packet)
{
  uint32_t tmp;

  if (memcmp(packet->args, "\0REQ", 4) == 0)
  {
    packet->magic= GEARMAN_MAGIC_REQUEST;
  }
  else if (memcmp(packet->args, "\0RES", 4) == 0)
  {
    packet->magic= GEARMAN_MAGIC_RESPONSE;
  }
  else
  {
    gearmand_warning("invalid magic value");
    return GEARMAND_INVALID_MAGIC;
  }

  memcpy(&tmp, packet->args + 4, 4);
  packet->command= static_cast<gearman_command_t>(ntohl(tmp));

  if (packet->command == GEARMAN_COMMAND_TEXT ||
      packet->command >= GEARMAN_COMMAND_MAX)
  {
    gearmand_error("invalid command value");
    return GEARMAND_INVALID_COMMAND;
  }

  memcpy(&tmp, packet->args + 8, 4);
  packet->data_size= ntohl(tmp);

  return GEARMAND_SUCCESS;
}

class Geartext : public gearmand::protocol::Context {

public:
  ~Geartext()
  { }

  bool is_owner()
  {
    return false;
  }

  void notify(gearman_server_con_st* connection)
  {
    gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Gear connection disconnected: %s:%s", connection->host(), connection->port());
  }

  size_t unpack(gearmand_packet_st *packet,
                gearman_server_con_st *,
                const void *data, const size_t data_size,
                gearmand_error_t& ret_ptr)
  {
    size_t used_size;
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Gear unpack");

    if (packet->args_size == 0)
    {
      if (data_size > 0 && ((uint8_t *)data)[0] != 0)
      {
        /* Try to parse a text-based command. */
        uint8_t* ptr= (uint8_t *)memchr(data, '\n', data_size);
        if (ptr == NULL)
        {
          ret_ptr= GEARMAND_IO_WAIT;
          return 0;
        }

        packet->magic= GEARMAN_MAGIC_TEXT;
        packet->command= GEARMAN_COMMAND_TEXT;

        used_size= size_t(ptr - ((uint8_t *)data)) +1;
        *ptr= 0;
        if (used_size > 1 && *(ptr - 1) == '\r')
        {
          *(ptr - 1)= 0;
        }

        size_t arg_size;
        for (arg_size= used_size, ptr= (uint8_t *)data; ptr != NULL; data= ptr)
        {
          ptr= (uint8_t *)memchr(data, ' ', arg_size);
          if (ptr != NULL)
          {
            *ptr= 0;
            ptr++;
            while (*ptr == ' ')
            {
              ptr++;
            }

            arg_size-= size_t(ptr - ((uint8_t *)data));
          }

          ret_ptr= gearmand_packet_create(packet, data, ptr == NULL ? arg_size :
                                          size_t(ptr - ((uint8_t *)data)));
          if (ret_ptr != GEARMAND_SUCCESS)
          {
            return used_size;
          }
        }

        return used_size;
      }
      else if (data_size < GEARMAND_PACKET_HEADER_SIZE)
      {
        ret_ptr= GEARMAND_IO_WAIT;
        return 0;
      }

      packet->args= packet->args_buffer;
      packet->args_size= GEARMAND_PACKET_HEADER_SIZE;
      memcpy(packet->args, data, GEARMAND_PACKET_HEADER_SIZE);

      if (gearmand_failed(ret_ptr= gearmand_packet_unpack_header(packet)))
      {
        return 0;
      }

      used_size= GEARMAND_PACKET_HEADER_SIZE;
    }
    else
    {
      used_size= 0;
    }

    while (packet->argc != gearman_command_info(packet->command)->argc)
    {
      if (packet->argc != (gearman_command_info(packet->command)->argc - 1) or
          gearman_command_info(packet->command)->data)
      {
        uint8_t* ptr= (uint8_t *)memchr(((uint8_t *)data) +used_size, 0,
                               data_size -used_size);
        if (ptr == NULL)
        {
          gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                             "Possible protocol error for %s, received only %u args",
                             gearman_command_info(packet->command)->name, packet->argc);
          ret_ptr= GEARMAND_IO_WAIT;
          return used_size;
        }

        size_t arg_size= size_t(ptr - (((uint8_t *)data) + used_size)) +1;
        if (gearmand_failed((ret_ptr= gearmand_packet_create(packet, ((uint8_t *)data) + used_size, arg_size))))
        {
          return used_size;
        }

        packet->data_size-= arg_size;
        used_size+= arg_size;
      }
      else
      {
        if ((data_size - used_size) < packet->data_size)
        {
          ret_ptr= GEARMAND_IO_WAIT;
          return used_size;
        }

        ret_ptr= gearmand_packet_create(packet, ((uint8_t *)data) + used_size, packet->data_size);
        if (gearmand_failed(ret_ptr))
        {
          return used_size;
        }

        used_size+= packet->data_size;
        packet->data_size= 0;
      }
    }

#if defined(VCS_CHECKOUT) && VCS_CHECKOUT
    if (packet->command == GEARMAN_COMMAND_ECHO_REQ and packet->data_size)
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "GEAR %s length: %" PRIu64,
                         gearman_strcommand(packet->command),
                         uint64_t(packet->data_size));
    }
    else if (packet->command == GEARMAN_COMMAND_TEXT and packet->data_size)
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "GEAR %s text: %.*s",
                         gearman_strcommand(packet->command),
                         int(packet->data_size),
                         packet->data);
    }
    else if (packet->command == GEARMAN_COMMAND_OPTION_REQ and packet->arg_size[0])
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "GEAR %s option: %.*s",
                         gearman_strcommand(packet->command),
                         int(packet->arg_size[0]),
                         packet->arg[0]);
    }
    else if (packet->command == GEARMAN_COMMAND_WORK_EXCEPTION and packet->data_size)
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "GEAR %s handle: %.*s exception: %.*s",
                         gearman_strcommand(packet->command),
                         int(packet->arg_size[0]),
                         packet->arg[0],
                         int(packet->data_size),
                         packet->data);
    }
    else if (packet->command == GEARMAN_COMMAND_WORK_FAIL and packet->arg_size[0])
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "GEAR %s handle: %.*s",
                         gearman_strcommand(packet->command),
                         int(packet->arg_size[0]),
                         packet->arg[0]);
    }
    else if (packet->command == GEARMAN_COMMAND_SET_CLIENT_ID and packet->arg_size[0])
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "GEAR %s identifier: %.*s",
                         gearman_strcommand(packet->command),
                         int(packet->arg_size[0]),
                         packet->arg[0]);
    }
    else
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "GEAR %s",
                         gearman_strcommand(packet->command));
    }
#endif

    ret_ptr= GEARMAND_SUCCESS;
    return used_size;
  }

  size_t pack(const gearmand_packet_st *packet,
              gearman_server_con_st*,
              void *data, const size_t data_size,
              gearmand_error_t& ret_ptr)
  {
#if defined(VCS_CHECKOUT) && VCS_CHECKOUT
    if (packet->command == GEARMAN_COMMAND_ECHO_RES and packet->data_size)
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "GEAR %s length: %" PRIu64,
                         gearman_strcommand(packet->command),
                         uint64_t(packet->data_size));
    }
    else if (packet->command == GEARMAN_COMMAND_OPTION_RES and packet->arg_size[0])
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "GEAR %s option: %.*s",
                         gearman_strcommand(packet->command),
                         int(packet->arg_size[0]),
                         packet->arg[0]);
    }
    else
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "GEAR %s",
                         gearman_strcommand(packet->command));
    }
#endif

    if (packet->args_size == 0)
    {
      ret_ptr= GEARMAND_SUCCESS;
      return 0;
    }

    if (packet->args_size > data_size)
    {
      ret_ptr= GEARMAND_FLUSH_DATA;
      return 0;
    }

    memcpy(data, packet->args, packet->args_size);
    ret_ptr= GEARMAND_SUCCESS;

    return packet->args_size;
  }

private:
};

static Geartext gear_context;

static gearmand_error_t _gear_con_remove(gearman_server_con_st* connection)
{
#if defined(HAVE_SSL) && HAVE_SSL
  if (connection->_ssl)
  {
    SSL_shutdown(connection->_ssl);
    SSL_free(connection->_ssl);
    connection->_ssl= NULL;
  }
#else
  (void)connection;
#endif
  return GEARMAND_SUCCESS;
}

static gearmand_error_t _gear_con_add(gearman_server_con_st *connection)
{
#if defined(HAVE_SSL) && HAVE_SSL
  if (Gearmand()->ctx_ssl())
  {
    if ((connection->_ssl= SSL_new(Gearmand()->ctx_ssl())) == NULL)
    {
      return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_MEMORY_ALLOCATION_FAILURE, "SSL_new() failed to return a valid object");
    }

    SSL_set_fd(connection->_ssl, connection->con.fd());

    int accept_error;
    while ((accept_error= SSL_accept(connection->_ssl)) != SSL_SUCCESS)
    {
      int wolfssl_error;
      switch (wolfssl_error= SSL_get_error(connection->_ssl, accept_error))
      {
        case SSL_ERROR_NONE:
          break;

        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
        case SSL_ERROR_WANT_ACCEPT:
        case SSL_ERROR_WANT_CONNECT:
        case SSL_ERROR_WANT_X509_LOOKUP:
          continue;

        case SSL_ERROR_SYSCALL:
          return gearmand_log_perror(GEARMAN_DEFAULT_LOG_PARAM, errno, "Error occurred on SSL_accept()");

        case SSL_ERROR_SSL:
        case SSL_ERROR_ZERO_RETURN:
        default:
          char wolfssl_error_buffer[SSL_ERROR_SIZE]= { 0 };
          ERR_error_string_n(wolfssl_error, wolfssl_error_buffer, sizeof(wolfssl_error_buffer));
          return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_LOST_CONNECTION, "%s(%d)", 
                                     wolfssl_error_buffer, wolfssl_error);
      }
    }
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "GearSSL connection made: %s:%s", connection->host(), connection->port());
  }
  else
#endif
  {
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Gear connection made: %s:%s", connection->host(), connection->port());
  }

  connection->set_protocol(&gear_context);

  return GEARMAND_SUCCESS;
}

namespace gearmand {
namespace protocol {

Gear::Gear() :
  Plugin("Gear"),
  _port(GEARMAN_DEFAULT_TCP_PORT_STRING),
  _ssl_ca_file(GEARMAND_CA_CERTIFICATE),
  _ssl_certificate(GEARMAND_SERVER_PEM),
  _ssl_key(GEARMAND_SERVER_KEY),
  opt_ssl(false)
  {
    command_line_options().add_options()
      ("port,p", boost::program_options::value(&_port)->default_value(GEARMAN_DEFAULT_TCP_PORT_STRING),
       "Port the server should listen on.")
      ("ssl", boost::program_options::bool_switch(&opt_ssl)->default_value(false),
       "Enable ssl connections.")
      ("ssl-ca-file", boost::program_options::value(&_ssl_ca_file),
       "CA file.")
      ("ssl-certificate", boost::program_options::value(&_ssl_certificate),
       "SSL certificate.")
      ("ssl-key", boost::program_options::value(&_ssl_key),
       "SSL key for certificate.")
      ;
  }

Gear::~Gear()
{
}

gearmand_error_t Gear::start(gearmand_st *gearmand)
{
  gearmand_error_t rc;

  if (_port.compare(GEARMAN_DEFAULT_TCP_PORT_STRING) == 0)
  {
    char* service;
    if ((service= getenv("GEARMAND_PORT")) and service[0])
    {
      _port.clear();
      _port.append(service);
    }
  }

  if (_port.empty())
  {
    const char* service= GEARMAN_DEFAULT_TCP_PORT_STRING;
    struct servent *gearman_servent;
    if ((gearman_servent= getservbyname(GEARMAN_DEFAULT_TCP_SERVICE, NULL)))
    {
      if (gearman_servent and gearman_servent->s_name)
      {
        service= gearman_servent->s_name;
      }
    }

    _port.clear();
    _port.append(service);
  }

  gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Initializing Gear on port %s with SSL: %s", _port.c_str(), opt_ssl ? "true" : "false");

#if defined(HAVE_SSL) && HAVE_SSL

  if (opt_ssl)
  {
    if (getenv("GEARMAND_CA_CERTIFICATE"))
    {
      _ssl_ca_file= getenv("GEARMAND_CA_CERTIFICATE");
    }

    if (getenv("GEARMAND_SERVER_PEM"))
    {
      _ssl_certificate= getenv("GEARMAND_SERVER_PEM");
    }

    if (getenv("GEARMAND_SERVER_KEY"))
    {
      _ssl_key= getenv("GEARMAND_SERVER_KEY");
    }

    gearmand->init_ssl();

    if (SSL_CTX_load_verify_locations(gearmand->ctx_ssl(), _ssl_ca_file.c_str(), 0) != SSL_SUCCESS)
    {
      gearmand_log_fatal(GEARMAN_DEFAULT_LOG_PARAM, "SSL_CTX_load_verify_locations() cannot local the ca certificate %s", _ssl_ca_file.c_str());
    }
    gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Loading CA certificate : %s", _ssl_ca_file.c_str());

    if (SSL_CTX_use_certificate_file(gearmand->ctx_ssl(), _ssl_certificate.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS)
    {   
      gearmand_log_fatal(GEARMAN_DEFAULT_LOG_PARAM, "SSL_CTX_use_certificate_file() cannot obtain certificate %s", _ssl_certificate.c_str());
    }
    gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Loading certificate : %s", _ssl_certificate.c_str());

    if (SSL_CTX_use_PrivateKey_file(gearmand->ctx_ssl(), _ssl_key.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS)
    {   
      gearmand_log_fatal(GEARMAN_DEFAULT_LOG_PARAM, "SSL_CTX_use_PrivateKey_file() cannot obtain certificate %s", _ssl_key.c_str());
    }
    gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Loading certificate key : %s", _ssl_key.c_str());

    assert(gearmand->ctx_ssl());
  }
#endif

  rc= gearmand_port_add(gearmand, _port.c_str(), _gear_con_add, _gear_con_remove);

  return rc;
}

} // namespace protocol
} // namespace gearmand
