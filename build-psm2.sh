#!/bin/sh
LIBFABRIC=/home/b/bzaztsch/apps/libfabric/1.2.0

LDFLAGS="-L$LIBFABRIC/lib -lfabric"
CXXFLAGS="-I$LIBFABRIC/include -std=c++11"

export LD_LIBRARY_PATH=$LIBFABRIC/lib:$LD_LIBRARY_PATH
#export FI_LOG_LEVEL=Info

echo "g++ $CXXFLAGS $LDFLAGS test-gni.cpp -o test-gni"
g++ $CXXFLAGS $LDFLAGS test-gni.cpp -o test-gni

./get_ips.rb > nodes

aprun -N1 -n2 ./test-gni
