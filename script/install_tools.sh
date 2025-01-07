#!/bin/sh
echo "intall tools"
CURRENT_DIR=$(pwd)
PARENT_DIR=$(dirname "$(pwd)")
TOOLS_DIR=$PARENT_DIR/tools
IP_PREFIX=192.168.7.

for i in $(seq $1 $2)
do
{
printf "install iperf3, wondershaper, nasm-2.15, isa-l ....: root@192.168.7.%d\n" $i
ssh root@$IP_PREFIX$i "cd $TOOLS_DIR && unzip -oq iperf-master.zip && unzip -oq wondershaper-master.zip &&unzip -oq nasm-2.15.zip && unzip -oq isa-l.zip &&  tar -xzvf glpk-5.0.tar.gz"
ssh root@$IP_PREFIX$i "cd $TOOLS_DIR/iperf-master && ./configure && ldconfig /usr/local/lib && make && make install"
ssh root@$IP_PREFIX$i "cd $TOOLS_DIR/nasm-2.15 && ./configure && make && make install"
ssh root@$IP_PREFIX$i "cd $TOOLS_DIR/isa-l && ./autogen.sh && ./configure && make && sudo make install"
ssh root@$IP_PREFIX$i "cd $TOOLS_DIR/glpk-5.0 && ./configure && make && make install"
} &
done
wait

