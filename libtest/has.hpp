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

#pragma once

namespace libtest {

LIBTEST_API
bool has_libmemcached_sasl(void);

LIBTEST_API
bool has_libmemcached();

LIBTEST_API
bool has_libdrizzle();

LIBTEST_API
bool has_postgres_support();

LIBTEST_API
bool has_memcached();

LIBTEST_API
bool has_memcached_sasl();

LIBTEST_API
bool has_gearmand();

LIBTEST_API
bool has_drizzled();

LIBTEST_API
bool has_mysqld();

LIBTEST_API
const char* memcached_binary();

LIBTEST_API
const char *gearmand_binary();

LIBTEST_API
const char *drizzled_binary();

LIBTEST_API
bool has_redis();

LIBTEST_API
const char* redis_server_binary();

LIBTEST_API
bool has_mysql_install_db();

LIBTEST_API
const char* mysqld_binary();

LIBTEST_API
const char* mysql_install_db_binary();

LIBTEST_API
const char* mysql_client_binary();
} // namespace libtest
