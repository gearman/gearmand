# ===========================================================================
# https://github.com/BrianAker/ddm4
# ===========================================================================
#
# SYNOPSIS
#
#   AX_PROG_SPHINX_BUILD([ACTION-IF-FOUND], [ACTION-IF-NOT_FOUND])
#
# DESCRIPTION
#
#   Look for sphinx-build and make sure it is a recent version of it.
#
# LICENSE
#
#   Copyright (c) 2012-2013 Brian Aker <brian@tangent.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 6

AC_DEFUN([AX_PROG_SPHINX_BUILD],
         [AX_WITH_PROG([SPHINXBUILD],[sphinx-build],[:])
         AS_IF([test x"$SPHINXBUILD" = x":"],
               [SPHINXBUILD=],
               [AS_IF([test -x "$SPHINXBUILD"],
                      [AC_MSG_CHECKING([whether $SPHINXBUILD is recent enough])
                       # Prefer --version; fall back for very old Sphinx that lacked it
                       if $SPHINXBUILD --version >conftest.sphinx 2>&1; then
                         ax_sphinx_need_smoke=no
                       else
                         $SPHINXBUILD >conftest.sphinx 2>&1
                         ax_sphinx_need_smoke=yes
                       fi
                       ax_sphinx_build_version=`head -1 conftest.sphinx`
                       rm -f conftest.sphinx
                       # Smoke test only when --version was unavailable
                       AS_IF([test "x$ax_sphinx_need_smoke" = xyes],
                             [$SPHINXBUILD -Q -C -b man -d conftest.d . . >/dev/null 2>&1
                              AS_IF([test $? -eq 0], [], [SPHINXBUILD=])
                              rm -rf conftest.d])
                       AS_IF([test -n "$SPHINXBUILD"],
                             [AC_MSG_RESULT([$ax_sphinx_build_version])],
                             [AC_MSG_RESULT([no])])
                      ],
                      [SPHINXBUILD=])
                ])

         INC_SPHINXBUILD='
ifneq ($(filter $(SPHINX_TARGETS), $(MAKECMDGOALS)), )
.NOTPARALLEL:
endif
'
          AC_SUBST([INC_SPHINXBUILD])
          AM_SUBST_NOTMAKE([INC_SPHINXBUILD])

         AS_IF([test -n "${SPHINXBUILD}"],
               [AC_SUBST([SPHINXBUILD])
               ifelse([$1], , :, [$1])],
               [ifelse([$2], , :, [$2])])
         ])
