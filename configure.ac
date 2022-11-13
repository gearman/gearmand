# configure.ac -*- autoconf -*- vim: filetype=config
#
# Gearman server and library
# Copyright (C) 2011-2014 Data Differential, http://datadifferential.com/
# Copyright (C) 2008 Brian Aker, Eric Day, Monty Taylor
# All rights reserved.
#
# Use and distribution licensed under the BSD license.  See
# the COPYING file in this directory for full text.

m4_include([version.m4])
AC_REVISION([m4_esyscmd_s([git describe --always])])
AC_PREREQ([2.69])
AC_INIT([gearmand],[VERSION_NUMBER],[https://github.com/gearman/gearmand/issues],[gearmand],[http://gearman.info/])

AC_CONFIG_AUX_DIR([build-aux])

# Setup the compilers early on
AC_PROG_CC([cc gcc clang])
AC_PROG_CXX([c++ g++ clang++])

AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_HOST
AC_ARG_PROGRAM
AC_USE_SYSTEM_EXTENSIONS

AX_GENERATE_CHANGELOG

AM_INIT_AUTOMAKE([1.11 color-tests -Wno-portability subdir-objects foreign tar-ustar])

AC_CONFIG_HEADERS([gear_config.h:gear_config.in])dnl Keep filename to 8.3 for MS-DOS.
AC_SUBST([AUTOHEADER_FILE],[gear_config.h])
AC_CONFIG_SRCDIR([libgearman/gearman.cc])

GEARMAN_LIBRARY_VERSION=8:0:0
#                       | | |
#                +------+ | +---+
#                |        |     |
#             current:revision:age
#                |        |     |
#                |        |     +- increment if interfaces have been added
#                |        |        set to zero if interfaces have been removed
#                |        |        or changed
#                |        +- increment if source code has changed
#                |           set to zero if current is incremented
#                +- increment if interfaces have been added, removed or changed
AC_SUBST([GEARMAN_LIBRARY_VERSION])

LT_PREREQ([2.2])
LT_INIT([dlopen])
LT_LANG([C++])
LT_LIB_M

AC_SUBST([lt_cv_dlopen_libs])

# AC_REQUIRE(AC_PROG_CC)

AC_DEFUN([CHECK_CXX0X],[
          AC_LANG_PUSH([C++])
          AX_CHECK_COMPILE_FLAG([-std=c++0x],[
                                 CXX_STD="c++0x"])
          AC_LANG_POP([C++])
          ])
CHECK_CXX0X


AX_PLATFORM
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# First look for Boost, maybe for a specific minimum version:
AC_DEFUN([CHECK_BOOST],
         [AX_BOOST_BASE([1.39],,[AC_MSG_ERROR(could not find boost)])
         AX_BOOST_PROGRAM_OPTIONS])
CHECK_BOOST

m4_include([libhashkit/include.m4])

AX_CXX_CINTTYPES
CONFIG_EXTRA

m4_include([libtest/yatl.m4])

AM_CONDITIONAL([BUILDING_GEARMAN],true)
AM_CONDITIONAL([HAVE_LIBGEARMAN],true)
AC_SUBST([_WITH_LIBGEARMAN_SUPPORT],["_WITH_LIBGEARMAN_SUPPORT 0"])
AC_DEFINE([GEARMAND_BINARY],["./gearmand/gearmand"],[Name of the gearmand binary used in make test])
AC_DEFINE([HAVE_LIBGEARMAN],[1], [libgearman support])
AC_DEFINE([HAVE_GEARMAND_BINARY],[1],[gearmand binary was found])

AC_DEFUN([CHECK_CURL],
         [AX_LIB_CURL([7.21.7],
                      [AX_CHECK_LIBRARY([LIBCURL],[curl/curl.h],[curl])],
                      [AC_DEFINE([HAVE_LIBCURL],[0],[Have the LIBCURL library])
                      ])
         ])
CHECK_CURL
AM_CONDITIONAL([HAVE_LIBCURL],[test "x${ax_cv_have_LIBCURL}" = "xyes"])

m4_include([m4/drizzled.m4])
WITH_LIBDRIZZLE

AX_LIB_SQLITE3
AS_IF([test "x${WANT_SQLITE3}" = "xyes"],
      [AC_DEFINE([HAVE_LIBSQLITE3],[1],[Have the SQLITE3 library])],
      [AC_DEFINE([HAVE_LIBSQLITE3],[0],[Have the SQLITE3 library])])
AM_CONDITIONAL([HAVE_LIBSQLITE3],[test "x${WANT_SQLITE3}" = "xyes"])


PANDORA_ENABLE_DTRACE
AX_HAVE_LIBPQ
PANDORA_HAVE_LIBTOKYOCABINET
AC_FUNC_STRERROR_R
SOCKET_SEND_FLAGS
FCNTL_FLAGS
AX_ENABLE_LIBHIREDIS

# Checks for programs.
AC_CHECK_PROGS([DPKG_GENSYMBOLS],[dpkg-gensymbols])
AC_CHECK_PROGS([OPENSSL],[openssl])
AC_CHECK_PROGS([VALGRIND],['valgrind'])
AC_CHECK_PROGS([WINE],['wine'])
AC_CHECK_PROGS([RPMBUILD],[rpmbuild --nocheck])
AC_CHECK_PROGS([RPMDEV_SETUPTREE],[rpmdev-setuptree])
AC_CHECK_PROGS([RPM],[rpm])
AC_PATH_PROG([GPERF],['gperf'],[])
AS_IF([test -z "$GPERF"],
      [AC_MSG_ERROR([could not find gperf])]])
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AX_PROG_MEMCACHED
AX_PROG_SPHINX_BUILD(,[AC_MSG_WARN([sphinx-build version 1.0 or greater is required to build man pages])])
AX_WITH_PROG([LCOV],[lcov])
AX_WITH_PROG([LCOV_GENHTML],[genhtml])

AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS_ONCE([arpa/inet.h])
AC_CHECK_HEADERS_ONCE([errno.h])
AC_CHECK_HEADERS_ONCE([execinfo.h])
AC_CHECK_HEADERS_ONCE([fcntl.h])
AC_CHECK_HEADERS_ONCE([features.h])
AC_CHECK_HEADERS_ONCE([fnmatch.h])
AC_CHECK_HEADERS_ONCE([getopt.h])
AC_CHECK_HEADERS_ONCE([inttypes.h])
AC_CHECK_HEADERS_ONCE([limits.h])
AC_CHECK_HEADERS_ONCE([mach/mach.h])
AC_CHECK_HEADERS_ONCE([netdb.h])
AC_CHECK_HEADERS_ONCE([netinet/in.h])
AC_CHECK_HEADERS_ONCE([netinet/tcp.h])
AC_CHECK_HEADERS_ONCE([poll.h])
AC_CHECK_HEADERS_ONCE([pwd.h])
AC_CHECK_HEADERS_ONCE([signal.h])
AC_CHECK_HEADERS_ONCE([spawn.h])
AC_CHECK_HEADERS_ONCE([stdarg.h])
AC_CHECK_HEADERS_ONCE([stddef.h])
AC_CHECK_HEADERS_ONCE([stdint.h])
AC_CHECK_HEADERS_ONCE([stdio.h])
AC_CHECK_HEADERS_ONCE([stdlib.h])
AC_CHECK_HEADERS_ONCE([string.h])
AC_CHECK_HEADERS_ONCE([strings.h])
AC_CHECK_HEADERS_ONCE([sys/epoll.h])
AC_CHECK_HEADERS_ONCE([sys/resource.h])
AC_CHECK_HEADERS_ONCE([sys/socket.h])
AC_CHECK_HEADERS_ONCE([sys/stat.h])
AC_CHECK_HEADERS_ONCE([sys/time.h])
AC_CHECK_HEADERS_ONCE([sys/types.h])
AC_CHECK_HEADERS_ONCE([sys/utsname.h])
AC_CHECK_HEADERS_ONCE([sys/wait.h])
AC_CHECK_HEADERS_ONCE([syslog.h])
AC_CHECK_HEADERS_ONCE([unistd.h])
AC_CHECK_HEADERS_ONCE([winsock2.h])
AC_CHECK_HEADERS_ONCE([libmemcached-1.0/types/return.h])
AC_CHECK_DECL([htonll],[AC_DEFINE([HAVE_HTONLL],[1],
              [Define to 1 if you have htonll.])])

AM_CONDITIONAL([BUILD_WIN32_WRAPPERS],[test "x$ac_cv_header_winsock2_h" = "xyes"])
AS_IF([test "x$ac_cv_header_winsock2_h" = "xyes"],
      [AM_LDFLAGS="$AM_LDFLAGS -lws2_32"
       AM_CFLAGS="$AM_CFLAGS $NO_WERROR"
       AM_CXXFLAGS="$AM_CXXFLAGS $NO_WERROR"
       ])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_TYPES([ptrdiff_t])
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_HEADER_STDBOOL
AC_CHECK_HEADERS_ONCE([sys/time.h])

AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UID_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MEMCMP
AC_FUNC_STRERROR_R
AC_FUNC_VPRINTF
AC_REPLACE_FNMATCH
AC_CHECK_FUNCS([accept4])
AC_CHECK_FUNCS([dup2])
AC_CHECK_FUNCS([fork])
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([memchr])
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([pipe])
AC_CHECK_FUNCS([pipe2])
AC_CHECK_FUNCS([select])
AC_CHECK_FUNCS([setenv])
AC_CHECK_FUNCS([socket])
AC_CHECK_FUNCS([strcasecmp])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([strncasecmp])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([strtoul])
AC_CHECK_FUNCS([uname])
AC_CHECK_FUNCS([atexit])
AC_CHECK_FUNCS([getcwd])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNC([setsockopt],[],[AC_CHECK_LIB([socket],[setsockopt])])
AC_CHECK_FUNC([bind],[],[AC_CHECK_LIB([bind],[bind])])

# Check for -lrt
AC_CHECK_LIB([rt],[clock_gettime],
             [AX_SAVE_FLAGS
              LIBS="$LIB -lrt"
              AC_CHECK_FUNCS([clock_gettime])
              AX_RESTORE_FLAGS

              AC_SUBST([RT_LIB],[-lrt])
              AC_DEFINE([HAVE_LIBRT],[1],[Have -lrt])
             ],
             [AC_CHECK_FUNCS([clock_gettime])])

# Check for OPENSSL
AX_CHECK_OPENSSL([have_openssl=yes
                  AC_DEFINE([HAVE_OPENSSL],[1],[Enable openssl Support])],
                  [have_openssl=no]) 
AM_CONDITIONAL([ENABLE_OPENSSL],[test "x${have_openssl}" = "xyes"])

# Check for wolfSSL
AX_CHECK_WOLFSSL([have_wolfssl=yes],[have_wolfssl=no]) 
AM_CONDITIONAL([ENABLE_WOLFSSL],[test "x${have_wolfssl}" = "xyes"])

AC_DEFUN([AX_ENABLE_SSL],
         [AC_PREREQ([2.69])dnl
         m4_define([_SSL_ENABLE_DEFAULT], [m4_if($1, no, no, no)])dnl
         AC_ARG_ENABLE([ssl],
                       [AS_HELP_STRING([--enable-ssl],
                                       [Enable ssl support for Gearman @<:@default=]_SSL_ENABLE_DEFAULT[@:>@])],
                                       [AS_CASE([$enableval],
                                                [yes],[enable_ssl=yes],
                                                [no],[enable_ssl=no],
                                                [enable_ssl=no])
                                        ],
                                        [enable_ssl=]_SSL_ENABLE_DEFAULT)
         AS_IF([test "x${enable_ssl}" = "xyes"],
               [AS_IF([test "x${have_wolfssl}" = "xyes"],
                      [AC_DEFINE([HAVE_SSL],[1],[Enable SSL Support])
                        AC_DEFINE([HAVE_OPENSSL],[0],[Enable OPENSSL Support])
                      ],
                      [AS_IF([test "x${have_openssl}" = "xyes"],
                        [AC_DEFINE([HAVE_SSL],[1],[Enable SSL Support])]
                       )]
                      )
               ]
             )
         AM_CONDITIONAL([ENABLE_SSL],[test "x${enable_ssl}" = "xyes"])
         ])

if [ test "x${enable_ssl}" = "xyes" ]; then
  if ! [ test "x${have_openssl}" = "xyes" ] &&  ! [ test "x${have_wolfssl}" = "xyes" ]; then
    AC_MSG_ERROR([Unable to find either openssl or wolfssl])
  fi
fi

AX_ENABLE_SSL

AX_ENABLE_LIBMEMCACHED

AC_DEFINE([GEARMAND_BLOBSLAP_WORKER],[1],[Have Gearman Blobslap Worker])

AX_LIBEVENT(,[AC_MSG_ERROR([Unable to find libevent])])
AX_LIBEVENT2(,[AC_MSG_ERROR([Unable to find libevent2])])
AX_LIBEVENT2_PTHREADS(,[AC_MSG_ERROR([Unable to find libevent_pthreads])])

AX_UUID(,[AC_MSG_ERROR([Unable to find libuuid])])
AX_UUID_GENERATE_TIME_SAFE

AX_AM_JOBSERVER([yes])

AS_IF([test "x${TARGET_LINUX}" = "xtrue"],
      [AC_DEFINE([BUILD_LIBHOSTILE],[1],[Build libhostile])],[AC_DEFINE([BUILD_LIBHOSTILE],[0],[Build libhostile])])
AM_CONDITIONAL([TARGET_HOSTILE],[test "x${TARGET_LINUX}" = "xtrue"])

AC_DEFUN([CHECK_FOR_PTHREAD_AND_SASL],
         [AC_LANG_PUSH([C++])
         AX_PTHREAD(, [AC_MSG_ERROR(could not find Posix Pthread support)])
         AX_PTHREAD_TIMEDJOIN_NP
         AC_LANG_POP([C++])
         ])
CHECK_FOR_PTHREAD_AND_SASL

AX_ENDIAN
AX_HEX_VERSION([LIBGEARMAN],[$VERSION])
AC_SUBST([RPM_RELEASE],[1])

AX_HARDEN_COMPILER_FLAGS

AM_CONDITIONAL([TARGET_MINGW],[test "x${MINGW}" = "xtrue"])

# backtrace(), others require shared builds
AS_IF([test "x$enable_shared" = "xyes"],
      [AC_DEFINE([HAVE_SHARED_ENABLED],[1],[Enable code which requires shared library support. Like backtrace().])
      AX_CXX_GCC_ABI_DEMANGLE])
AM_CONDITIONAL([SHARED_ENABLED],[test "x$enable_shared" = "xyes"])

AC_CONFIG_FILES([Makefile
                 docs/source/conf.py
                 libgearman-1.0/version.h
                 scripts/gearmand
                 scripts/gearmand.xml
                 scripts/smf_install.sh
                 support/gearmand.init
                 support/gearmand.pc
                 support/gearmand.spec])

AS_IF([test "x${ax_enable_debug}" = "xyes"],
      [AC_DEFUN([CHECK_CXX11],
          [AC_LANG_PUSH([C++])
          AX_CHECK_COMPILE_FLAG([-std=c++11], [CXX_STD="c++11"])
          AC_LANG_POP([C++])
          ])
      CHECK_CXX11])

CXXFLAGS=" $CXXFLAGS -std=$CXX_STD"

AC_OUTPUT

echo "---"
echo "Configuration summary for $PACKAGE_NAME version $VERSION"
echo ""
echo "   * Installation prefix:       $prefix"
echo "   * System type:               $host_vendor-$host_os"
echo "   * Host CPU:                  $host_cpu"
echo "   * C Compiler:                $CC $CC_VERSION"
echo "   * C Flags:                   $CFLAGS"
echo "   * C++ Compiler:              $CXX $CXX_VERSION"
echo "   * C++ Flags:                 $CXXFLAGS"
echo "   * CPP Flags:                 $CPPFLAGS"
echo "   * LIBS:                      $LIBS"
echo "   * LDFLAGS Flags:             $LDFLAGS"
echo "   * Assertions enabled:        $ax_enable_assert"
echo "   * Debug enabled:             $ax_enable_debug"
echo "   * Warnings as failure:       $ac_cv_warnings_as_errors"
echo "   * Building with hiredis      $ac_enable_hiredis"
echo "   * Building with libsqlite3   $WANT_SQLITE3"
echo "   * Building with libdrizzle   $ac_enable_libdrizzle"
echo "   * Building with libmemcached $ax_enable_libmemcached"
echo "   * Building with libpq        $ac_cv_libpq"
echo "   * Building with tokyocabinet $ac_enable_libtokyocabinet"
echo "   * Building with libmysql     $found_mysql"
echo "   * SSL enabled:               $enable_ssl"
echo "   * wolfssl found:             $have_wolfssl"
echo "   * openssl found:             $have_openssl"
echo "   * make -j:                   $enable_jobserver"
echo "   * VCS checkout:              $ac_cv_vcs_checkout"
echo "   * sphinx-build:              $SPHINXBUILD"
echo ""
echo "---"

if [test "x${ax_enable_debug}" = "xyes"]; then
    echo "*****"
    echo "*"
    echo "*  NOTE: You should execute gearmand with --threads 0 when --enable-debug is specified."
    echo "*"
    echo "*****"
fi

case "$host_os" in
  *freebsd*)
    echo "*****"
    echo "*"
    echo "*  NOTE: You are on FreeBSD. BSD 'make' will not work."
    echo "*"
    echo "*   use 'gmake' To build gearmand"
    echo "*"
    echo "*****"
  ;;
esac

