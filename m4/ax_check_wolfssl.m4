# ===========================================================================
#     http://www.gnu.org/software/autoconf-archive/ax_check_wolfssl.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CHECK_WOLFSSL([action-if-found],[action-if-not-found])
#
# DESCRIPTION
#
#   Look for WolfSSL (formerly CyaSSL) in a number of default spots, or in a
#   user-selected spot (via --with-wolfssl).  Sets
#
#     WOLFSSL_CPPFLAGS
#     WOLFSSL_LIB
#     WOLFSSL_LDFLAGS
#
#   and calls ACTION-IF-FOUND or ACTION-IF-NOT-FOUND appropriately
#
# LICENSE
#
#   Copyright (c) 2013 Brian Aker. <brian@tangent.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 1

AC_DEFUN([AX_CHECK_WOLFSSL],
         [AC_PREREQ([2.63])dnl
         m4_define([_WOLFSSL_ENABLE_DEFAULT], [m4_if($1, yes, yes, yes)])dnl
         AC_ARG_ENABLE([wolfssl],
                       [AS_HELP_STRING([--enable-wolfssl],
                                       [Enable ssl support for Gearman @<:@default=]_WOLFSSL_ENABLE_DEFAULT[@:>@])],
                       [AS_CASE([$enableval],
                                [yes],[enable_wolfssl=yes],
                                [no],[enable_wolfssl=no],
                                [enable_wolfssl=no])
                       ],
                       [enable_wolfssl=]_WOLFSSL_ENABLE_DEFAULT)

         AS_IF([test "x${enable_wolfssl}" = "xyes"],
               [AX_CHECK_LIBRARY([WOLFSSL],[wolfssl/ssl.h],[wolfssl],[],
                                 [enable_wolfssl=no])])

         AS_IF([test "x${enable_wolfssl}" = "xyes"],
               [AC_MSG_RESULT([yes])
               $1],
               [AC_MSG_RESULT([no])
               $2
               ])
         ])
