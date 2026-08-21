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


#include "gear_config.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <unistd.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <libgearman/gearman.h>
#include "arguments.h"

namespace gearman_client
{

Args::Args(int p_argc, char *p_argv[]) :
  _host(NULL),
  _port(0),
  _count(0),
  _unique(NULL),
  _job_per_newline(false),
  _strip_newline(false),
  _worker(false),
  _suppress_input(false),
  _verbose(false),
  _prefix(false),
  _background(false),
  _daemon(false),
  _usage(false),
  _is_error(false),
  _use_ssl(false),
  _ping(false),
  _arg_error(NULL),
  _priority(GEARMAN_JOB_PRIORITY_NORMAL),
  _timeout(-1),
  argv(p_argv),
  _error(0)
{
  init(p_argc);
}

Args::~Args()
{
  reset_arg_error();
}

void Args::init(int argc)
{
  int c;

  /* Portable handling of --ping to avoid non-portable getopt_long(). */
  for (int i= 1; i < argc; )
  {
    if (strcmp(argv[i], "--ping") == 0)
    {
      _ping= true;
      /* Remove this argument so getopt() does not treat it as invalid. */
      for (int j= i; j < argc - 1; j++)
      {
        argv[j]= argv[j + 1];
      }
      argv[--argc]= NULL;
    }
    else
    {
      i++;
    }
  }

  opterr= 0;

  while ((c = getopt(argc, argv, "bc:f:h:HILnNp:Pst:u:vwi:dS")) != -1)
  {
    switch(c)
    {
    case 'b':
      _background= true;
      break;

    case 'c':
      _count= static_cast<uint32_t>(atoi(optarg));
      break;

    case 'd':
      _daemon= true;
      break;

    case 'f':
      add(optarg);
      break;

    case 'h':
      _host= optarg;
      break;

    case 'i':
      _pid_file= optarg;
      break;

    case 'I':
      _priority= GEARMAN_JOB_PRIORITY_HIGH;
      break;

    case 'L':
      _priority= GEARMAN_JOB_PRIORITY_LOW;
      break;

    case 'n':
      _job_per_newline= true;
      break;

    case 'N':
      _job_per_newline= true;
      _strip_newline= true;
      break;

    case 'p':
      _port= static_cast<in_port_t>(atoi(optarg));
      break;

    case 'P':
      _prefix= true;
      break;

    case 's':
      _suppress_input= true;
      break;

    case 't':
      _timeout= atoi(optarg);
      break;

    case 'u':
      _unique= optarg;
      break;

    case 'w':
      _worker= true;
      break;

    case 'H':
      _usage= true;
      break;

    case 'v':
      _verbose= true;
      break;

    case 'S':
      _use_ssl= true;
      break;

    default:
      _is_error= true;

      if (optarg)
      {
        size_t length= snprintf(NULL, 0, "-%c %s", char(c), optarg);
        ++length; // Add in space for null
        reset_arg_error();
        _arg_error= (char*)malloc(length);
        if (_arg_error)
        {
          snprintf(_arg_error, length, "-%c %s", char(c), optarg);
        }
      }
      else
      {
        size_t length= snprintf(NULL, 0, "-%c", char(c));
        length++;
        reset_arg_error();
        _arg_error= (char*)malloc(length);
        if (_arg_error)
        {
          snprintf(_arg_error, length, "-%c", char(c));
        }
      }
      break;
    }
  }

  argv+= optind;
}

} // namespace gearman_client
