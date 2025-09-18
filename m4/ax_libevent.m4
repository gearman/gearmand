# https://github.com/BrianAker/ddm4
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIBEVENT([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]]) 
#   AX_LIBEVENT2() 
#   AX_LIBEVENT2_EVHTTP()
#   AX_LIBEVENT2_PTHREADS()
#
# DESCRIPTION
#
#   libevent library
#
# LICENSE
#
#   Copyright (c) 2012 Brian Aker <brian@tangent.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 6
 
AC_DEFUN([AX_LIBEVENT],
         [AC_PREREQ([2.63])dnl
         AC_CACHE_CHECK([test for a working libevent],[ax_cv_libevent],
           [AX_SAVE_FLAGS
           LIBS="-levent $LIBS"
           AC_LANG_PUSH([C])
           AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <event.h>
               ]],[[
               struct event_base *tmp_event= event_init();
               event_base_free(tmp_event);
               ]])],
             [ax_cv_libevent=yes],
             [ax_cv_libevent=no],
             [AC_MSG_WARN([test program execution failed])])
           AC_LANG_POP
           AX_RESTORE_FLAGS
           ])

         AS_IF([test "x$ax_cv_libevent" = "xyes"],
             [AC_SUBST([LIBEVENT_LIB],[-levent])
             AC_DEFINE([HAVE_LIBEVENT],[1],[Define if event_init is present in event.h.])],
             [AC_DEFINE([HAVE_LIBEVENT],[0],[Define if event_init is present in event.h.])])

         AM_CONDITIONAL(HAVE_LIBEVENT, test "x$ax_cv_libevent" = "xyes")
# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
         AS_IF([test "x$ax_cv_libevent" = xyes],
             [$1],
             [$2])
         ])

AC_DEFUN([AX_LIBEVENT2],
    [AC_REQUIRE([AX_LIBEVENT])
    AC_CACHE_CHECK([test for a working libevent version 2],[ax_cv_libevent2],
      [AX_SAVE_FLAGS
      LIBS="-levent $LIBS"
      AC_LANG_PUSH([C])
      AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <event2/event.h>
          ]],[[
          struct event_base *___event_base= event_base_new();  
          event_base_free(___event_base);
          ]])],
        [ax_cv_libevent2=yes],
        [ax_cv_libevent2=no],
        [AC_MSG_WARN([test program execution failed])])
      AC_LANG_POP([C])
      AX_RESTORE_FLAGS
      ])

    AS_IF([test "x$ax_cv_libevent2" = xyes],
        [AC_SUBST([LIBEVENT2_LIB],[-levent])
        AC_DEFINE([HAVE_LIBEVENT2],[1],[Define if event_init is present in event2/event.h.])],
        [AC_DEFINE([HAVE_LIBEVENT2],[0],[Define if event_init is present in event2/event.h.])])

    AM_CONDITIONAL([HAVE_LIBEVENT2], [test "x$ax_cv_libevent2" = xyes])
# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
    AS_IF([test "x$ax_cv_libevent2" = xyes],
        [$1],
        [$2])
    ])dnl AX_LIBEVENT2

# 
AC_DEFUN([AX_LIBEVENT2_EVHTTP],
    [AC_REQUIRE([AX_LIBEVENT2])
    AC_CACHE_CHECK([test for a working libevent2 evhttp interface], [ax_cv_libevent2_evhttp],
      [AX_SAVE_FLAGS
      LIBS="-levent $LIBS"
      AC_LANG_PUSH([C])
      AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <event2/event.h>
#include <event2/http.h>
          ]],[[
          struct event_base *___event_base= event_base_new();  
          struct evhttp *___evhttp= evhttp_new(___event_base);
          evhttp_free(___evhttp);
          event_base_free(___event_base);
          ]])],
        [ax_cv_libevent2_evhttp=yes],
        [ax_cv_libevent2_evhttp=no],
        [AC_MSG_WARN([test program execution failed])])
      AC_LANG_POP([C])
      AX_RESTORE_FLAGS
      ])

    AS_IF([test "x$ax_cv_libevent2_evhttp" = "xyes"],
        [AC_DEFINE([HAVE_LIBEVENT2_EVHTTP],[1],[Define if event_init is present in event2/http.h.])],
        [AC_DEFINE([HAVE_LIBEVENT2_EVHTTP],[0],[Define if event_init is present in event2/http.h.])])

    AM_CONDITIONAL([HAVE_LIBEVENT2_EVHTTP],[test "x$ax_cv_libevent2_evhttp" = xyes])
# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
    AS_IF([test "x$ax_cv_libevent2_evhttp" = xyes],
        [$1],
        [$2])
    ])dnl AX_LIBEVENT2_EVHTTP

#
AC_DEFUN([AX_LIBEVENT2_PTHREADS],
    [AC_REQUIRE([AX_LIBEVENT2])
    AC_CACHE_CHECK([test for a working libevent pthreads interface], [ax_cv_libevent2_pthreads],
      [AX_SAVE_FLAGS
      CFLAGS="-pthread $CFLAGS"
      LIBS="-levent_pthreads -levent $LIBS"
      AC_LANG_PUSH([C])
      AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <event2/thread.h>
          ]],[[
          evthread_use_pthreads();
          ]])],
        [ax_cv_libevent2_pthreads=yes],
        [ax_cv_libevent2_pthreads=no],
        [AC_MSG_WARN([test program execution failed])])
      AC_LANG_POP([C])
      AX_RESTORE_FLAGS
      ])

    AS_IF([test "x$ax_cv_libevent2_pthreads" = "xyes"],
        [AC_SUBST([LIBEVENT2_PTHREADS_LIB],[-levent_pthreads])
        AC_DEFINE([HAVE_LIBEVENT2_PTHREADS],[1],[Define if evthread_use_pthreads is present in event2/thread.h.])],
        [AC_DEFINE([HAVE_LIBEVENT2_PTHREADS],[0],[Define if evthread_use_pthreads is present in event2/thread.h.])])

    AM_CONDITIONAL([HAVE_LIBEVENT2_PTHREADS],[test "x$ax_cv_libevent2_pthreads" = xyes])
# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
    AS_IF([test "x$ax_cv_libevent2_pthreads" = xyes],
        [$1],
        [$2])
    ])dnl AX_LIBEVENT2_PTHREADS
