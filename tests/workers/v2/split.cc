/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
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

#include <gear_config.h>
#include <libtest/test.hpp>

using namespace libtest;

#include <libgearman-1.0/gearman.h>

#include "tests/workers/v2/split.h"

#include <cassert>
#include <cstring>


gearman_return_t split_worker(gearman_job_st *job, void* /* context */)
{
  const char *workload= static_cast<const char *>(gearman_job_workload(job));
  size_t workload_size= gearman_job_workload_size(job);

#if 0
  assert(job->assigned.command == GEARMAN_COMMAND_JOB_ASSIGN_ALL);
#endif

  const char *chunk_begin= workload;
  for (size_t x= 0; x < workload_size; x++)
  {
    if (int(workload[x]) == 0 or int(workload[x]) == int(' '))
    {
      if ((workload +x -chunk_begin) == 11 and memcmp(chunk_begin, test_literal_param("mapper_fail")) == 0)
      {
        return gearman_job_send_exception(job, test_literal_param("Error occurred on purpose"));
      }

      // NULL Chunk
      gearman_return_t rc= gearman_job_send_data(job, chunk_begin, workload +x -chunk_begin);
      if (gearman_failed(rc))
      {
        return gearman_job_send_exception(job, test_literal_param("gearman_job_send_data() failed"));
      }

      chunk_begin= workload +x +1;
    }
  }

  if (chunk_begin < workload +workload_size)
  {
    if ((size_t(workload +workload_size) -size_t(chunk_begin) ) == 11 and not memcmp(chunk_begin, test_literal_param("mapper_fail")))
    {
      return gearman_job_send_exception(job, test_literal_param("Error occurred on purpose"));
    }

    gearman_return_t rc= gearman_job_send_data(job, chunk_begin, size_t(workload +workload_size) -size_t(chunk_begin));
    if (gearman_failed(rc))
    {
      return gearman_job_send_exception(job, test_literal_param("gearman_job_send_data() failed"));
    }
  }

  return GEARMAN_SUCCESS;
}
