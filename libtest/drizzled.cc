/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Data Differential YATL (i.e. libtest)  library
 *
 *  Copyright (C) 2012 Data Differential, http://datadifferential.com/
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
#include <libtest/common.h>

#include <libtest/drizzled.h>

#include "util/instance.hpp"
#include "util/operation.hpp"

using namespace datadifferential;
using namespace libtest;

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#if defined(HAVE_LIBDRIZZLE) && HAVE_LIBDRIZZLE
# include <libdrizzle-5.1/drizzle_client.h>
#endif

using namespace libtest;

namespace libtest {
bool ping_drizzled(const in_port_t _port)
{
  (void)(_port);
#if defined(HAVE_LIBDRIZZLE) && HAVE_LIBDRIZZLE
  if (HAVE_LIBDRIZZLE)
  {
    drizzle_st *drizzle= drizzle_create(getenv("MYSQL_SERVER"),
                                        getenv("MYSQL_PORT") ? atoi("MYSQL_PORT") : DRIZZLE_DEFAULT_TCP_PORT,
                                        getenv("MYSQL_USER"),
                                        getenv("MYSQL_PASSWORD"),
                                        getenv("MYSQL_SCHEMA"), 0);

    if (drizzle == NULL)
    {
      return false;
    }

    bool success= false;

    drizzle_return_t rc;
    if ((rc= drizzle_connect(drizzle)) == DRIZZLE_RETURN_OK)
    {
      drizzle_result_st *result= drizzle_ping(drizzle, &rc);
      success= bool(result);
      drizzle_result_free(result);
    }

    if (success == true)
    { }
    else if (rc != DRIZZLE_RETURN_OK)
    {
      Error << drizzle_error(drizzle) << " localhost:" << _port;
    }

    drizzle_quit(drizzle);

    return success;
  }
#endif

  return false;
}
} // namespace libtest

class Drizzle : public libtest::Server
{
private:
public:
  Drizzle(const std::string& host_arg, in_port_t port_arg) :
    libtest::Server(host_arg, port_arg, DRIZZLED_BINARY, false)
  {
    set_pid_file();
  }

  bool ping()
  {
    size_t limit= 5;
    while (_app.check() and --limit)
    {
      if (ping_drizzled(_port))
      {
        return true;
      }
      libtest::dream(1, 0);
    }

    return false;
  }

  const char *name()
  {
    return "drizzled";
  };

  void log_file_option(Application&, const std::string&)
  {
  }

  bool has_log_file_option() const
  {
    return true;
  }

  bool broken_pid_file()
  {
    return true;
  }

  bool is_libtool()
  {
    return false;
  }

  bool has_syslog() const
  {
    return true;
  }

  bool is_valgrind() const
  {
    return false;
  }

  bool has_port_option() const
  {
    return true;
  }

  void port_option(Application& app, in_port_t arg)
  {
    if (arg > 0)
    {
      libtest::vchar_t buffer;
      buffer.resize(1024);
      snprintf(&buffer[1024], buffer.size(), "--drizzle-protocol.port=%d", int(arg));
/* The buffer is reading as 0 length but it was resized above, there's
   probably a way to define buffer so this does't happen but for now
   we'll disable the warning. */

#pragma GCC diagnostic push
#ifdef __GNUC__
#if __GNUC__ > 10
#pragma GCC diagnostic ignored "-Wstringop-overread"
#endif
#endif
      app.add_option(&buffer[1024]);
#pragma GCC diagnostic pop
    }
  }

  bool build();
};

bool Drizzle::build()
{
  if (getuid() == 0 or geteuid() == 0)
  {
    add_option("--user=root");
  }

  add_option("--verbose=INSPECT");
#if 0
  add_option("--datadir=var/drizzle");
#endif

  return true;
}

namespace libtest {

libtest::Server *build_drizzled(const char *hostname, in_port_t try_port)
{
  return new Drizzle(hostname, try_port);
}

}
