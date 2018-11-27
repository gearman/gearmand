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
#include "configmake.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <pwd.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>
#include <grp.h>

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "libgearman-server/gearmand.h"
#include "libgearman-server/plugins.h"
#include "libgearman-server/queue.hpp"

#define GEARMAND_LOG_REOPEN_TIME 60

#include "util/daemon.hpp"
#include "util/pidfile.hpp"

#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/token_functions.hpp>
#include <boost/tokenizer.hpp>

#include <iostream>

#include "libtest/cpu.hpp"

#include "gearmand/error.hpp"
#include "gearmand/log.hpp"

#include "libgearman/backtrace.hpp"

using namespace datadifferential;
using namespace gearmand;

static bool _set_fdlimit(rlim_t fds);
static bool _switch_user(const char *user);

extern "C" {
static bool _set_signals(bool core_dump= false);
}

static void _log(const char *line, gearmand_verbose_t verbose, void *context);

int main(int argc, char *argv[])
{
  gearmand::error::init(argv[0]);

  int backlog;
  rlim_t fds= 0;
  uint32_t job_retries;
  uint32_t worker_wakeup;

  std::string host;
  std::string user;
  std::string log_file;
  std::string pid_file;
  std::string protocol;
  std::string queue_type;
  std::string job_handle_prefix;
  std::string verbose_string;
  std::string config_file;

  uint32_t threads;
  bool opt_exceptions;
  bool opt_round_robin;
  bool opt_daemon;
  bool opt_check_args;
  bool opt_syslog;
  bool opt_coredump;
  uint32_t hashtable_buckets;
  bool opt_keepalive;
  int opt_keepalive_idle;
  int opt_keepalive_interval;
  int opt_keepalive_count;


  boost::program_options::options_description general("General options");

  general.add_options()
  ("backlog,b", boost::program_options::value(&backlog)->default_value(32),
   "Number of backlog connections for listen.")

  ("daemon,d", boost::program_options::bool_switch(&opt_daemon)->default_value(false),
   "Daemon, detach and run in the background.")

  ("exceptions", boost::program_options::bool_switch(&opt_exceptions)->default_value(false),
   "Enable protocol exceptions by default.")

  ("file-descriptors,f", boost::program_options::value(&fds),
   "Number of file descriptors to allow for the process (total connections will be slightly less). Default is max allowed for user.")

  ("help,h", "Print this help menu.")

  ("job-retries,j", boost::program_options::value(&job_retries)->default_value(0),
   "Number of attempts to run the job before the job server removes it. This is helpful to ensure a bad job does not crash all available workers. Default is no limit.")

  ("job-handle-prefix", boost::program_options::value(&job_handle_prefix),
   "Prefix used to generate a job handle string. If not provided, the default \"H:<host_name>\" is used.")

  ("hashtable-buckets", boost::program_options::value(&hashtable_buckets)->default_value(GEARMAND_DEFAULT_HASH_SIZE),
   "Number of buckets in the internal job hash tables. The default of 991 works well for about three million jobs in queue. If the number of jobs in the queue at any time will exceed three million, use proportionally larger values (991 * # of jobs / 3M). For example, to accommodate 2^32 jobs, use 1733003. This will consume ~26MB of extra memory. Gearmand cannot support more than 2^32 jobs in queue at this time.")

  ("keepalive", boost::program_options::bool_switch(&opt_keepalive)->default_value(false),
   "Enable keepalive on sockets.")
  ("keepalive-idle", boost::program_options::value(&opt_keepalive_idle)->default_value(-1),
   "If keepalive is enabled, set the value for TCP_KEEPIDLE for systems that support it. A value of -1 means that either the system does not support it or an error occurred when trying to retrieve the default value.")

  ("keepalive-interval", boost::program_options::value(&opt_keepalive_interval)->default_value(-1),
   "If keepalive is enabled, set the value for TCP_KEEPINTVL for systems that support it. A value of -1 means that either the system does not support it or an error occurred when trying to retrieve the default value.")

  ("keepalive-count", boost::program_options::value(&opt_keepalive_count)->default_value(-1),
   "If keepalive is enabled, set the value for TCP_KEEPCNT for systems that support it. A value of -1 means that either the system does not support it or an error occurred when trying to retrieve the default value.")

  ("log-file,l", boost::program_options::value(&log_file)->default_value(LOCALSTATEDIR"/log/gearmand.log"),
   "Log file to write errors and information to. If the log-file parameter is specified as 'stderr', then output will go to stderr. If 'none', then no logfile will be generated.")

  ("listen,L", boost::program_options::value(&host),
   "Address the server should listen on. Default is INADDR_ANY.")

  ("pid-file,P", boost::program_options::value(&pid_file)->default_value(GEARMAND_PID),
   "File to write process ID out to.")

  ("protocol,r", boost::program_options::value(&protocol),
   "Load protocol module.")

  ("round-robin,R", boost::program_options::bool_switch(&opt_round_robin)->default_value(false),
   "Assign work in round-robin order per worker connection. The default is to assign work in the order of functions added by the worker.")

  ("queue-type,q", boost::program_options::value(&queue_type)->default_value("builtin"),
   "Persistent queue type to use.")

  ("config-file", boost::program_options::value(&config_file)->default_value(GEARMAND_CONFIG),
   "Can be specified with '@name', too")

  ("syslog", boost::program_options::bool_switch(&opt_syslog)->default_value(false),
   "Use syslog.")

  ("coredump", boost::program_options::bool_switch(&opt_coredump)->default_value(false),
   "Whether to create a core dump for uncaught signals.")

  ("threads,t", boost::program_options::value(&threads)->default_value(4),
   "Number of I/O threads to use, 0 means that gearmand will try to guess the maximum number it can use. Default=4.")

  ("user,u", boost::program_options::value(&user),
   "Switch to given user after startup.")

  ("verbose", boost::program_options::value(&verbose_string)->default_value("ERROR"),
   "Set verbose level (FATAL, ALERT, CRITICAL, ERROR, WARNING, NOTICE, INFO, DEBUG).")

  ("version,V", "Display the version of gearmand and exit.")
  ("worker-wakeup,w", boost::program_options::value(&worker_wakeup)->default_value(0),
   "Number of workers to wakeup for each job received. The default is to wakeup all available workers.")
  ;

  boost::program_options::options_description all("Allowed options");
  all.add(general);

  gearmand::protocol::HTTP http;
  all.add(http.command_line_options());

  gearmand::protocol::Gear gear;
  all.add(gear.command_line_options());

  gearmand::plugins::initialize(all);

  boost::program_options::positional_options_description positional;
  positional.add("provided", -1);

  // Now insert all options that we want to make visible to the user
  boost::program_options::options_description visible("Allowed options");
  visible.add(all);

  boost::program_options::options_description hidden("Hidden options");
  hidden.add_options()
  ("check-args", boost::program_options::bool_switch(&opt_check_args)->default_value(false),
   "Check command line and configuration file arguments and then exit.");
  all.add(hidden);

  boost::program_options::variables_map vm;
  try {
    // Disable allow_guessing
    int style= boost::program_options::command_line_style::default_style ^ boost::program_options::command_line_style::allow_guessing;
    boost::program_options::parsed_options parsed= boost::program_options::command_line_parser(argc, argv)
      .options(all)
      .positional(positional)
      .style(style)
      .run();
    store(parsed, vm);
    notify(vm);

    if (config_file.empty() == false)
    {
      // Load the file and tokenize it
      std::ifstream ifs(config_file.c_str());
      if (ifs)
      {
        // Read the whole file into a string
        std::stringstream ss;
        ss << ifs.rdbuf();
        // Split the file content
        boost::char_separator<char> sep(" \n\r");
        std::string sstr= ss.str();
        boost::tokenizer<boost::char_separator<char> > tok(sstr, sep);
        std::vector<std::string> args;
        std::copy(tok.begin(), tok.end(), back_inserter(args));

#if defined(DEBUG) && DEBUG
        for (std::vector<std::string>::iterator iter= args.begin();
             iter != args.end();
             ++iter)
        {
          std::cerr << *iter << std::endl;
        }
#endif

        // Parse the file and store the options
        store(boost::program_options::command_line_parser(args).options(visible).run(), vm);
      }
      else if (config_file.compare(GEARMAND_CONFIG))
      {
        error::message("Could not open configuration file.");
        return EXIT_FAILURE;
      }
    }

    notify(vm);
  }
  catch(boost::program_options::validation_error &e)
  {
    error::message(e.what());
    return EXIT_FAILURE;
  }
  catch(std::exception &e)
  {
    if (e.what() and strncmp("-v", e.what(), 2) == 0)
    {
      error::message("Option -v has been deprecated, please use --verbose");
    }
    else
    {
      error::message(e.what());
    }

    return EXIT_FAILURE;
  }

  gearmand_verbose_t verbose= GEARMAND_VERBOSE_ERROR;
  if (gearmand_verbose_check(verbose_string.c_str(), verbose) == false)
  {
    error::message("Invalid value for --verbose supplied");
    return EXIT_FAILURE;
  }

  if (hashtable_buckets <= 0)
  {
    error::message("hashtable-buckets has to be greater than 0");
    return EXIT_FAILURE;
  }

  if (opt_check_args)
  {
    return EXIT_SUCCESS;
  }

  if (vm.count("help"))
  {
    std::cout << visible << std::endl;
    return EXIT_SUCCESS;
  }

  if (vm.count("version"))
  {
    std::cout << std::endl << "gearmand " << gearmand_version() << " - " <<  gearmand_bugreport() << std::endl;
    return EXIT_SUCCESS;
  }

  if (fds > 0 and _set_fdlimit(fds))
  {
    return EXIT_FAILURE;
  }

  if (not user.empty() and _switch_user(user.c_str()))
  {
    return EXIT_FAILURE;
  }

  if (opt_daemon)
  {
    util::daemonize(false, true);
  }

  if (_set_signals(opt_coredump))
  {
    return EXIT_FAILURE;
  }

  util::Pidfile _pid_file(pid_file);

  if (_pid_file.create() == false and pid_file.compare(GEARMAND_PID))
  {
    error::perror(_pid_file.error_message().c_str());
    return EXIT_FAILURE;
  }

  gearmand::gearmand_log_info_st log_info(log_file, opt_syslog);

  if (log_info.initialized() == false)
  {
    return EXIT_FAILURE;
  }

  if (threads == 0)
  {
    uint32_t number_of_threads= libtest::number_of_cpus();

    if (number_of_threads > 4)
    {
      threads= number_of_threads;
    }
  }

  gearmand_config_st *gearmand_config= gearmand_config_create();

  if (gearmand_config == NULL)
  {
    return EXIT_FAILURE;
  }

  gearmand_config_sockopt_keepalive(gearmand_config, opt_keepalive);

  gearmand_config_sockopt_keepalive_count(gearmand_config, opt_keepalive_count);

  gearmand_config_sockopt_keepalive_idle(gearmand_config, opt_keepalive_idle);

  gearmand_config_sockopt_keepalive_interval(gearmand_config, opt_keepalive_interval);

  gearmand_st *_gearmand= gearmand_create(gearmand_config,
                                          host.empty() ? NULL : host.c_str(),
                                          threads, backlog,
                                          static_cast<uint8_t>(job_retries),
                                          job_handle_prefix.empty() ? NULL : job_handle_prefix.c_str(),
                                          static_cast<uint8_t>(worker_wakeup),
                                          _log, &log_info, verbose,
                                          opt_round_robin, opt_exceptions,
                                          hashtable_buckets);
  if (_gearmand == NULL)
  {
    error::message("Could not create gearmand library instance.");
    return EXIT_FAILURE;
  }

  gearmand_config_free(gearmand_config);

  assert(queue_type.size());
  if (queue_type.empty() == false)
  {
    gearmand_error_t rc;
    if ((rc= gearmand::queue::initialize(_gearmand, queue_type.c_str())) != GEARMAND_SUCCESS)
    {
      error::message("Error while initializing the queue", queue_type.c_str());
      gearmand_free(_gearmand);

      return EXIT_FAILURE;
    }
  }

  if (gear.start(_gearmand) != GEARMAND_SUCCESS)
  {
    error::message("Error while enabling Gear protocol module");
    gearmand_free(_gearmand);

    return EXIT_FAILURE;
  }

  if (protocol.compare("http") == 0)
  {
    if (http.start(_gearmand) != GEARMAND_SUCCESS)
    {
      error::message("Error while enabling protocol module", protocol.c_str());
      gearmand_free(_gearmand);

      return EXIT_FAILURE;
    }
  }
  else if (protocol.empty() == false)
  {
    error::message("Unknown protocol module", protocol.c_str());
    gearmand_free(_gearmand);

    return EXIT_FAILURE;
  }

  if (opt_daemon)
  {
    if (util::daemon_is_ready(true) == false)
    {
      return EXIT_FAILURE;
    }
  }

  gearmand_error_t ret= gearmand_run(_gearmand);

  gearmand_free(_gearmand);
  _gearmand= NULL;

  return (ret == GEARMAND_SUCCESS || ret == GEARMAND_SHUTDOWN) ? 0 : 1;
}

static bool _set_fdlimit(rlim_t fds)
{
  struct rlimit rl;

  if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
  {
    error::perror("Could not get file descriptor limit");
    return true;
  }

  rl.rlim_cur= fds;
  if (rl.rlim_max < rl.rlim_cur)
  {
    rl.rlim_max= rl.rlim_cur;
  }

  if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
  {
    error::perror("Failed to set limit for the number of file "
		  "descriptors.  Try running as root or giving a "
		  "smaller value to -f.");
    return true;
  }

  return false;
}

static bool _switch_user(const char *user)
{

  if (getuid() == 0 or geteuid() == 0)
  {
    struct passwd *pw= getpwnam(user);

    if (not pw)
    {
      error::message("Could not find user", user);
      return EXIT_FAILURE;
    }

    if (setgroups(0, NULL) == -1 ||
        setgid(pw->pw_gid) == -1 ||
        setuid(pw->pw_uid) == -1)
    {
      error::message("Could not switch to user", user);
      return EXIT_FAILURE;
    }
  }
  else
  {
    error::message("Must be root to switch users.");
    return true;
  }

  return false;
}

extern "C" void _shutdown_handler(int signal_, siginfo_t*, void*)
{
  if (signal_== SIGUSR1)
  {
    gearmand_wakeup(Gearmand(), GEARMAND_WAKEUP_SHUTDOWN_GRACEFUL);
  }
  else
  {
    gearmand_wakeup(Gearmand(), GEARMAND_WAKEUP_SHUTDOWN);
  }
}

extern "C" void _reset_log_handler(int, siginfo_t*, void*) // signal_arg
{
  gearmand_log_info_st *log_info= static_cast<gearmand_log_info_st *>(Gearmand()->log_context);
  
  log_info->write(GEARMAND_VERBOSE_NOTICE, "SIGHUP, reopening log file");

  log_info->reset();
}

static bool segfaulted= false;
extern "C" void _crash_handler(int signal_, siginfo_t*, void*)
{
  if (segfaulted)
  {
    error::message("\nFatal crash while backtrace from signal:", strsignal(signal_));
    _exit(EXIT_FAILURE); /* Quit without running destructors */
  }

  segfaulted= true;
  custom_backtrace();
  _exit(EXIT_FAILURE); /* Quit without running destructors */
}

extern "C" {
static bool _set_signals(bool core_dump)
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(struct sigaction));

  sa.sa_handler= SIG_IGN;
  if (sigemptyset(&sa.sa_mask) == -1 or
      sigaction(SIGPIPE, &sa, 0) == -1)
  {
    error::perror("Could not set SIGPIPE handler.");
    return true;
  }

  sa.sa_sigaction= _shutdown_handler;
  sa.sa_flags= SA_SIGINFO;
  if (sigaction(SIGTERM, &sa, 0) == -1)
  {
    error::perror("Could not set SIGTERM handler.");
    return true;
  }

  if (sigaction(SIGINT, &sa, 0) == -1)
  {
    error::perror("Could not set SIGINT handler.");
    return true;
  }

  if (sigaction(SIGUSR1, &sa, 0) == -1)
  {
    error::perror("Could not set SIGUSR1 handler.");
    return true;
  }

  sa.sa_sigaction= _reset_log_handler;
  if (sigaction(SIGHUP, &sa, 0) == -1)
  {
    error::perror("Could not set SIGHUP handler.");
    return true;
  }

  bool in_gdb_libtest= bool(getenv("LIBTEST_IN_GDB"));

  if ((in_gdb_libtest == false) and (core_dump == false))
  {
    sa.sa_sigaction= _crash_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1)
    {
      error::perror("Could not set SIGSEGV handler.");
      return true;
    }

    if (sigaction(SIGABRT, &sa, NULL) == -1)
    {
      error::perror("Could not set SIGABRT handler.");
      return true;
    }

#ifdef SIGBUS
    if (sigaction(SIGBUS, &sa, NULL) == -1)
    {
      error::perror("Could not set SIGBUS handler.");
      return true;
    }
#endif
    if (sigaction(SIGILL, &sa, NULL) == -1)
    {
      error::perror("Could not set SIGILL handler.");
      return true;
    }

    if (sigaction(SIGFPE, &sa, NULL) == -1)
    {
      error::perror("Could not set SIGFPE handler.");
      return true;
    }
  }

  return false;
}
}

static void _log(const char *mesg, gearmand_verbose_t verbose, void *context)
{
  gearmand_log_info_st *log_info= static_cast<gearmand_log_info_st *>(context);

  log_info->write(verbose, mesg);
}
