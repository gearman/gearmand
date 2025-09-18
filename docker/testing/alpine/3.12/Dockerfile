ARG version=3.12
FROM alpine:${version}

ARG version
ARG GEARMAN_REPO=https://github.com/gearman/gearmand

LABEL description="Gearman Job Server Image (Alpine ${version})"
LABEL maintainer="Gearmand Developers https://github.com/gearman/gearmand"
LABEL version="${version}"

# Install packages
RUN apk add --no-cache \
	make \
	gcc \
	g++ \
	autoconf \
	automake \
	m4 \
	git \
	libtool \
	bash \
	file \
	py3-sphinx \
	util-linux-dev \
	libuuid \
	libevent-dev \
	gperf \
	boost-dev \
	openssl-dev

# Switch to a non-root user
RUN adduser --disabled-password --shell /bin/bash gearman
USER gearman

# Clone the GitHub repository master branch
RUN cd /tmp && git clone --depth 1 --branch master ${GEARMAN_REPO}.git

# Bootstrap, configure, make, and make test
WORKDIR /tmp/gearmand
RUN ./bootstrap.sh -a
RUN ./configure --enable-ssl 2>&1 | tee ./configure.log
RUN make 2>&1 | tee ./build.log
RUN make test 2>&1 | tee ./test.log
