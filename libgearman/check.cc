/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2013 Data Differential, http://datadifferential.com/ 
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

#include "libgearman/common.h"

#include "libgearman/error_code.h"

#include <algorithm>

EchoCheck::EchoCheck(gearman_universal_st& universal_,
    const void *workload_, const size_t workload_size_) :
    _universal(universal_),
    _workload(workload_),
    _workload_size(workload_size_)
{
}

gearman_return_t EchoCheck::success(gearman_connection_st* con)
{
  if (con->_packet.command != GEARMAN_COMMAND_ECHO_RES)
  {
    return gearman_error(_universal, GEARMAN_INVALID_COMMAND, "Wrong command sent in response to ECHO request");
  }

  size_t compared= std::min(con->_packet.size(), _workload_size);

  if (compared != _workload_size or compared != con->_packet.size())
  {
    // If the workload_size is smaller
    if (memcmp(_workload, con->_packet.value(), compared) == 0)
    {
      return gearman_universal_set_error(_universal, GEARMAN_ECHO_DATA_CORRUPTION, GEARMAN_AT,
                                         "Truncation occurred, Expected %u, received %u",
                                         uint32_t(_workload_size), uint32_t(con->_packet.data_size));
    }

    return gearman_universal_set_error(_universal, GEARMAN_ECHO_DATA_CORRUPTION, GEARMAN_AT,
                                       "Expected data was not received, expected %u, received %u",
                                       uint32_t(_workload_size), uint32_t(con->_packet.data_size));
  }
  assert(compared == _workload_size);

  if (memcmp(_workload, con->_packet.value(), compared))
  {
    return gearman_universal_set_error(_universal, GEARMAN_ECHO_DATA_CORRUPTION, GEARMAN_AT,
                                       "Data sent was not what was received %u == %u == %u",
                                       uint32_t(_workload_size), uint32_t(con->_packet.data_size), uint32_t(compared));
  }

  return GEARMAN_SUCCESS;
}

gearman_return_t CancelCheck::success(gearman_connection_st* con)
{
  if (con->_packet.command == GEARMAN_COMMAND_ERROR)
  {
    if (con->_packet.argc)
    {
      gearman_return_t maybe_server_error= string2return_code(static_cast<char *>(con->_packet.arg[0]), int(con->_packet.arg_size[0]));
      if (maybe_server_error == GEARMAN_MAX_RETURN)
      {
        maybe_server_error= GEARMAN_SERVER_ERROR;
      }

      return gearman_universal_set_error(_universal, maybe_server_error, GEARMAN_AT, "%s:%s %.*s:%.*s",
                                         con->host(), con->service(),
                                         con->_packet.arg_size[0], con->_packet.arg[0],
                                         con->_packet.arg_size[1], con->_packet.arg[1]
                                        );
    }

    return gearman_universal_set_error(_universal, GEARMAN_SERVER_ERROR, GEARMAN_AT, "%s:%s lacks support for client's to cancel a job",
                                       con->host(), con->service()
                                      );
  }

  return GEARMAN_SUCCESS;
}

gearman_return_t OptionCheck::success(gearman_connection_st* con)
{
  if (con->_packet.command == GEARMAN_COMMAND_ERROR)
  {
    if (con->_packet.argc)
    {
      gearman_return_t maybe_server_error= string2return_code(static_cast<char *>(con->_packet.arg[0]), int(con->_packet.arg_size[0]));
      if (maybe_server_error == GEARMAN_MAX_RETURN)
      {
        maybe_server_error= GEARMAN_INVALID_SERVER_OPTION;
      }

      return gearman_universal_set_error(_universal, maybe_server_error, GEARMAN_AT, "%s:%s Invalid option %.*s",
                                         con->host(), con->service(),
                                         con->_packet.arg_size[0], con->_packet.arg[0]
                                        );
    }

    return gearman_universal_set_error(_universal, GEARMAN_INVALID_SERVER_OPTION, GEARMAN_AT, "%s:%s Invalid option %.*s",
                                       con->host(), con->service(),
                                       con->_packet.arg_size[0], con->_packet.arg[0]
                                      );
  }

  // @todo mark option set for connection

  return GEARMAN_SUCCESS;
}
