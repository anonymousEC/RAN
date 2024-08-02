#!/bin/sh
echo "kill storagenodes..."
CURRENT_DIR=$(pwd)
IP_PREFIX=192.168.7.

for i in $(seq $1 $2)
do
{
printf "root@192.168.7.%d\n" $i
ssh root@$IP_PREFIX$i "$CURRENT_DIR/kill_storagenodes.sh $CURRENT_DIR";
} &
done
wait