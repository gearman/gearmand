/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2012 Data Differential, http://datadifferential.com/
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

#include <bin/function.h>

namespace gearman_client
{

/**
 * Data structure for arguments and state.
 */
class Args
{
public:
  Args(int p_argc, char *p_argv[]);

  ~Args();

  bool usage()
  {
    return _usage;
  }

  bool prefix() const
  {
    return _prefix;
  }

  bool background() const
  {
    return _background;
  }

  bool daemon() const
  {
    return _daemon;
  }

  gearman_job_priority_t priority() const
  {
    return _priority;
  }

  int timeout() const
  {
    return _timeout;
  }

  const char *unique() const
  {
    return _unique;
  }

  bool job_per_newline() const
  {
    return _job_per_newline;
  }

  Function::vector::iterator begin()
  {
    return _functions.begin();
  }

  Function::vector::iterator end()
  {
    return _functions.end();
  }

  bool strip_newline() const
  {
    return _strip_newline;
  }

  bool worker() const
  {
    return _worker;
  }

  const std::string &pid_file() const
  {
    return _pid_file;
  }

  int error() const
  {
    return _error;
  }

  void set_error() const
  {
    _error= 1;
  }

  uint32_t &count()
  {
    return _count;
  }

  const char *host() const
  {
    return _host;
  }

  in_port_t port() const
  {
    return _port;
  }

  bool arguments() const
  {
    if (argv[0])
      return true;

    return false;
  }

  bool verbose() const
  {
    return _verbose;
  }

  bool suppress_input() const
  {
    return _suppress_input;
  }

  bool use_ssl() const
  {
    return _use_ssl;
  }

  bool ping() const
  {
    return _ping;
  }

  const char *argument(size_t arg)
  {
    return argv[arg];
  }

  char **argumentv()
  {
    return argv;
  }

  void reset_arg_error()
  {
    if (_arg_error)
    {
      free(_arg_error);
      _arg_error= NULL;
    }
  }

  bool is_error()
  {
    if ((! _ping) && _functions.empty())
    {
      reset_arg_error();
      _arg_error= strdup("No Functions were provided");

      return true;
    }

    return _is_error;
  }

  const char* arg_error() const
  {
    return _arg_error;
  }

  bool is_valid() const
  {
    return _functions.size();
  }

private:
  Function::vector _functions;
  char *_host;
  in_port_t _port;
  uint32_t _count;
  char *_unique;
  bool _job_per_newline;
  bool _strip_newline;
  bool _worker;
  bool _suppress_input;
  bool _verbose;

  bool _prefix;
  bool _background;
  bool _daemon;
  bool _usage;
  bool _is_error;
  bool _use_ssl;
  bool _ping;
  char *_arg_error;
  gearman_job_priority_t _priority;
  int _timeout;
  char **argv;
  mutable int _error;
  std::string _pid_file;

  void init(int argc);

  void add(const char *name)
  {
    _functions.push_back(Function(name));
  }
};

} // namespace gearman_client
