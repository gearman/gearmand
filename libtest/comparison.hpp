/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Data Differential YATL (i.e. libtest)  library
 *
 *  Copyright (C) 2012 Data Differential, http://datadifferential.com/
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

#include <typeinfo>

#if defined(HAVE_LIBMEMCACHED) && HAVE_LIBMEMCACHED
#include <libmemcached-1.0/memcached.h>
#include <libmemcachedutil-1.0/ostream.hpp>
#include <libtest/memcached.hpp>
#endif

#if defined(HAVE_LIBGEARMAN) && HAVE_LIBGEARMAN
#include <libgearman-1.0/ostream.hpp>
#endif

namespace libtest {

LIBTEST_API
bool jenkins_is_caller(void);

LIBTEST_API
bool gdb_is_caller(void);

LIBTEST_API
bool _in_valgrind(const char *file, int line, const char *func);

LIBTEST_API
bool helgrind_is_caller(void);

template <class T_comparable>
bool _compare_truth(const char *file, int line, const char *func, T_comparable __expected, const char *assertation_label)
{
  if (__expected == false)
  {
    libtest::stream::make_cerr(file, line, func) << "Assertation  \"" << assertation_label << "\"";
    return false;
  }

  return true;
}

template <class T1_comparable, class T2_comparable>
bool _compare(const char *file, int line, const char *func, const T1_comparable& __expected, const T2_comparable& __actual, bool use_io)
{
  if (__expected != __actual)
  {
    if (use_io)
    {
      libtest::stream::make_cerr(file, line, func) << "Expected \"" << __expected << "\" got \"" << __actual << "\"";
    }

    return false;
  }

  return true;
}

template <class T1_comparable, class T2_comparable>
bool _compare_strcmp(const char *file, int line, const char *func, const T1_comparable& __expected, const T2_comparable& __actual)
{
  if (strncmp(__expected, __actual, strlen(__expected)))
  {
    libtest::stream::make_cerr(file, line, func) << "Expected " << __expected << " passed \"" << __actual << "\"";
    return false;
  }

  return true;
}

template <class T_comparable>
bool _compare_zero(const char *file, int line, const char *func, T_comparable __actual)
{
  if (T_comparable(0) != __actual)
  {
    libtest::stream::make_cerr(file, line, func) << "Expected 0 got \"" << __actual << "\"";
    return false;
  }

  return true;
}

template <class T1_comparable, class T2_comparable>
bool _ne_compare(const char *file, int line, const char *func, T1_comparable __expected, T2_comparable __actual, bool io_error= true)
{
  if (__expected == __actual)
  {
    if (io_error)
    {
      libtest::stream::make_cerr(file, line, func) << "Expected \"" << __expected << "\" got \"" << __actual << "\"";
    }

    return false;
  }

  return true;
}

template <class T_comparable, class T_expression_string>
bool _assert_truth(const char *file, int line, const char *func, T_comparable __truth, T_expression_string __expression, const char* __explain= NULL)
{
  if (__truth)
  {
    return true;
  }

  if (__explain)
  {
    libtest::stream::make_cerr(file, line, func) << "Assertion \"" << __expression << "\" warning:" << __explain;
  }

  return false;
}

} // namespace libtest
