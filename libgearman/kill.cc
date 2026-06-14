/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
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

#include <cerrno>
#include <cstring>
#include <poll.h>
#include <unistd.h>

gearman_id_t gearman_id_initialize(void)
{
  static gearman_id_t tmp= { -1, -1 };

  return tmp;
}

bool gearman_id_valid(const gearman_id_t handle)
{
  if (handle.write_fd <= 0 and handle.read_fd <= 0)
  {
    return false;
  }

  return true;
}

gearman_return_t gearman_kill(const gearman_id_t handle, const gearman_signal_t sig)
{
  if (handle.write_fd <= 0 or handle.read_fd <= 0)
  {
    return GEARMAN_COULD_NOT_CONNECT;
  }

  switch (sig)
  {
  case GEARMAN_SIGNAL_INTERRUPT:
    if (write(handle.write_fd, "1", 1) == 1)
    {
      return GEARMAN_SUCCESS;
    }
    break;

  case GEARMAN_SIGNAL_KILLWAIT:
    if (close(handle.write_fd) == 0)
    {
      gearman_kill(handle, GEARMAN_SIGNAL_CHECK);
      return GEARMAN_SUCCESS;
    }
    break;

  case GEARMAN_SIGNAL_KILL:
    // Write a signal byte rather than closing the write end of the pipe.
    // Closing it externally causes a double-close: the universal destructor
    // calls close_wakeup() which also closes wakeup_fd[1], and if the fd was
    // reused between the two closes the wrong descriptor gets closed (crash).
    if (write(handle.write_fd, "1", 1) == 1)
    {
      return GEARMAN_SUCCESS;
    }
    break;

  case GEARMAN_SIGNAL_CHECK:
    {
      struct pollfd pfds[1];
      memset(&pfds, 0, sizeof(pfds));
      pfds[0].fd= handle.read_fd;
      pfds[0].events= POLLIN;
      pfds[0].revents= 0;

      int ret= ::poll(pfds, 1, 1500);

      if (ret >= 0)
      {
        return GEARMAN_SUCCESS;
      }
    }
    break;
  }

  return GEARMAN_COULD_NOT_CONNECT;
}
