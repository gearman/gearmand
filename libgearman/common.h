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


#pragma once

struct Client;
struct Task;
struct Worker;
struct gearman_client_st;
struct gearman_connection_st;
struct gearman_result_st;
struct gearman_universal_st;

#include "libgearman-1.0/gearman.h"

#include "libgearman/server_options.hpp"
#include "libgearman/connection.hpp"
#include "libgearman/universal.hpp"
#include "libgearman/allocator.hpp"
#include "libgearman/packet.hpp"
#include "libgearman/run.hpp"
#include "libgearman/aggregator.hpp"

#include "libgearman/error.hpp"
#include "libgearman/error_code.h"

/* These are private not to be installed headers */
#include "libgearman/strcommand.h"
#include "libgearman/vector.hpp"
#include "libgearman/unique.hpp"
#include "libgearman/add.hpp"
#include "libgearman/is.hpp"
#include "libgearman/result.hpp"
#include "libgearman/actions.hpp"
#include "libgearman/string.hpp"
#include "libgearman/command.h"
#include "libgearman/task.hpp"
#include "libgearman/job.h"
#include "libgearman/job.hpp"
#include "libgearman/client.h"
#include "libgearman/server_options.hpp"
#include "libgearman/check.h"

#include "libgearman/status.hpp"

#include "libgearman/task_attr.h"

#include "libgearman/protocol/submit.h"
#include "libgearman/interface/client.hpp"
#include "libgearman/interface/task.hpp"
#include "libgearman/interface/worker.hpp"

gearman_function_t gearman_function_create_null(void);
