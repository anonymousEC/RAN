#!/bin/sh
echo "kill pnetdevice..."
CURRENT_DIR=$(pwd)
IP_PREFIX=192.168.7.

printf "root@192.168.7.%d\n" $1
ssh root@$IP_PREFIX$1 "$CURRENT_DIR/kill_pnetdevice.sh $CURRENT_DIR";
