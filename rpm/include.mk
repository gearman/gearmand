# vim:ft=automake

RPM_BUILDDIR= ~/rpmbuild
RPM_SOURCESDIR= $(RPM_BUILDDIR)/SOURCES

RPM_BUILD_TARGET= @PACKAGE@-@LIBGEARMAN_VERSION_STRING@-@RPM_RELEASE@.@build_cpu@.rpm
RPM_SOURCE= $(RPM_SOURCESDIR)/$(DIST_ARCHIVES)

RPMS=
RPMS+= $(RPM_BUILD_TARGET)
RPMS+= @PACKAGE@-devel-@LIBGEARMAN_VERSION_STRING@-@RPM_RELEASE@.@build_cpu@.rpm
# If you build Debug packages enable this
#RPMS+= @PACKAGE@-debuginfo-@LIBGEARMAN_VERSION_STRING@-@RPM_RELEASE@.@build_cpu@.rpm

SRPMS= @PACKAGE@-@LIBGEARMAN_VERSION_STRING@-@RPM_RELEASE@.src.rpm

RPM_DIST= $(RPMS) $(SRPMS)

BUILD_RPMS= $(foreach rpm_iterator,$(RPMS),$(addprefix $(RPM_BUILDDIR)/RPMS/@build_cpu@/, $(rpm_iterator)))
BUILD_SRPMS= $(foreach srpm_iterator,$(SRPMS),$(addprefix $(RPM_BUILDDIR)/SRPMS/, $(srpm_iterator)))
BUILD_RPM_DIR= $(RPM_BUILDDIR)/BUILD/@PACKAGE@-@VERSION@
BUILD_DAEMON_INIT= $(RPM_SOURCESDIR)/@PACKAGE@.init

$(RPM_BUILDDIR):
	@@RPMDEV_SETUPTREE@

$(BUILD_DAEMON_INIT): support/@PACKAGE@.init
	@cp $< $@

$(DIST_ARCHIVES): $(DISTFILES)
	$(MAKE) $(AM_MAKEFLAGS) dist-gzip

$(RPM_SOURCE): $(DIST_ARCHIVES) $(RPM_BUILDDIR)
	@cp $< $@

$(RPM_BUILD_TARGET): support/@PACKAGE@.spec $(RPM_SOURCE) $(BUILD_DAEMON_INIT)
	-@rm -f $(BUILD_RPMS) $(BUILD_SRPMS)
	-@rm -rf $(BUILD_RPM_DIR)
	@@RPMBUILD@ -ba $<
	@cp $(BUILD_RPMS) $(BUILD_SRPMS) .

.PHONY: rpm-sign
rpm-sign: $(RPM_BUILD_TARGET)
	@@RPM@ --addsign $(RPM_DIST)
	@@RPM@ --checksig $(RPM_DIST)

.PHONY: clean-rpm
clean-rpm:
	-@rm -f $(BUILD_RPMS) $(BUILD_SRPMS) $(BUILD_RPM_SOURCE) $(RPM_DIST) $(BUILD_DAEMON_INIT)
	-@rm -rf $(BUILD_RPM_DIR)

dist-rpm: $(RPM_BUILD_TARGET)

.PHONY: release
release: rpm rpm-sign

.PHONY: auto-rpmbuild
auto-rpmbuild: support/@PACKAGE@.spec
	@auto-br-rpmbuild -ba $<
