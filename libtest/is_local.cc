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

#include "libgearman/ssl.h"


namespace libtest {

bool test_is_local()
{
  return (getenv("LIBTEST_LOCAL"));
}

static bool _is_massive= false;
void is_massive(bool arg)
{
  _is_massive= arg;
}

bool is_massive()
{
  return _is_massive;
}

static bool _is_ssl= false;
void is_ssl(bool arg)
{
  (void)arg;
#if defined(HAVE_SSL)
  _is_ssl= arg;
#endif // defined(HAVE_SSL)

  if (_is_ssl)
  {
    setenv("GEARMAND_CA_CERTIFICATE", YATL_CA_CERT_PEM, 0);
    setenv("GEARMAND_SERVER_PEM", YATL_CERT_PEM, 0);
    setenv("GEARMAND_SERVER_KEY", YATL_CERT_KEY_PEM, 0);
    setenv("GEARMAND_CLIENT_PEM", YATL_CERT_PEM, 0);
    setenv("GEARMAND_CLIENT_KEY", YATL_CERT_KEY_PEM, 0);
  }
  else
  {
    unsetenv("GEARMAND_CA_CERTIFICATE");
    unsetenv("GEARMAND_SERVER_PEM");
    unsetenv("GEARMAND_SERVER_KEY");
    unsetenv("GEARMAND_CLIENT_PEM");
    unsetenv("GEARMAND_CLIENT_KEY");
  }
}

bool is_ssl()
{
  return _is_ssl;
}

} // namespace libtest

