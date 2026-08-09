/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  YATL (i.e. libtest) library
 *
 *  Copyright (C) 2026 Alexei Pastuchov
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

#include "libtest/yatlcon.h"

#include "libtest/common.h"

#include <cstdio>
#include <cstring>

#include <libtest/server.h>
#include <libtest/has.hpp>
#include <libtest/redis.h>

#if defined(HAVE_HIREDIS) && HAVE_HIREDIS
#include <hiredis/hiredis.h>
#endif

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

namespace libtest {

class Redis : public libtest::Server
{
public:
  Redis(const std::string& host_arg, const in_port_t port_arg) :
    libtest::Server(host_arg, port_arg, redis_server_binary(), false)
  {
    set_pid_file();
  }

  bool ping()
  {
    if (out_of_ban_killed())
    {
      return false;
    }

#if defined(HAVE_HIREDIS) && HAVE_HIREDIS
    redisContext *context= redisConnect(hostname().c_str(), int(port()));
    if (context == nullptr)
    {
      return false;
    }

    bool success= (context->err == 0);
    if (success)
    {
      redisReply *reply= (redisReply*)redisCommand(context, "PING");
      success= (reply != nullptr and reply->type == REDIS_REPLY_STATUS);
      if (reply)
      {
        freeReplyObject(reply);
      }
    }

    redisFree(context);

    return success;
#else
    return false;
#endif
  }

  const char *name()
  {
    return "redis";
  }

  const char *executable()
  {
    return redis_server_binary();
  }

  bool is_libtool()
  {
    return false;
  }

  bool has_port_option() const
  {
    return true;
  }

  virtual void port_option(Application& app, in_port_t arg)
  {
    char buffer[30];
    snprintf(buffer, sizeof(buffer), "%d", int(arg));
    app.add_option("--port", buffer);
  }

  virtual void pid_file_option(Application& app, const std::string& arg)
  {
    if (arg.empty() == false)
    {
      app.add_option("--pidfile", arg);
    }
  }

  bool is_valgrind() const
  {
    return false;
  }

  bool build();
};

bool Redis::build()
{
  // Keep the throwaway test instance in the foreground so libtest tracks
  // its actual pid (it forks/execs redis-server itself and needs that pid
  // to stay accurate; a self-daemonizing redis-server would otherwise
  // leave libtest tracking the wrong process).
  add_option("--daemonize", "no");

  return true;
}

libtest::Server *build_redis(const std::string& hostname, const in_port_t try_port)
{
  if (has_redis())
  {
    return new Redis(hostname, try_port);
  }

  return NULL;
}

} // namespace libtest
