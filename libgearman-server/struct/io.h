/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
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

#pragma once

#include "libgearman-server/plugins/base.h"

#include "libgearman/ssl.h"

#include <atomic>

struct gearmand_io_st
{
  struct {
    bool ready{};
    bool packet_in_use{};
    bool external_fd{};
    bool ignore_lost_connection{};
    bool close_after_flush{};
  } options;
  enum {
    GEARMAND_CON_UNIVERSAL_INVALID,
    GEARMAND_CON_UNIVERSAL_CONNECTED
  } _state;
  enum {
    GEARMAND_CON_SEND_STATE_NONE,
    GEARMAND_CON_SEND_UNIVERSAL_PRE_FLUSH,
    GEARMAND_CON_SEND_UNIVERSAL_FORCE_FLUSH,
    GEARMAND_CON_SEND_UNIVERSAL_FLUSH,
    GEARMAND_CON_SEND_UNIVERSAL_FLUSH_DATA
  } send_state;
  enum {
    GEARMAND_CON_RECV_UNIVERSAL_NONE,
    GEARMAND_CON_RECV_UNIVERSAL_READ,
    GEARMAND_CON_RECV_STATE_READ_DATA
  } recv_state;
  short events{};
  short revents{};
  private:
  int _fd{};

  public:
  gearmand_error_t set_fd(const int fd_)
  {
    options.external_fd= true;
    _fd= fd_;
    _state= gearmand_io_st::GEARMAND_CON_UNIVERSAL_CONNECTED;
    return _io_setsockopt();
  }

  gearmand_error_t _io_setsockopt();

  int fd() const
  {
    return _fd;
  }

  bool has_fd() const
  {
    return _fd != INVALID_SOCKET;
  }

  void clear()
  {
    _fd= INVALID_SOCKET;
    events= 0;
    revents= 0;
  }

  uint32_t created_id{};
  uint32_t created_id_next{};
  size_t send_buffer_size{};
  size_t send_data_size{};
  size_t send_data_offset{};
  size_t recv_buffer_size{};
  size_t recv_data_size{};
  size_t recv_data_offset{};
  gearmand_connection_list_st *universal{nullptr};
  gearmand_io_st *next{nullptr};
  gearmand_io_st *prev{nullptr};
  gearmand_io_st *ready_next{nullptr};
  gearmand_io_st *ready_prev{nullptr};
  gearmand_con_st *context{nullptr};
  char *send_buffer_ptr{nullptr};
  gearmand_packet_st *recv_packet{nullptr};
  char *recv_buffer_ptr{nullptr};
  gearmand_packet_st packet{};
  gearman_server_con_st *root{nullptr};
  char send_buffer[GEARMAND_SEND_BUFFER_SIZE];
  char recv_buffer[GEARMAND_RECV_BUFFER_SIZE];

  gearmand_io_st() {
    this->options = {};
    this->_state = GEARMAND_CON_UNIVERSAL_INVALID;
    this->send_state = GEARMAND_CON_SEND_STATE_NONE;
    this->recv_state = GEARMAND_CON_RECV_UNIVERSAL_NONE;
  }

  const char* host() const;
  const char* port() const;

#if 0
  void close_socket();
#endif
};

namespace gearmand { namespace protocol {class Context; } }

/*
  Free list for these are stored in gearman_server_thread_st[], otherwise they are owned by gearmand_con_st[]
  */
struct gearman_server_con_st
{
  gearmand_io_st con;
  bool is_sleeping{};
  bool is_exceptions{};
  // These flags are used to hand connection ownership off between the IO,
  // proc, and main threads (e.g. gearman_server_con_attempt_free() sets
  // is_dead from an IO thread while _proc() reads it from the proc thread
  // with no other synchronization), so they must be atomic rather than
  // plain bool.
  std::atomic<bool> is_dead{false};
  bool is_noop_sent{};
  bool is_cleaned_up{};
  gearmand_error_t ret{};
  std::atomic<bool> io_list{false};
  std::atomic<bool> proc_list{false};
  std::atomic<bool> proc_removed{false};
  std::atomic<bool> to_be_freed_list{false};
  uint32_t io_packet_count{};
  uint32_t proc_packet_count{};
  uint32_t worker_count{};
  uint32_t client_count{};
  gearman_server_thread_st *thread{nullptr};
  gearman_server_con_st *next{nullptr};
  gearman_server_con_st *prev{nullptr};
  gearman_server_packet_st *packet{nullptr};
  gearman_server_packet_st *io_packet_list{nullptr};
  gearman_server_packet_st *io_packet_end{nullptr};
  gearman_server_packet_st *proc_packet_list{nullptr};
  gearman_server_packet_st *proc_packet_end{nullptr};
  gearman_server_con_st *io_next{nullptr};
  gearman_server_con_st *io_prev{nullptr};
  gearman_server_con_st *proc_next{nullptr};
  gearman_server_con_st *proc_prev{nullptr};
  gearman_server_con_st *to_be_freed_next{nullptr};
  gearman_server_con_st *to_be_freed_prev{nullptr};
  struct gearman_server_worker_st *worker_list{nullptr};
  struct gearman_server_client_st *client_list{nullptr};
  const char *_host{nullptr}; // client host
  const char *_port{nullptr}; // client port
  char id[GEARMAND_SERVER_CON_ID_SIZE];
  gearmand::protocol::Context* protocol{nullptr};
  struct event *timeout_event{nullptr};
  SSL* _ssl{nullptr};

  gearman_server_con_st()
  {
  }

  ~gearman_server_con_st()
  {
  }

  const char* host() const
  {
    if (_host)
    {
      return _host;
    }

    return "-";
  }

  const char* port() const
  {
    if (_port)
    {
      return _port;
    }

    return "-";
  }

  void set_protocol(gearmand::protocol::Context* arg)
  {
    protocol= arg;
  }

  void protocol_release()
  {
    if (protocol)
    {
      protocol->notify(this);
      if (protocol->is_owner())
      {
        delete protocol;
      }
      protocol= nullptr;
    }
  }
};
