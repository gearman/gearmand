/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Data Differential's libhostle
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

#include "gear_config.h"

#if defined(BUILD_LIBHOSTILE) && BUILD_LIBHOSTILE

#include <libhostile/initialize.h>
#include "libhostile/socket_server.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#define HOSTILE_PORT 6666

static pthread_once_t start_key_once= PTHREAD_ONCE_INIT;

static void startup(void)
{
  unsigned int seed;
  const char *seed_env= getenv("HOSTILE_SEED");
  if (seed_env)
  {
    seed= (unsigned int)strtoul(seed_env, NULL, 10);
  }
  else
  {
    seed= (unsigned int)time(NULL);
  }

  fprintf(stderr, "--------------------------------------------------------\n\n");
  fprintf(stderr, "\t\tHostile Engaged\n\n");
  fprintf(stderr, "Seed used %u (set HOSTILE_SEED to reproduce this run)\n", seed);
  fprintf(stderr, "\n--------------------------------------------------------\n");
  srand(seed);

  make_socket(HOSTILE_PORT);
}

void hostile_initialize(void)
{
  (void) pthread_once(&start_key_once, startup);
}

void hostile_dump(void)
{
  fprintf(stderr, "--------------------------------------------------------\n\n");
  fprintf(stderr, "send() used:  \n");
  fprintf(stderr, "\n--------------------------------------------------------\n");
}

#else // BUILD_LIBHOSTILE

void hostile_initialize(void);
void hostile_initialize(void)
{
}

void hostile_dump(void);
void hostile_dump(void)
{
}

#endif // BUILD_LIBHOSTILE
