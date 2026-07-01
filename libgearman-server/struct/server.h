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

struct queue_st {
  void *_context;
  gearman_queue_add_fn *_add_fn;
  gearman_queue_flush_fn *_flush_fn;
  gearman_queue_done_fn *_done_fn;
  gearman_queue_replay_fn *_replay_fn;

  queue_st() :
    _context(NULL),
    _add_fn(NULL),
    _flush_fn(NULL),
    _done_fn(NULL),
    _replay_fn(NULL)
  {
  }
};

enum queue_version_t {
  QUEUE_VERSION_NONE,
  QUEUE_VERSION_FUNCTION,
  QUEUE_VERSION_CLASS
};

namespace gearmand { namespace queue { class Context; } }

struct Queue_st {
  struct queue_st* functions{};
  gearmand::queue::Context* object{};
};

struct gearman_server_st
{
  struct Flags {
    /*
      Sets the round-robin mode on the server object. RR will distribute work
      fairly among every function assigned to a worker, instead of draining
      each function before moving on to the next.
    */
    bool round_robin;
    bool threaded;
  } flags;
  struct State {
    bool queue_startup;
  } state;
  bool shutdown{};
  bool shutdown_graceful{};
  bool proc_wakeup{};
  bool proc_shutdown{};
  uint32_t job_retries{}; // Set maximum job retry count.
  uint8_t worker_wakeup{}; // Set maximum number of workers to wake up per job.
  uint32_t job_handle_count{};
  uint32_t thread_count{};
  uint32_t function_count{};
  uint32_t job_count{};
  uint32_t unique_count{};
  uint32_t free_packet_count{};
  uint32_t free_job_count{};
  uint32_t free_client_count{};
  uint32_t free_worker_count{};
  gearman_server_thread_st *thread_list{nullptr};
  gearman_server_function_st **function_hash{nullptr};
  gearman_server_packet_st *free_packet_list{nullptr};
  gearman_server_job_st *free_job_list{nullptr};
  gearman_server_client_st *free_client_list{nullptr};
  gearman_server_worker_st *free_worker_list{nullptr};
  enum queue_version_t queue_version{};
  struct Queue_st queue{};
  pthread_mutex_t proc_lock{};
  pthread_cond_t proc_cond{};
  pthread_t proc_id{};
  /* Guards epoch_wakeup_timer/epoch_next_wakeup on every gearman_server_function_st,
     since those are armed/torn down from the proc thread, per-connection IO threads,
     and the main event thread. */
  pthread_mutex_t epoch_lock{};
  char job_handle_prefix[GEARMAND_JOB_HANDLE_SIZE];
  uint32_t hashtable_buckets{};
  gearman_server_job_st **job_hash{nullptr};
  gearman_server_job_st **unique_hash{nullptr};

  gearman_server_st()
  {
  }

  ~gearman_server_st()
  {
  }
};
