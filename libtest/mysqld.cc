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

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

#include <libtest/server.h>
#include <libtest/has.hpp>
#include <libtest/mysqld.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

namespace libtest {

bool mysqld_create_database(const in_port_t port, const std::string& database_name)
{
  if (mysql_client_binary() == NULL)
  {
    return false;
  }

  std::ostringstream host_arg_stream;
  host_arg_stream << "--port=" << int(port);
  std::string host_arg= host_arg_stream.str();

  std::string query_arg= "CREATE DATABASE IF NOT EXISTS " + database_name + ";";

  const char *args[]= {
    "--protocol=tcp",
    "--host=127.0.0.1",
    host_arg.c_str(),
    "--user=root",
    "--execute", query_arg.c_str(),
    NULL
  };

  return (exec_cmdline(mysql_client_binary(), args, false) == 0);
}

class MySQLd : public libtest::Server
{
private:
  std::string _datadir;

public:
  MySQLd(const std::string& host_arg, const in_port_t port_arg) :
    libtest::Server(host_arg, port_arg, mysqld_binary(), false)
  {
    set_pid_file();
  }

  bool ping()
  {
    if (out_of_ban_killed())
    {
      return false;
    }

    if (mysql_client_binary() == NULL)
    {
      return false;
    }

    std::ostringstream host_arg_stream;
    host_arg_stream << "--port=" << int(port());
    std::string host_arg= host_arg_stream.str();

    const char *args[]= {
      "--protocol=tcp",
      "--host=127.0.0.1",
      host_arg.c_str(),
      "--user=root",
      "--execute", "SELECT 1;",
      NULL
    };

    return (exec_cmdline(mysql_client_binary(), args, false) == 0);
  }

  const char *name()
  {
    return "mysqld";
  }

  const char *executable()
  {
    return mysqld_binary();
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
      app.add_option("--pid-file", arg);
    }
  }

  bool is_valgrind() const
  {
    return false;
  }

  // mysqld/mariadbd resolves a relative --pid-file against its own
  // datadir, not the caller's cwd (same reasoning as Drizzle's override
  // of this in libtest/drizzled.cc), so use the absolute /tmp form
  // Server::set_pid_file() offers instead of the default relative one.
  bool broken_pid_file()
  {
    return true;
  }

  bool build();
};

bool MySQLd::build()
{
  // mysqld/mariadbd, unlike redis-server, refuses to start against a
  // datadir that hasn't been initialized first, so give this instance its
  // own and run mariadb-install-db/mysql_install_db once before the first
  // start(). Absolute, under /tmp: mariadb-install-db resolves a relative
  // --datadir against its own install prefix (basedir), not the caller's
  // cwd, so a relative path here silently ends up somewhere under the nix
  // store instead of the intended location.
  std::ostringstream datadir_stream;
  datadir_stream << "/tmp/gearmand-test-mysqld." << int(port());
  _datadir= datadir_stream.str();

  if (mkdir(_datadir.c_str(), 0755) == -1 and errno != EEXIST)
  {
    Error << "mkdir(" << _datadir << ") failed: " << strerror(errno);
    return false;
  }

  if (mysql_install_db_binary())
  {
    // mariadb-install-db is a shell script that shells out to sed and its
    // own sibling tools via $PATH, but Application/exec_cmdline's
    // posix_spawn() call always passes an empty environment (by design,
    // for reproducible test subprocesses), so it can't find them that way.
    // Use system() for this one-off setup step instead, since it inherits
    // the environment naturally through /bin/sh -c.
    std::string log_path= _datadir + "/install-db.log";
    std::string command= std::string(mysql_install_db_binary()) +
      " --datadir=" + _datadir +
      " --auth-root-authentication-method=normal"
      " >" + log_path + " 2>&1";

    if (system(command.c_str()) != 0)
    {
      std::string log_contents;
      FILE *log_fp= fopen(log_path.c_str(), "r");
      if (log_fp)
      {
        char buf[4096];
        size_t n;
        while ((n= fread(buf, 1, sizeof(buf), log_fp)) > 0)
        {
          log_contents.append(buf, n);
        }
        fclose(log_fp);
      }
      Error << "mysql_install_db/mariadb-install-db failed for datadir " << _datadir
            << " output:" << log_contents;
      return false;
    }
  }

  add_option("--datadir", _datadir);
  add_option("--tmpdir", _datadir);
  // Avoid the system-wide default socket path (/run/mysqld/mysqld.sock),
  // which isn't writable in a sandboxed/unprivileged environment.
  add_option("--socket", _datadir + "/mysqld.sock");
  add_option("--skip-grant-tables");
  add_option("--bind-address", "127.0.0.1");
  // Small on purpose: lets tests exercise the oversized-packet/
  // CR_SERVER_LOST path (#126) with a payload of a few megabytes instead
  // of needing to actually send something close to the real-world default.
  add_option("--max_allowed_packet", "1M");

  return true;
}

libtest::Server *build_mysqld(const std::string& hostname, const in_port_t try_port)
{
  if (has_mysqld() and has_mysql_install_db())
  {
    return new MySQLd(hostname, try_port);
  }

  return NULL;
}

} // namespace libtest
