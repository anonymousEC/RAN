#!/bin/sh
if [ "$#" -ne 5 ]; then
    echo "Usage: $0 <start_ip> <end_ip> <network_interface> <upload_speed_Mbps> <download_speed_Mbps> "
    echo "Example: $0 1 10 eth0 1000 1000"
    exit 1
fi


PARENT_DIR=$(dirname "$(pwd)")
TOOLS_DIR=$PARENT_DIR/tools
IP_PREFIX=192.168.7.
UP_SPEED_Mbps=$4
DW_SPEED_Mbps=$5
UP_SPEED_Kbps=$((UP_SPEED_Mbps * 1000))
DW_SPEED_Kbps=$((DW_SPEED_Mbps * 1000))


for i in $(seq $1 $2)
do
    {
        printf "limit network speed:  uplink = %d Mbps, downlink = %d Mbps,  for root@%s%d\n" $UP_SPEED_Mbps $DW_SPEED_Mbps $IP_PREFIX $i
        ssh root@$IP_PREFIX$i "$TOOLS_DIR/wondershaper-master/wondershaper -a $3 -u $UP_SPEED_Kbps -d $DW_SPEED_Kbps";
    } &
done
wait