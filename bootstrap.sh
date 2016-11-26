#!/usr/bin/env bash
# 
# Copyright (C) 2012-2013 Brian Aker
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
# 
#     * The names of its contributors may not be used to endorse or
# promote products derived from this software without specific prior
# written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Environment Variables that will influence the build:
#   AUTOMAKE
#   AUTORECONF
#   LIBTOOLIZE
#   MAKE
#   PREFIX
#   LOG_COMPILER
#   VERBOSE
#   WARNINGS
#

use_banner ()
{
  echo "#####################################################################################"
  echo "#"
  echo "#"
  echo "#"
  echo "#    TARGET:$1"
  echo "#"
  echo "#"
  echo "#"
  echo "#####################################################################################"
}

command_not_found_handle ()
{
  warn "$@: command not found"

  #if $DEBUG; then 
    echo ""
    echo "Stack trace:"
    local frame=0
    while caller $frame; do
      ((frame++));
    done
    echo ""
  #fi

  return 127
}

error ()
{ 
  echo "$BASH_SOURCE:$BASH_LINENO: $@" >&2
}

die ()
{ 
  echo "$BASH_SOURCE:$BASH_LINENO: $@" >&2
  exit 1; 
}

warn ()
{ 
  echo "$BASH_SOURCE:$BASH_LINENO: $@"
  #echo "$BASH_SOURCE:$BASH_LINENO: $@" >&1
}

nassert ()
{
  local param_name=\$"$1"
  local param_value="$(eval "expr \"$param_name\" ")"

  if [ -n "$param_value" ]; then
    echo "$bash_source:$bash_lineno: assert($param_name) had value of "$param_value"" >&2
    exit 1
  fi
}

assert ()
{
  local param_name=\$"$1"
  local param_value="$(eval "expr \"$param_name\" ")"

  if [ -z "$param_value" ]; then
    echo "$bash_source:$bash_lineno: assert($param_name)" >&2
    exit 1
  fi
}

assert_file ()
{
  if [ ! -f "$1" ]; then
    echo "$BASH_SOURCE:$BASH_LINENO: assert($1) does not exist: $2" >&2
    exit 1; 
  fi
}

assert_no_file ()
{
  if [ -f "$1" ]; then
    echo "$BASH_SOURCE:$BASH_LINENO: assert($1) file exists: $2" >&2
    exit 1;
  fi
}

assert_no_directory ()
{
  if [ -d "$1" ]; then
    echo "$BASH_SOURCE:$BASH_LINENO: assert($1) directory exists: $2" >&2
    exit 1;
  fi
}

assert_exec_file ()
{
  if [ ! -f "$1" ]; then
    echo "$BASH_SOURCE:$BASH_LINENO: assert($1) does not exist: $2" >&2
    exit 1;
  fi

  if [ ! -x "$1" ]; then
    echo "$BASH_SOURCE:$BASH_LINENO: assert($1) exists but is not executable: $2" >&2
    exit 1;
  fi
}

command_exists ()
{
  type "$1" &> /dev/null ;
}

rebuild_host_os ()
{
  HOST_OS="${UNAME_MACHINE_ARCH}-${VENDOR}-${VENDOR_DISTRIBUTION}-${VENDOR_RELEASE}-${UNAME_KERNEL}-${UNAME_KERNEL_RELEASE}"
  if [ -z "$1" ]; then
    if $verbose; then
      echo "HOST_OS=$HOST_OS"
    fi
  fi
}

# Validate the distribution name, or toss an erro
#  values: darwin,fedora,rhel,ubuntu,debian,opensuse
set_VENDOR_DISTRIBUTION ()
{
  local dist="$(echo "$1" | tr '[:upper:]' '[:lower:]')"
  case "$dist" in
    darwin)
      VENDOR_DISTRIBUTION='darwin'
      ;;
    fedora)
      VENDOR_DISTRIBUTION='fedora'
      ;;
    rhel)
      VENDOR_DISTRIBUTION='rhel'
      ;;
    debian)
      VENDOR_DISTRIBUTION='debian'
      ;;
    ubuntu)
      VENDOR_DISTRIBUTION='ubuntu'
      ;;
    suse)
      VENDOR_DISTRIBUTION='opensuse'
      ;;
    opensuse)
      VENDOR_DISTRIBUTION='opensuse'
      ;;
    *)
      die "attempt to set an invalid VENDOR_DISTRIBUTION=$dist"
      ;;
  esac
}

# Validate a Vendor's release name/number 
set_VENDOR_RELEASE ()
{
  local release="$(echo "$1" | tr '[:upper:]' '[:lower:]')"

  if $verbose; then 
    echo "VENDOR_DISTRIBUTION:$VENDOR_DISTRIBUTION"
    echo "VENDOR_RELEASE:$release"
  fi

  case $VENDOR_DISTRIBUTION in
    darwin)
      case $release in
        10.6*)
          VENDOR_RELEASE='snow_leopard'
          ;;
        10.7*)
          VENDOR_RELEASE='mountain'
          ;;
        mountain)
          VENDOR_RELEASE='mountain'
          ;;
        10.8.*)
          echo "mountain_lion"
          VENDOR_RELEASE='mountain_lion'
          ;;
        10.9)
          echo "mavericks"
          VENDOR_RELEASE='mavericks'
          ;;
        10.9.*)
          echo "mavericks"
          VENDOR_RELEASE='mavericks'
          ;;
        *)
          echo $release
          VENDOR_RELEASE='unknown'
          ;;
      esac
      ;;
    fedora)
      VENDOR_RELEASE="$release"
      if [[ "x$VENDOR_RELEASE" == '18' ]]; then
        VENDOR_RELEASE='sphericalcow'
      fi
      ;;
    rhel)
      VENDOR_RELEASE="$release"
      ;;
    debian)
      VENDOR_RELEASE="$release"
      ;;
    ubuntu)
      VENDOR_RELEASE="$release"
      if [[ "x$VENDOR_RELEASE" == 'x12.04' ]]; then
        VENDOR_RELEASE="precise"
      elif [[ "x$VENDOR_RELEASE" == 'x12.10' ]]; then
        VENDOR_RELEASE="quantal"
      elif [[ "x$VENDOR_RELEASE" == 'x13.04' ]]; then
        VENDOR_RELEASE="raring"
      fi
      ;;
    opensuse)
      VENDOR_RELEASE="$release"
      ;;
    unknown)
      die "attempt to set VENDOR_RELEASE without setting VENDOR_DISTRIBUTION"
      ;;
    *)
      die "attempt to set with an invalid VENDOR_DISTRIBUTION=$VENDOR_DISTRIBUTION"
      ;;
  esac
}


#  Valid values are: apple, redhat, centos, canonical, oracle, suse
set_VENDOR ()
{
  local vendor="$(echo "$1" | tr '[:upper:]' '[:lower:]')"

  case $vendor in
    apple)
      VENDOR='apple'
      ;;
    redhat)
      VENDOR='redhat'
      ;;
    fedora)
      VENDOR='redhat'
      ;;
    redhat-release-server-*)
      VENDOR='redhat'
      ;;
    enterprise-release-*)
      VENDOR='oracle'
      ;;
    centos)
      VENDOR='centos'
      ;;
    canonical)
      VENDOR='canonical'
      ;;
    ubuntu)
      VENDOR='canonical'
      ;;
    debian)
      VENDOR='debian'
      ;;
    opensuse)
      VENDOR='suse'
      ;;
    suse)
      VENDOR='suse'
      ;;
    *)
      die "An attempt was made to set an invalid VENDOR=$vendor"
      ;;
  esac

  set_VENDOR_DISTRIBUTION "$2"
  set_VENDOR_RELEASE "$3"

  # Set which vendor/versions we trust for autoreconf
  case $VENDOR_DISTRIBUTION in
    fedora)
      if [[ "x$VENDOR_RELEASE" == 'x18' ]]; then
        AUTORECONF_REBUILD_HOST=true
      elif [[ "x$VENDOR_RELEASE" == 'xsphericalcow' ]]; then
        AUTORECONF_REBUILD_HOST=true
      elif [[ "x$VENDOR_RELEASE" == 'x19' ]]; then
        AUTORECONF_REBUILD_HOST=true
      fi
      ;;
    canonical)
      if [[ "x$VENDOR_RELEASE" == 'xprecise' ]]; then
        AUTORECONF_REBUILD_HOST=true
      elif [[ "x$VENDOR_RELEASE" == 'xquantal' ]]; then
        AUTORECONF_REBUILD_HOST=true
      fi
      ;;
  esac

}

determine_target_platform ()
{
  UNAME_MACHINE_ARCH="$(uname -m 2>/dev/null)" || UNAME_MACHINE_ARCH=unknown
  UNAME_KERNEL="$(uname -s 2>/dev/null)"  || UNAME_SYSTEM=unknown
  UNAME_KERNEL_RELEASE="$(uname -r 2>/dev/null)" || UNAME_KERNEL_RELEASE=unknown

  if [[ -x '/usr/bin/sw_vers' ]]; then 
    local _VERSION="$(/usr/bin/sw_vers -productVersion)"
    set_VENDOR 'apple' 'darwin' $_VERSION
  elif [[ $(uname) == 'Darwin' ]]; then
    set_VENDOR 'apple' 'darwin' 'mountain'
  elif [[ -f '/etc/fedora-release' ]]; then 
    local fedora_version="$(awk ' { print $3 } ' < /etc/fedora-release)"
    set_VENDOR 'redhat' 'fedora' $fedora_version
  elif [[ -f '/etc/centos-release' ]]; then
    local centos_version="$(awk ' { print $7 } ' < /etc/centos-release)"
    set_VENDOR 'centos' 'rhel' $centos_version
  elif [[ -f '/etc/SuSE-release' ]]; then
    local suse_distribution="$(head -1 /etc/SuSE-release | awk ' { print $1 } ')"
    local suse_version="$(head -1 /etc/SuSE-release | awk ' { print $2 } ')"
    set_VENDOR 'suse' $suse_distribution $suse_version
  elif [[ -f '/etc/redhat-release' ]]; then
    local rhel_version="$(awk ' { print $7 } ' < /etc/redhat-release)"
    local _vendor="$(rpm -qf /etc/redhat-release)"
    set_VENDOR $_vendor 'rhel' $rhel_version
  elif [[ -f '/etc/os-release' ]]; then 
    source '/etc/os-release'
    set_VENDOR $ID $ID $VERSION_ID
  elif [[ -x '/usr/bin/lsb_release' ]]; then 
    local _ID="$(/usr/bin/lsb_release -s -i)"
    local _VERSION="$(/usr/bin/lsb_release -s -r)"
    set_VENDOR $_ID $_ID $_VERSION_ID
  elif [[ -f '/etc/lsb-release' ]]; then 
    source '/etc/lsb-release'
    set_VENDOR 'canonical' $DISTRIB_ID $DISTRIB_CODENAME
  fi

  rebuild_host_os
}

run_configure ()
{
  # We will run autoreconf if we are required
  run_autoreconf_if_required

  # We always begin at the root of our build
  if [ ! $? ]; then
    die "Programmer error, we entered run_configure with a stacked directory"
  fi

  if ! command_exists "$CONFIGURE"; then
    die "$CONFIGURE does not exist"
  fi

  local BUILD_DIR="$1"
  if [[ -n "$BUILD_DIR" ]]; then
    rm -r -f $BUILD_DIR
    mkdir -p $BUILD_DIR
  fi

  # Arguments for configure
  local BUILD_CONFIGURE_ARG='' 

  # If debug is set we enable both debug and asssert, otherwise we see if this is a VCS checkout and if so enable assert
  # Set ENV ASSERT in order to enable assert.
  # If we are doing a valgrind run, we always compile with assert disabled
  if $valgrind_run; then
    BUILD_CONFIGURE_ARG="--enable-assert=no $BUILD_CONFIGURE_ARG"
  else
    if $debug; then 
      BUILD_CONFIGURE_ARG="--enable-debug --enable-assert $BUILD_CONFIGURE_ARG"
    elif [[ -n "$VCS_CHECKOUT" ]]; then
      BUILD_CONFIGURE_ARG="--enable-assert $BUILD_CONFIGURE_ARG"
    fi
  fi

  if [[ -n "$CONFIGURE_ARG" ]]; then 
    BUILD_CONFIGURE_ARG="$CONFIGURE_ARG $BUILD_CONFIGURE_ARG"
  fi

  if [[ -n "$PREFIX_ARG" ]]; then 
    BUILD_CONFIGURE_ARG="$PREFIX_ARG $BUILD_CONFIGURE_ARG"
  fi

  ret=1;
  # If we are executing on OSX use CLANG, otherwise only use it if we find it in the ENV
  case $HOST_OS in
    rhel-5*)
      command_exists 'gcc44' || die "Could not locate gcc44"
      run CC=gcc44 CXX=gcc44 $top_srcdir/configure "$BUILD_CONFIGURE_ARG" || die "Cannot execute CC=gcc44 CXX=gcc44 configure $BUILD_CONFIGURE_ARG"
      ret=$?
      ;;
    *)
      run $CONFIGURE "$BUILD_CONFIGURE_ARG"
      ret=$?
      ;;
  esac

  if [ $ret -ne 0 ]; then
    die "Could not execute $CONFIGURE $BUILD_CONFIGURE_ARG"
  fi

  if [ ! -f 'Makefile' ]; then
    die "Programmer error, configure was run but no Makefile existed after $CONFIGURE was run"
  fi
}

setup_gdb_command ()
{
  GDB_TMPFILE=$(mktemp /tmp/gdb.XXXXXXXXXX)
  echo 'set logging overwrite on' > "$GDB_TMPFILE"
  echo 'set logging on' >> "$GDB_TMPFILE"
  echo 'set environment LIBTEST_IN_GDB=1' >> "$GDB_TMPFILE"
  echo 'run' >> "$GDB_TMPFILE"
  echo 'thread apply all bt' >> "$GDB_TMPFILE"
  echo 'quit' >> "$GDB_TMPFILE"
  GDB_COMMAND="gdb -f -batch -x $GDB_TMPFILE"
}

setup_valgrind_command ()
{
  VALGRIND_PROGRAM="$(type -p valgrind)"
  if [[ -n "$VALGRIND_PROGRAM" ]]; then
    VALGRIND_COMMAND="$VALGRIND_PROGRAM --error-exitcode=1 --leak-check=yes --malloc-fill=A5 --free-fill=DE --xml=yes --xml-file=\"valgrind-%p.xml\""
  fi
}

save_BUILD ()
{
  if [[ -n "$OLD_CONFIGURE" ]]; then
    die "OLD_CONFIGURE($OLD_CONFIGURE) was set on push, programmer error!"
  fi

  if [[ -n "$OLD_CONFIGURE_ARG" ]]; then
    die "OLD_CONFIGURE_ARG($OLD_CONFIGURE_ARG) was set on push, programmer error!"
  fi

  if [[ -n "$OLD_PREFIX" ]]; then
    die "OLD_PREFIX($OLD_PREFIX) was set on push, programmer error!"
  fi

  if [[ -n "$OLD_MAKE" ]]; then
    die "OLD_MAKE($OLD_MAKE) was set on push, programmer error!"
  fi

  if [[ -n "$OLD_LOG_COMPILER" ]]; then
    die "OLD_LOG_COMPILER($OLD_LOG_COMPILER) was set on push, programmer error!"
  fi

  if [[ -n "$CONFIGURE" ]]; then
    OLD_CONFIGURE=$CONFIGURE
  fi

  if [[ -n "$CONFIGURE_ARG" ]]; then
    OLD_CONFIGURE_ARG=$CONFIGURE_ARG
  fi

  if [[ -n "$MAKE" ]]; then
    OLD_MAKE=$MAKE
  fi

  if [[ -n "$LOG_COMPILER" ]]; then
    OLD_LOG_COMPILER=$LOG_COMPILER
  fi
}

restore_BUILD ()
{
  if [[ -n "$OLD_CONFIGURE" ]]; then
    CONFIGURE=$OLD_CONFIGURE
  fi

  if [[ -n "$OLD_CONFIGURE_ARG" ]]; then
    CONFIGURE_ARG=$OLD_CONFIGURE_ARG
  fi

  if [[ -n "$OLD_PREFIX" ]]; then
    PREFIX_ARG=$OLD_PREFIX
  fi

  if [[ -n "$OLD_MAKE" ]]; then
    MAKE=$OLD_MAKE
  fi

  if [[ -n "$OLD_LOG_COMPILER" ]]; then
    LOG_COMPILER=$OLD_LOG_COMPILER
  fi

  OLD_CONFIGURE=
  OLD_CONFIGURE_ARG=
  OLD_PREFIX=
  OLD_MAKE=
  OLD_LOG_COMPILER=

  export -n CC CXX
}

make_valgrind ()
{
  # If the env VALGRIND_COMMAND is set then we assume it is valid
  local valgrind_was_set=false
  if [[ -z "$VALGRIND_COMMAND" ]]; then
    setup_valgrind_command
    if [[ -n "$VALGRIND_COMMAND" ]]; then
      valgrind_was_set=true
    fi
  else
    valgrind_was_set=true
  fi

  # If valgrind_was_set is set to no we bail
  if ! $valgrind_was_set; then
    echo 'valgrind was not present'
    return 1
  fi

  save_BUILD

  valgrind_run=true

  # If we are required to run configure, do so now
  run_configure

  # If we don't have a configure, then most likely we will be missing libtool
  assert_file 'configure'
  if [[ -x 'libtool' ]]; then
    LOG_COMPILER="./libtool --mode=execute $VALGRIND_COMMAND"
  else
    LOG_COMPILER="$VALGRIND_COMMAND"
  fi

  make_target 'all'
  make_target 'check'
  ret=$?

  valgrind_run=false

  restore_BUILD

  if [ "$ret" -ne 0 ]; then
    return 1
  fi
}

make_install_system ()
{
  local INSTALL_LOCATION="$(mktemp -d /tmp/XXXXXXXXXX)"

  save_BUILD
  PREFIX_ARG="--prefix=$INSTALL_LOCATION"

  if [ ! -d $INSTALL_LOCATION ] ; then
    die "ASSERT temp directory not found '$INSTALL_LOCATION'"
  fi

  run_configure #install_buid_dir

  make_target 'install'

  make_target 'installcheck'

  make_target 'uninstall'

  rm -r -f $INSTALL_LOCATION
  make 'distclean'

  if [ -f 'Makefile' ]; then
    die "ASSERT Makefile should not exist"
  fi

  restore_BUILD
}

make_darwin_malloc ()
{
  run_configure_if_required

  old_MallocGuardEdges=$MallocGuardEdges
  MallocGuardEdges=1
  old_MallocErrorAbort=$MallocErrorAbort
  MallocErrorAbort=1
  old_MallocScribble=$MallocScribble
  MallocScribble=1

  make_check

  MallocGuardEdges=$old_MallocGuardEdges
  MallocErrorAbort=$old_MallocErrorAbort
  MallocScribble=$old_MallocScribble
}

# This will reset our environment, and make sure built files are available.
make_for_snapshot ()
{
  # Lets make sure we have a clean environment
  assert_no_file 'Makefile'
  assert_no_file 'configure'
  assert_no_directory 'autom4te.cache'

  run_configure
  make_target 'all'
  make_target 'distclean'

  # We should have a configure, but no Makefile at the end of this exercise
  assert_no_file 'Makefile'
  assert_exec_file 'configure'
}

check_mingw ()
{
  command_exists 'mingw64-configure'
  ret=$?
  if [ "$ret" -ne 0 ]; then
    return 1
  fi

  command_exists 'mingw64-make'
  ret=$?
  if [ "$ret" -ne 0 ]; then
    return 1
  fi

  return 0
}

check_clang ()
{
  command_exists 'clang'
  ret=$?
  if [ "$ret" -ne 0 ]; then
    return 1
  fi

  return 0
}

check_clang_analyzer ()
{
  command_exists 'scan-build'
  ret=$?
  if [ "$ret" -ne 0 ]; then
    return 1
  fi

  return 0
}

make_skeleton ()
{
  run_configure
  ret=$?

  if [ $ret -eq 0 ]; then
    assert_file 'Makefile'

    make_target 'all' 'warn'
    ret=$?
    if [ $ret -ne 0 ]; then
      warn "$MAKE failed"
    else
      if [[ -n "$DISPLAY" ]]; then
        if command_exists 'wine'; then
          LOG_COMPILER='wine'
        fi
      fi

      if [[ -n "$LOG_COMPILER" ]]; then
        make_target 'check' 'warn' || warn "$MAKE check failed"
        ret=$?
      fi
    fi

    if $jenkins_build_environment; then
      make_target 'clean' 'warn'
    fi
  fi

  return $ret
}

make_for_mingw ()
{
  if ! check_mingw; then
    return 1
  fi

  # Make sure it is clean
  if [ -f Makefile -o -f configure ]; then
    make_maintainer_clean
  fi

  run_autoreconf

  save_BUILD

  CONFIGURE='mingw64-configure'
  MAKE='mingw64-make'
  CONFIGURE_ARGS='--enable-static --disable-shared'

  make_skeleton
  ret=$?

  restore_BUILD

  return $ret
}

make_for_clang ()
{
  if ! check_clang; then
    return 1
  fi

  # Make sure it is clean
  if [ -f Makefile -o -f configure ]; then
    make_maintainer_clean
  fi

  run_autoreconf

  save_BUILD

  CC=clang CXX=clang++
  export CC CXX

  make_skeleton
  ret=$?

  make_target 'check'

  restore_BUILD

  return $ret
}

make_for_clang_analyzer ()
{
  if ! check_clang; then
    return 1
  fi

  if ! check_clang_analyzer; then
    die 'clang-analyzer was not found'
  fi

  # Make sure it is clean
  if [ -f Makefile -o -f configure ]; then
    make_maintainer_clean
  fi

  run_autoreconf

  save_BUILD

  CC=clang CXX=clang++
  export CC CXX
  CONFIGURE='scan-build ./configure'
  CONFIGURE_ARGS='--enable-debug'

  run_configure

  scan-build -o clang-html make -j4 -k

  restore_BUILD
}

# If we are locally testing, we should make sure the environment is setup correctly
check_for_jenkins ()
{
  if ! $jenkins_build_environment; then
    echo "Not inside of jenkins, simulating environment"

    if [ -f 'configure' ]; then
      make_maintainer_clean
    fi

    if $BOOTSTRAP_SNAPSHOT; then
      make_for_snapshot
    fi
  fi
}

make_universe ()
{
  use_banner 'make maintainer-clean'
  make_maintainer_clean

  use_banner 'snapshot'
  make_for_snapshot

  use_banner 'valgrind'
  make_valgrind

  use_banner 'gdb'
  make_gdb

  use_banner 'rpm'
  make_rpm

  use_banner 'clang'
  make_for_clang

  use_banner 'clang analyzer'
  make_for_clang_analyzer

  use_banner 'mingw'
  check_mingw
  if [ $? -eq 0 ]; then
    make_for_mingw
  fi

  use_banner 'make distcheck'
  make_distcheck

  use_banner 'make install'
  make_install_system
}

check_snapshot ()
{
  if [ -n "$BOOTSTRAP_SNAPSHOT_CHECK" ]; then
    assert_file "$BOOTSTRAP_SNAPSHOT_CHECK" 'snapshot check failed'
  fi
}

make_for_continuus_integration ()
{
  # Setup the environment if we are local
  check_for_jenkins

  # No matter then evironment, we should not have a Makefile at this point
  assert_no_file 'Makefile'

  # Platforms which require bootstrap should have some setup done before we hit this stage.
  # If we are building locally, skip this step, unless we are just testing locally. 
  if $BOOTSTRAP_SNAPSHOT; then
    if $BOOTSTRAP_SNAPSHOT; then
      assert_file 'configure'
    fi

    check_snapshot
  else
    # If we didn't require a snapshot, then we should not have a configure
    assert_no_file 'configure'

    run_autoreconf
  fi

  assert_no_file 'Makefile' 'Programmer error, Makefile existed where build state should have been clean'

  case $HOST_OS in
    *)
      make_jenkins_default
      ;;
  esac

  make_maintainer_clean
}

# The point to this test is to test bootstrap.sh itself
self_test ()
{
  # We start off with a clean env
  make_maintainer_clean

#  eval "./bootstrap.sh jenkins" || die "failed 'jenkins'"
#  eval "./bootstrap.sh all" || die "failed 'all'"
#  eval "./bootstrap.sh gdb" || die "failed 'gdb'"
#  eval "./bootstrap.sh maintainer-clean" || die "failed 'maintainer-clean'"
}

make_install_html ()
{
  run_configure_if_required
  assert_file 'configure'

  make_target 'install-html'
}

make_gdb ()
{
  save_BUILD

  if command_exists 'gdb'; then
    run_configure_if_required

    # Set ENV GDB_COMMAND
    if [[ -z "$GDB_COMMAND" ]]; then
      setup_gdb_command
    fi

    # If we don't have a configure, then most likely we will be missing libtool
    assert_file 'configure'
    if [[ -f 'libtool' ]]; then
      LOG_COMPILER="./libtool --mode=execute $GDB_COMMAND"
    else
      LOG_COMPILER="$GDB_COMMAND"
    fi

    make_target 'check'

    if [ -f 'gdb.txt' ]; then
      rm 'gdb.txt'
    fi

    if [ -f '.gdb_history' ]; then
      rm '.gdb_history'
    fi

    if $jenkins_build_environment; then
      make_target 'clean'
    fi
  else
    echo 'gdb was not present'
    return 1
  fi

  restore_BUILD
}

# $1 target to compile
# $2 to die, or not to die, based on contents
make_target ()
{
  if [ -z "$1" ]; then
    die "Programmer error, no target provided for make"
  fi

  if [ ! -f 'Makefile' ]; then
    die "Programmer error, make was called before configure"
    run_configure
  fi

  if [ -n "$LOG_COMPILER" ]; then
    if $verbose; then
      echo "LOG_COMPILER=$LOG_COMPILER"
    fi
  fi

  if [ -z "$MAKE" ]; then
    die "MAKE was not set"
  fi

  # $2 represents error or warn
  run "$MAKE" "$1"
  ret=$?

  if [ $ret -ne 0 ]; then
    if [ -n "$2" ]; then
      warn "Failed to execute $MAKE $1: $ret"
    elif [ $ret -eq 2 ]; then
      die "Failed to execute $MAKE $1"
    else
      die "Failed to execute $MAKE $1: $ret"
    fi
  fi

  return $ret
}

make_distcheck ()
{
  make_target 'distcheck'
}

make_rpm ()
{
  if command_exists 'rpmbuild'; then
    if [ -f 'rpm.am' -o -d 'rpm' ]; then
      run_configure_if_required
      make_target 'dist-rpm'

      return $?
    fi
  fi

  return 1
}

make_maintainer_clean ()
{
  run_configure_if_required
  make_target 'maintainer-clean' 'no_error'

  # Lets make sure we really cleaned up the environment
  assert_no_file 'Makefile'
  assert_no_file 'configure'
  assert_no_directory 'autom4te.cache'
}

make_distclean ()
{
  run_configure_if_required
  make_target 'distclean' 'no_error'

  # Lets make sure we really cleaned up the environment
  assert_no_file 'Makefile'
  assert_file 'configure'
}

make_check ()
{
  make_target 'check'
}

make_jenkins_default ()
{
  run_configure
  make_target 'all'
}

make_default ()
{
  run_configure_if_required
  make_target 'all'
}

run_configure_if_required () 
{
  run_autoreconf_if_required

  if [ ! -f 'Makefile' ]; then
    run_configure
  fi

  assert_file 'Makefile' 'configure did not produce a Makefile'
}

run_make_maintainer_clean_if_possible () 
{
  if [ -f 'Makefile' ]; then
    make_maintainer_clean
  fi
}

run_autoreconf_if_required () 
{
  if [ ! -x 'configure' ]; then
    run_autoreconf
  fi

  assert_exec_file 'configure'
  bash -n configure
}

run_autoreconf () 
{
  if [[ -z "$AUTORECONF" ]]; then
    die "Programmer error, tried to call run_autoreconf () but AUTORECONF was not set"
  fi

  if $use_libtool; then
    assert $BOOTSTRAP_LIBTOOLIZE
    if $jenkins_build_environment; then
      run "$BOOTSTRAP_LIBTOOLIZE" '--copy' '--install' || die "Cannot execute $BOOTSTRAP_LIBTOOLIZE"
    else
      run "$BOOTSTRAP_LIBTOOLIZE" '--copy' '--install' '--force' || die "Cannot execute $BOOTSTRAP_LIBTOOLIZE"
    fi
  fi

  run "$AUTORECONF" "$AUTORECONF_ARGS" || die "Cannot execute $AUTORECONF"

  eval 'bash -n configure' || die "autoreconf generated a malformed configure"
}

run ()
{
  if $verbose; then
    echo "\`$@' $ARGS"
  fi

  if [ -z "$1" ]; then
    return 127;
  fi

  eval "$@" "$ARGS"
} 

parse_command_line_options ()
{
  local SHORTOPTS=':apcmt:dvh'

  nassert OPT_TARGET

  while getopts "$SHORTOPTS" opt; do
    case $opt in
      a) #--autoreconf
        AUTORECONF_OPTION=true
        OPT_TARGET+='autoreconf'
        ;;
      p) #--print-env
        print_setup_opt=true
        ;;
      c) # --configure
        CONFIGURE_OPTION=true
        OPT_TARGET+='configure'
        ;;
      m) # maintainer-clean
        CLEAN_OPTION=true
        ;;
      o) # target
        CONFIGURE_ARG="$OPTARG"
        ;;
      t) # target
        TARGET_OPTION=true
        TARGET_OPTION_ARG="$OPTARG"
        OPT_TARGET+="$OPTARG"
        ;;
      d) # debug
        opt_debug=true
        enable_debug
        ;;
      h) # help
        echo "bootstrap.sh [options] optional_target ..."
        echo "  -a # Just run autoreconf";
        echo "  -p # Print ENV";
        echo "  -c # Just run configure";
        echo "  -m # Just run maintainer-clean";
        echo "  -o # Specify configure arguments";
        echo "  -t # Make target";
        echo "  -d # Enable debug";
        echo "  -h # Show help";
        echo "  -v # Be more verbose in output";
        exit
        ;;
      v) # verbose
        opt_verbose=true
        verbose=true
        ;;
      :)
        echo "Option -$OPTARG requires an argument." >&2
        exit 1
        ;;
      *)
        echo "$0: error - unrecognized option $1" 1>&2
        exit 1
        ;;
    esac
  done

  shift $((OPTIND-1))

  if [ -n "$1" ]; then
    OPT_TARGET="$@"
  fi
}

determine_vcs ()
{
  if [[ -d '.git' ]]; then
    VCS_CHECKOUT=git
  elif [[ -d '.bzr' ]]; then
    VCS_CHECKOUT=bzr
  elif [[ -d '.svn' ]]; then
    VCS_CHECKOUT=svn
  elif [[ -d '.hg' ]]; then
    VCS_CHECKOUT=hg
  else
    VCS_CHECKOUT=
  fi

  if [[ -n "$VCS_CHECKOUT" ]]; then
    verbose=true
  fi
}

require_libtoolise ()
{
  grep '^[         ]*A[CM]_PROG_LIBTOOL' configure.ac >/dev/null \
    && use_libtool=true
  grep '^[         ]*LT_INIT' configure.ac >/dev/null \
    && use_libtool=true
}

autoreconf_setup ()
{
  # Set ENV MAKE in order to override "make"
  if [[ -z "$MAKE" ]]; then 
    if command_exists 'gmake'; then
      MAKE="$(type -p gmake)"
    else
      if command_exists 'make'; then
        MAKE="$(type -p make)"
      fi
    fi
    
    if [ "$VCS_CHECKOUT" ]; then
      if $debug; then
        MAKE="$MAKE --warn-undefined-variables"
      fi
    fi

    if $debug; then
      MAKE="$MAKE -d"
    fi
  fi

  if [[ -z "$GNU_BUILD_FLAGS" ]]; then
    if $jenkins_build_environment; then
      GNU_BUILD_FLAGS="--install"
    else
      GNU_BUILD_FLAGS="--install --force"
    fi
  fi

  if $verbose; then
    GNU_BUILD_FLAGS="$GNU_BUILD_FLAGS --verbose"
  fi

  if [ -z "$ACLOCAL_PATH" ]; then
    ACLOCAL_PATH="/usr/local/share/aclocal $ACLOCAL_PATH"
  fi

  if [[ -z "$WARNINGS" ]]; then
    if [[ -n "$VCS_CHECKOUT" ]]; then
      WARNINGS="all,error"
    else
      WARNINGS="all"
    fi
  fi

  if $use_libtool; then
    if [[ -n "$LIBTOOLIZE" ]]; then
      BOOTSTRAP_LIBTOOLIZE="$(type -p $LIBTOOLIZE)"

      if [[ -z "$BOOTSTRAP_LIBTOOLIZE" ]]; then
        echo "Couldn't find user supplied libtoolize, it is required"
        return 1
      fi
    else
      # If we are using OSX, we first check to see glibtoolize is available
      if [[ "$VENDOR_DISTRIBUTION" == "darwin" ]]; then
        BOOTSTRAP_LIBTOOLIZE="$(type -p glibtoolize)"

        if [[ -z "$BOOTSTRAP_LIBTOOLIZE" ]]; then
          echo "Couldn't find glibtoolize, it is required on OSX"
          return 1
        fi
      else
        BOOTSTRAP_LIBTOOLIZE="$(type -p libtoolize)"

        if [[ -z "$BOOTSTRAP_LIBTOOLIZE" ]]; then
          echo "Couldn't find libtoolize, it is required"
          return 1
        fi
      fi
    fi

    if $verbose; then
      LIBTOOLIZE_OPTIONS="--verbose $BOOTSTRAP_LIBTOOLIZE_OPTIONS"
    fi

    if $debug; then
      LIBTOOLIZE_OPTIONS="--debug $BOOTSTRAP_LIBTOOLIZE_OPTIONS"
    fi

    # Here we set LIBTOOLIZE to true since we are going to invoke it via BOOTSTRAP_LIBTOOLIZE
    LIBTOOLIZE=true
  fi

  # Test the ENV AUTOMAKE if it exists
  if [[ -n "$AUTOMAKE" ]]; then
    run "$AUTOMAKE" '--help'    &> /dev/null    || die "Failed to run AUTOMAKE:$AUTOMAKE"
  fi

  # Test the ENV AUTOCONF if it exists
  if [[ -n "$AUTOCONF" ]]; then
    run "$AUTOCONF" '--help'    &> /dev/null    || die "Failed to run AUTOCONF:$AUTOCONF"
  fi

  # Test the ENV AUTOHEADER if it exists
  if [[ -n "$AUTOHEADER" ]]; then
    run "$AUTOHEADER" '--help'  &> /dev/null    || die "Failed to run AUTOHEADER:$AUTOHEADER"
  fi

  # Test the ENV AUTOM4TE if it exists
  if [[ -n "$AUTOM4TE" ]]; then
    run "$AUTOM4TE" '--help'    &> /dev/null    || die "Failed to run AUTOM4TE:$AUTOM4TE"
  fi

  # Test the ENV AUTOHEADER if it exists, if not we add one and add --install
  if [[ -z "$ACLOCAL" ]]; then
    ACLOCAL="aclocal --install"
  fi
  run "$ACLOCAL" '--help'  &> /dev/null    || die "Failed to run ACLOCAL:$ACLOCAL"

  if [[ -z "$AUTORECONF" ]]; then
    AUTORECONF="$(type -p autoreconf)"

    if [[ -z "$AUTORECONF" ]]; then
      die "Couldn't find autoreconf"
    fi

    if [[ -n "$GNU_BUILD_FLAGS" ]]; then
      AUTORECONF_ARGS="$GNU_BUILD_FLAGS"
    fi
  fi

  run "$AUTORECONF" '--help'  &> /dev/null    || die "Failed to run AUTORECONF:$AUTORECONF"
}

print_setup ()
{
  local saved_debug_status=$debug
  if $debug; then
    disable_debug
  fi

  echo '----------------------------------------------' 
  echo 'BOOTSTRAP ENV' 
  echo "AUTORECONF=$AUTORECONF"
  echo "HOST_OS=$HOST_OS"
  echo "VENDOR=$VENDOR"
  echo "VENDOR_DISTRIBUTION=$VENDOR_DISTRIBUTION"
  echo "VENDOR_RELEASE=$VENDOR_RELEASE"

  echo "getopt()"
  if $AUTORECONF_OPTION; then
    echo "--autoreconf"
  fi

  if $CLEAN_OPTION; then
    echo "--clean"
  fi

  if $CONFIGURE_OPTION; then
    echo "--configure"
  fi

  if $opt_debug; then
    echo "--debug"
  fi

  if $print_setup_opt; then
    echo "--print-env"
  fi

  if $TARGET_OPTION; then
    echo "--target=$TARGET_OPTION_ARG"
  fi

  if $opt_verbose; then
    echo "--verbose"
  fi

  if [[ -n "$MAKE" ]]; then
    echo "MAKE=$MAKE"
  fi

  if [[ -n "$BOOTSTRAP_TARGET" ]]; then
    echo "BOOTSTRAP_TARGET=$BOOTSTRAP_TARGET"
  fi

  if [[ -n "$PREFIX" ]]; then
    echo "PREFIX=$PREFIX"
  fi

  if [[ -n "$LOG_COMPILER" ]]; then
    echo "LOG_COMPILER=$LOG_COMPILER"
  fi

  if [[ -n "$VCS_CHECKOUT" ]]; then
    echo "VCS_CHECKOUT=$VCS_CHECKOUT"
  fi

  if $debug; then
    echo "debug=true"
  fi

  if [[ -n "$WARNINGS" ]]; then
    echo "WARNINGS=$WARNINGS"
  fi

  if $saved_debug_status; then
    echo "DEBUG=true"
  fi

  echo '----------------------------------------------' 

  if $saved_debug_status; then
    enable_debug
  fi
}

make_clean_option ()
{
  run_configure_if_required

  make_maintainer_clean

  if [[ "$VCS_CHECKOUT" == 'git' ]]; then
    run "$VCS_CHECKOUT" status --ignored
  elif [[ -n "$VCS_CHECKOUT" ]]; then
    run "$VCS_CHECKOUT" status
  fi
}

make_for_autoreconf ()
{
  if [ -f 'Makefile' ]; then
    make_maintainer_clean
  fi

  run_autoreconf

  assert_no_file 'Makefile'
}

check_make_target()
{
  local ret=0
  case $1 in
    'self')
      ;;
    'rpm')
      ;;
    'gdb')
      ;;
    'clean_op')
      ;;
    'autoreconf')
      ;;
    'install-system')
      ;;
    'configure')
      ;;
    'distcheck')
      ;;
    'check')
      ;;
    'snapshot')
      ;;
    'mingw')
      ;;
    'universe')
      ;;
    'valgrind')
      ;;
    'jenkins')
      ;;
    'distclean')
      ;;
    'maintainer-clean')
      ;;
    'install')
      ;;
    'all')
      ;;
    'make_default')
      ;;
    'clang')
      ;;
    'clang-analyzer')
      ;;
    test-*)
      ;;
    valgrind-*)
      ;;
    gdb-*)
      ;;
    'dist')
      ;;
    *)
      echo "Matched default"
      ret=1
      ;;
  esac

  return $ret
}

execute_job ()
{
  # We should always have a target by this point
  assert BOOTSTRAP_TARGET

  determine_target_platform

  determine_vcs

  # Set up whatever we need to do to use autoreconf later
  use_libtool=false
  require_libtoolise
  if ! autoreconf_setup; then
    return 1
  fi

  if $print_setup_opt -o  $debug; then
    echo 
    print_setup
    echo 

    # Exit if all we were looking for were the currently used options
    if $print_setup_opt; then
      exit
    fi
  fi

  # Use OLD_LOG_COMPILER for tracking the state of the variable
  local OLD_LOG_COMPILER=

  # Set ENV PREFIX in order to set --prefix for ./configure
  if [[ -n "$PREFIX" ]]; then 
    PREFIX_ARG="--prefix=$PREFIX"
  fi

  if $CLEAN_OPTION; then
    make_maintainer_clean
  fi

  local BOOTSTRAP_TARGET_ARRAY
  BOOTSTRAP_TARGET_ARRAY=( $BOOTSTRAP_TARGET )

  for target in "${BOOTSTRAP_TARGET_ARRAY[@]}"
  do
    # If we are running inside of Jenkins, we want to only run some of the possible tests
    if $jenkins_build_environment; then
      check_make_target $target
      ret=$?
      if [ $ret -ne 0 ]; then
        warn "Unknown BOOTSTRAP_TARGET option: $target"
        target="jenkins"
      fi
    fi

    if $jenkins_build_environment; then
      use_banner $target
    fi

    local snapshot_run=false
    local valgrind_run=false

    case $target in
      'self')
        self_test
        ;;
      'gdb')
        make_gdb
        ;;
      'install-html')
        make_install_html
        ;;
      'clean_op')
        make_clean_option
        ;;
      'autoreconf')
        make_for_autoreconf
        ;;
      'install-system')
        make_install_system
        ;;
      'configure')
        run_configure
        ;;
      'make_default')
        make_default
        ;;
      'clang')
        make_distclean
        if ! check_clang; then
          die "clang was not found"
        fi

        if ! make_for_clang; then
          die "Failed to build clang: $?"
        fi
        ;;
      'clang-analyzer')
        make_distclean
        if ! check_clang_analyzer; then
          die "clang-analyzer was not found"
        fi
        if ! check_clang; then
          die "clang was not found"
        fi

        if ! make_for_clang_analyzer; then
          die "Failed to build clang-analyzer: $?"
        fi
        ;;
      'mingw')
        make_distclean
        if ! make_for_mingw; then
          die "Failed to build mingw: $?"
        fi
        ;;
      'snapshot')
        make_for_snapshot
        snapshot_run=true
        check_snapshot
        ;;
      'rpm')
        make_rpm
        ;;
      'darwin_malloc')
        make_darwin_malloc
        ;;
      'valgrind')
        make_valgrind
        ;;
      'universe')
        make_universe
        ;;
      'jenkins')
        make_for_continuus_integration
        ;;
      *)
        run_configure_if_required
        make_target "$target"
        ;;
    esac
  done
}

main ()
{
  # Are we running inside of Jenkins?
  if [[ -n "$JENKINS_HOME" ]]; then 
    declare -r jenkins_build_environment=true
  else
    declare -r jenkins_build_environment=false
  fi

  # Variables we export
  declare -x VCS_CHECKOUT=

  # Variables we control globally
  local -a BOOTSTRAP_TARGET=
  local CONFIGURE=
  local use_libtool=false
  local verbose=false

  #getop variables
  local opt_debug=false
  local opt_verbose=false

  if [[ -n "$VERBOSE" ]]; then
    verbose=true
  fi

  # Options for getopt
  local AUTORECONF_OPTION=false
  local CLEAN_OPTION=false
  local CONFIGURE_OPTION=false
  local print_setup_opt=false
  local TARGET_OPTION=false
  local TARGET_OPTION_ARG=

  local OLD_CONFIGURE=
  local OLD_CONFIGURE_ARG=
  local OLD_PREFIX=
  local OLD_MAKE=
  local OLD_LOG_COMPILER=

  # If we call autoreconf on the platform or not
  local AUTORECONF_REBUILD_HOST=false
  local AUTORECONF_REBUILD=false

  local -r top_srcdir="$(pwd)"

  # Default configure
  if [ -z "$CONFIGURE" ]; then
    CONFIGURE="$top_srcdir/configure"
  fi


  # Variables for determine_target_platform () and rebuild_host_os ()
  #   UNAME_MACHINE_ARCH= uname -m
  #   VENDOR= apple, redhat, centos, canonical
  #   VENDOR_RELEASE=  
  #                  RHEL{rhel,Tikanga,Santiago}
  #                  Ubuntu{ubuntu,Lucid,Maverick,Natty,Oneiric,Precise,Quantal}
  #                  Fedora{fedora,Verne,Beefy}
  #                  OSX{osx,lion,snow,mountain}
  #   VENDOR_DISTRIBUTION= darwin,fedora,rhel,ubuntu
  #   UNAME_KERNEL= Linux, Darwin,...
  #   UNAME_KERNEL_RELEASE= Linux, Darwin,...
  local UNAME_MACHINE_ARCH=unknown
  local VENDOR=unknown
  local VENDOR_RELEASE=unknown
  local VENDOR_DISTRIBUTION=unknown
  local UNAME_KERNEL=unknown
  local UNAME_KERNEL_RELEASE=unknown
  local HOST_OS=

  rebuild_host_os no_output

  local OPT_TARGET=
  parse_command_line_options "$@"

  nassert BOOTSTRAP_TARGET

  if [ -n "$OPT_TARGET" ]; then
    BOOTSTRAP_TARGET="$OPT_TARGET"
  fi

  if [ -z "$BOOTSTRAP_TARGET" ]; then
    BOOTSTRAP_TARGET="make_default"
  fi

  # We should always have a target by this point
  assert BOOTSTRAP_TARGET

  execute_job
  local ret=$?

  jobs -l
  wait

  exit $ret
}

set_branch ()
{
  if [ -z "$BRANCH" ]; then 
    if [ -z "$CI_PROJECT_TEAM" ]; then 
      die "Variable CI_PROJECT_TEAM has not been set"
    fi
    if [ -z "$PROJECT" ]; then 
      die "Variable PROJECT has not been set"
    fi
    if [ -z "$BUILD_TAG" ]; then 
      die "Variable BUILD_TAG has not been set"
    fi

    BRANCH="lp:~$CI_PROJECT_TEAM/$PROJECT/$BUILD_TAG"
    export BRANCH
  fi

  if [ -z "$BRANCH" ]; then 
    die "Missing values required to build BRANCH variable."
  fi
}

merge ()
{
  if [ -z "$VCS_CHECKOUT" ]; then
    die "Merges require VCS_CHECKOUT."
  fi

  set_branch

  if [[ "$VCS_CHECKOUT" == 'bzr' ]]; then
    if test -n "$BRANCH_TO_MERGE"; then
      bzr merge $BRANCH_TO_MERGE
      bzr commit --message="Merge $BRANCH_TO_MERGE Build: $BUILD_TAG" --unchanged
    fi

    bzr push "$BRANCH"
  elif [[ -n "$VCS_CHECKOUT" ]]; then
    die "Merge attempt occured, current VCS setup does not support this"
  fi
}

enable_debug ()
{
  if ! $debug; then
    local caller_loc="$(caller)"
    if [[ -n "$1" ]]; then
      echo "$caller_loc Enabling debug: $1"
    else
      echo "$caller_loc Enabling debug"
    fi
    set -x
    debug=true
  fi
}

usage ()
{
  cat << EOF
  Usage: $program_name [OPTION]..

  Bootstrap this package from the checked-out sources, and optionally walk through CI run.

  Options:

EOF
}

disable_debug ()
{
  set +x
  debug=false
}

check_shell ()
{
  if [ -x '/usr/local/bin/shellcheck' ]; then
    /usr/local/bin/shellcheck "$1"
    local ret=$?

    if [ "$ret" -ne 0 ]; then
      die "$1 failed shellcheck"
    fi
  fi
}

bootstrap ()
{
  check_shell 'bootstrap.sh'
  local env_debug_enabled=false
  local debug=false

  export ACLOCAL
  export AUTOCONF
  export AUTOHEADER
  export AUTOM4TE
  export AUTOMAKE
  export AUTORECONF
  export CONFIGURE_ARG
  export DEBUG
  export GNU_BUILD_FLAGS
  export LIBTOOLIZE
  export LIBTOOLIZE_OPTIONS
  export MAKE
  export PREFIX_ARG
  export LOG_COMPILER
  export VERBOSE
  export WARNINGS

  case $OSTYPE in
    darwin*)
      export MallocGuardEdges
      export MallocErrorAbort
      export MallocScribble
      ;;
  esac

  # We check for DEBUG twice, once before we source the config file, and once afterward
  if [[ -n "$DEBUG" ]]; then
    env_debug_enabled=true
  fi

  # Variables which only can be set by .bootstrap
  BOOTSTRAP_SNAPSHOT=false
  BOOTSTRAP_SNAPSHOT_CHECK=

  if [ -f '.bootstrap' ]; then
    source '.bootstrap'
  fi

  # We do this in order to protect the case where DEBUG that came from the ENV (i.e. it overrides what is found in .bootstrap
  if $env_debug_enabled; then
    enable_debug
  elif [[ -n "$DEBUG" ]]; then
    enable_debug "Enabling DEBUG from '.bootstrap'"
  fi

  if $env_debug_enabled; then
    print_setup
  fi


  main "$@"
}

# Script begins here
declare -r program_name="$0"
bootstrap "$@"
