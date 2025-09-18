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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

namespace libtest {

bool lookup(const char* host)
{
  bool success= false;
  assert(host and host[0]);
  if (host and host[0])
  {
    struct addrinfo *addrinfo= NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype= SOCK_STREAM;
    hints.ai_protocol= IPPROTO_TCP;

    int limit= 5;
    while (--limit and success == false)
    {
      if (addrinfo)
      {
        freeaddrinfo(addrinfo);
        addrinfo= NULL;
      }

      int ret;
      if ((ret= getaddrinfo(host, "echo", &hints, &addrinfo)) == 0)
      {
        success= true;
        break;
      }

      switch (ret)
      {
      case EAI_AGAIN:
        continue;

      case EAI_NONAME:
      default:
        break;
      }

      break;
    }

    if (addrinfo)
    {
      freeaddrinfo(addrinfo);
    }
  }

  return success;
}


bool check_dns()
{
  if (valgrind_is_caller())
  {
    return false;
  }

  if (lookup("www.google.com") == false)
  {
    return false;
  }

  if (lookup("does_not_exist.google.com")) // This should fail, if it passes,...
  {
    fatal_assert("Your service provider is providing bogus DNS. You might be in an airport.");
  }

  return true;
}

} // namespace libtest

