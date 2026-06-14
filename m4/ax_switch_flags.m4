# ===========================================================================
#     https://www.gnu.org/software/autoconf-archive/ax_switch_flags.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_SWITCH_FLAGS(newnamespace,[oldnamespace])
#
# DESCRIPTION
#
#   Switch common compilation flags from temporary variables between two
#   compilation namespace.
#
#   Compilation flags includes: CPPFLAGS, CFLAGS, CXXFLAGS, LDFLAGS, LIBS,
#   OBJCFLAGS.
#
#   By default these flags are restored to a global (empty) namespace, but
#   user could restore from specific NAMESPACE by using
#   AX_RESTORE_FLAGS(NAMESPACE) macro.
#
#   Typical usage is like:
#
#     AX_SAVE_FLAGS(beginprogram)
#     CPPFLAGS="-Imypackagespath ${CPPFLAGS}"
#     AX_SWITCH_FLAGS(mypackage,beginprogram)
#
# LICENSE
#
#   Copyright (c) 2009 Filippo Giunchedi <filippo@esaurito.net>
#   Copyright (c) 2011 The Board of Trustees of the Leland Stanford Junior University
#   Copyright (c) 2011 Russ Allbery <rra@stanford.edu>
#   Copyright (c) 2013 Bastien ROUCARIES <roucaries.bastien+autoconf@gmail.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 6

AC_DEFUN([AX_SWITCH_FLAGS], [
  m4_if($1, [], [m4_fatal([$0: namespace is empty])])
  AC_REQUIRE(AX_SAVE_FLAGS)
  AC_REQUIRE(AX_RESTORE_FLAGS)
  AX_SAVE_FLAGS($1[])
  AX_RESTORE_FLAGS($2[])
])
