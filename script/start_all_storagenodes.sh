#!/bin/sh
echo "start all storagenodes..."
CURRENT_DIR=$(pwd)
IP_PREFIX=192.168.7.

for i in $(seq $1 $2)
do
{
    printf "start storagenodes: root@$IP_PREFIX%d\n" $i
    ssh root@$IP_PREFIX$i "$CURRENT_DIR/start_storagenodes.sh $CURRENT_DIR"
} &
done
wait
