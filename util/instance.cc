/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  DataDifferential Utility Library
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
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


#include "gear_config.h"

#include "util/instance.hpp"
#include "libgearman/ssl.h"

#include <cstdio>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


namespace datadifferential {
namespace util {

Instance::Instance(const std::string& hostname_arg, const std::string& service_arg) :
  _host(hostname_arg),
  _service(service_arg),
  _sockfd(INVALID_SOCKET),
  _use_ssl(false),
  state(NOT_WRITING),
  _addrinfo(0),
  _addrinfo_next(0),
  _finish_fn(NULL),
  _operations(),
  _ctx_ssl(NULL),
  _ssl(NULL)
  {
  }

Instance::Instance(const std::string& hostname_arg, const in_port_t port_arg) :
  _host(hostname_arg),
  _sockfd(INVALID_SOCKET),
  _use_ssl(false),
  state(NOT_WRITING),
  _addrinfo(0),
  _addrinfo_next(0),
  _finish_fn(NULL),
  _operations(),
  _ctx_ssl(NULL),
  _ssl(NULL)
  {
    char tmp[BUFSIZ];
    snprintf(tmp, sizeof(tmp), "%u", static_cast<unsigned int>(port_arg));
    _service= tmp;
  }

Instance::~Instance()
{
  close_socket();
  free_addrinfo();
  for (Operation::vector::iterator iter= _operations.begin(); iter != _operations.end(); ++iter)
  {
    delete *iter;
  }
  _operations.clear();

  delete _finish_fn;

#if defined(HAVE_SSL) && HAVE_SSL
  if (_ssl)
  {
    SSL_shutdown(_ssl);
    SSL_free(_ssl);
  }

  if (_ctx_ssl)
  {
    SSL_CTX_free(_ctx_ssl);
  }
# if defined(HAVE_WOLFSSL) && HAVE_WOLFSSL
  wolfSSL_Cleanup();
# endif // defined(HAVE_WOLFSSL)
#endif
}

bool Instance::init_ssl()
{
#if defined(HAVE_SSL) && HAVE_SSL
  SSL_load_error_strings();
  SSL_library_init();

  if ((_ctx_ssl= SSL_CTX_new(TLSv1_client_method())) == NULL)
  {
    _last_error= "SSL_CTX_new error";
    return false;
  }

  if (SSL_CTX_load_verify_locations(_ctx_ssl, ssl_ca_file(), 0) != SSL_SUCCESS)
  {
    std::stringstream message;
    message << "Error loading CA file " << ssl_ca_file();
    _last_error= message.str();
    return false;
  }

  if (SSL_CTX_use_certificate_file(_ctx_ssl, ssl_certificate(), SSL_FILETYPE_PEM) != SSL_SUCCESS)
  {
    std::stringstream message;
    message << "Error loading certificate file " << ssl_certificate();
    _last_error= message.str();
    return false;
  }

  if (SSL_CTX_use_PrivateKey_file(_ctx_ssl, ssl_key(), SSL_FILETYPE_PEM) != SSL_SUCCESS)
  {
    std::stringstream message;
    message << "Error loading private key file " << ssl_key();
    _last_error= message.str();
    return false;
  }
#endif // defined(HAVE_WOLFSSL) && HAVE_WOLFSSL
  return true;
}

bool Instance::run()
{
  if (_use_ssl)
  {
    if (not init_ssl())
    {
      return false;
    }
  }

  while (not _operations.empty())
  {
    Operation::vector::value_type operation= _operations.back();

    switch (state)
    {
    case NOT_WRITING:
      {
        free_addrinfo();

        struct addrinfo ai;
        memset(&ai, 0, sizeof(struct addrinfo));
        ai.ai_socktype= SOCK_STREAM;
        ai.ai_protocol= IPPROTO_TCP;

        int ret= getaddrinfo(_host.c_str(), _service.c_str(), &ai, &_addrinfo);
        if (ret)
        {
          std::stringstream message;
          message << "Failed to connect on " << _host.c_str() << ":" << _service.c_str() << " with "  << gai_strerror(ret);
          _last_error= message.str();
          return false;
        }
      }
      _addrinfo_next= _addrinfo;
      state= CONNECT;
      break;

    case NEXT_CONNECT_ADDRINFO:
      if (_addrinfo_next->ai_next == NULL)
      {
        std::stringstream message;
        message << "Error connecting to " << _host.c_str() << "." << std::endl;
        _last_error= message.str();
        return false;
      }
      _addrinfo_next= _addrinfo_next->ai_next;


    case CONNECT:
      close_socket();

      _sockfd= socket(_addrinfo_next->ai_family,
                      _addrinfo_next->ai_socktype,
                      _addrinfo_next->ai_protocol);
      if (_sockfd == INVALID_SOCKET)
      {
        perror("socket");
        continue;
      }

      if (connect(_sockfd, _addrinfo_next->ai_addr, _addrinfo_next->ai_addrlen) < 0)
      {
        switch(errno)
        {
        case EAGAIN:
        case EINTR:
          state= CONNECT;
          break;

        case EINPROGRESS:
          state= CONNECTING;
          break;

        case ECONNREFUSED:
        case ENETUNREACH:
        case ETIMEDOUT:
        default:
          state= NEXT_CONNECT_ADDRINFO;
          break;
        }
      }
      else
      {
        state= CONNECTING;
      }
      break;

    case CONNECTING:
      // Add logic for poll() for nonblocking.
      state= CONNECTED;
      break;

    case CONNECTED:
    case WRITING:
      {
        size_t packet_length= operation->size();
        const char *packet= operation->ptr();

#if defined(HAVE_SSL) && HAVE_SSL
        if (_ctx_ssl and not _ssl)
        {
          _ssl= SSL_new(_ctx_ssl);
          if (_ssl == NULL)
          {
            _last_error= "SSL_new() failed";
            return false;
          }

          int ssl_error;
          if ((ssl_error= SSL_set_fd(_ssl, _sockfd)) != SSL_SUCCESS)
          {
            _last_error= "SSL_set_fd() failed";
            return false;
          }
        }
#endif

        while(packet_length)
        {
          ssize_t write_size;
#if defined(HAVE_SSL) && HAVE_SSL
          if (_ssl)
          {
            write_size= SSL_write(_ssl, (const void*)packet, int(packet_length));
            int ssl_error;
            switch ((ssl_error= SSL_get_error(_ssl, int(write_size))))
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
                errno= EAGAIN;
                write_size= SOCKET_ERROR;
                break;

              case SSL_ERROR_SYSCALL:
                {
                  if (errno)
                  {
                    write_size= SOCKET_ERROR;
                    break;
                  }
                }

              case SSL_ERROR_SSL:
              default:
                {
                  char wolfssl_error_buffer[SSL_ERROR_SIZE]= { 0 };
                  ERR_error_string_n(ssl_error, wolfssl_error_buffer, sizeof(wolfssl_error_buffer));
                  _last_error= wolfssl_error_buffer;

                  errno= ECONNRESET;
                  write_size= SOCKET_ERROR;

                  break;
                }
            }
          }
          else
#endif
          {
            write_size= send(_sockfd, packet, packet_length, 0);
          }

          if (write_size == SOCKET_ERROR)
          {
            if (_last_error.empty())
            {
              std::stringstream msg;
              msg << "Failed during send(" << strerror(errno) << ")";
              _last_error= msg.str();
            }

            return false;
          }

          packet_length-= static_cast<size_t>(write_size);
          packet+= static_cast<size_t>(write_size);
        }
      }
      state= READING;
      break;

    case READING:
      if (operation->has_response())
      {
        ssize_t read_size;

        do
        {
          char buffer[BUFSIZ];
#if defined(HAVE_SSL) && HAVE_SSL
          if (_ssl)
          {
            {
              read_size= SSL_read(_ssl, (void *)buffer, sizeof(buffer));
              int ssl_error;
              switch ((ssl_error= SSL_get_error(_ssl, int(read_size))))
              {
                case SSL_ERROR_NONE:
                  break;

                case SSL_ERROR_ZERO_RETURN:
                  read_size= 0;
                  break;

                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                case SSL_ERROR_WANT_ACCEPT:
                case SSL_ERROR_WANT_CONNECT:
                case SSL_ERROR_WANT_X509_LOOKUP:
                  read_size= SOCKET_ERROR;
                  errno= EAGAIN;
                  break;

                case SSL_ERROR_SYSCALL:
                  {
                    if (errno)
                    {
                      std::stringstream msg;
                      msg << "Error occurred on SSL_acceptsend(" << strerror(errno) << ")";
                      _last_error= msg.str();
                      read_size= SOCKET_ERROR;
                      break;
                    }
                  }

                case SSL_ERROR_SSL:
                default:
                  {
                    char wolfssl_error_buffer[SSL_ERROR_SIZE]= { 0 };
                    ERR_error_string_n(ssl_error, wolfssl_error_buffer, sizeof(wolfssl_error_buffer));
                    _last_error= wolfssl_error_buffer;
                    read_size= SOCKET_ERROR;
                    break;
                  }
              }
            }
          }
          else
#endif
          {
            read_size= ::recv(_sockfd, buffer, sizeof(buffer), 0);
          }

          if (read_size == 0)
          {
            _last_error.clear();
            _last_error+= "Socket was shutdown while reading from ";
            _last_error+= _host;

            return false;
          }
          else if (read_size == SOCKET_ERROR)
          {
            if (_last_error.empty())
            {
              _last_error.clear();
              _last_error+= "Error occurred while reading data from ";
              _last_error+= _host;
            }

            return false;
          }

          operation->push(buffer, static_cast<size_t>(read_size));

        } while (more_to_read());
      } // end has_response

      state= FINISHED;
      break;

    case FINISHED:
      std::string response;
      bool success= operation->response(response);
      if (_finish_fn)
      {
        if (not _finish_fn->call(success, response))
        {
          // Error was sent from _finish_fn 
          return false;
        }
      }

      if (operation->reconnect())
      {
      }
      _operations.pop_back();
      delete operation;

      state= CONNECTED;
      break;
    } // end switch
  }

  return true;
} // end run()

bool Instance::more_to_read() const
{
  struct pollfd fds;
  fds.fd= _sockfd;
  fds.events = POLLIN;

  if (poll(&fds, 1, 5) < 1) // Default timeout is 5
  {
    return false;
  }

  return true;
}

void Instance::close_socket()
{
  if (_sockfd != INVALID_SOCKET)
  {
    /* in case of death shutdown to avoid blocking at close() */
    if (shutdown(_sockfd, SHUT_RDWR) == SOCKET_ERROR && get_socket_errno() != ENOTCONN)
    {
      perror("shutdown");
    }
    else if (closesocket(_sockfd) == SOCKET_ERROR)
    {
      perror("close");
    }

    _sockfd= INVALID_SOCKET;
  }
}

void Instance::free_addrinfo()
{
  if (_addrinfo)
  {
    freeaddrinfo(_addrinfo);
    _addrinfo= NULL;
    _addrinfo_next= NULL;
  }
}

} /* namespace util */
} /* namespace datadifferential */
