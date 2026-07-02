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

#pragma once

#include <set>

struct gearman_server_function_st
{
  uint32_t worker_count;
  uint32_t job_count;
  uint32_t job_total;
  uint32_t job_running;
  uint32_t max_queue_size[GEARMAN_JOB_PRIORITY_MAX];
  size_t function_name_size;
  gearman_server_function_st *next;
  gearman_server_function_st *prev;
  char *function_name;
  gearman_server_worker_st *worker_list;
  struct gearman_server_job_st *job_list[GEARMAN_JOB_PRIORITY_MAX];
  gearman_server_job_st *job_end[GEARMAN_JOB_PRIORITY_MAX];
  /* One-shot libevent timer that fires when the earliest epoch job becomes
     runnable, waking sleeping workers.  NULL when no timer is pending. */
  struct event *epoch_wakeup_timer;
  int64_t epoch_next_wakeup;
  /* `when` timestamps of queued-but-not-yet-fired-for epoch jobs, guarded by
     server->epoch_lock. Tracked independently of job_list (which is owned
     exclusively by the proc thread) so the main-thread wakeup callback never
     reads job_list. A stale entry (job already grabbed/removed elsewhere)
     just costs one harmless extra NOOP. */
  std::multiset<int64_t> epoch_pending_whens;
};

