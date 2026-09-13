/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2026 Data Differential, http://datadifferential.com/
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

/**
 * Pick the connection that should receive a job keyed by (key, key_length),
 * using a ketama consistent-hash ring built from the client's current
 * connection list. This gives the same server for the same key as long as
 * that server stays in the list, and only reshuffles the keys owned by a
 * server that is added or removed -- unlike plain modula hashing.
 *
 * Connections that are mid-send (send_state != GEARMAN_CON_SEND_STATE_NONE)
 * are skipped, matching the availability check the default head-of-list
 * selection already performs.
 *
 * If after_con is non-NULL, the hash point for (key, key_length) is ignored
 * and the search instead starts just past after_con's own position on the
 * ring, returning the next distinct, available connection in ring order.
 * This is the fail-over path: used when after_con could not be used to
 * submit the job.
 *
 * Returns NULL only if there is no eligible connection (empty connection
 * list, or none available for a fail-over search).
 */
gearman_connection_st* client_select_connection_by_key(gearman_universal_st& universal,
                                                        const char* key,
                                                        size_t key_length,
                                                        gearman_connection_st* after_con);
