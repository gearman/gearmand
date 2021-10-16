FROM gearmand/supervisord:latest

ARG version=1.1.19.1

LABEL description="Gearman Job Server Image"
LABEL maintainer="Gearmand Developers https://github.com/gearman/gearmand"
LABEL version="${version}"

# Install packages
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get -y upgrade \
 && apt-get -y install \
	make \
	gcc \
	g++ \
	gperf \
	libboost-all-dev \
	libevent-dev \
	libhiredis-dev \
	libssl-dev \
	libtokyocabinet-dev \
	uuid-dev \
	wget \
 && apt-get clean autoclean \
 && apt-get -y autoremove \
 && rm -rf /var/lib/apt/lists/*

# Retrieve the source code and untar
WORKDIR /var/lib/gearman
RUN wget https://github.com/gearman/gearmand/releases/download/${version}/gearmand-${version}.tar.gz \
 && tar xzvf gearmand-${version}.tar.gz \
 && rm -f gearmand-${version}.tar.gz

# Configure, make, install, and clean up
RUN cd gearmand-${version}/ \
 && ./configure --prefix=/usr --enable-ssl 2>&1 | tee ../configure.log \
 && make 2>&1 | tee ../build.log \
 && make test 2>&1 | tee ../test.log \
 && make install 2>&1 | tee ../install.log \
 && cp -p test-suite.log .. \
 && cd .. \
 && gzip -9 *.log \
 && rm -rf gearmand-${version}*

# Copy SSL certificates
###COPY gearmand-ca.pem gearmand.pem gearmand.key /var/lib/gearman/

# Configure service
RUN groupadd gearman \
 && useradd -r -d /var/lib/gearman -g gearman -G supervisor gearman
RUN touch /var/log/gearmand.log \
 && chown gearman:gearman /var/log/gearmand.log
COPY gearmand.conf /etc/supervisor/conf.d/gearmand.conf

# Links
RUN ln /usr/sbin/gearmand /sbin/gearmand \
 && ln /usr/bin/gearman /bin/gearman \
 && ln /usr/bin/gearadmin /bin/gearadmin

HEALTHCHECK --interval=5m --timeout=3s --retries=2 \
        CMD test $(supervisorctl status gearmand | awk '{print $2}' | grep 'RUNNING' | wc -l) -eq 1 || exit 1

EXPOSE 4730
