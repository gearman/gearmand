ARG version=7
FROM i386/centos:${version}

ARG version
ARG GEARMAN_REPO=https://github.com/gearman/gearmand

LABEL description="Gearman Job Server Image (CentOS ${version}/i686/32-bit)"
LABEL maintainer="Gearmand Developers https://github.com/gearman/gearmand"
LABEL version="https://github.com/gearman/gearmand/tree/master CentOS ${version}/i686/32-bit"

# Install packages
RUN yum install -y \
	rpm-build \
	make \
	boost-devel \
	boost-thread \
	gcc-c++ \
	gperf \
	gperftools-devel \
	libevent-devel \
	libmemcached-devel \
	memcached \
	libuuid-devel \
	openssl-devel \
	sqlite-devel \
	tokyocabinet-devel \
	zlib-devel \
	git \
	libtool \
	python-sphinx

### Not available:
# 	libpq-devel \
# 	hiredis-devel \
# 	mariadb-connector-c-devel \

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
