#!/bin/sh
echo "start client..."
PARENT_DIR=$(dirname "$(pwd)")
BUILD_DIR=$PARENT_DIR/build
LOG_DIR=$PARENT_DIR/log

$BUILD_DIR/client $1 $2 $3  | tee $LOG_DIR/$(date +"%Y%m%d%H%M%S").log





