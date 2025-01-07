#!/bin/sh
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <start_ip> <end_ip> <network_interface>"
    echo "Example: $0 1 10 eth0"
    exit 1
fi

echo "wondershaper unlimit NIC network speed"
PARENT_DIR=$(dirname "$(pwd)")
TOOLS_DIR=$PARENT_DIR/tools
IP_PREFIX=192.168.7.

for i in $(seq $1 $2)
do
{
printf "wondershaper unlimit NIC network speed: for root@%s%d\n" $IP_PREFIX $i
ssh root@$IP_PREFIX$i "$TOOLS_DIR/wondershaper-master/wondershaper -c -a $3";
} &
done
wait