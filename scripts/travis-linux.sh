sudo apt-get update -qq
COMPILER_PACKAGE=$CXX
if echo "$COMPILER_PACKAGE" | grep -q '^clang++-'; then
    COMPILER_PACKAGE=$CC
fi
sudo apt-get install -y libboost-all-dev gperf libevent-dev uuid-dev python-sphinx libhiredis-dev $COMPILER_PACKAGE
