/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2013 Data Differential, http://datadifferential.com/
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

#if defined(HAVE_SSL) && HAVE_SSL

# define SSL_ERROR_SIZE 120

# if defined(HAVE_WOLFSSL) && HAVE_WOLFSSL
#  include <wolfssl/ssl.h>
#  include <wolfssl/openssl/ssl.h>
# elif defined(HAVE_OPENSSL) && HAVE_OPENSSL
enum {
  SSL_SUCCESS= 1,
};
#  include <openssl/ssl.h>
#  include <openssl/err.h>
# endif

/**
 * Formats the next pending error off the OpenSSL/wolfSSL error queue into
 * buf. SSL_get_error()'s return value (SSL_ERROR_SSL, SSL_ERROR_SYSCALL, ...)
 * must never be passed directly to ERR_error_string_n() -- it is a small
 * enum, not a packed OpenSSL error code, so doing so decodes garbage (e.g.
 * SSL_ERROR_SYSCALL == 5 decodes as "DH lib"). Returns false if the queue is
 * empty, which happens for SSL_ERROR_SYSCALL when the peer simply closed the
 * connection without a clean SSL shutdown -- not a genuine SSL failure.
 */
static inline bool gearman_ssl_error_string(char *buf, size_t buf_size)
{
  unsigned long code= ERR_get_error();
  if (code == 0)
  {
    return false;
  }
  ERR_error_string_n(code, buf, buf_size);
  return true;
}
#else
struct SSL_CTX {
};

struct SSL {
};
#endif

#include "configmake.h"
