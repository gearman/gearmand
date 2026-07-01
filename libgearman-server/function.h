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
 * @brief Function Declarations
 */

#pragma once

#include <libgearman-server/struct/function.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup gearman_server_function Function Declarations
 * @ingroup gearman_server
 *
 * This is a low level interface for gearman server functions. This is used
 * internally by the server interface, so you probably want to look there first.
 *
 * @{
 */

/** 
  Add a new function to a server instance.
 */
GEARMAN_API
  gearman_server_function_st * gearman_server_function_get(gearman_server_st *server,
                                                           const char *function_name,
                                                           size_t function_name_size);

/**
 * Free a server function structure.
 */
GEARMAN_API
void gearman_server_function_free(gearman_server_st *server, gearman_server_function_st *function);

/**
 * Cancel any pending epoch-job wakeup timers on all functions.  Must be
 * called before the owning event_base is allowed to exit its loop, since a
 * pending timer keeps the loop alive until it fires.
 */
GEARMAN_API
void gearman_server_function_cancel_epoch_timers(gearman_server_st *server);

/**
 * Lock/unlock server->epoch_lock, which guards epoch_wakeup_timer and
 * epoch_next_wakeup on every gearman_server_function_st.  Those fields are
 * armed and torn down from the proc thread, per-connection IO threads, and
 * the main event thread, so every read-modify-write of them must hold this
 * lock.
 */
GEARMAN_API
void gearman_server_epoch_lock(gearman_server_st *server);

GEARMAN_API
void gearman_server_epoch_unlock(gearman_server_st *server);

/** @} */

#ifdef __cplusplus
}
#endif
