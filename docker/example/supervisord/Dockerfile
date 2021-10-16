FROM ubuntu:latest

ARG version=1.0

LABEL description="Supervisord Base Image for Gearmand"
LABEL maintainer="Gearmand Developers https://github.com/gearman/gearmand" 
LABEL version="${version}"

# Configure timezone
ENV DEBIAN_FRONTEND=noninteractive \
    TZ=America/New_York \
    HOME=/root
RUN (echo $TZ > /etc/timezone) \
 && dpkg-reconfigure tzdata

# Install packages
RUN apt-get update \
 && apt-get -y upgrade \
 && apt-get -y install \
	supervisor \
 && apt-get clean autoclean \
 && apt-get -y autoremove \
 && rm -rf /var/lib/apt/lists/*

# Configure service
RUN mkdir -p /var/log/supervisor \
 && mkdir -p /var/run/supervisor \
 && mkdir -p /etc/supervisor/conf.d

COPY supervisord.conf /etc/supervisor/supervisord.conf

RUN ln -s /etc/supervisor/supervisord.conf /etc/supervisord.conf \
 && groupadd supervisor \
 && chgrp supervisor /etc/supervisor/supervisord.conf \
 && chmod 640 /etc/supervisor/supervisord.conf

# Default command
CMD ["/usr/bin/supervisord", "-c", "/etc/supervisor/supervisord.conf"]
