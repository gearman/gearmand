# OSS Scanner build for gearmand (https://github.com/anthropics/oss-scanner).
#
#   /src        default build (-O2 -g, _GLIBCXX_ASSERTIONS, SSL, sqlite3,
#               tokyocabinet, hiredis, libmemcached). Server: /src/gearmand/gearmand,
#               CLI: /src/bin/gearman, admin tool: /src/bin/gearadmin.
#   /src-asan   the same configuration built with AddressSanitizer. Use it to
#               confirm memory-safety findings.
#
# The scan runs without network access, so everything the test suite needs
# (redis-server, memcached, sqlite3) is installed here.
FROM debian:trixie
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates build-essential git pkg-config python3 procps \
      autoconf automake libtool gperf \
      libboost-program-options-dev libevent-dev uuid-dev libssl-dev openssl \
      libhiredis-dev redis-server \
      libsqlite3-dev sqlite3 \
      libtokyocabinet-dev \
      libmemcached-dev memcached \
      libcurl4-openssl-dev curl \
    && rm -rf /var/lib/apt/lists/*

# scanner contract: the checkout lives inside the image, at /src
COPY . /src
WORKDIR /src

ARG CONFIGURE_FLAGS="--enable-ssl --with-sqlite3 --enable-libtokyocabinet"

RUN ./bootstrap.sh -a && cp -a /src /src-asan

RUN cd /src-asan \
    && ./configure ${CONFIGURE_FLAGS} \
         CFLAGS="-g -O1 -fno-omit-frame-pointer -fsanitize=address" \
         CXXFLAGS="-g -O1 -fno-omit-frame-pointer -fsanitize=address" \
         LDFLAGS="-fsanitize=address" \
    && make -j"$(nproc)"

RUN ./configure ${CONFIGURE_FLAGS} \
      CFLAGS="-g -O2 -fno-omit-frame-pointer" \
      CXXFLAGS="-g -O2 -fno-omit-frame-pointer -Wp,-D_GLIBCXX_ASSERTIONS" \
    && make -j"$(nproc)"

ENV ASAN_OPTIONS=abort_on_error=1:symbolize=1:detect_leaks=0

# Some tests are timing sensitive and the suite expects "localhost" to resolve
# to 127.0.0.1 only, so failures here are reported but do not fail the build.
RUN timeout 1800 make test || echo "WARNING: make test reported failures"
