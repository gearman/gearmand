/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Data Differential YATL (i.e. libtest)  library
 *
 *  Copyright (C) 2012-2013 Data Differential, http://datadifferential.com/
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

#include "libtest/yatlcon.h"

#include "libtest/common.h"

using namespace libtest;

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <memory>
#ifdef HAVE_POLL_H
# include <poll.h>
#endif
#ifdef HAVE_SPAWN_H
# include <spawn.h>
#endif
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <stdexcept>

#if defined(__APPLE__) && __APPLE__
# include <crt_externs.h>
# define environ (*_NSGetEnviron ())
#elif !defined(_GNU_SOURCE)
extern char **environ= NULL;
#endif

#ifndef FD_CLOEXEC
# define FD_CLOEXEC 0
#endif

namespace {

  std::string print_argv(libtest::vchar_ptr_t& built_argv)
  {
    std::stringstream arg_buffer;

    for (vchar_ptr_t::iterator iter= built_argv.begin();
         iter != built_argv.end();
         ++iter)
    {
      if (*iter)
      {
        arg_buffer << *iter << " ";
      }
    }

    return arg_buffer.str();
  }

#if 0
  std::string print_argv(char** argv)
  {
    std::stringstream arg_buffer;

    for (char** ptr= argv; *ptr; ++ptr)
    {
      arg_buffer << *ptr << " ";
    }

    return arg_buffer.str();
  }
#endif

  static Application::error_t int_to_error_t(int arg)
  {
    switch (arg)
    {
    case 127:
      return Application::INVALID_POSIX_SPAWN;

    case 0:
      return Application::SUCCESS;

    case 1:
      return Application::FAILURE;

    default:
      return Application::UNKNOWN;
    }
  }
}

namespace libtest {

Application::Application(const std::string& arg, const bool _use_libtool_arg) :
  _use_libtool(_use_libtool_arg),
  _use_valgrind(false),
  _use_gdb(false),
  _use_ptrcheck(false),
  _will_fail(false),
  _argc(0),
  _exectuble(arg),
  stdin_fd(STDIN_FILENO),
  stdout_fd(STDOUT_FILENO),
  stderr_fd(STDERR_FILENO),
  _pid(-1),
  _status(0),
  _app_exit_state(UNINITIALIZED)
  { 
    if (_use_libtool)
    {
      if (libtool() == NULL)
      {
        FATAL("libtool requested, but know libtool was found");
      }
    }

    // Find just the name of the application with no path
    {
      size_t found= arg.find_last_of("/\\");
      if (found)
      {
        _exectuble_name= arg.substr(found +1);
      }
      else
      {
        _exectuble_name= arg;
      }
    }

    if (_use_libtool and getenv("PWD"))
    {
      _exectuble_with_path+= getenv("PWD");
      _exectuble_with_path+= "/";
    }
    _exectuble_with_path+= _exectuble;
  }

Application::~Application()
{
  murder();
  delete_argv();
}

Application::error_t Application::run(const char *args[])
{
  stdin_fd.reset();
  stdout_fd.reset();
  stderr_fd.reset();
  _stdout_buffer.clear();
  _stderr_buffer.clear();

  posix_spawn_file_actions_t file_actions;
  posix_spawn_file_actions_init(&file_actions);

  stdin_fd.dup_for_spawn(file_actions);
  stdout_fd.dup_for_spawn(file_actions);
  stderr_fd.dup_for_spawn(file_actions);

  posix_spawnattr_t spawnattr;
  posix_spawnattr_init(&spawnattr);

  short flags= 0;

  // Child should not block signals
  flags |= POSIX_SPAWN_SETSIGMASK;

  sigset_t mask;
  sigemptyset(&mask);

  fatal_assert(posix_spawnattr_setsigmask(&spawnattr, &mask) == 0);

#if defined(POSIX_SPAWN_USEVFORK) || defined(__GLIBC__)
  // Use USEVFORK where appropriate
  flags |= POSIX_SPAWN_USEVFORK;
#endif

  flags |= POSIX_SPAWN_SETPGROUP;
  fatal_assert(posix_spawnattr_setpgroup(&spawnattr, 0) == 0);

  fatal_assert(posix_spawnattr_setflags(&spawnattr, flags) == 0);
  
  create_argv(args);

  int spawn_ret;
  if (_use_gdb)
  {
    std::string gdb_run_file= create_tmpfile(_exectuble_name);
    std::fstream file_stream;
    file_stream.open(gdb_run_file.c_str(), std::fstream::out | std::fstream::trunc);

    _gdb_filename= create_tmpfile(_exectuble_name);
    file_stream 
      << "set logging redirect on" << std::endl
      << "set logging file " << _gdb_filename << std::endl
      << "set logging overwrite on" << std::endl
      << "set logging on" << std::endl
      << "set environment LIBTEST_IN_GDB=1" << std::endl
      << "run " << arguments() << std::endl
      << "thread apply all bt" << std::endl
      << "quit" << std::endl;

    fatal_assert(file_stream.good());
    file_stream.close();

    if (_use_libtool)
    {
      // libtool --mode=execute gdb -f -x binary
      char *argv[]= {
        const_cast<char *>(libtool()),
        const_cast<char *>("--mode=execute"),
        const_cast<char *>("gdb"),
        const_cast<char *>("-batch"),
        const_cast<char *>("-f"),
        const_cast<char *>("-x"),
        const_cast<char *>(gdb_run_file.c_str()), 
        const_cast<char *>(_exectuble_with_path.c_str()), 
        0};

      spawn_ret= posix_spawnp(&_pid, libtool(), &file_actions, &spawnattr, argv, environ);
    }
    else
    {
      // gdb binary
      char *argv[]= {
        const_cast<char *>("gdb"),
        const_cast<char *>("-batch"),
        const_cast<char *>("-f"),
        const_cast<char *>("-x"),
        const_cast<char *>(gdb_run_file.c_str()), 
        const_cast<char *>(_exectuble_with_path.c_str()), 
        0};
      spawn_ret= posix_spawnp(&_pid, "gdb", &file_actions, &spawnattr, argv, environ);
    }
  }
  else
  {
    spawn_ret= posix_spawn(&_pid, built_argv[0], &file_actions, &spawnattr, &built_argv[0], NULL);
  }

  posix_spawn_file_actions_destroy(&file_actions);
  posix_spawnattr_destroy(&spawnattr);

  stdin_fd.close(Application::Pipe::READ);
  stdout_fd.close(Application::Pipe::WRITE);
  stderr_fd.close(Application::Pipe::WRITE);

  if (spawn_ret != 0)
  {
    if (_will_fail == false)
    {
      Error << strerror(spawn_ret) << "(" << spawn_ret << ")";
    }
    _pid= -1;
    return Application::INVALID_POSIX_SPAWN;
  }

  assert(_pid != -1);
  if (_pid == -1)
  {
    return Application::INVALID_POSIX_SPAWN;
  }

#if 0
  app_thread_st* _app_thread= new app_thread_st(_pid, _status, built_argv[0], _app_exit_state);
  int error;
  if ((error= pthread_create(&_thread, NULL, &app_thread, _app_thread)) != 0)
  {
    Error << "pthread_create() died during pthread_create(" << strerror(error) << ")";
    return Application::FAILURE;
  }
#endif

  return Application::SUCCESS;
}

bool Application::check() const
{
  if (_pid > 1 and kill(_pid, 0) == 0)
  {
    return true;
  }

  return false;
}

void Application::murder()
{
  if (check())
  {
    int count= 5;
    while ((count--) > 0 and check())
    {
      if (kill(_pid, SIGTERM) == 0)
      {
        join();
      }
      else
      {
        Error << "kill(pid, SIGTERM) failed after kill with error of " << strerror(errno);
        continue;
      }

      break;
    }

    // If for whatever reason it lives, kill it hard
    if (check())
    {
      Error << "using SIGKILL, things will likely go poorly from this point";
      (void)kill(_pid, SIGKILL);
    }
  }
  slurp();
}

// false means that no data was returned
bool Application::slurp()
{
  struct pollfd fds[2];
  fds[0].fd= stdout_fd.fd();
  fds[0].events= POLLRDNORM;
  fds[0].revents= 0;
  fds[1].fd= stderr_fd.fd();
  fds[1].events= POLLRDNORM;
  fds[1].revents= 0;

  int active_fd;
  if ((active_fd= poll(fds, 2, 0)) == -1)
  {
    int error;
    switch ((error= errno))
    {
#ifdef __linux
    case ERESTART:
#endif
    case EINTR:
      break;

    case EFAULT:
    case ENOMEM:
      FATAL(strerror(error));
      break;

    case EINVAL:
      FATAL("RLIMIT_NOFILE exceeded, or if OSX the timeout value was invalid");
      break;

    default:
      FATAL(strerror(error));
      break;
    }

    return false;
  }

  if (active_fd == 0)
  {
    return false;
  }

  bool data_was_read= false;
  if (fds[0].revents)
  {
    if (fds[0].revents & POLLRDNORM)
    {
      if (stdout_fd.read(_stdout_buffer) == true)
      {
        data_was_read= true;
      }
    }

    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
    {
      stdout_fd.close();

      if (fds[0].revents & POLLERR)
      {
        Error << "getsockopt(stdout)";
      }
    }
  }

  if (fds[1].revents)
  {
    if (fds[1].revents & POLLRDNORM)
    {
      if (stderr_fd.read(_stderr_buffer) == true)
      {
        data_was_read= true;
      }
    }

    if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL))
    {
      stderr_fd.close();

      if (fds[1].revents & POLLERR)
      {
        Error << "getsockopt(stderr)";
      }
    }
  }

  return data_was_read;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
Application::error_t Application::join()
{
  if (waitpid(_pid, &_status, 0) == -1)
  {
    std::string error_string;
    if (stdout_result_length())
    {
      error_string+= " stdout: ";
      error_string+= stdout_c_str();
    }

    if (stderr_result_length())
    {
      error_string+= " stderr: ";
      error_string+= stderr_c_str();
    }
    Error << "waitpid() returned errno:" << strerror(errno) << " " << error_string;
    return Application::UNKNOWN;
  }

  slurp();
  if (WIFEXITED(_status) == false)
  {
    /*
      What we are looking for here is how the exit status happened.
      - 127 means that posix_spawn() itself had an error.
      - If WEXITSTATUS is positive we need to see if it is a signal that we sent to kill the process. If not something bad happened in the process itself. 
      - Finally something has happened that we don't currently understand.
    */
    if (WEXITSTATUS(_status) == 127)
    {
      _app_exit_state= Application::INVALID_POSIX_SPAWN;
      std::string error_string("posix_spawn() failed pid:");
      error_string+= _pid;
      error_string+= " name:";
      error_string+= print_argv(built_argv);
      if (stderr_result_length())
      {
        error_string+= " stderr: ";
        error_string+= stderr_c_str();
      }
      throw std::logic_error(error_string);
    }
    else if (WIFSIGNALED(_status))
    {
      if (WTERMSIG(_status) != SIGTERM and WTERMSIG(_status) != SIGHUP)
      {
        slurp();
        _app_exit_state= Application::INVALID_POSIX_SPAWN;
        std::string error_string(print_argv(built_argv));
        error_string+= " was killed by signal ";
        error_string+= strsignal(WTERMSIG(_status));

        if (stdout_result_length())
        {
          error_string+= " stdout: ";
          error_string+= stdout_c_str();
        }

        if (stderr_result_length())
        {
          error_string+= " stderr: ";
          error_string+= stderr_c_str();
        }

        throw std::runtime_error(error_string);
      }

      // If we terminted it on purpose then it counts as a success.
#if defined(DEBUG)
      if (DEBUG)
      {
        Out << "waitpid() application terminated at request"
          << " pid:" << _pid 
          << " name:" << built_argv[0];
      }
#endif
    }
    else
    {
      _app_exit_state= Application::UNKNOWN;
      Error << "Unknown logic state at exit:" << WEXITSTATUS(_status) 
        << " pid:" << _pid
        << " name:" << built_argv[0];
    }
  }
  else if (WIFEXITED(_status))
  {
    return int_to_error_t(WEXITSTATUS(_status));
  }
  else
  {
    _app_exit_state= Application::UNKNOWN;
    throw std::logic_error("waitpid() returned an unknown value");
  }

  return _app_exit_state;
}
#pragma GCC diagnostic pop

void Application::add_long_option(const std::string& name, const std::string& option_value)
{
  std::string arg(name);
  arg+= option_value;
  _options.push_back(std::make_pair(arg, std::string()));
}

void Application::add_option(const std::string& arg)
{
  _options.push_back(std::make_pair(arg, std::string()));
}

void Application::add_option(const std::string& name, const std::string& value)
{
  _options.push_back(std::make_pair(name, value));
}

Application::Pipe::Pipe(int arg) :
  _std_fd(arg)
{
  _pipe_fd[READ]= -1;
  _pipe_fd[WRITE]= -1;
  _open[READ]= false;
  _open[WRITE]= false;
}

int Application::Pipe::Pipe::fd()
{
  if (_std_fd == STDOUT_FILENO)
  {
    return _pipe_fd[READ];
  }
  else if (_std_fd == STDERR_FILENO)
  {
    return _pipe_fd[READ];
  }

  return _pipe_fd[WRITE]; // STDIN_FILENO
}

void Application::Pipe::Pipe::close()
{
  if (_std_fd == STDOUT_FILENO)
  {
    ::close(_pipe_fd[READ]);
    _pipe_fd[READ]= -1;
  }
  else if (_std_fd == STDERR_FILENO)
  {
    ::close(_pipe_fd[READ]);
    _pipe_fd[READ]= -1;
  }
  else
  {
    ::close(_pipe_fd[WRITE]);
    _pipe_fd[WRITE]= -1;
  }
}


bool Application::Pipe::read(libtest::vchar_t& arg)
{
  fatal_assert(_std_fd == STDOUT_FILENO or _std_fd == STDERR_FILENO);

  bool data_was_read= false;

  libtest::vchar_t buffer;
  buffer.resize(1024);
  ssize_t read_length;
  while ((read_length= ::read(_pipe_fd[READ], &buffer[0], buffer.size())))
  {
    if (read_length == -1)
    {
      switch(errno)
      {
      case EAGAIN:
        break;

      default:
        Error << strerror(errno);
        break;
      }

      break;
    }

    data_was_read= true;
    arg.reserve(read_length +1);
    for (size_t x= 0; x < size_t(read_length); ++x)
    {
      arg.push_back(buffer[x]);
    }
    // @todo Suck up all errput code here
  }

  return data_was_read;
}

void Application::Pipe::nonblock()
{
  int flags;
  do 
  {
    flags= fcntl(_pipe_fd[READ], F_GETFL, 0);
  } while (flags == -1 and (errno == EINTR or errno == EAGAIN));

  if (flags == -1)
  {
    Error << "fcntl(F_GETFL) " << strerror(errno);
    throw std::runtime_error(strerror(errno));
  }

  int rval;
  do
  {
    rval= fcntl(_pipe_fd[READ], F_SETFL, flags | O_NONBLOCK);
  } while (rval == -1 and (errno == EINTR or errno == EAGAIN));

  if (rval == -1)
  {
    Error << "fcntl(F_SETFL) " << strerror(errno);
    throw std::runtime_error(strerror(errno));
  }
}

void Application::Pipe::reset()
{
  close(READ);
  close(WRITE);

#ifdef HAVE_PIPE2
  if (pipe2(_pipe_fd, O_NONBLOCK|O_CLOEXEC) == -1)
#endif
  {
    if (pipe(_pipe_fd) == -1)
    {
      FATAL(strerror(errno));
    }

    // Since either pipe2() was not found/called we set the pipe directly
    nonblock();
    cloexec();
  }
  _open[0]= true;
  _open[1]= true;
}

void Application::Pipe::cloexec()
{
  //if (SOCK_CLOEXEC == 0)
  {
    if (FD_CLOEXEC) 
    {
      int flags;
      do 
      {
        flags= fcntl(_pipe_fd[WRITE], F_GETFD, 0);
      } while (flags == -1 and (errno == EINTR or errno == EAGAIN));

      if (flags == -1)
      {
        Error << "fcntl(F_GETFD) " << strerror(errno);
        throw std::runtime_error(strerror(errno));
      }

      int rval;
      do
      { 
        rval= fcntl(_pipe_fd[WRITE], F_SETFD, flags | FD_CLOEXEC);
      } while (rval == -1 && (errno == EINTR or errno == EAGAIN));

      if (rval == -1)
      {
        Error << "fcntl(F_SETFD) " << strerror(errno);
        throw std::runtime_error(strerror(errno));
      }
    }
  }
}

Application::Pipe::~Pipe()
{
  if (_pipe_fd[0] != -1)
  {
    ::close(_pipe_fd[0]);
  }

  if (_pipe_fd[1] != -1)
  {
    ::close(_pipe_fd[1]);
  }
}

void Application::Pipe::dup_for_spawn(posix_spawn_file_actions_t& file_actions)
{
  int type= STDIN_FILENO == _std_fd ? 0 : 1;

  int ret;
  if ((ret= posix_spawn_file_actions_adddup2(&file_actions, _pipe_fd[type], _std_fd )) < 0)
  {
    FATAL("posix_spawn_file_actions_adddup2(%s)", strerror(ret));
  }

  if ((ret= posix_spawn_file_actions_addclose(&file_actions, _pipe_fd[type])) < 0)
  {
    FATAL("posix_spawn_file_actions_addclose(%s)", strerror(ret));
  }
}

void Application::Pipe::close(const close_t& arg)
{
  int type= int(arg);

  if (_open[type])
  {
    if (::close(_pipe_fd[type]) == -1)
    {
      Error << "close(" << strerror(errno) << ")";
    }
    _open[type]= false;
    _pipe_fd[type]= -1;
  }
}

void Application::create_argv(const char *args[])
{
  delete_argv();
  if (_use_libtool)
  {
    assert(libtool());
    vchar::append(built_argv, libtool());
    vchar::append(built_argv, "--mode=execute");
  }

  if (_use_valgrind)
  {
    /*
      valgrind --error-exitcode=1 --leak-check=yes --track-fds=yes --malloc-fill=A5 --free-fill=DE
    */
    vchar::append(built_argv, "valgrind");
    vchar::append(built_argv, "--error-exitcode=1");
    vchar::append(built_argv, "--leak-check=yes");
#if 0
    vchar::append(built_argv, "--show-reachable=yes"));
    vchar::append(built_argv, "--track-fds=yes");
#endif
#if 0
    built_argv[x++]= strdup("--track-origin=yes");
#endif
    vchar::append(built_argv, "--malloc-fill=A5");
    vchar::append(built_argv, "--free-fill=DE");
    vchar::append(built_argv, "--xml=yes");
    if (getenv("VALGRIND_HOME"))
    {
      libtest::vchar_t buffer;
      buffer.resize(1024);
      int length= snprintf(&buffer[0], buffer.size(), "--xml-file=%s/cmd-%%p.xml", getenv("VALGRIND_HOME"));
      fatal_assert(length > 0 and size_t(length) < buffer.size());
      vchar::append(built_argv, &buffer[0]);
    }
    else
    {
      vchar::append(built_argv, "--xml-file=var/tmp/valgrind-cmd-%p.xml");
    }

    std::string log_file= create_tmpfile("valgrind");
    libtest::vchar_t buffer;
    buffer.resize(1024);
    int length= snprintf(&buffer[0], buffer.size(), "--log-file=%s", log_file.c_str());
    fatal_assert(length > 0 and size_t(length) < buffer.size());
    vchar::append(built_argv, &buffer[0]);
  }
  else if (_use_ptrcheck)
  {
    /*
      valgrind --error-exitcode=1 --tool=exp-ptrcheck --log-file= 
    */
    vchar::append(built_argv, "valgrind");
    vchar::append(built_argv, "--error-exitcode=1");
    vchar::append(built_argv, "--tool=exp-ptrcheck");
    std::string log_file= create_tmpfile("ptrcheck");
    libtest::vchar_t buffer;
    buffer.resize(1024);
    int length= snprintf(&buffer[0], buffer.size(), "--log-file=%s", log_file.c_str());
    fatal_assert(length > 0 and size_t(length) < buffer.size());
    vchar::append(built_argv, &buffer[0]);
  }
  else if (_use_gdb)
  {
    vchar::append(built_argv, "gdb");
  }

  vchar::append(built_argv, _exectuble_with_path.c_str());

  for (Options::const_iterator iter= _options.begin(); iter != _options.end(); ++iter)
  {
    vchar::append(built_argv, (*iter).first.c_str());
    if ((*iter).second.empty() == false)
    {
      vchar::append(built_argv, (*iter).second.c_str());
    }
  }

  if (args)
  {
    for (const char **ptr= args; *ptr; ++ptr)
    {
      vchar::append(built_argv, *ptr);
    }
  }
  built_argv.push_back(NULL);
}

std::string Application::print()
{
  return print_argv(built_argv);
}

std::string Application::arguments()
{
  std::stringstream arg_buffer;

  // Skip printing out the libtool reference
  for (size_t x= _use_libtool ? 2 : 0; x < _argc; ++x)
  {
    if (built_argv[x])
    {
      arg_buffer << built_argv[x] << " ";
    }
  }

  return arg_buffer.str();
}

void Application::delete_argv()
{
  std::for_each(built_argv.begin(), built_argv.end(), FreeFromVector());

  built_argv.clear();
  _argc= 0;
}


int exec_cmdline(const std::string& command, const char *args[], bool use_libtool)
{
  Application app(command, use_libtool);

  Application::error_t ret= app.run(args);

  if (ret != Application::SUCCESS)
  {
    return int(ret);
  }

  return int(app.join());
}

} // namespace exec_cmdline
