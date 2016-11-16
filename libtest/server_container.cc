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

#include "libtest/common.h"

#include <cerrno>
#include <cstdlib>
#include <iostream>

#include <algorithm> 
#include <functional> 
#include <locale>

// trim from end 
static inline std::string &rtrim(std::string &s)
{ 
  s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end()); 
  return s; 
}

namespace libtest {

Server* server_startup_st::last()
{
  return servers.back();
}

void server_startup_st::push_server(Server *arg)
{
  assert(arg);
  servers.push_back(arg);

  std::string server_config_string;
  if (arg->has_socket())
  {
    server_config_string+= "--socket=";
    server_config_string+= '"';
    server_config_string+= arg->socket();
    server_config_string+= '"';
    server_config_string+= " ";
  }
  else
  {
    libtest::vchar_t port_str;
    port_str.resize(NI_MAXSERV);
    snprintf(&port_str[0], port_str.size(), "%u", int(arg->port()));

    server_config_string+= "--server=";
    server_config_string+= arg->hostname();
    server_config_string+= ":";
    server_config_string+= &port_str[0];
    server_config_string+= " ";
  }

  server_list+= server_config_string;
}

Server* server_startup_st::pop_server()
{
  Server *tmp= servers.back();
  servers.pop_back();
  return tmp;
}

// host_to_shutdown => host number to shutdown in array
bool server_startup_st::shutdown(uint32_t host_to_shutdown)
{
  if (servers.size() > host_to_shutdown)
  {
    Server* tmp= servers[host_to_shutdown];

    if (tmp and tmp->kill() == false)
    { }
    else
    {
      return true;
    }
  }

  return false;
}

void server_startup_st::clear()
{
  std::for_each(servers.begin(), servers.end(), DeleteFromVector());
  servers.clear();
}

bool server_startup_st::check() const
{
  bool success= true;
  for (std::vector<Server *>::const_iterator iter= servers.begin(); iter != servers.end(); ++iter)
  {
    if ((*iter)->check()  == false)
    {
      success= false;
    }
  }

  return success;
}

bool server_startup_st::shutdown()
{
  bool success= true;
  for (std::vector<Server *>::iterator iter= servers.begin(); iter != servers.end(); ++iter)
  {
    if ((*iter)->has_pid() and (*iter)->kill() == false)
    {
      Error << "Unable to kill:" <<  *(*iter);
      success= false;
    }
  }

  return success;
}

void server_startup_st::restart()
{
  for (std::vector<Server *>::iterator iter= servers.begin(); iter != servers.end(); ++iter)
  {
    (*iter)->start();
  }
}

#define MAGIC_MEMORY 123575
server_startup_st::server_startup_st() :
  _magic(MAGIC_MEMORY),
  _socket(false),
  _sasl(false),
  udp(0),
  _servers_to_run(5)
{ }

server_startup_st::~server_startup_st()
{
  clear();
}

bool server_startup_st::validate()
{
  return _magic == MAGIC_MEMORY;
}

bool server_startup(server_startup_st& construct, const std::string& server_type, in_port_t try_port, const char *argv[])
{
  return construct.start_server(server_type, try_port, argv);
}

libtest::Server* server_startup_st::create(const std::string& server_type, in_port_t try_port, const bool is_socket)
{
  libtest::Server *server= NULL;

  if (is_socket == false)
  {
    if (try_port <= 0)
    {
      throw libtest::fatal(LIBYATL_DEFAULT_PARAM, "was passed the invalid port number %d", int(try_port));
    }
  }

  if (is_socket)
  { 
    if (server_type.compare("memcached") == 0)
    {
      server= build_memcached_socket("localhost", try_port);
    }
    else
    {
      Error << "Socket is not support for server: " << server_type;
      return NULL;
    }
  }
  else if (server_type.compare("gearmand") == 0)
  {
    server= build_gearmand("localhost", try_port);
  }
  else if (server_type.compare("hostile-gearmand") == 0)
  {
    server= build_gearmand("localhost", try_port, "gearmand/hostile_gearmand");
  }
  else if (server_type.compare("drizzled") == 0)
  {
    if (has_drizzled())
    {
      if (has_libdrizzle())
      {
        server= build_drizzled("localhost", try_port);
      }
    }
  }
  else if (server_type.compare("blobslap_worker") == 0)
  {
    if (has_gearmand())
    {
#ifdef GEARMAND_BLOBSLAP_WORKER
      if (GEARMAND_BLOBSLAP_WORKER)
      {
        if (HAVE_LIBGEARMAN)
        {
          server= build_blobslap_worker(try_port);
        }
      }
#endif // GEARMAND_BLOBSLAP_WORKER
    }
  }
  else if (server_type.compare("memcached") == 0)
  {
    if (has_memcached())
    {
      server= build_memcached("localhost", try_port);
    }
  }

  return server;
}

class ServerPtr {
public:
  ServerPtr(libtest::Server* server_):
    _server(server_)
  { }

  ~ServerPtr()
  {
    delete _server;
  }

  void reset()
  {
    delete _server;
    _server= NULL;
  }

  libtest::Server* release(libtest::Server* server_= NULL)
  {
    libtest::Server* tmp= _server;
    _server= server_;
    return tmp;
  }

  libtest::Server* operator->() const
  {
    return _server;
  }

  libtest::Server* operator&() const
  { 
    return _server;
  }

private:
  libtest::Server* _server;
};

bool server_startup_st::_start_server(const bool is_socket,
                                      const std::string& server_type,
                                      in_port_t try_port,
                                      const char *argv[])
{
  try {
    ServerPtr server(create(server_type, try_port, is_socket));

    if (&server == NULL)
    {
      Error << "Could not allocate server: " << server_type;
      return false;
    }

    /*
      We will now cycle the server we have created. (In case it was already running?)
    */
    if (server->cycle() == false)
    {
      Error << "Could not start up server " << &server;
      return false;
    }

    server->init(argv);

    if (server->start())
    {
      {
#ifdef DEBUG
        if (DEBUG)
        {
          Outn();
          Out << "STARTING SERVER(pid:" << server->pid() << "): " << server->running();
          Outn();
        }
#endif
      }
    }
    else
    {
      return false;
    }

    if (is_socket and &server)
    {
      set_default_socket(server->socket().c_str());
    }

    push_server(server.release());
  }
  catch (const libtest::disconnected& err)
  {
    if (fatal::is_disabled() == false and try_port != LIBTEST_FAIL_PORT)
    {
      stream::cerr(err.file(), err.line(), err.func()) << err.what();
      return false;
    }
  }
  catch (const libtest::__test_result& err)
  {
    stream::cerr(err.file(), err.line(), err.func()) << err.what();
    return false;
  }
  catch (const std::exception& err)
  {
    Error << err.what();
    return false;
  }
  catch (...)
  {
    Error << "error occurred while creating server: " << server_type;
    return false;
  }

  return true;
}

bool server_startup_st::start_server(const std::string& server_type, in_port_t try_port, const char *argv[])
{
  return _start_server(false, server_type, try_port, argv);
}

bool server_startup_st::start_socket_server(const std::string& server_type, const in_port_t try_port, const char *argv[])
{
  return _start_server(true, server_type, try_port, argv);
}

std::string server_startup_st::option_string() const
{
  std::string temp= server_list;
  rtrim(temp);
  return temp;
}


} // namespace libtest
