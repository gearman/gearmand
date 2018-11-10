/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2013 Data Differential, http://datadifferential.com/
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
 * @brief Gearmand Declarations
 */

#pragma once

#include <netinet/in.h>
#include <stdlib.h>
#include <poll.h>

#include <event.h>

#include <libgearman-1.0/visibility.h>
#include <libgearman-1.0/protocol.h>

#include <libgearman-server/error.h>

#include <libgearman-server/constants.h>
#include <libgearman-server/wakeup.h>
#include <libgearman-server/log.h>
#include <libgearman-server/packet.h>
#include <libgearman-server/connection.h>
#ifdef __cplusplus
#include <libgearman-server/connection.hpp>
#endif
#include <libgearman-server/function.h>
#include <libgearman-server/client.h>
#include <libgearman-server/worker.h>
#include <libgearman-server/job.h>
#include <libgearman-server/thread.h>
#include <libgearman-server/server.h>
#include <libgearman-server/gearmand_thread.h>
#include <libgearman-server/gearmand_con.h>

#include <libgearman-server/struct/gearmand.h>

#include <libgearman-server/text.h>

#include <libgearman-server/config.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup gearmand Gearmand Declarations
 *
 * This is a server implementation using the gearman_server interface.
 *
 * @{
 */

GEARMAN_API
gearmand_st *Gearmand(void);

#define Server (&(Gearmand()->server))

/**
 * Create a server instance.
 * @param host Host for the server to listen on.
 * @param port Port for the server to listen on.
 * @return Pointer to an allocated gearmand structure.
 */
GEARMAN_API
gearmand_st *gearmand_create(gearmand_config_st *config,
                             const char *host,
                             uint32_t threads,
                             int backlog,
                             const uint32_t job_retries,
                             const char *job_handle_prefix,
                             uint8_t worker_wakeup,
                             gearmand_log_fn *function, void *log_context, const gearmand_verbose_t verbose,
                             bool round_robin,
                             bool exceptions_,
                             uint32_t hashtable_buckets);

/**
 * Free resources used by a server instace.
 * @param gearmand Server instance structure previously initialized with
 *        gearmand_create.
 */
GEARMAN_API
void gearmand_free(gearmand_st *gearmand);


GEARMAN_API
gearman_server_st *gearmand_server(gearmand_st *gearmand);

/**
 * Add a port to listen on when starting server with optional callback.
 * @param gearmand Server instance structure previously initialized with
 *        gearmand_create.
 * @param port Port for the server to listen on.
 * @param function Optional callback function that is called when a connection
           has been accepted on the given port.
 * @return Standard gearman return value.
 */
GEARMAN_API
gearmand_error_t gearmand_port_add(gearmand_st *gearmand,
                                   const char *port,
                                   gearmand_connection_add_fn*,
                                   gearmand_connection_remove_fn*);

/**
 * Run the server instance.
 * @param gearmand Server instance structure previously initialized with
 *        gearmand_create.
 * @return Standard gearman return value.
 */
GEARMAN_API
gearmand_error_t gearmand_run(gearmand_st *gearmand);

bool gearmand_exceptions(gearmand_st *gearmand);

/**
 * Interrupt a running gearmand server from another thread. You should only
 * call this when another thread is currently running gearmand_run() and you
 * want to wakeup the server with the given event.
 * @param gearmand Server instance structure previously initialized with
 *        gearmand_create.
 * @param wakeup Wakeup event to send to running gearmand.
 */
GEARMAN_API
void gearmand_wakeup(gearmand_st *gearmand, gearmand_wakeup_t wakeup);

GEARMAN_API
const char *gearmand_version(void);

GEARMAN_API
const char *gearmand_bugreport(void);

GEARMAN_API
const char *gearmand_verbose_name(gearmand_verbose_t verbose);

GEARMAN_API
  gearmand_error_t gearmand_set_socket_keepalive(gearmand_st *gearmand, bool arg);

GEARMAN_API
  gearmand_error_t gearmand_set_socket_keepalive_idle(gearmand_st *gearmand, int arg);

GEARMAN_API
  gearmand_error_t gearmand_set_socket_keepalive_interval(gearmand_st *gearmand, int arg);

GEARMAN_API
  gearmand_error_t gearmand_set_socket_keepalive_count(gearmand_st *gearmand, int arg);

/** @} */

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
GEARMAN_API
bool gearmand_verbose_check(const char *name, gearmand_verbose_t& level);
#endif
