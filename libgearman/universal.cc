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
 * @brief Gearman State Definitions
 */

#include "gear_config.h"
#include "configmake.h"
#include <libgearman/common.h>

#include "libgearman/assert.hpp"
#include "libgearman/interface/push.hpp"
#include "libgearman/server_options.hpp"
#include "libgearman/log.hpp"
#include "libgearman/vector.h"
#include "libgearman/uuid.hpp"
#include "libgearman/pipe.h"


#include "libgearman/protocol/echo.h"
#include "libgearman/protocol/option.h"

#include "libgearman/ssl.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <unistd.h>
#include <memory>
#ifdef HAVE_SYS_EPOLL_H
# include <sys/epoll.h>
#endif

void gearman_nap(int arg)
{
  if (arg < 1)
  { }
  else
  {
#ifdef WIN32
    sleep(arg/1000000);
#else
    struct timespec global_sleep_value= { 0, static_cast<long>(arg * 1000)};
    nanosleep(&global_sleep_value, NULL);
#endif
  }
}

void gearman_nap(gearman_universal_st &self)
{
  gearman_nap(self.timeout);
}

void gearman_universal_clone(gearman_universal_st &destination, const gearman_universal_st &source)
{
  destination.wakeup(source.has_wakeup());

  (void)gearman_universal_set_option(destination, GEARMAN_UNIVERSAL_NON_BLOCKING, source.options.non_blocking);
  (void)gearman_universal_set_option(destination, GEARMAN_UNIVERSAL_STOP_WAIT_ON_SIGNAL, source.options.stop_wait_on_signal);

  destination.ssl(source.ssl());

  destination.timeout= source.timeout;

  destination._namespace= gearman_string_clone(source._namespace);
  destination._identifier= gearman_string_clone(source._identifier);
  destination.verbose= source.verbose;
  destination.log_fn= source.log_fn;
  destination.log_context= source.log_context;

  for (gearman_connection_st *con= source.con_list; con; con= con->next_connection())
  {
    if (gearman_connection_copy(destination, *con) == NULL)
    {
      return;
    }
  }

  assert(destination.con_count == source.con_count);
}

void gearman_universal_free(gearman_universal_st &universal)
{
  gearman_free_all_cons(universal);
  gearman_free_all_packets(universal);

  if (universal.pfds)
  {
    // created realloc()
    free(universal.pfds);
    universal.pfds= NULL;
  }

  // clean-up server options
  while (universal.server_options_list)
  {
    delete universal.server_options_list;
  }
}

gearman_return_t gearman_universal_set_option(gearman_universal_st &self, universal_options_t option, bool value)
{
  switch (option)
  {
  case GEARMAN_UNIVERSAL_NON_BLOCKING:
    self.options.non_blocking= value;
    break;

  case GEARMAN_UNIVERSAL_STOP_WAIT_ON_SIGNAL:
    self.options.stop_wait_on_signal= value;
    break;

  case GEARMAN_UNIVERSAL_DONT_TRACK_PACKETS:
    break;

  case GEARMAN_UNIVERSAL_IDENTIFY:
    if (value)
    {
      if (self._identifier == NULL)
      {
        self._identifier= gearman_string_create_guid();
      }
      assert(self._identifier);
    }
    else
    {
      gearman_string_free(self._identifier);
      self._identifier= NULL;
    }
    break;

  case GEARMAN_UNIVERSAL_MAX:
  default:
    return gearman_gerror(self, GEARMAN_INVALID_COMMAND);
  }

  return GEARMAN_SUCCESS;
}

int gearman_universal_timeout(gearman_universal_st &self)
{
  return self.timeout;
}

void gearman_universal_set_timeout(gearman_universal_st &self, int timeout)
{
  self.timeout= timeout;
}

void gearman_set_log_fn(gearman_universal_st &self, gearman_log_fn *function,
                        void *context, gearman_verbose_t verbose)
{
  self.log_fn= function;
  self.log_context= context;
  self.verbose= verbose;
  
  gearman_log_debug(self, "Enabled logging");
}

void gearman_set_workload_malloc_fn(gearman_universal_st& universal,
                                    gearman_malloc_fn *function,
                                    void *context)
{
  universal.allocator.malloc= function;
  universal.allocator.context= context;
}

void gearman_set_workload_free_fn(gearman_universal_st& universal,
                                  gearman_free_fn *function,
                                  void *context)
{
  universal.allocator.free= function;
  universal.allocator.context= context;
}

void gearman_free_all_cons(gearman_universal_st& universal)
{
  while (universal.con_list)
  {
    delete universal.con_list;
  }
}

bool gearman_universal_st::wakeup(bool has_wakeup_)
{
  if (has_wakeup_)
  {
    if (wakeup_fd[0] == INVALID_SOCKET)
    {
      return setup_shutdown_pipe(wakeup_fd);
    }

    return true;
  }

  close_wakeup();

  return true;
}

void gearman_universal_st::reset()
{
  for (gearman_connection_st *con= con_list; con; con= con->next_connection())
  {
    con->close_socket();
  }
}

/*
 * Flush all shouldn't return any error, because there's no way to indicate
 * which connection experienced an issue. Error detection is better done in gearman_wait()
 * after flushing all the connections here.
 */
void gearman_universal_st::flush()
{
  for (gearman_connection_st *con= con_list; con; con= con->next_connection())
  {
    if (con->is_events(POLLOUT))
    {
      continue;
    }

    con->flush();
  }
}

gearman_return_t gearman_wait(gearman_universal_st& universal)
{
  struct pollfd *pfds;

  bool have_shutdown_pipe= universal.has_wakeup();
  size_t con_count= universal.con_count +int(have_shutdown_pipe);

  if (universal.pfds_size < con_count)
  {
    pfds= static_cast<pollfd*>(realloc(universal.pfds, con_count * sizeof(struct pollfd)));
    if (pfds == NULL)
    {
      return gearman_universal_set_error(universal, GEARMAN_MEMORY_ALLOCATION_FAILURE, GEARMAN_AT,
                                         "realloc failed to allocate %u pollfd", uint32_t(con_count));
    }

    universal.pfds= pfds;
    universal.pfds_size= int(con_count);
  }
  else
  {
    pfds= universal.pfds;
  }

  nfds_t x= 0;
  for (gearman_connection_st *con= universal.con_list; con; con= con->next_connection())
  {
    if (con->events())
    {
      con->set_pollfd(pfds[x]);
      x++;
    }
  }

  if (x == 0)
  {
    return gearman_error(universal, GEARMAN_NO_ACTIVE_FDS, "no active file descriptors");
  }

  // Wakeup handling, we only make use of this if we have active connections
  size_t pipe_array_iterator= 0;
  if (have_shutdown_pipe)
  {
    pipe_array_iterator= x;
    pfds[x].fd= universal.wakeup_fd[0];
    pfds[x].events= POLLIN;
    pfds[x].revents= 0;
    x++;
  }

  int ret= 0;
  while (universal.timeout)
  {
    ret= poll(pfds, x, universal.timeout);
    if (ret == -1)
    {
      switch(errno)
      {
      case EINTR:
        if (universal.is_stop_wait_on_signal())
        {
          return GEARMAN_IO_WAIT;
        }
        continue;

      case EINVAL:
        return gearman_perror(universal, errno, "RLIMIT_NOFILE exceeded, or if OSX the timeout value was invalid");

      default:
        return gearman_perror(universal, errno, "poll");
      }
    }

    break;
  }

  if (ret == 0)
  {
    return gearman_universal_set_error(universal, GEARMAN_TIMEOUT, GEARMAN_AT,
                                       "timeout reached, %u servers were poll(), no servers were available, pipe:%s",
                                       uint32_t(x - have_shutdown_pipe), have_shutdown_pipe ? "true" : "false");
  }

  x= 0;
  for (gearman_connection_st *con= universal.con_list; con; con= con->next_connection())
  {
    if (con->events())
    {
      if (pfds[x].revents & (POLLERR | POLLHUP | POLLNVAL))
      {
        int err;
        socklen_t len= sizeof (err);
        if (getsockopt(pfds[x].fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0)
        {
          con->error(err);
        }
      }

      con->set_revents(pfds[x].revents);

      x++;
    }
  }

  if (have_shutdown_pipe and pfds[pipe_array_iterator].revents)
  {
    char buffer[1];
    ssize_t read_length= read(universal.wakeup_fd[0], buffer, sizeof(buffer));
    if (read_length > 0)
    {
      gearman_return_t local_ret= gearman_kill(gearman_universal_id(universal), GEARMAN_INTERRUPT);
      if (gearman_failed(local_ret))
      {
        return gearman_gerror(universal, GEARMAN_SHUTDOWN);
      }

      return gearman_gerror(universal, GEARMAN_SHUTDOWN_GRACEFUL);
    }

    if (read_length == 0)
    {
      return gearman_gerror(universal, GEARMAN_SHUTDOWN);
    }

    if (read_length == -1)
    {
      gearman_perror(universal, errno, "read() from shutdown pipe");
    }

#if 0
    perror("shudown read");
#endif
    // @todo figure out what happens in an error
  }

  return GEARMAN_SUCCESS;
}

gearman_connection_st *gearman_ready(gearman_universal_st& universal)
{
  /* 
    We can't keep universal between calls since connections may be removed during
    processing. If this list ever gets big, we may want something faster.
  */
  for (gearman_connection_st *con= universal.con_list; con; con= con->next_connection())
  {
    if (con->options.ready)
    {
      con->options.ready= false;
      return con;
    }
  }

  return NULL;
}

void gearman_universal_st::close_wakeup()
{
  if (wakeup_fd[0] != INVALID_SOCKET)
  {
    close(wakeup_fd[0]);
  }

  if (wakeup_fd[1] != INVALID_SOCKET)
  {
    close(wakeup_fd[1]);
  }
}

gearman_universal_st::~gearman_universal_st()
{
  close_wakeup();

  gearman_string_free(_identifier);
  gearman_string_free(_namespace);
#if defined(HAVE_SSL) && HAVE_SSL
  if (_ctx_ssl)
  {
    SSL_CTX_free(_ctx_ssl);
  }
#else
  assert(_ctx_ssl == NULL);
#endif
}

gearman_return_t gearman_universal_st::option(const universal_options_t& option_, bool value)
{
  switch (option_)
  {
    case GEARMAN_UNIVERSAL_NON_BLOCKING:
      non_blocking(value);
      break;

    case GEARMAN_UNIVERSAL_STOP_WAIT_ON_SIGNAL:
        stop_wait_on_signal(value);
      break;

    case GEARMAN_UNIVERSAL_DONT_TRACK_PACKETS:
      break;

    case GEARMAN_UNIVERSAL_IDENTIFY:
      _identifier= gearman_string_create_guid();
      assert(_identifier);
      break;

    case GEARMAN_UNIVERSAL_MAX:
    default:
      return gearman_gerror(*this, GEARMAN_INVALID_COMMAND);
  }

  return GEARMAN_SUCCESS;
}

bool gearman_universal_st::init_ssl()
{
  if (ssl())
  {
#if defined(HAVE_SSL) && HAVE_SSL
    SSL_load_error_strings();
    SSL_library_init();

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    if ((_ctx_ssl= SSL_CTX_new(TLSv1_2_client_method())) == NULL)
#else
    if ((_ctx_ssl= SSL_CTX_new(TLS_client_method())) == NULL)
#endif
    {
      gearman_universal_set_error(*this, GEARMAN_INVALID_ARGUMENT, GEARMAN_AT, "CyaTLSv1_client_method() failed");
      return false;
    }

    if (SSL_CTX_load_verify_locations(_ctx_ssl, ssl_ca_file(), 0) != SSL_SUCCESS)
    {
      gearman_universal_set_error(*this, GEARMAN_INVALID_ARGUMENT, GEARMAN_AT, "Failed to load CA certificate %s", ssl_ca_file());
      return false;
    }

    if (SSL_CTX_use_certificate_file(_ctx_ssl, ssl_certificate(), SSL_FILETYPE_PEM) != SSL_SUCCESS)
    {   
      gearman_universal_set_error(*this, GEARMAN_INVALID_ARGUMENT, GEARMAN_AT, "Failed to load certificate %s", ssl_certificate());
      return false;
    }

    if (SSL_CTX_use_PrivateKey_file(_ctx_ssl, ssl_key(), SSL_FILETYPE_PEM) != SSL_SUCCESS)
    {   
      gearman_universal_set_error(*this, GEARMAN_INVALID_ARGUMENT, GEARMAN_AT, "Failed to load certificate key %s", ssl_key());
      return false;
    }
#endif // defined(HAVE_SSL) && HAVE_SSL
  }

  return true;
}

void gearman_universal_st::identifier(const char *identifier_, const size_t identifier_size_)
{
  gearman_string_free(_identifier);
  if (identifier_ and identifier_size_)
  {
    _identifier= gearman_string_create(NULL, identifier_, identifier_size_);
  }
  else
  {
    _identifier= NULL;
  }
}

gearman_return_t gearman_set_identifier(gearman_universal_st& universal,
                                        const char *id, size_t id_size)
{
  if (id == NULL)
  {
    return gearman_error(universal, GEARMAN_INVALID_ARGUMENT, "id was NULL");
  }

  if (id_size == 0)
  {
    return gearman_error(universal, GEARMAN_INVALID_ARGUMENT,  "id_size was 0");
  }

  if (id_size > GEARMAN_MAX_IDENTIFIER)
  {
    return gearman_error(universal, GEARMAN_ARGUMENT_TOO_LARGE,  "id_size was greater then GEARMAN_MAX_ECHO_SIZE");
  }

  for (size_t x= 0; x < id_size; x++)
  {
    if (isgraph(id[x]) == false)
    {
      return gearman_error(universal, GEARMAN_INVALID_ARGUMENT,  "Invalid character found in identifier");
    }
  }

  universal.identifier(id, id_size);

  for (gearman_connection_st *con= universal.con_list; con; con= con->next_connection())
  {
    con->send_identifier();
  }

  return GEARMAN_SUCCESS;
}

static gearman_return_t connection_loop(gearman_universal_st& universal,
                                        const gearman_packet_st& message,
                                        Check& check)
{
  gearman_return_t ret= GEARMAN_SUCCESS;

  for (gearman_connection_st *con= universal.con_list; con; con= con->next_connection())
  {
    ret= con->send_packet(message, true);
    if (gearman_failed(ret))
    {
#if 0
      assert_msg(con->universal.error.rc != GEARMAN_SUCCESS, "Programmer error, error returned but not recorded");
#endif
      break;
    }

    con->options.packet_in_use= true;
    gearman_packet_st *packet_ptr= con->receiving(con->_packet, ret, true);
    if (packet_ptr == NULL)
    {
      if (ret != GEARMAN_NOT_CONNECTED and ret != GEARMAN_LOST_CONNECTION)
      {
        assert(&con->_packet == universal.packet_list);
      }
      con->options.packet_in_use= false;
      break;
    }

    assert(packet_ptr == &con->_packet);
    if (gearman_failed(ret))
    {
#if 0
      assert_msg(con->universal.error.rc != GEARMAN_SUCCESS, "Programmer error, error returned but not recorded");
#endif
      con->free_private_packet();
      con->reset_recv_packet();

      break;
    }
    assert(packet_ptr);

    if (gearman_failed(ret= check.success(con)))
    {
#if 0
      assert_msg(con->universal.error.rc != GEARMAN_SUCCESS, "Programmer error, error returned but not recorded");
#endif
      con->free_private_packet();
      con->reset_recv_packet();

      break;
    }

    con->reset_recv_packet();
    con->free_private_packet();
  }

  return ret;
}

gearman_return_t gearman_server_option(gearman_universal_st& universal, gearman_string_t& option)
{
  if (universal.has_connections() == false)
  {
    return gearman_universal_set_error(universal, GEARMAN_NO_SERVERS, GEARMAN_AT, "no servers provided");
  }

  gearman_packet_st message;
  gearman_return_t ret=  libgearman::protocol::option(universal, message, option);
  if (gearman_success(ret))
  {
    PUSH_BLOCKING(universal);

    OptionCheck check(universal, option);
    ret= connection_loop(universal, message, check);
  }
  else
  {
    return universal.error_code();
  }

  gearman_packet_free(&message);

  return ret;
}


gearman_return_t gearman_echo(gearman_universal_st& universal,
                              const void *workload_str,
                              size_t workload_size)
{
  if (universal.has_connections() == false)
  {
    return gearman_universal_set_error(universal, GEARMAN_NO_SERVERS, GEARMAN_AT, "no servers provided");
  }

  gearman_string_t workload= { static_cast<const char*>(workload_str), workload_size };
  gearman_packet_st message;
  gearman_return_t ret=  libgearman::protocol::echo(universal, message, workload);
  if (gearman_success(ret))
  {
    PUSH_BLOCKING(universal);

    EchoCheck check(universal, workload_str, workload_size);
    ret= connection_loop(universal, message, check);
  }
  else
  {
    return universal.error_code();
  }

  gearman_packet_free(&message);

  return ret;
}

gearman_return_t cancel_job(gearman_universal_st& universal,
                            gearman_job_handle_t job_handle)
{
  if (universal.has_connections() == false)
  {
    return gearman_universal_set_error(universal, GEARMAN_NO_SERVERS, GEARMAN_AT, "no servers provided");
  }

  const void *args[1];
  size_t args_size[1];

  args[0]= job_handle;
  args_size[0]= strlen(job_handle);

  gearman_packet_st cancel_packet;

  gearman_return_t ret= gearman_packet_create_args(universal,
                                                   cancel_packet,
                                                   GEARMAN_MAGIC_REQUEST,
                                                   GEARMAN_COMMAND_WORK_FAIL,
                                                   args, args_size, 1);
  if (gearman_success(ret))
  {
    PUSH_BLOCKING(universal);

    CancelCheck check(universal);
    ret= connection_loop(universal, cancel_packet, check);
  }
  else
  {
    gearman_packet_free(&cancel_packet);
    return universal.error_code();
  }

  gearman_packet_free(&cancel_packet);

  return ret;
}

void gearman_free_all_packets(gearman_universal_st &universal)
{
  while (universal.packet_list)
  {
    gearman_packet_free(universal.packet_list);
  }
}

gearman_id_t gearman_universal_id(gearman_universal_st &universal)
{
  gearman_id_t handle= { universal.wakeup_fd[0], universal.wakeup_fd[1] };

  return handle;
}

/*
 * Local Definitions
 */

void gearman_universal_set_namespace(gearman_universal_st& universal, const char *namespace_key, size_t namespace_key_size)
{
  gearman_string_free(universal._namespace);
  if (namespace_key)
  {
    universal._namespace= gearman_string_create(NULL, namespace_key, namespace_key_size);
  }
  else
  {
    universal._namespace= NULL;
  }
}

const char *gearman_univeral_namespace(gearman_universal_st& universal)
{
  return gearman_string_value(universal._namespace);
}
