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

#include "gear_config.h"

#include <libtest/test.hpp>

#include "libgearman/client.hpp"

using namespace org::gearmand;
using namespace libtest;

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <vector>

test_return_t fork_SETUP(void*)
{
  return TEST_SUCCESS;
}

test_return_t check_client_fork_TEST(void*)
{
  test_skip_valgrind();

  libgearman::Client client(libtest::default_port());

#ifdef HAVE_FORK
  {
    ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_echo(&client, test_literal_param("begin all forked")));
    pid_t child= fork();

    switch (child)
    {
      case -1:
        FAIL("fork() failed with %s", strerror(errno));

      case 0:
        {
          gearman_return_t ret;
          if (gearman_failed(ret= gearman_client_echo(&client, test_literal_param("child forked"))))
          {
            Error << gearman_client_error(&client);
            _exit(int(ret));
          }
          _exit(0);
        }

      default:
        {
          int exit_code= EXIT_FAILURE;
          int status;
          if (waitpid(child, &status, 0) == -1)
          {
            FAIL("waitpid() failed with %s", strerror(errno));
          }
          else
          {
            if (WIFEXITED(status))
            {
              exit_code= WEXITSTATUS(status);
            }

            if (WIFSIGNALED(status))
            {
              FAIL("child was killed by signal %s", strsignal(WTERMSIG(status)));
            }
            ASSERT_EQ(EXIT_SUCCESS, exit_code);
          }

          ASSERT_EQ(GEARMAN_SUCCESS, gearman_client_echo(&client, test_literal_param("parent forked")));
        }
    }
    return TEST_SUCCESS;
  }
#else

  return TEST_SKIPPED;
#endif // HAVE_FORK
}
