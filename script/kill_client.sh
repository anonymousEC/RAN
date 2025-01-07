#!/bin/sh
echo "kill client..."
CURRENT_DIR=$(pwd)
PARENT_DIR=$(dirname "$CURRENT_DIR")
BUILD_DIR=$PARENT_DIR/build

kill -9 $(pidof client)
fuser -k $BUILD_DIR/client