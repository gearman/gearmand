ARG version=36
FROM fedora:${version}

MAINTAINER gearmand

ARG version
ARG GEARMAN_REPO=https://github.com/gearman/gearmand

LABEL description="Gearman Job Server Image (Fedora ${version})"
LABEL maintainer="Gearmand Developers https://github.com/gearman/gearmand"
LABEL version="https://github.com/gearman/gearmand/tree/master Fedora ${version}"

# Install packages
RUN dnf makecache \
 && dnf update -y \
 && dnf install -y \
	rpm-build \
	git \
	libtool \
	autoconf \
	automake \
	make \
	gettext-devel \
	boost-devel \
	boost-thread \
	chrpath \
	gcc-c++ \
	gperf \
	gperftools-devel \
	hiredis-devel \
	libevent-devel \
	libmemcached-devel \
	libpq-devel \
	libuuid-devel \
	mariadb-connector-c-devel \
	memcached \
	openssl-devel \
	sqlite-devel \
	tokyocabinet-devel \
	zlib-devel \
	python3-sphinx \
 && dnf clean all \
 && rm -rf /var/cache/yum

# Switch to a non-root user
RUN adduser -M --shell /bin/bash gearman
USER gearman

# Clone the GitHub repository master branch
RUN cd /tmp && git clone --depth 1 --branch master ${GEARMAN_REPO}.git

# Bootstrap, configure, make, and make test
WORKDIR /tmp/gearmand
RUN ./bootstrap.sh -a
RUN ./configure --enable-ssl 2>&1 | tee ./configure.log
RUN make 2>&1 | tee ./build.log
RUN make test 2>&1 | tee ./test.log
