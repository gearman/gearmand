#!/bin/bash
set -eux
sudo apt-get update
sudo apt-get install -y \
    automake \
    build-essential \
    gperf \
    libboost-all-dev \
    libevent-dev \
    libhiredis-dev \
    libtool \
    python-sphinx \
    uuid-dev \

./bootstrap.sh -a
./configure
make
make test
