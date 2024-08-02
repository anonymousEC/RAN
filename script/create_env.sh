#!/bin/sh
echo "mkdir save directory ..."
PARENT_DIR=$(dirname "$(pwd)")
CURRENT_DIR=$(pwd)
BUILD_DIR=$PARENT_DIR/build
SCRIPT_DIR=$PARENT_DIR/script
TOOLS_DIR=$PARENT_DIR/tools
LOG_DIR=$PARENT_DIR/log
TEST_FILE_DIR=$PARENT_DIR/test_file
WRITE_DIR=$TEST_FILE_DIR/write
REPAIR_DIR=$TEST_FILE_DIR/repair
IP_PREFIX=192.168.7.

cp $TOOLS_DIR/arm/libisal.so /usr/local/lib/armlibisal.so
for i in $(seq $1 $2)
do
{
    printf "mkdir save directory : $BUILD_DIR @$IP_PREFIX%d\n" $i
    ssh root@$IP_PREFIX$i "mkdir -p $BUILD_DIR"

    printf "mkdir save directory : $SCRIPT_DIR @$IP_PREFIX%d\n" $i
    scp -rp $SCRIPT_DIR  root@$IP_PREFIX$i:$PARENT_DIR 

    printf "mkdir save directory : $TOOLS_DIR @$IP_PREFIX%d\n" $i
    scp -rp $TOOLS_DIR  root@$IP_PREFIX$i:$PARENT_DIR 

    printf "mkdir save directory : $LOG_DIR @$IP_PREFIX%d\n" $i
    scp -rp $LOG_DIR  root@$IP_PREFIX$i:$PARENT_DIR 

    printf "mkdir save directory : $WRITE_DIR @$IP_PREFIX%d\n" $i
    ssh root@$IP_PREFIX$i "mkdir -p $WRITE_DIR"

    printf "mkdir save directory : $REPAIR_DIR @$IP_PREFIX%d\n" $i
    ssh root@$IP_PREFIX$i "mkdir -p $REPAIR_DIR"
}&
done
wait

sh $CURRENT_DIR/install_tools.sh $1 $2
