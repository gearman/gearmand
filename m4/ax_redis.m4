# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
# ===========================================================================
#      https://github.com/BrianAker/ddm4/
# ===========================================================================
#
# SYNOPSIS
#
#   AX_PROG_REDIS
#
# DESCRIPTION
#
#   Check for redis-server, used to spawn a throwaway instance for the
#   redis queue plugin's test suite.
#
# LICENSE
#
#  Copyright (C) 2026 Alexei Pastuchov
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

#serial 1

AC_DEFUN([AX_PROG_REDIS],
         [AX_WITH_PROG([REDIS_SERVER_BINARY],[redis-server],[unknown])
         AS_IF([test x"$REDIS_SERVER_BINARY" != xunknown -a -x "$REDIS_SERVER_BINARY"],
               [AC_DEFINE([HAVE_REDIS_SERVER_BINARY], [1], [If redis-server binary is available])
               AC_DEFINE_UNQUOTED([REDIS_SERVER_BINARY],"$REDIS_SERVER_BINARY",[Name of the redis-server binary used in make test])
               ],
               [AC_DEFINE([HAVE_REDIS_SERVER_BINARY], [0], [If redis-server binary is available])
               REDIS_SERVER_BINARY=
               ])
         ])
