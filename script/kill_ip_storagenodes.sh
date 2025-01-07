#!/bin/sh
echo "kill $1 storagenodes..."
CURRENT_DIR=$(pwd)
IP_PREFIX=192.168.7.
ssh root@$IP_PREFIX$1 "$CURRENT_DIR/kill_storagenodes.sh $CURRENT_DIR"
