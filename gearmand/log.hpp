/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2012 Data Differential, http://datadifferential.com/
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

namespace gearmand {

struct gearmand_log_info_st
{
  std::string filename;
  int fd;
  bool opt_syslog;
  bool opt_file;
  bool init_success;

  gearmand_log_info_st(const std::string &filename_arg, const bool syslog_arg) :
    filename(filename_arg),
    fd(-1),
    opt_syslog(syslog_arg),
    opt_file(false),
    init_success(false)
  {
    if (opt_syslog)
    {
      openlog("gearmand", LOG_PID | LOG_NDELAY, LOG_USER);
    }

    init();
  }

  void init()
  {
    if (filename.size())
    {
      if (filename.compare("stderr") == 0)
      {
        fd= STDERR_FILENO;
        opt_file= true;
      }
      else if (filename.compare("none") == 0)
      { }
      else
      {
        fd= open(filename.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
        if (fd == -1)
        {
          char cwd_buffer[1024];
          char *cwd= getcwd(cwd_buffer, sizeof(cwd_buffer));

          char error_mesg[1024];
          int error_mesg_length= snprintf(error_mesg, sizeof(error_mesg),
                                          "Could not open log file \"%.*s\", from \"%s\", switching to stderr.",
                                          int(filename.size()), filename.c_str(), 
                                          cwd);
          if (opt_syslog)
          {
            syslog(LOG_ERR, "%.*s", error_mesg_length, error_mesg);
          }
          error::perror(error_mesg);

          fd= STDERR_FILENO;
        }

        opt_file= true;
      }
    }

    init_success= true;
  }

  bool initialized() const
  {
    return init_success;
  }

  void reset()
  {
    int new_fd= open(filename.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);

    if (new_fd != -1)
    {
      int old_fd= fd;
      fd= new_fd;
      close(old_fd);
    }
  }

  int file() const
  {
    return fd;
  }

  void write(gearmand_verbose_t verbose, const char *mesg)
  {
    if (opt_file)
    {
      char buffer[GEARMAN_MAX_ERROR_SIZE * 2 + 16];
      int buffer_length= snprintf(buffer, sizeof(buffer), "%7s %s\n", gearmand_verbose_name(verbose), mesg);
      if (::write(file(), buffer, buffer_length) == -1)
      {
        error::perror("Could not write to log file.");
        if (opt_syslog)
        {
          char getcwd_buffer[1024];
          char *ptr_buffer= getcwd(getcwd_buffer, sizeof(getcwd_buffer));
          syslog(LOG_ERR, "Could not open log file \"%.*s\", from \"%s\", open failed with (%s)", 
                 int(filename.size()), filename.c_str(), 
                 ptr_buffer,
                 strerror(errno));
        }
      }
    }

    if (opt_syslog)
    {
      syslog(int(verbose), "%7s %s", gearmand_verbose_name(verbose), mesg);
    }
  }

  ~gearmand_log_info_st()
  {
    if (fd != -1 and fd != STDERR_FILENO)
    {
      close(fd);
    }

    if (opt_syslog)
    {
      closelog();
    }
  }
};

} // namespace gearmand
