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

#include "libtest/yatlcon.h"

#include <libtest/common.h>

#if defined(HAVE_LIBCURL) && HAVE_LIBCURL
#include <curl/curl.h>
#else
class CURL;
#endif


static void cleanup_curl(void)
{
#if defined(HAVE_LIBCURL) && HAVE_LIBCURL
  curl_global_cleanup();
#endif
}

static void initialize_curl_startup()
{
#if defined(HAVE_LIBCURL) && HAVE_LIBCURL
  if (curl_global_init(CURL_GLOBAL_ALL))
  {
    FATAL("curl_global_init(CURL_GLOBAL_ALL) failed");
  }
#endif

  if (atexit(cleanup_curl))
  {
    FATAL("atexit() failed");
  }
}

static pthread_once_t start_key_once= PTHREAD_ONCE_INIT;
static void initialize_curl(void)
{
  int ret;
  if ((ret= pthread_once(&start_key_once, initialize_curl_startup)) != 0)
  {
    FATAL(strerror(ret));
  }
}

namespace libtest {
namespace http {

#define YATL_USERAGENT "YATL/1.0"

static size_t http_get_result_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
  vchar_t *_body= (vchar_t*)data;

  _body->resize(size * nmemb);
  if (_body->size())
  {
    memcpy(static_cast<void*>(&_body[0]), ptr, _body->size());
  }

  return _body->size();
}

static void init(CURL *curl, const std::string& url)
{
  (void)http_get_result_callback;
  (void)curl;
  (void)url;
#if defined(HAVE_LIBCURL) && HAVE_LIBCURL
  if (HAVE_LIBCURL)
  {
    assert(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, YATL_USERAGENT);
  }
#endif
}

HTTP::HTTP(const std::string& url_arg) :
  _url(url_arg),
  _response(0)
{
  initialize_curl();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
bool GET::execute()
{
  (void)init;

#if defined(HAVE_LIBCURL) && HAVE_LIBCURL
  if (HAVE_LIBCURL)
  {
    CURL *curl= curl_easy_init();

    init(curl, url());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_get_result_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&_body);

    CURLcode retref= curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, _response);

    curl_easy_cleanup(curl);

    return bool(retref == CURLE_OK);
  }
#endif

  return false;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
bool POST::execute()
{
#if defined(HAVE_LIBCURL) && HAVE_LIBCURL
  if (HAVE_LIBCURL && _body.size())
  {
    CURL *curl= curl_easy_init();;

    init(curl, url());

    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, _body.size());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (void *)&_body[0]);

    CURLcode retref= curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, _response);

    curl_easy_cleanup(curl);

    return bool(retref == CURLE_OK);
  }
#endif

  return false;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
bool TRACE::execute()
{
#if defined(HAVE_LIBCURL) && HAVE_LIBCURL
  if (HAVE_LIBCURL && _body.size())
  {
    CURL *curl= curl_easy_init();;

    init(curl, url());

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "TRACE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_get_result_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&_body[0]);

    CURLcode retref= curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, _response);

    curl_easy_cleanup(curl);

    return retref == CURLE_OK;
  }
#endif

  return false;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
bool HEAD::execute()
{
#if defined(HAVE_LIBCURL) && HAVE_LIBCURL
  if (HAVE_LIBCURL)
  {
    CURL *curl= curl_easy_init();

    init(curl, url());

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "HEAD");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_get_result_callback);

    CURLcode retref= curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, _response);

    curl_easy_cleanup(curl);

    return retref == CURLE_OK;
  }
#endif

  return false;
}
#pragma GCC diagnostic pop

} // namespace http
} // namespace libtest
