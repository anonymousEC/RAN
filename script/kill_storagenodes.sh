#!/bin/sh
echo "kill storagenodes..."
PARENT_DIR=$(dirname "$1")
BUILD_DIR=$PARENT_DIR/build

kill -9 $(pidof storagenodes)
fuser -k $BUILD_DIR/storagenodes