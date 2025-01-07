#!/bin/sh
echo "start pnetdevice..."
CURRENT_DIR=$(pwd)
IP_PREFIX=192.168.7.

printf "start pnetdevice: root@$IP_PREFIX%d\n" $1
ssh root@$IP_PREFIX$1 "$CURRENT_DIR/start_pnetdevice.sh $CURRENT_DIR"

