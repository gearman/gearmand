# vim:ft=automake
#
# Gearman server and library
# Copyright (C) 2011-2012 DataDifferential LLC, http://datadifferential.com/
# Copyright (C) 2008 Brian Aker, Eric Day
# All rights reserved.
#
# Use and distribution licensed under the BSD license.  See
# the COPYING file in this directory for full text.

ACLOCAL_AMFLAGS= -I m4

bin_PROGRAMS=
sbin_PROGRAMS= 
check_PROGRAMS=
noinst_PROGRAMS=
lib_LTLIBRARIES=
man_MANS=
noinst_LTLIBRARIES=
check_LTLIBRARIES=
nobase_include_HEADERS=
noinst_HEADERS=
DISTCLEANFILES=
MAINTAINERCLEANFILES=
EXTRA_DIST=
CLEANFILES=
BUILT_SOURCES=
TESTS=
XFAIL_TESTS=
dist_man_MANS=

if IS_VCS_CHECKOUT
SUBDIRS= docs
endif

#includes additional rules from aminclude.am
@INC_AMINCLUDE@
DISTCLEANFILES+= aminclude.am

# Should always be first
include libtest/include.am
include libhostile/include.am
include libhashkit-1.0/include.am

include docs/include.am
include man/include.am
include libgearmancore/include.am
include libhashkit/include.am
include benchmark/include.am
include bin/include.am
include examples/include.am
include gearmand/include.am
include libgearman-server/include.am
include libgearman/include.am
include libgearman-1.0/include.am
include util/include.am
include tests/include.am
include support/include.am

include m4/include.am
include rpm/include.mk

@GENERATE_CHANGELOG_RULES@
CHANGELOG_GIT_FLAGS = --date=short --pretty=tformat:"%+D - %ad %s" --no-merges
dist-hook: dist-ChangeLog

TESTS += $(bin_PROGRAMS)
TESTS += $(check_PROGRAMS)
XFAIL_TESTS += $(bin_PROGRAMS)

EXTRA_DIST+= HACKING
EXTRA_DIST+= README.md
EXTRA_DIST+= scripts/README.solaris

test: check

BUILT_SOURCES+= configmake.h
CLEANFILES+= configmake.h configmake.h-t
configmake.h: Makefile
	@rm -f $@-t $@
	@{ echo '/* DO NOT EDIT! GENERATED AUTOMATICALLY! */'; \
	  echo '#define SYSCONFDIR "$(sysconfdir)"'; \
	  echo '#define GEARMAND_CONFIG "$(sysconfdir)/gearmand.conf"'; \
	  echo '#define GEARMAND_CA_CERTIFICATE "$(sysconfdir)/ssl/certs/gearmand-ca.pem"'; \
	  echo '#define GEARMAND_SERVER_PEM "$(sysconfdir)/ssl/certs/gearmand.pem"'; \
	  echo '#define GEARMAND_SERVER_KEY "$(sysconfdir)/ssl/certs/gearmand.key"'; \
	  echo '#define GEARMAN_CLIENT_PEM "$(sysconfdir)/ssl/certs/gearman.pem"'; \
	  echo '#define GEARMAN_CLIENT_KEY "$(sysconfdir)/ssl/certs/gearman.key"'; \
	  echo '#define LOCALSTATEDIR "$(localstatedir)"'; \
	  echo '#define GEARMAND_PID "$(localstatedir)/gearmand.pid"'; \
	  } | sed '/""/d' > $@-t
	@if diff $@-t $@ >/dev/null 2>&1 ; then \
	  rm @-t ; \
	  else \
	  mv $@-t $@ ; \
	  fi

clean-local: clean-libtest-check clean-rpm

maintainer-clean-local:
	find . -type f -name '*~' -exec rm -f '{}' \;
	-$(RM) $(dist_man_MANS)
	-rm Makefile.in
	-rm aclocal.m4
	-rm build-aux/config.guess
	-rm build-aux/config.sub
	-rm build-aux/depcomp
	-rm build-aux/install-sh
	-rm build-aux/ltmain.sh
	-rm build-aux/missing
	-rm build-aux/test-driver 
	-rm gear_config.in
	-rm config.log
	-rm config.status
	-rm configure
	-rm m4/libtool.m4
	-rm m4/ltoptions.m4
	-rm m4/ltsugar.m4
	-rm m4/ltversion.m4
	-rm m4/lt~obsolete.m4
	-rmdir build-aux
	-rm -rf ${top_builddir}/html
	-rm -f $(DIST_ARCHIVES)

merge-clean:
	@find ./ | $(GREP) \.gcda | xargs rm -f
	@find ./ | $(GREP) \.gcno | xargs rm -f
	@find ./ | $(GREP) \.gz | xargs rm -f
	@find ./ | $(GREP) \.moved | xargs rm -r -f
	@find ./ | $(GREP) \\.orig | xargs rm -f
	@find ./ | $(GREP) \.rej | xargs rm -f
	@find ./ | $(GREP) \.THIS | xargs rm -f
	@find ./ | $(GREP) \.OTHER | xargs rm -f
	@find ./ | $(GREP) \.BASE | xargs rm -f
	@find ./ | $(GREP) \~$$ | xargs rm -f
	@echo "Files that need to be either removed or checked in:"
	@git clean -n
