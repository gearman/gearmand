sudo apt-get update -qq
COMPILER_PACKAGE=$CXX
sudo apt-get install -y libboost-all-dev gperf libevent-dev uuid-dev python-sphinx libhiredis-dev $COMPILER_PACKAGE
