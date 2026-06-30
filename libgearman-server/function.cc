/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2012 Data Differential, http://datadifferential.com/
 *  Copyright (C) 2008 Brian Aker, Eric Day
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

/**
 * @file
 * @brief Server function definitions
 */

#include "gear_config.h"
#include "libgearman-server/common.h"

#include <cstring>
#include <memory>

/*
 * Public definitions
 */

static uint32_t _server_function_hash(const char *name, size_t size)
{
  const char *ptr= name;
  int32_t value= 0;

  while (size--)
  {
    value += (int32_t)*ptr++;
    value += (value << 10);
    value ^= (value >> 6);
  }

  value += (value << 3);
  value ^= (value >> 11);
  value += (value << 15);

  return (uint32_t)(value == 0 ? 1 : value);
}

#pragma GCC diagnostic push
#ifndef __INTEL_COMPILER
# pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
static gearman_server_function_st* gearman_server_function_create(gearman_server_st *server,
                                                                  const char *function_name,
                                                                  size_t function_name_size,
                                                                  uint32_t function_key)
{
  gearman_server_function_st* function= new (std::nothrow) gearman_server_function_st;

  if (function == NULL)
  {
    gearmand_merror("new gearman_server_function_st", gearman_server_function_st, 0);
    return NULL;
  }

  function->worker_count= 0;
  function->job_count= 0;
  function->job_total= 0;
  function->job_running= 0;
  memset(function->max_queue_size, GEARMAND_DEFAULT_MAX_QUEUE_SIZE, sizeof(uint32_t) * GEARMAN_JOB_PRIORITY_MAX);

  function->function_name= new char[function_name_size +1];
  if (function->function_name == NULL)
  {
    gearmand_merror("new[]", char,  function_name_size +1);
    delete function;
    return NULL;
  }

  memcpy(function->function_name, function_name, function_name_size);
  function->function_name[function_name_size]= 0;
  function->function_name_size= function_name_size;
  function->worker_list= NULL;
  memset(function->job_list, 0,
         sizeof(gearman_server_job_st *) * GEARMAN_JOB_PRIORITY_MAX);
  memset(function->job_end, 0,
         sizeof(gearman_server_job_st *) * GEARMAN_JOB_PRIORITY_MAX);
  function->epoch_wakeup_timer= NULL;
  function->epoch_next_wakeup= 0;
  GEARMAND_HASH__ADD(server->function, function_key, function);
  return function;
}

gearman_server_function_st *
gearman_server_function_get(gearman_server_st *server,
                            const char *function_name,
                            size_t function_name_size)
{
  gearman_server_function_st *function;

  uint32_t function_hash = _server_function_hash(function_name, function_name_size) % GEARMAND_DEFAULT_HASH_SIZE;
  for (function= server->function_hash[function_hash]; function != NULL;
       function= function->next)
  {
    if (function->function_name_size == function_name_size and
        memcmp(function->function_name, function_name, function_name_size) == 0)
    {
      return function;
    }
  }

  return gearman_server_function_create(server, function_name, function_name_size, function_hash);
}

void gearman_server_function_free(gearman_server_st *server, gearman_server_function_st *function)
{
  uint32_t function_key;
  function_key= _server_function_hash(function->function_name, function->function_name_size);
  function_key= function_key % GEARMAND_DEFAULT_HASH_SIZE;
  GEARMAND_HASH__DEL(server->function, function_key, function);
  if (function->epoch_wakeup_timer != NULL)
  {
    timeout_del(function->epoch_wakeup_timer);
    free(function->epoch_wakeup_timer);
    function->epoch_wakeup_timer= NULL;
  }
  delete [] function->function_name;
  delete function;
}
#pragma GCC diagnostic pop
