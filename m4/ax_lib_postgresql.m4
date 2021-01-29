# ===========================================================================
#    https://www.gnu.org/software/autoconf-archive/ax_lib_postgresql.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LIB_POSTGRESQL([MINIMUM-VERSION])
#
# DESCRIPTION
#
#   This macro provides tests of availability of PostgreSQL 'libpq' library
#   of particular version or newer.
#
#   AX_LIB_POSTGRESQL macro takes only one argument which is optional. If
#   there is no required version passed, then macro does not run version
#   test.
#
#   The --with-postgresql option takes one of three possible values:
#
#   no - do not check for PostgreSQL client library
#
#   yes - do check for PostgreSQL library in standard locations (pg_config
#   should be in the PATH)
#
#   path - complete path to pg_config utility, use this option if pg_config
#   can't be found in the PATH (You could set also PG_CONFIG variable)
#
#   This macro calls:
#
#     AC_SUBST(POSTGRESQL_CPPFLAGS)
#     AC_SUBST(POSTGRESQL_LDFLAGS)
#     AC_SUBST(POSTGRESQL_LIBS)
#     AC_SUBST(POSTGRESQL_VERSION)
#
#   And sets:
#
#     HAVE_POSTGRESQL
#
# LICENSE
#
#   Copyright (c) 2008 Mateusz Loskot <mateusz@loskot.net>
#   Copyright (c) 2014 Sree Harsha Totakura <sreeharsha@totakura.in>
#   Copyright (c) 2018 Bastien Roucaries <rouca@debian.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 16

AC_DEFUN([_AX_LIB_POSTGRESQL_OLD],[
        AC_CACHE_CHECK([for the pg_config program], [ac_cv_path_PG_CONFIG],
            [AC_PATH_PROGS_FEATURE_CHECK([PG_CONFIG], [pg_config],
              [[$ac_path_PG_CONFIG --includedir > /dev/null \
                && ac_cv_path_PG_CONFIG=$ac_path_PG_CONFIG ac_path_PG_CONFIG_found=:]],
              [AC_MSG_FAILURE([could not find pg_config program. You could specify it throught variable PG_CONFIG])])])
	PG_CONFIG=$ac_cv_path_PG_CONFIG

	AC_CACHE_CHECK([for the PostgreSQL libraries CPPFLAGS],[ac_cv_POSTGRESQL_CPPFLAGS],
		       [ac_cv_POSTGRESQL_CPPFLAGS="-I`$PG_CONFIG --includedir`"])
	POSTGRESQL_CPPFLAGS="$ac_cv_POSTGRESQL_CPPFLAGS"

	AC_CACHE_CHECK([for the PostgreSQL libraries LDFLAGS],[ac_cv_POSTGRESQL_LDFLAGS],
		       [ac_cv_POSTGRESQL_LDFLAGS="-L`$PG_CONFIG --libdir`"])
	POSTGRESQL_LDFLAGS="$ac_cv_POSTGRESQL_LDFLAGS"

	AC_CACHE_CHECK([for the PostgreSQL libraries LIBS],[ac_cv_POSTGRESQL_LIBS],
		       [ac_cv_POSTGRESQL_LIBS="-lpq"])
	POSTGRESQL_LIBS="$ac_cv_POSTGRESQL_LIBS"

	AC_CACHE_CHECK([for the PostgreSQL version],[ac_cv_POSTGRESQL_VERSION],
	               [ac_cv_POSTGRESQL_VERSION=`$PG_CONFIG --version | sed "s/^PostgreSQL[[[:space:]]][[[:space:]]]*\([[0-9.]][[0-9.]]*\).*/\1/"`])
        POSTGRESQL_VERSION=$ac_cv_POSTGRESQL_VERSION
	found_postgresql="yes"
])

AC_DEFUN([AX_LIB_POSTGRESQL],
[
    AC_ARG_WITH([postgresql],
	AS_HELP_STRING([--with-postgresql=@<:@ARG@:>@],
	    [use PostgreSQL library @<:@default=yes@:>@, optionally specify path to pg_config]
	),
	[
	AS_CASE([$withval],
		[[[nN]][[oO]]],[want_postgresql="no"],
		[[[yY]][[eE]][[sS]]],[want_postgresql="yes"],
		[
			want_postgresql="yes"
			PG_CONFIG="$withval"
		])
	],
	[want_postgresql="yes"]
    )
    found_postgresql="no"

    POSTGRESQL_CPPFLAGS=""
    POSTGRESQL_LDFLAGS=""
    POSTGRESQL_LIBS=""
    POSTGRESQL_VERSION=""

    dnl
    dnl Check PostgreSQL libraries (libpq)
    dnl
    AS_IF([test X"$want_postgresql" = "Xyes"],
	  [_AX_LIB_POSTGRESQL_OLD])

    AS_IF([test X"$found_postgresql" = Xyes],[
    	dnl try to compile
    	_AX_LIB_POSTGRESQL_OLD_CPPFLAGS="$CPPFLAGS"
    	CPPFLAGS="$CPPFLAGS $POSTGRESQL_CPPFLAGS"
	_AX_LIB_POSTGRESQL_OLD_LDFLAGS="$LDFLAGS"
    	LDFLAGS="$LDFLAGS $POSTGRESQL_LDFLAGS"
	_AX_LIB_POSTGRESQL_OLD_LIBS="$LIBS"
    	LIBS="$LIBS $POSTGRESQL_LIBS"
    	AC_CHECK_HEADER([libpq-fe.h],[],[AC_MSG_FAILURE([could not find libpq-fe.h header])])
	
	dnl try now to link
	AC_CACHE_CHECK([for the PostgreSQL library linking is working],[ac_cv_postgresql_found],
	  [
	    AC_LINK_IFELSE([
		AC_LANG_PROGRAM(
		  [
		   #include <libpq-fe.h>
		  ],
		  [
		    char conninfo[]="dbname = postgres";
		    PGconn     *conn;
		    conn = PQconnectdb(conninfo);
		  ]
		)
	     ],[ac_cv_postgresql_found=yes],[AC_MSG_FAILURE([could not find libpq-fe.h header])])
	  ]
	)
    	CPPFLAGS="$_AX_LIB_POSTGRESQL_OLD_CPPFLAGS"
	LDFLAGS="$_AX_LIB_POSTGRESQL_OLD_LDFLAGS"
	LIBS="$_AX_LIB_POSTGRESQL_OLD_LIBS"
	])

    dnl
    dnl Check if required version of PostgreSQL is available
    dnl


    postgresql_version_req=ifelse([$1], [], [], [$1])

    if test "$found_postgresql" = "yes" -a -n "$postgresql_version_req"; then
        AC_MSG_CHECKING([if PostgreSQL version $POSTGRESQL_VERSION is >= $postgresql_version_req])
        AX_COMPARE_VERSION([$POSTGRESQL_VERSION],[ge],[$postgresql_version_req],
	                 [found_postgresql=yes],[found_postgresql=no])
	AC_MSG_RESULT([$found_postgresql])
    fi

    AS_IF([test "x$found_postgresql" = "xyes"],[
		AC_DEFINE([HAVE_POSTGRESQL], [1],
			  [Define to 1 if PostgreSQL libraries are available])])

    AC_SUBST([POSTGRESQL_VERSION])
    AC_SUBST([POSTGRESQL_CPPFLAGS])
    AC_SUBST([POSTGRESQL_LDFLAGS])
    AC_SUBST([POSTGRESQL_LIBS])
])
