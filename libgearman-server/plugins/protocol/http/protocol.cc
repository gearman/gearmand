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
 * @brief HTTP Protocol Definitions
 */

#include <gear_config.h>
#include <libgearman-server/common.h>
#include <libgearman/strcommand.h>

#include <cstdio>
#include <cstdlib>

#include <libgearman-server/plugins/protocol/http/protocol.h>

/**
 * @addtogroup gearmand::protocol::HTTPatic Static HTTP Protocol Definitions
 * @ingroup gearman_protocol_http
 * @{
 */

/**
 * Default values.
 */
#define GEARMAND_PROTOCOL_HTTP_DEFAULT_PORT "8080"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

class HTTPtext;

/* Protocol callback functions. */


class HTTPtext : public gearmand::protocol::Context 
{
public:

  HTTPtext() :
    _method(gearmand::protocol::httpd::TRACE),
    _sent_header(false),
    _background(false),
    _keep_alive(false),
    _http_response(gearmand::protocol::httpd::HTTP_OK)
  {
  }

  ~HTTPtext()
  { }

  void notify(gearman_server_con_st*)
  {
    gearmand_debug("HTTP connection disconnected");
  }

  size_t pack(const gearmand_packet_st *packet,
              gearman_server_con_st *connection,
              void *send_buffer, const size_t send_buffer_size,
              gearmand_error_t& ret_ptr)
  {
    switch (packet->command)
    {
    case GEARMAN_COMMAND_WORK_DATA:
      {
        for (const char *ptr= packet->data; ptr <= (packet->data +packet->data_size) -2; ptr++)
        {
          content.push_back(*ptr);
        }

        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "HTTP gearmand_command_t: GEARMAN_COMMAND_WORK_DATA length:%" PRIu64, uint64_t(content.size()));
        ret_ptr= GEARMAND_IGNORE_PACKET;
        return 0;
      }

    default:
    case GEARMAN_COMMAND_TEXT:
    case GEARMAN_COMMAND_CAN_DO:
    case GEARMAN_COMMAND_CANT_DO:
    case GEARMAN_COMMAND_RESET_ABILITIES:
    case GEARMAN_COMMAND_PRE_SLEEP:
    case GEARMAN_COMMAND_UNUSED:
    case GEARMAN_COMMAND_NOOP:
    case GEARMAN_COMMAND_SUBMIT_JOB:
    case GEARMAN_COMMAND_GRAB_JOB:
    case GEARMAN_COMMAND_NO_JOB:
    case GEARMAN_COMMAND_JOB_ASSIGN:
    case GEARMAN_COMMAND_WORK_STATUS:
    case GEARMAN_COMMAND_GET_STATUS:
    case GEARMAN_COMMAND_ECHO_REQ:
    case GEARMAN_COMMAND_SUBMIT_JOB_BG:
    case GEARMAN_COMMAND_ERROR:
    case GEARMAN_COMMAND_STATUS_RES:
    case GEARMAN_COMMAND_SUBMIT_JOB_HIGH:
    case GEARMAN_COMMAND_SET_CLIENT_ID:
    case GEARMAN_COMMAND_CAN_DO_TIMEOUT:
    case GEARMAN_COMMAND_ALL_YOURS:
    case GEARMAN_COMMAND_WORK_EXCEPTION:
    case GEARMAN_COMMAND_OPTION_REQ:
    case GEARMAN_COMMAND_OPTION_RES:
    case GEARMAN_COMMAND_WORK_WARNING:
    case GEARMAN_COMMAND_GRAB_JOB_UNIQ:
    case GEARMAN_COMMAND_JOB_ASSIGN_UNIQ:
    case GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG:
    case GEARMAN_COMMAND_SUBMIT_JOB_LOW:
    case GEARMAN_COMMAND_SUBMIT_JOB_LOW_BG:
    case GEARMAN_COMMAND_SUBMIT_JOB_SCHED:
    case GEARMAN_COMMAND_SUBMIT_JOB_EPOCH:
    case GEARMAN_COMMAND_SUBMIT_REDUCE_JOB:
    case GEARMAN_COMMAND_SUBMIT_REDUCE_JOB_BACKGROUND:
    case GEARMAN_COMMAND_GRAB_JOB_ALL:
    case GEARMAN_COMMAND_JOB_ASSIGN_ALL:
    case GEARMAN_COMMAND_GET_STATUS_UNIQUE:
    case GEARMAN_COMMAND_STATUS_RES_UNIQUE:
    case GEARMAN_COMMAND_MAX:
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                         "Bad packet command: gearmand_command_t:%s", 
                         gearman_strcommand(packet->command));
      assert(0);
    case GEARMAN_COMMAND_WORK_FAIL:
    case GEARMAN_COMMAND_ECHO_RES:
    case GEARMAN_COMMAND_WORK_COMPLETE:
    {
      ret_ptr = GEARMAND_SUCCESS;
    }
      break;

    case GEARMAN_COMMAND_JOB_CREATED:
      {
        gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                           "Sending HTTP told to ignore packet: gearmand_command_t:%s", 
                           gearman_strcommand(packet->command));
        ret_ptr= GEARMAND_IGNORE_PACKET;
        return 0;
      }
    }

    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                       "Sending HTTP response: Content-length:%" PRIu64 " data_size:%" PRIu64 " gearmand_command_t:%s response:%s", 
                       uint64_t(content.size()),
                       uint64_t(packet->data_size),
                       gearman_strcommand(packet->command),
                       gearmand::protocol::httpd::response(response()));

    size_t pack_size= 0;
    if (_sent_header == false)
    {
      if (response() != gearmand::protocol::httpd::HTTP_OK)
      {
        pack_size= (size_t)snprintf((char *)send_buffer, send_buffer_size,
                                    "HTTP/1.0 %u %s\r\n"
                                    "Server: Gearman/" PACKAGE_VERSION "\r\n"
                                    "Content-Length: 0\r\n"
                                    "\r\n",
                                    int(response()), gearmand::protocol::httpd::response(response()));
      }
      else if (method() == gearmand::protocol::httpd::HEAD)
      {
        pack_size= (size_t)snprintf((char *)send_buffer, send_buffer_size,
                                    "HTTP/1.0 200 OK\r\n"
                                    "X-Gearman-Job-Handle: %.*s\r\n"
                                    "Content-Length: %" PRIu64 "\r\n"
                                    "Server: Gearman/" PACKAGE_VERSION "\r\n"
                                    "\r\n",
                                    packet->command == GEARMAN_COMMAND_JOB_CREATED ?  (int)packet->arg_size[0] : (int)packet->arg_size[0] - 1,
                                    (const char *)packet->arg[0],
                                    (uint64_t)packet->data_size);
      }
      else if (method() == gearmand::protocol::httpd::TRACE)
      {
        pack_size= (size_t)snprintf((char *)send_buffer, send_buffer_size,
                                    "HTTP/1.0 200 OK\r\n"
                                    "Server: Gearman/" PACKAGE_VERSION "\r\n"
                                    "Connection: close\r\n"
                                    "Content-Type: message/http\r\n"
                                    "\r\n");
      }
      else if (method() == gearmand::protocol::httpd::POST)
      {
        pack_size= (size_t)snprintf((char *)send_buffer, send_buffer_size,
                                    "HTTP/1.0 200 OK\r\n"
                                    "X-Gearman-Job-Handle: %.*s\r\n"
                                    "X-Gearman-Command: %s\r\n"
                                    "Content-Length: %" PRIu64 "\r\n"
                                    "Server: Gearman/" PACKAGE_VERSION "\r\n"
                                    "\r\n",
                                    packet->command == GEARMAN_COMMAND_JOB_CREATED ?  int(packet->arg_size[0]) : int(packet->arg_size[0] - 1),
                                    (const char *)packet->arg[0], // Job handle
                                    gearman_strcommand(packet->command),
                                    (uint64_t)packet->data_size); // Content-length
      }
      else
      {
        pack_size= (size_t)snprintf((char *)send_buffer, send_buffer_size,
                                    "HTTP/1.0 200 OK\r\n"
                                    "X-Gearman-Job-Handle: %.*s\r\n"
                                    "X-Gearman-Command: %s\r\n"
                                    "Content-Length: %" PRIu64 "\r\n"
                                    "Server: Gearman/" PACKAGE_VERSION "\r\n"
                                    "\r\n",
                                    packet->command == GEARMAN_COMMAND_JOB_CREATED ?  int(packet->arg_size[0]) : int(packet->arg_size[0] - 1),
                                    (const char *)packet->arg[0],
                                    gearman_strcommand(packet->command),
                                    (uint64_t)packet->data_size); // Content-length
      }

      _sent_header= true;
    }

    if (pack_size > send_buffer_size)
    {
      gearmand_debug("Sending HTTP had to flush");
      ret_ptr= GEARMAND_FLUSH_DATA;
      return 0;
    }

    memcpy(send_buffer, &content[0], content.size());
    pack_size+= content.size();

#if 0
    if (keep_alive() == false)
#endif
    {
      gearman_io_set_option(&connection->con, GEARMAND_CON_CLOSE_AFTER_FLUSH, true);
    }

    ret_ptr= GEARMAND_SUCCESS;

    return pack_size;
  }

  size_t unpack(gearmand_packet_st *packet,
                gearman_server_con_st *, //connection
                const void *data, const size_t data_size,
                gearmand_error_t& ret_ptr)
  {
    const char *unique= "-";
    size_t unique_size= 2;
    gearman_job_priority_t priority= GEARMAN_JOB_PRIORITY_NORMAL;

    gearmand_info("Receiving HTTP response");

    /* Get the request line first. */
    size_t request_size;
    size_t offset= 0;
    const char *request= parse_line(data, data_size, request_size, offset);
    if (request == NULL or request_size == 0)
    {
      gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "Zero length request made");
      ret_ptr= GEARMAND_IO_WAIT;
      return offset;
    }

    reset();

    /* Parse out the method, URI, and HTTP version from the request line. */
    const char *uri= (const char *)memchr(request, ' ', request_size);
    if (uri == NULL)
    {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "bad request line: %.*s", (uint32_t)request_size, request);
      set_response(gearmand::protocol::httpd::HTTP_NOT_FOUND);
      ret_ptr= GEARMAND_INVALID_PACKET;
      return 0;
    }

    {
      const char *method_str= request;
      ptrdiff_t method_size= uri -request;
      if (method_size == 3 and 
          strncmp(method_str, "GET", 3) == 0)
      {
        set_method(gearmand::protocol::httpd::GET);
      }
      else if (method_size == 3 and 
               strncmp(method_str, "PUT", 3) == 0)
      {
        set_method(gearmand::protocol::httpd::PUT);
      }
      else if (method_size == 4 and 
               strncmp(method_str, "POST", 4) == 0)
      {
        set_method(gearmand::protocol::httpd::POST);
      }
      else if (method_size == 4 and 
               strncmp(method_str, "HEAD", 4) == 0)
      {
        set_method(gearmand::protocol::httpd::HEAD);
      }
      else if (method_size == 5 and 
               strncmp(method_str, "TRACE", 5) == 0)
      {
        set_method(gearmand::protocol::httpd::TRACE);
      }
      else
      {
        gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "bad method: %.*s", (uint32_t)method_size, method_str);
        set_response(gearmand::protocol::httpd::HTTP_METHOD_NOT_ALLOWED);
        ret_ptr= GEARMAND_INVALID_PACKET;
        return 0;
      }
    }

    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "HTTP METHOD: %s", str_method(method()));

    while (*uri == ' ')
    {
      uri++;
    }

    // Remove leading /
    while (*uri == '/')
    {
      uri++;
    }

    const char *version= (const char *)memchr(uri, ' ', request_size - (size_t)(uri - request));
    if (version == NULL)
    {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "bad request line: %.*s",
                         (uint32_t)request_size, request);
      ret_ptr= GEARMAND_INVALID_PACKET;
      return 0;
    }

    ptrdiff_t uri_size= version -uri;
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "HTTP URI: \"%.*s\"", (int)uri_size, uri);
    switch (method())
    {
    case gearmand::protocol::httpd::POST:
    case gearmand::protocol::httpd::PUT:
    case gearmand::protocol::httpd::GET:
      if (uri_size == 0)
      {
        gearmand_error("must give function name in URI");
        set_response(gearmand::protocol::httpd::HTTP_NOT_FOUND);
      }

    case gearmand::protocol::httpd::TRACE:
    case gearmand::protocol::httpd::HEAD:
      break;
    }

    while (*version == ' ')
    {
      version++;
    }

    size_t version_size= request_size - size_t(version - request);
    if (version_size == 8 and 
        strncmp(version, "HTTP/1.1", 8) == 0)
    {
      set_keep_alive(true);
    }
    else if (version_size == 8 and 
             strncmp(version, "HTTP/1.0", 8) == 0)
    { }
    else
    {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "bad version: %.*s", (uint32_t)version_size, version);
      ret_ptr= GEARMAND_INVALID_PACKET;
      return 0;
    }

    /* Loop through all the headers looking for ones of interest. */
    const char *header;
    size_t header_size;
    while ((header= parse_line(data, data_size, header_size, offset)) != NULL)
    {
      if (header_size == 0)
      {
        break;
      }

      if (header_size > 16 and 
          strncasecmp(header, "Content-Length: ", 16) == 0)
      {
        if (method() == gearmand::protocol::httpd::PUT or
            method() == gearmand::protocol::httpd::POST)
        {
          char content_length[11]; /* 11 bytes to fit max display length of uint32_t */
          snprintf(content_length, sizeof(content_length), "%.*s", (int)header_size - 16,
                   header + 16);
          packet->data_size= size_t(atoi(content_length));
        }
      }
      else if (header_size == 22 and
               strncasecmp(header, "Connection: Keep-Alive", 22) == 0)
      {
        set_keep_alive(true);
      }
      else if (header_size > 18 and 
               strncasecmp(header, "X-Gearman-Unique: ", 18) == 0)
      {
        unique= header + 18;
        unique_size= header_size -18;
      }
      else if (header_size == 26 and
               strncasecmp(header, "X-Gearman-Background: true", 26) == 0)
      {
        set_background(true);
      }
      else if (header_size == 24 and
               strncasecmp(header, "X-Gearman-Priority: high", 24) == 0)
      {
        priority= GEARMAN_JOB_PRIORITY_HIGH;
      }
      else if (header_size == 23 and
               strncasecmp(header, "X-Gearman-Priority: low", 23) == 0)
      {
        priority= GEARMAN_JOB_PRIORITY_LOW;
      }
    }

    /* Make sure we received the end of headers. */
    if (header == NULL and response() == gearmand::protocol::httpd::HTTP_OK)
    {
      gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "No headers were found");
      ret_ptr= GEARMAND_IO_WAIT;
      return 0;
    }

    /* Request and all headers complete, build a packet based on HTTP request. */
    packet->magic= GEARMAN_MAGIC_REQUEST;

    if (response() != gearmand::protocol::httpd::HTTP_OK)
    {
      packet->command= GEARMAN_COMMAND_ECHO_REQ;

      if ((ret_ptr= gearmand_packet_pack_header(packet)) != GEARMAND_SUCCESS)
      {
        return 0;
      }

      packet->data_size= 0;
      packet->data= NULL;
    }
    else if (method() == gearmand::protocol::httpd::TRACE)
    {
      packet->command= GEARMAN_COMMAND_ECHO_REQ;

      if ((ret_ptr= gearmand_packet_pack_header(packet)) != GEARMAND_SUCCESS)
      {
        return 0;
      }

      packet->data_size= data_size;
      packet->data= (const char*)data;
    }
    else if (method() == gearmand::protocol::httpd::HEAD and uri_size == 0)
    {
      packet->command= GEARMAN_COMMAND_ECHO_REQ;

      if ((ret_ptr= gearmand_packet_pack_header(packet)) != GEARMAND_SUCCESS)
      {
        return 0;
      }
    }
    else
    {
      if (background())
      {
        if (priority == GEARMAN_JOB_PRIORITY_NORMAL)
        {
          packet->command= GEARMAN_COMMAND_SUBMIT_JOB_BG;
        }
        else if (priority == GEARMAN_JOB_PRIORITY_HIGH)
        {
          packet->command= GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG;
        }
        else
        {
          packet->command= GEARMAN_COMMAND_SUBMIT_JOB_LOW_BG;
        }
      }
      else
      {
        if (priority == GEARMAN_JOB_PRIORITY_NORMAL)
        {
          packet->command= GEARMAN_COMMAND_SUBMIT_JOB;
        }
        else if (priority == GEARMAN_JOB_PRIORITY_HIGH)
        {
          packet->command= GEARMAN_COMMAND_SUBMIT_JOB_HIGH;
        }
        else
        {
          packet->command= GEARMAN_COMMAND_SUBMIT_JOB_LOW;
        }
      }

      if ((ret_ptr= gearmand_packet_pack_header(packet)) != GEARMAND_SUCCESS)
      {
        return 0;
      }

      if ((ret_ptr= gearmand_packet_create(packet, uri, (size_t)uri_size +1)) != GEARMAND_SUCCESS)
      {
        return 0;
      }

      if ((ret_ptr= gearmand_packet_create(packet, unique, unique_size +1)) != GEARMAND_SUCCESS)
      {
        return 0;
      }

      /* Make sure function and unique are NULL terminated. */
      packet->arg[0][uri_size]= 0;
      packet->arg[1][unique_size]= 0;

      ret_ptr= GEARMAND_SUCCESS;
    }

    gearmand_info("Receiving HTTP response(finished)");

    return offset;
  }

  bool background()
  {
    return _background;
  }

  void set_background(bool arg)
  {
    _background= arg;
  }

  bool keep_alive()
  {
    return _keep_alive;
  }

  void set_keep_alive(bool arg)
  {
    _keep_alive= arg;
  }

  void set_response(gearmand::protocol::httpd::response_t arg)
  {
    _http_response= arg;
  }

  gearmand::protocol::httpd::response_t response() const
  {
    return _http_response;
  }

  gearmand::protocol::httpd::method_t method()
  {
    return _method;
  }

  void set_method(gearmand::protocol::httpd::method_t arg)
  {
    _method= arg;
  }

  void reset()
  {
    _sent_header= false;
    _background= false;
    _keep_alive= false;
    content.clear();
    _method= gearmand::protocol::httpd::TRACE;
    _http_response= gearmand::protocol::httpd::HTTP_OK;
  }

  const char *parse_line(const void *data, const size_t data_size,
                         size_t& line_size, size_t& offset)
  {
    const char *start= (const char *)data +offset;

    const char *end= (const char *)memchr(start, '\n', data_size -offset);
    if (end == NULL)
    {
      return NULL;
    }

    offset+= size_t(end - start) +1;

    if (end != start && *(end - 1) == '\r')
    {
      end--;
    }

    line_size= size_t(end - start);

    return start;
  }

private:
  gearmand::protocol::httpd::method_t _method;
  bool _sent_header;
  bool _background;
  bool _keep_alive;
  std::string global_port;
  gearmand::protocol::httpd::response_t _http_response;
  std::vector<char> content;
};

static gearmand_error_t _http_con_remove(gearman_server_con_st*)
{
  return GEARMAND_SUCCESS;
}

static gearmand_error_t _http_con_add(gearman_server_con_st *connection)
{
  gearmand_info("HTTP connection made");

  HTTPtext *http= new (std::nothrow) HTTPtext;
  if (http == NULL)
  {
    gearmand_error("new");
    return GEARMAND_MEMORY_ALLOCATION_FAILURE;
  }

  connection->set_protocol(http);

  return GEARMAND_SUCCESS;
}

namespace gearmand {
namespace protocol {

HTTP::HTTP() :
  Plugin("HTTP")
{
  command_line_options().add_options()
    ("http-port", boost::program_options::value(&_port)->default_value(GEARMAND_PROTOCOL_HTTP_DEFAULT_PORT), "Port to listen on.");
}

HTTP::~HTTP()
{
}

gearmand_error_t HTTP::start(gearmand_st *gearmand)
{
  gearmand_info("Initializing HTTP");
  return gearmand_port_add(gearmand, _port.c_str(), _http_con_add, _http_con_remove);
}

} // namespace protocol
} // namespace gearmand

/** @} */
#pragma GCC diagnostic pop
