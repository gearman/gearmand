ARG version=18.04
FROM ubuntu:${version}

ARG version
ARG GEARMAN_REPO=https://github.com/gearman/gearmand

LABEL description="Gearman Job Server Image (Ubuntu ${version}/gcc-8)"
LABEL maintainer="Gearmand Developers https://github.com/gearman/gearmand"
LABEL version="https://github.com/gearman/gearmand/tree/master Ubuntu ${version}/gcc-8"

# Configure environment
ENV DEBIAN_FRONTEND=noninteractive \
    HOME=/root

# Install packages
RUN apt-get update \
 && apt-get -y upgrade \
 && apt-get -y install \
	automake \
	autoconf \
	libtool \
	make \
	curl \
	gcc-8 \
	g++-8 \
	git \
	gperf \
	libssl-dev \
	libboost-all-dev \
	libevent-dev \
	libhiredis-dev \
	libpq-dev \
	libtokyocabinet-dev \
	tcsh \
	python3-sphinx \
	uuid-dev \
	wget \
 && apt-get clean autoclean \
 && apt-get -y autoremove \
 && rm -rf /var/lib/apt/lists/*

# Switch to a non-root user
RUN adduser --no-create-home --disabled-password --gecos '' --shell /bin/bash gearman
USER gearman

# Clone the GitHub repository master branch
ENV HOME=/tmp
RUN cd /tmp && git clone --depth 1 --branch master ${GEARMAN_REPO}.git

# Bootstrap, configure, make, and make test
WORKDIR /tmp/gearmand
RUN ./bootstrap.sh -a
RUN env CC=gcc-8 CXX=g++-8 ./configure --enable-ssl 2>&1 | tee ./configure.log
RUN make 2>&1 | tee ./build.log
RUN make test 2>&1 | tee ./test.log
