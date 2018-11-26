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
#include <libtest/common.h>

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fnmatch.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <signal.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

using namespace libtest;

#include <getopt.h>
#include <unistd.h>

#if __GNUC__ >= 7
  #pragma GCC diagnostic warning "-Wimplicit-fallthrough"
#endif
int main(int argc, char *argv[], char* environ_[])
{
  bool opt_massive= false;
  bool opt_ssl= false;
  unsigned long int opt_repeat= 1; // Run all tests once
  bool opt_verbose= false;
  bool opt_quiet= false;
  bool opt_list_collection= false;
  bool opt_list_tests= false;
  std::string collection_to_run;
  std::string wildcard;
  std::string binary_name;

  const char *just_filename= rindex(argv[0], '/');
  if (just_filename)
  {
    just_filename++;
  }
  else
  {
    just_filename= argv[0];
  }

  if (just_filename[0] == 'l' and just_filename[1] == 't' and  just_filename[2] == '-')
  {
    just_filename+= 3;
  }
  binary_name.append(just_filename);

  /*
    Valgrind does not currently work reliably, or sometimes at all, on OSX
    - Fri Jun 15 11:24:07 EDT 2012
  */
#if defined(__APPLE__) && __APPLE__
  if (valgrind_is_caller())
  {
    return EXIT_SKIP;
  }
#endif

  // Options parsing
  {
    enum long_option_t {
      OPT_LIBYATL_HELP,
      OPT_LIBYATL_VERBOSE,
      OPT_LIBYATL_VERSION,
      OPT_LIBYATL_MATCH_COLLECTION,
      OPT_LIBYATL_LIST_COLLECTIONS,
      OPT_LIBYATL_LIST_TESTS,
      OPT_LIBYATL_MASSIVE,
      OPT_LIBYATL_QUIET,
      OPT_LIBYATL_MATCH_WILDCARD,
      OPT_LIBYATL_REPEAT,
      OPT_LIBYATL_SSL,
      OPT_LIBYATL_MAX
    };

    static struct option long_options[]=
    {
      { "help", no_argument, NULL, OPT_LIBYATL_HELP },
      { "verbose", no_argument, NULL, OPT_LIBYATL_VERBOSE },
      { "version", no_argument, NULL, OPT_LIBYATL_VERSION },
      { "quiet", no_argument, NULL, OPT_LIBYATL_QUIET },
      { "repeat", required_argument, NULL, OPT_LIBYATL_REPEAT },
      { "collection", required_argument, NULL, OPT_LIBYATL_MATCH_COLLECTION },
      { "list-collections", no_argument, NULL, OPT_LIBYATL_LIST_COLLECTIONS },
      { "list-tests", no_argument, NULL, OPT_LIBYATL_LIST_TESTS },
      { "wildcard", required_argument, NULL, OPT_LIBYATL_MATCH_WILDCARD },
      { "massive", no_argument, NULL, OPT_LIBYATL_MASSIVE },
      { "ssl", no_argument, NULL, OPT_LIBYATL_SSL },
      { 0, 0, 0, 0 }
    };

    int option_index= 0;
    while (1)
    {
      int option_rv= getopt_long(argc, argv, "", long_options, &option_index);
      if (option_rv == -1)
      {
        break;
      }

      switch (option_rv)
      {
      case OPT_LIBYATL_HELP:
        for (struct option *opt= long_options; opt->name; ++opt)
        {
          Out << "--" << opt->name;
        }
        exit(EXIT_SUCCESS);

      case OPT_LIBYATL_VERBOSE:
        opt_verbose= true;
        break;

      case OPT_LIBYATL_VERSION:
        break;

      case OPT_LIBYATL_QUIET:
        opt_quiet= true;
        break;

      case OPT_LIBYATL_REPEAT:
        errno= 0;
        opt_repeat= strtoul(optarg, (char **) NULL, 10);
        if (errno != 0)
        {
          Error << "unknown value passed to --repeat: `" << optarg << "`";
          exit(EXIT_FAILURE);
        }
        break;

      case OPT_LIBYATL_LIST_TESTS:
        opt_list_tests= true;
        opt_list_collection= true;
        break;

      case OPT_LIBYATL_LIST_COLLECTIONS:
        opt_list_collection= true;
        break;

      case OPT_LIBYATL_MATCH_COLLECTION:
        collection_to_run= optarg;
        break;

      case OPT_LIBYATL_MATCH_WILDCARD:
        wildcard= optarg;
        break;

      case OPT_LIBYATL_SSL:
        opt_ssl= true;
        break;

      case OPT_LIBYATL_MASSIVE:
        opt_massive= true;
        break;

      case '?':
        /* getopt_long already printed an error message. */
        Error << "unknown option to getopt_long()";
        exit(EXIT_FAILURE);

      default:
        break;
      }
    }
  }

  if (opt_verbose)
  {
    for (char** ptr= environ_; *ptr; ptr++)
    {
      Out << *ptr;
    }
  }

  srandom((unsigned int)time(NULL));

  errno= 0;
  if (bool(getenv("YATL_REPEAT")))
  {
    errno= 0;
    opt_repeat= strtoul(getenv("YATL_REPEAT"), (char **) NULL, 10);
    if (errno != 0)
    {
      Error << "ENV YATL_REPEAT passed an invalid value: `" << getenv("YATL_REPEAT") << "`";
      exit(EXIT_FAILURE);
    }
  }

  if ((bool(getenv("YATL_QUIET")) and (strcmp(getenv("YATL_QUIET"), "0") == 0)) or opt_quiet)
  {
    opt_quiet= true;
  }
  else if (getenv("JENKINS_URL"))
  {
    if (bool(getenv("YATL_QUIET")) and (strcmp(getenv("YATL_QUIET"), "1") == 0))
    { }
    else
    {
      opt_quiet= true;
    }
  }

  if (getenv("YATL_WILDCARD"))
  {
    wildcard= getenv("YATL_WILDCARD");
  }

  if (getenv("YATL_RUN_MASSIVE_TESTS"))
  {
    opt_massive= true;
  }

  if (getenv("YATL_SSL"))
  {
    opt_ssl= true;
  }

  if (opt_quiet)
  {
    close(STDOUT_FILENO);
  }

  if (opt_ssl)
  {
    is_ssl(opt_ssl);
  }

  if (opt_massive)
  {
    is_massive(opt_massive);
  }

  libtest::vchar_t tmp_directory;
  tmp_directory.resize(1024);
  if (getenv("LIBTEST_TMP"))
  {
    snprintf(&tmp_directory[0], tmp_directory.size(), "%s", getenv("LIBTEST_TMP"));
  }
  else
  {
    snprintf(&tmp_directory[0], tmp_directory.size(), "%s", LIBTEST_TEMP);
  }

  if (chdir(&tmp_directory[0]) == -1)
  {
    libtest::vchar_t getcwd_buffer;
    getcwd_buffer.resize(1024);
    char *dir= getcwd(&getcwd_buffer[0], getcwd_buffer.size());

    Error << "Unable to chdir() from " << dir << " to " << &tmp_directory[0] << " errno:" << strerror(errno);
    return EXIT_FAILURE;
  }

  if (libtest::libtool() == NULL)
  {
    Error << "Failed to locate libtool";
    return EXIT_FAILURE;
  }

  if (getenv("YATL_COLLECTION_TO_RUN"))
  {
    if (strlen(getenv("YATL_COLLECTION_TO_RUN")))
    {
      collection_to_run= getenv("YATL_COLLECTION_TO_RUN");
    }
  }

  if (collection_to_run.compare("none") == 0)
  {
    return EXIT_SUCCESS;
  }

  if (collection_to_run.empty() == false)
  {
    Out << "Only testing " <<  collection_to_run;
  }

  int exit_code;

  try 
  {
    do
    {
      exit_code= EXIT_SUCCESS;
      fatal_assert(sigignore(SIGPIPE) == 0);

      libtest::SignalThread signal;
      if (signal.setup() == false)
      {
        Error << "Failed to setup signals";
        return EXIT_FAILURE;
      }

      std::unique_ptr<libtest::Framework> frame(new libtest::Framework(signal, binary_name, collection_to_run, wildcard));

      if (opt_list_collection)
      {
        for (Suites::iterator iter= frame->suites().begin();
             iter != frame->suites().end();
             ++iter)
        {
          if (opt_list_tests)
          {
            for (TestCases::iterator test_iter= (*iter)->tests().begin();
                 test_iter != (*iter)->tests().end();
                 ++test_iter)
            {
              Out << (*iter)->name() << "." << (*test_iter)->name();
            }
          }
          else
          {
            Out << (*iter)->name();
          }
        }
        
        continue;
      }

      // Run create(), bail on error.
      {
        switch (frame->create())
        {
        case TEST_SUCCESS:
          break;

        case TEST_SKIPPED:
          SKIP("SKIP was returned from framework create()");
          break;

        case TEST_FAILURE:
          std::cerr << "Could not call frame->create()" << std::endl;
          return EXIT_FAILURE;
        }
      }

      frame->exec();

      if (signal.is_shutdown() == false)
      {
        signal.set_shutdown(SHUTDOWN_GRACEFUL);
      }

      shutdown_t status= signal.get_shutdown();
      if (status == SHUTDOWN_FORCED)
      {
        // Tests were aborted
        exit_code= EXIT_FAILURE;
      }
      else if (frame->failed())
      {
        // Some test failed
        exit_code= EXIT_FAILURE;
      }
      else if (frame->skipped() and frame->failed() and frame->success())
      {
        // Some tests were skipped
      }
      else if (frame->success() and (frame->failed() == 0))
      {
        // Success
      }

#if 0
      {
        std::ofstream xml_file;
        std::string file_name;
        if (getenv("WORKSPACE"))
        {
          file_name.append(getenv("WORKSPACE"));
          file_name.append("/");
        }
        file_name.append(frame->name());
        file_name.append(".xml");
        xml_file.open(file_name.c_str(), std::ios::trunc);
        libtest::Formatter::xml(*frame, xml_file);
      }
#endif

#if 0
      {
        std::ofstream tap_file;
        std::string file_name;
        if (getenv("WORKSPACE"))
        {
          file_name.append(getenv("WORKSPACE"));
          file_name.append("/");
        }
        file_name.append(frame->name());
        file_name.append(".tap");
        tap_file.open(file_name.c_str(), std::ios::trunc);
        libtest::Formatter::tap(*frame, tap_file);
      }
#endif
    } while (exit_code == EXIT_SUCCESS and --opt_repeat);
  }
  catch (const libtest::__skipped& e)
  {
    exit_code= EXIT_SKIP;
  }
  catch (const libtest::__failure& e)
  {
    libtest::stream::make_cout(e.file(), e.line(), e.func()) << e.what();
    exit_code= EXIT_FAILURE;
  }
  catch (const libtest::fatal& e)
  {
    std::cerr << "FATAL:" << e.what() << std::endl;
    exit_code= EXIT_FAILURE;
  }
  catch (const libtest::disconnected& e)
  {
    std::cerr << "Unhandled disconnection occurred:" << e.what() << std::endl;
    exit_code= EXIT_FAILURE;
  }
  catch (const std::exception& e)
  {
    std::cerr << "std::exception:" << e.what() << std::endl;
    exit_code= EXIT_FAILURE;
  }
  catch (char const* s)
  {
    std::cerr << "Exception:" << s << std::endl;
    exit_code= EXIT_FAILURE;
  }
  catch (...)
  {
    std::cerr << "Unknown exception halted execution." << std::endl;
    exit_code= EXIT_FAILURE;
  }

  return exit_code;
}
