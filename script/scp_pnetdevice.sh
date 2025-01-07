#!/bin/sh
echo "scp send program and scripts..."
PARENT_DIR=$(dirname "$(pwd)")
BUILD_DIR=$PARENT_DIR/build
IP_PREFIX=192.168.7.

printf "scp $BUILD_DIR/pnetdevice to root@$IP_PREFIX%d\n" $1
scp -rp $BUILD_DIR/pnetdevice  root@$IP_PREFIX$1:$BUILD_DIR/pnetdevice


