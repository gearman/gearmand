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

#include <boost/program_options.hpp>

#include <libgearman-server/error.h>
#include <libgearman-server/constants.h>

struct gearman_server_con_st;
struct gearman_server_job_st;
struct gearman_server_st;
struct gearmand_packet_st;

namespace gearmand {

class Plugin {
public:

  Plugin(const std::string&);

  const std::string &name() const
  {
    return _name;
  }

  int compare(const std::string&);

  virtual ~Plugin()= 0;

  boost::program_options::options_description &command_line_options();

  bool has_error()
  {
    return _error_string.size();
  }

  const std::string& error_string()
  {
    return _error_string;
  }

protected:
  void reset_error()
  {
    _error_string.clear();
  }

  std::string _error_string;

private:
  boost::program_options::options_description _command_line_options;
  std::string _name;
  std::string _match;
};

namespace queue {

class Context {
public:
  Context():
    _store_on_shutdown(false)
  {
  }

  virtual ~Context()= 0;

  gearmand_error_t store(gearman_server_st *server,
                         const char *unique,
                         size_t unique_size,
                         const char *function_name,
                         size_t function_name_size,
                         const void *data,
                         size_t data_size,
                         gearman_job_priority_t priority,
                         int64_t when);

protected:
  virtual gearmand_error_t add(gearman_server_st *server,
                               const char *unique,
                               size_t unique_size,
                               const char *function_name,
                               size_t function_name_size,
                               const void *data,
                               size_t data_size,
                               gearman_job_priority_t priority,
                               int64_t when)= 0;
public:

  virtual gearmand_error_t flush(gearman_server_st *server)= 0;

  virtual gearmand_error_t done(gearman_server_st *server,
                                const char *unique,
                                size_t unique_size,
                                const char *function_name,
                                size_t function_name_size)= 0;

  virtual gearmand_error_t replay(gearman_server_st *server)= 0;

  void save_job(gearman_server_st& server,
                const gearman_server_job_st* server_job);

  static gearmand_error_t replay_add(gearman_server_st *server,
                                     void *context,
                                     const char *unique, size_t unique_size,
                                     const char *function_name, size_t function_name_size,
                                     const void *data, size_t data_size,
                                     gearman_job_priority_t priority,
                                     int64_t when);
  void store_on_shutdown(bool store_on_shutdown_)
  {
    _store_on_shutdown= store_on_shutdown_;
  }

  bool is_store_on_shutdown()
  {
    return _store_on_shutdown;
  }

  bool has_error()
  {
    return _error_string.size();
  }

  const std::string& error_string()
  {
    return _error_string;
  }

protected:
  void reset_error()
  {
    _error_string.clear();
  }

  std::string _error_string;

private:
  bool _store_on_shutdown;
};

} // namespace queue

namespace protocol {

class Context {
public:
  virtual ~Context()= 0;
  
  // If the caller should free the Context, or leave it up to the plugin
  virtual bool is_owner()
  {
    return true;
  }

  // Notify on disconnect
  virtual void notify(gearman_server_con_st*)
  {
    return;
  }

  virtual size_t pack(const gearmand_packet_st *packet,
                      gearman_server_con_st *con,
                      void *data, const size_t data_size,
                      gearmand_error_t& ret_ptr)= 0;

  virtual size_t unpack(gearmand_packet_st *packet,
                        gearman_server_con_st *con,
                        const void *data,
                        const size_t data_size,
                        gearmand_error_t& ret_ptr)= 0;

  bool has_error()
  {
    return _error_string.size();
  }

  const std::string& error_string()
  {
    return _error_string;
  }

protected:
  void reset_error()
  {
    _error_string.clear();
  }

  std::string _error_string;
};

} // namespace protocol
} // namespace gearmand
