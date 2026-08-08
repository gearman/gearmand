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

#include <atomic>

struct gearmand_st;

struct gearmand_thread_st
{
  bool is_thread_lock;
  bool is_wakeup_event;
  uint32_t count;
  uint32_t dcon_count;
  // Written unlocked by the main thread (gearmand_con_create()'s "no need
  // to lock if empty" fast path) and read unlocked by this thread's own
  // gearmand_con_check_queue(), so it must be atomic rather than plain
  // uint32_t.
  std::atomic<uint32_t> dcon_add_count;
  uint32_t free_dcon_count;
  int wakeup_fd[2];
  gearmand_st& _gearmand;
  gearmand_thread_st *next;
  gearmand_thread_st *prev;
  struct event_base *base;
  gearmand_con_st *dcon_list;
  gearmand_con_st *dcon_add_list;
  gearmand_con_st *free_dcon_list;
  gearman_server_thread_st server_thread;
  struct event wakeup_event;
  pthread_t id;
  pthread_mutex_t lock;

  gearmand_thread_st(gearmand_st&);

  gearmand_st& gearmand()
  {
    return _gearmand;
  }
};
