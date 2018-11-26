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

enum test_return_t {
  TEST_SUCCESS,
  TEST_FAILURE,
  TEST_SKIPPED
};


static inline bool test_success(test_return_t rc)
{
  return (rc == TEST_SUCCESS);
}

static inline bool test_failed(test_return_t rc)
{
  return (rc != TEST_SUCCESS);
}

namespace libtest {

class Error
{
  public:
    Error():
      _is_error(false),
      _file(NULL),
      _line(-1),
      _func(NULL)
    {
    }

    Error(const Error& error_):
      _is_error(false),
      _file(NULL),
      _line(0),
      _func(NULL)
    {
      if (error_.is_error())
      {
        _is_error= true;
        _file= error_.file();
        _line= error_.line();
        _func= error_.func();
        _message= error_.error();
      }
    }

    Error(const char* file_, int line_, const char* func_, const std::string& message_):
      _is_error(true),
      _file(file_),
      _line(line_),
      _func(func_),
      _message(message_)
    {
    }

    Error& operator=(const Error&) = delete;

    bool is_error() const
    {
      return _is_error;
    }

    const char* func() const
    {
      return _func;
    }

    const char* file() const
    {
      return _file;
    }

    int line() const
    {
      return _line;
    }

    const std::string& error() const
    {
      return _message;
    }

  private:
    bool _is_error;
    const char* _file;
    int _line;
    const char* _func;
    std::string _message;
};

} // namespace libtest

