#!/bin/sh
echo "kill pnetdevice..."
PARENT_DIR=$(dirname "$1")
BUILD_DIR=$PARENT_DIR/build

kill -9 $(pidof pnetdevice)
fuser -k $BUILD_DIR/pnetdevice