/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2013 Data Differential, http://datadifferential.com/
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include <netdb.h>
#include <sys/socket.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libgearman-1.0/protocol.h>
#include <boost/program_options.hpp>

#include <util/instance.hpp>
#include <util/string.hpp>

using namespace datadifferential;

/*
  This is a very quickly build application, I am just tired of telneting to the port.
*/

class Finish : public util::Instance::Finish
{
public:
  bool call(bool success, const std::string &response)
  {
    if (success)
    {
      if (response.empty())
      {
        std::cout << "OK" << std::endl;
      }
      else
      {
        std::cout << response;
      }
    }
    else if (not response.empty())
    {
      std::cerr << "Error: " << response;
    }
    else
    {
#if 0
      std::cerr << "Error" << std::endl;
#endif
    }

    return true;
  }
};


int main(int args, char *argv[])
{
  boost::program_options::options_description desc("Options");
  std::string host;
  std::string port;

  desc.add_options()
    ("help", "Options related to the program.")
    ("host,h", boost::program_options::value<std::string>(&host)->default_value("localhost"),"Connect to the host")
    ("port,p", boost::program_options::value<std::string>(&port)->default_value(GEARMAN_DEFAULT_TCP_PORT_STRING), "Port number or service to use for connection")
    ("server-version", "Fetch the version number for the server.")
    ("server-verbose", "Fetch the verbose setting for the server.")
    ("create-function", boost::program_options::value<std::string>(), "Create the function from the server.") 
    ("cancel-job", boost::program_options::value<std::string>(), "Remove a given job from the server's queue")
    ("drop-function", boost::program_options::value<std::string>(), "Drop the function from the server.")
    ("show-unique-jobs", "Show unique jobs on server.")
    ("show-jobs", "Show all jobs on the server.")
    ("getpid", "Get Process ID for the server.")
    ("status", "Status for the server.")
    ("priority-status", "Queued jobs status by priority.")
    ("workers", "Workers for the server.")
    ("ssl,S", "Enable SSL connections.")
            ;

  boost::program_options::variables_map vm;
  try
  {
    boost::program_options::store(boost::program_options::parse_command_line(args, argv, desc), vm);
    boost::program_options::notify(vm);
  }
  catch(std::exception &e)
  { 
    std::cout <<  argv[0] << " : " << e.what() << std::endl;
    std::cout <<  std::endl << desc << std::endl;
    return EXIT_FAILURE;
  }

  util::Instance instance(host, port);
  instance.set_finish(new Finish);

  if (vm.count("ssl"))
  {
    instance.use_ssl(true);
  }

  if (vm.count("help"))
  {
    std::cout << desc << std::endl;
    return EXIT_SUCCESS;
  }

  if (vm.count("server-version") == 0 and
     vm.count("server-verbose")  == 0 and
     vm.count("create-function")  == 0 and
     vm.count("drop-function")  == 0 and
     vm.count("cancel-job") == 0 and
     vm.count("show-unique-jobs") == 0 and
     vm.count("show-jobs") == 0 and
     vm.count("getpid") == 0 and
     vm.count("status") == 0 and
     vm.count("priority-status") == 0 and
     vm.count("workers") == 0)
  {
    std::cout << "No option execution operation given." << std::endl << std::endl;
    std::cout << desc << std::endl;
    return EXIT_FAILURE;
  }


  if (vm.count("status"))
  {
    instance.push(new util::Operation(util_literal_param("status\r\n")));
  }

  if (vm.count("priority-status"))
  {
    instance.push(new util::Operation(util_literal_param("prioritystatus\r\n")));
  }

  if (vm.count("workers"))
  {
    instance.push(new util::Operation(util_literal_param("workers\r\n")));
  }

  if (vm.count("server-version"))
  {
    instance.push(new util::Operation(util_literal_param("version\r\n")));
  }

  if (vm.count("server-verbose"))
  {
    instance.push(new util::Operation(util_literal_param("verbose\r\n")));
  }

  if (vm.count("cancel-job"))
  {
    std::string execute(util_literal_param("cancel job "));
    execute.append(vm["cancel-job"].as<std::string>());
    execute.append("\r\n");
    instance.push(new util::Operation(execute.c_str(), execute.size()));
  }

  if (vm.count("show-unique-jobs"))
  {
    instance.push(new util::Operation(util_literal_param("show unique jobs\r\n")));
  }

  if (vm.count("show-jobs"))
  {
    instance.push(new util::Operation(util_literal_param("show jobs\r\n")));
  }

  if (vm.count("drop-function"))
  {
    std::string execute(util_literal_param("drop function "));
    execute.append(vm["drop-function"].as<std::string>());
    execute.append("\r\n");
    instance.push(new util::Operation(execute.c_str(), execute.size()));
  }

  if (vm.count("create-function"))
  {
    std::string execute(util_literal_param("create function "));
    execute.append(vm["create-function"].as<std::string>());
    execute.append("\r\n");
    instance.push(new util::Operation(execute.c_str(), execute.size()));
  }

  if (vm.count("getpid"))
  {
    instance.push(new util::Operation(util_literal_param("getpid\r\n")));
  }

  if (not instance.run())
  {
    /* will produce a read error since nothing is read */
    std::cerr << "Error: " << instance.last_error() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
