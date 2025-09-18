# ===========================================================================
#      https//libmemcached.org/
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIBMEMCACHED, AX_LIBMEMCACHED_UTIL, AX_ENABLE_LIBMEMCACHED
#
# DESCRIPTION
#
#   Checked for installation of libmemcached
#
#  AC_SUBST(LIBMEMCACHED_CFLAGS) 
#  AC_SUBST(LIBMEMCACHED_LIB)
#  AC_SUBST(LIBMEMCACHED_UTIL_LIB)
#
#   NOTE: Implementation uses AC_CHECK_HEADER.
#
# LICENSE
#
#  Copyright (C) 2012 Brian Aker
#  All rights reserved.
#  
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#  
#      * Redistributions of source code must retain the above copyright
#  notice, this list of conditions and the following disclaimer.
#  
#      * Redistributions in binary form must reproduce the above
#  copyright notice, this list of conditions and the following disclaimer
#  in the documentation and/or other materials provided with the
#  distribution.
#  
#      * The names of its contributors may not be used to endorse or
#  promote products derived from this software without specific prior
#  written permission.
#  
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#serial 2

AC_DEFUN([AX_LIBMEMCACHED], [
    AC_CHECK_HEADER([libmemcached-1.0/memcached.h], [
      AC_CACHE_CHECK([check for -lmemcached], [ax_cv_libmemcached], [
        AC_LANG_PUSH([C])
        AX_SAVE_FLAGS
        LIBS="-lmemcached $LIBS"
        AC_RUN_IFELSE([
          AC_LANG_PROGRAM([#include <libmemcached-1.0/memcached.h>], [
            memcached_st *memc;
            memc= memcached(NULL, 0);
            memcached_free(memc);
            ])],
          [ax_cv_libmemcached=yes],
          [ax_cv_libmemcached=no],
          [AC_MSG_WARN([test program execution failed])])
        AC_LANG_POP
        AX_RESTORE_FLAGS
        ])
      ])

  AS_IF([test "x$ax_cv_libmemcached" = "xyes"], [
      AC_DEFINE([HAVE_LIBMEMCACHED_MEMCACHED_H], [1], [Have libmemcached-1.0/memcached.h])
      ],[
      AC_DEFINE([HAVE_LIBMEMCACHED_MEMCACHED_H], [0], [Have libmemcached-1.0/memcached.h])
      ])
  ])

  AC_DEFUN([AX_LIBMEMCACHED_UTIL], [
      AC_REQUIRE([AX_LIBMEMCACHED])
      AS_IF([test "$ax_cv_libmemcached" = yes], [
        AC_CHECK_HEADER([libmemcachedutil-1.0/util.h], [
          AC_CACHE_CHECK([check for -lmemcachedutil], [ax_cv_libmemcached_util], [
            AX_SAVE_FLAGS
            AC_LANG_PUSH([C])
            LIBS="-lmemcachedutil -lmemcached $LIBS"
            AC_RUN_IFELSE([
              AC_LANG_PROGRAM([#include <libmemcachedutil-1.0/util.h>], [
                memcached_pool_st *memc_pool= memcached_pool_create(NULL, 0, 3);
                memcached_pool_destroy(memc_pool);
                ])],
              [ax_cv_libmemcached_util=yes],
              [ax_cv_libmemcached_util=no],
              [AC_MSG_WARN([test program execution failed])])
            AC_LANG_POP
            AX_RESTORE_FLAGS
            ])
          ])
        ])

      AS_IF([test "x$ax_cv_libmemcached_util" = "xyes"], [
        AC_DEFINE([HAVE_LIBMEMCACHED_UTIL_H], [1], [Have libmemcachedutil-1.0/util.h])
        ],[
        AC_DEFINE([HAVE_LIBMEMCACHED_UTIL_H], [0], [Have libmemcachedutil-1.0/util.h])
        ])
      ])

AC_DEFUN([_ENABLE_LIBMEMCACHED], [
         AC_REQUIRE([AX_LIBMEMCACHED_UTIL])
         AC_ARG_ENABLE([libmemcached],
                       [AS_HELP_STRING([--disable-libmemcached],
                                       [Build with libmemcached support @<:@default=on@:>@])],
                       [ax_enable_libmemcached="$enableval"],
                       [ax_enable_libmemcached="yes"])

         AS_IF([test "x$ax_cv_libmemcached" != "xyes"], [
               ax_enable_libmemcached="not found"
               ])

         AS_IF([test "x$ax_enable_libmemcached" = "xyes"], [
               AC_DEFINE([HAVE_LIBMEMCACHED], [1], [Enable libmemcached support])
               LIBMEMCACHED_CFLAGS=
               AC_SUBST([LIBMEMCACHED_CFLAGS])
               LIBMEMCACHED_LIB="-lmemcached"
               AC_SUBST([LIBMEMCACHED_LIB])
               AS_IF([test "x$ax_cv_libmemcached_util" = "xyes"], [
                     LIBMEMCACHED_UTIL_LIB="-lmemcachedutil -lmemcached"
                     AC_SUBST([LIBMEMCACHED_UTIL_LIB])
                     ])
               ],[
               AC_DEFINE([HAVE_LIBMEMCACHED], [0], [Enable libmemcached support])
               ])
         AM_CONDITIONAL(HAVE_LIBMEMCACHED, test "x${ax_enable_libmemcached}" = "xyes")
         ])

AC_DEFUN([AX_ENABLE_LIBMEMCACHED], [ AC_REQUIRE([_ENABLE_LIBMEMCACHED]) ])
