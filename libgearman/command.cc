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

#include "gear_config.h"
#include <libgearman/common.h>

#include <libgearman-1.0/visibility.h>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-register"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif
#include "libgearman/command.hpp"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#include "libgearman/strcommand.h"

#include "libgearman/assert.hpp"

/**
 * Command info. Update GEARMAN_MAX_COMMAND_ARGS to the largest number in the
 * args column.
 */
gearman_command_info_st gearmand_command_info_list[GEARMAN_COMMAND_MAX]=
{
  { "GEARMAN_TEXT", GEARMAN_COMMAND_TEXT, 5, false },
  { "GEARMAN_CAN_DO", GEARMAN_COMMAND_CAN_DO, 1, false },
  { "GEARMAN_CANT_DO", GEARMAN_COMMAND_CANT_DO, 1, false },
  { "GEARMAN_RESET_ABILITIES", GEARMAN_COMMAND_RESET_ABILITIES, 0, false },
  { "GEARMAN_PRE_SLEEP", GEARMAN_COMMAND_PRE_SLEEP, 0, false },
  { "GEARMAN_UNUSED", GEARMAN_COMMAND_UNUSED, 0, false },
  { "GEARMAN_NOOP", GEARMAN_COMMAND_NOOP, 0, false },
  { "GEARMAN_SUBMIT_JOB", GEARMAN_COMMAND_SUBMIT_JOB, 2, true  },
  { "GEARMAN_JOB_CREATED", GEARMAN_COMMAND_JOB_CREATED, 1, false },
  { "GEARMAN_GRAB_JOB", GEARMAN_COMMAND_GRAB_JOB, 0, false },
  { "GEARMAN_NO_JOB", GEARMAN_COMMAND_NO_JOB, 0, false },
  { "GEARMAN_JOB_ASSIGN", GEARMAN_COMMAND_JOB_ASSIGN, 2, true  },
  { "GEARMAN_WORK_STATUS", GEARMAN_COMMAND_WORK_STATUS, 3, false },
  { "GEARMAN_WORK_COMPLETE", GEARMAN_COMMAND_WORK_COMPLETE, 1, true  },
  { "GEARMAN_WORK_FAIL",GEARMAN_COMMAND_WORK_FAIL, 1, false },
  { "GEARMAN_GET_STATUS",GEARMAN_COMMAND_GET_STATUS, 1, false },
  { "GEARMAN_ECHO_REQ", GEARMAN_COMMAND_ECHO_REQ, 0, true  },
  { "GEARMAN_ECHO_RES", GEARMAN_COMMAND_ECHO_RES, 0, true  },
  { "GEARMAN_SUBMIT_JOB_BG", GEARMAN_COMMAND_SUBMIT_JOB_BG, 2, true  },
  { "GEARMAN_ERROR", GEARMAN_COMMAND_ERROR, 2, false },
  { "GEARMAN_STATUS_RES", GEARMAN_COMMAND_STATUS_RES, 5, false },
  { "GEARMAN_SUBMIT_JOB_HIGH", GEARMAN_COMMAND_SUBMIT_JOB_HIGH, 2, true  },
  { "GEARMAN_SET_CLIENT_ID", GEARMAN_COMMAND_SET_CLIENT_ID, 1, false },
  { "GEARMAN_CAN_DO_TIMEOUT", GEARMAN_COMMAND_CAN_DO_TIMEOUT, 2, false },
  { "GEARMAN_ALL_YOURS", GEARMAN_COMMAND_ALL_YOURS, 0, false },
  { "GEARMAN_WORK_EXCEPTION", GEARMAN_COMMAND_WORK_EXCEPTION, 1, true  },
  { "GEARMAN_OPTION_REQ", GEARMAN_COMMAND_OPTION_REQ, 1, false },
  { "GEARMAN_OPTION_RES", GEARMAN_COMMAND_OPTION_RES, 1, false },
  { "GEARMAN_WORK_DATA", GEARMAN_COMMAND_WORK_DATA, 1, true  },
  { "GEARMAN_WORK_WARNING", GEARMAN_COMMAND_WORK_WARNING, 1, true  },
  { "GEARMAN_GRAB_JOB_UNIQ", GEARMAN_COMMAND_GRAB_JOB_UNIQ, 0, false },
  { "GEARMAN_JOB_ASSIGN_UNIQ", GEARMAN_COMMAND_JOB_ASSIGN_UNIQ, 3, true  },
  { "GEARMAN_SUBMIT_JOB_HIGH_BG", GEARMAN_COMMAND_SUBMIT_JOB_HIGH_BG, 2, true  },
  { "GEARMAN_SUBMIT_JOB_LOW", GEARMAN_COMMAND_SUBMIT_JOB_LOW, 2, true  },
  { "GEARMAN_SUBMIT_JOB_LOW_BG", GEARMAN_COMMAND_SUBMIT_JOB_LOW_BG, 2, true  },
  { "GEARMAN_SUBMIT_JOB_SCHED", GEARMAN_COMMAND_SUBMIT_JOB_SCHED, 7, true  },
  { "GEARMAN_SUBMIT_JOB_EPOCH", GEARMAN_COMMAND_SUBMIT_JOB_EPOCH, 3, true  },
  { "GEARMAN_SUBMIT_REDUCE_JOB", GEARMAN_COMMAND_SUBMIT_REDUCE_JOB, 4, true },
  { "GEARMAN_SUBMIT_REDUCE_JOB_BACKGROUND", GEARMAN_COMMAND_SUBMIT_REDUCE_JOB_BACKGROUND, 4, true },
  { "GEARMAN_GRAB_JOB_ALL", GEARMAN_COMMAND_GRAB_JOB_ALL, 0, false  },
  { "GEARMAN_JOB_ASSIGN_ALL", GEARMAN_COMMAND_JOB_ASSIGN_ALL,   4, true  },
  { "GEARMAN_GET_STATUS_UNIQUE", GEARMAN_COMMAND_GET_STATUS_UNIQUE, 1, false },
  { "GEARMAN_STATUS_RES_UNIQUE", GEARMAN_COMMAND_STATUS_RES_UNIQUE, 6, false }
};

const char *gearman_strcommand(gearman_command_t command)
{
  if ((command >= GEARMAN_COMMAND_TEXT) and (command <= GEARMAN_COMMAND_STATUS_RES_UNIQUE))
  {
    const char* str=  gearmand_command_info_list[command].name;

    str+= sizeof("GEARMAN_") -1;
    return str;
  }

  assert(0); // We should never reach this
  return "__UNKNOWN";
}

const char *gearman_enum_strcommand(gearman_command_t command)
{
  if ((command >= GEARMAN_COMMAND_TEXT) and (command <= GEARMAN_COMMAND_STATUS_RES_UNIQUE))
  {
    return gearmand_command_info_list[command].name;
  }

  assert(0); // We should never reach this
  return "__UNKNOWN";
}

const gearman_command_info_st *gearman_command_info(gearman_command_t command)
{
  if (uint32_t(command) >= uint32_t(GEARMAN_COMMAND_MAX))
  {
    return NULL;
  }

  const struct gearman_command_info_st* command_info= &gearmand_command_info_list[command];
  assert(command_info->code == command);
  return command_info;
}

const struct gearman_command_info_st * gearman_command_lookup (const char *str, unsigned int len)
{
  const struct gearman_command_string_st* com_str= String2gearman_command_t::in_word_set(str, len);
  return gearman_command_info(com_str->code);
}
