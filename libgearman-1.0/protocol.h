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

#pragma once

#define GEARMAN_DEFAULT_TCP_PORT 4730
#define GEARMAN_DEFAULT_TCP_PORT_STRING "4730"
#define GEARMAN_DEFAULT_TCP_SERVICE "gearman"

enum gearman_command_t
{
  GEARMAN_COMMAND_TEXT,
  GEARMAN_COMMAND_CAN_DO,              /* W->J: FUNC */
  GEARMAN_COMMAND_CANT_DO,             /* W->J: FUNC */
  GEARMAN_COMMAND_RESET_ABILITIES,     /* W->J: -- */
  GEARMAN_COMMAND_PRE_SLEEP,           /* W->J: -- */
  GEARMAN_COMMAND_UNUSED,
  GEARMAN_COMMAND_NOOP,                /* J->W: -- */
  GEARMAN_COMMAND_SUBMIT_JOB,          /* C->J: FUNC[0]UNIQ[0]ARGS */
  GEARMAN_COMMAND_JOB_CREATED,         /* J->C: HANDLE  */
  GEARMAN_COMMAND_GRAB_JOB,            /* W->J: --  */
  GEARMAN_COMMAND_NO_JOB,              /* J->W: -- */
  GEARMAN_COMMAND_JOB_ASSIGN,          /* J->W: HANDLE[0]FUNC[0]ARG  */
  GEARMAN_COMMAND_WORK_STATUS,         /* W->J/C: HANDLE[0]NUMERATOR[0]DENOMINATOR */
  GEARMAN_COMMAND_WORK_COMPLETE,       /* W->J/C: HANDLE[0]RES */
  GEARMAN_COMMAND_WORK_FAIL,           /* W->J/C: HANDLE */
  GEARMAN_COMMAND_GET_STATUS,          /* C->J: HANDLE */
  GEARMAN_COMMAND_ECHO_REQ,            /* ?->J: TEXT */
  GEARMAN_COMMAND_ECHO_RES,            /* J->?: TEXT */
  GEARMAN_COMMAND_SUBMIT_JOB_BG,       /* C->J: FUNC[0]UNIQ[0]ARGS  */
  GEARMAN_COMMAND_ERROR,               /* J->?: ERRCODE[0]ERR_TEXT */
  GEARMAN_COMMAND_STATUS_RES,          /* J->C: HANDLE[0]KNOWN[0]RUNNING[0]NUM[0]DENOM */
  GEARMAN_COMMAND_SUBMIT_JOB_HIGH,     /* C->J: FUNC[0]UNIQ[0]ARGS  */
  GEARMAN_COMMAND_SET_CLIENT_ID,       /* W->J: [RANDOM_STRING_NO_WHITESPACE] */
  GEARMAN_COMMAND_CAN_DO_TIMEOUT,      /* W->J: FUNC[0]TIMEOUT */
  GEARMAN_COMMAND_ALL_YOURS,
  GEARMAN_COMMAND_WORK_EXCEPTION,      /* W->J/C: HANDLE[0] */
  GEARMAN_COMMAND_OPTION_REQ,          /* ?->J: TEXT */
  GEARMAN_COMMAND_OPTION_RES,          /* J->?: TEXT */
  GEARMAN_COMMAND_WORK_DATA,
  GEARMAN_COMMAND_WORK_WARNING,
  GEARMAN_COMMAND_GRAB_JOB_UNIQ,       /* W->J: -- */
  GEARMAN_COMMAND_JOB_ASSIGN_UNIQ,     /* J->W: */
  GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG,
  GEARMAN_COMMAND_SUBMIT_JOB_LOW,
  GEARMAN_COMMAND_SUBMIT_JOB_LOW_BG,
  GEARMAN_COMMAND_SUBMIT_JOB_SCHED,
  GEARMAN_COMMAND_SUBMIT_JOB_EPOCH,
  GEARMAN_COMMAND_SUBMIT_REDUCE_JOB,          // C->J: FUNC[0]UNIQ[0]REDUCER[0]UNUSED[0]ARGS
  GEARMAN_COMMAND_SUBMIT_REDUCE_JOB_BACKGROUND,          // C->J: FUNC[0]UNIQ[0]REDUCER[0]UNUSED[0]ARGS
  GEARMAN_COMMAND_GRAB_JOB_ALL,          /* W->J -- */
  GEARMAN_COMMAND_JOB_ASSIGN_ALL,          /* J->W: HANDLE[0]FUNC[0]UNIQ[0]REDUCER[0]ARGS */
  GEARMAN_COMMAND_GET_STATUS_UNIQUE,          /* C->J: UNIQUE */
  GEARMAN_COMMAND_STATUS_RES_UNIQUE,          /* J->C: UNIQUE[0]KNOWN[0]RUNNING[0]NUM[0]DENOM[0]CLIENT_COUNT */
  GEARMAN_COMMAND_MASS_DO,              /* W->J: FUNC */
  GEARMAN_COMMAND_MAX /* Always add new commands before this. */
};

#ifndef __cplusplus

typedef enum gearman_command_t gearman_command_t;

#endif
