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
/*
  Common include file for libtest
*/

#pragma once

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <string>

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H 
# include <sys/resource.h> 
#endif
 
#ifdef HAVE_FNMATCH_H
# include <fnmatch.h>
#endif

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#if defined(WIN32)
# include "win32/wrappers.h"
# define get_socket_errno() WSAGetLastError()
#else
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
# define INVALID_SOCKET -1
# define SOCKET_ERROR -1
# define closesocket(a) close(a)
# define get_socket_errno() errno
#endif

#include <libtest/test.hpp>

#include <libtest/is_pid.hpp>

#include <libtest/gearmand.h>
#include <libtest/blobslap_worker.h>
#include <libtest/memcached.h>
#include <libtest/drizzled.h>
#include <libtest/redis.h>

#include <libtest/libtool.hpp>
#include <libtest/killpid.h>
#include <libtest/signal.h>
#include <libtest/dns.hpp>
#include <libtest/formatter.hpp>

struct FreeFromVector
{
  template <class T>
    void operator() ( T* ptr) const
    {
      if (ptr)
      {
        free(ptr);
        ptr= NULL;
      }
    }
};

struct DeleteFromVector
{
  template <class T>
    void operator() ( T* ptr) const
    {
      delete ptr;
      ptr= NULL;
    }
};
