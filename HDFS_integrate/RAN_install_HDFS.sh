#!/bin/bash

# Check if the correct number of arguments is provided
if [ "$#" -ne 2 ]; then
  echo "Usage: $0 START_NODE END_NODE"
  exit 1
fi

set -e # command fail will make scripe exit: add [false] Breakpoints
# Arguments
START_NODE=$1
END_NODE=$2

# Constants
BASE_DIR="/home/ecRepair"
SCRIPT_DIR="${BASE_DIR}/RAN/script"
TOOLS_DIR="${BASE_DIR}/RAN/tools/dfs_tools_scripts"
HADOOP_SRC_DIR="${BASE_DIR}/hadoop-3.1.4-src"
CURRENT_DIR=$(pwd)

# Configure environment variables and hostname
cp ${TOOLS_DIR}/etc/* /etc/
${SCRIPT_DIR}/scp_same.sh ${START_NODE} ${END_NODE} /etc/hosts
${SCRIPT_DIR}/scp_same.sh ${START_NODE} ${END_NODE} /etc/profile
${SCRIPT_DIR}/same_command.sh ${START_NODE} ${END_NODE} "source /etc/profile"
source /etc/profile

# Install the required tools and libraries
apt-get -y install build-essential autoconf automake libtool cmake zlib1g-dev pkg-config libssl-dev expect
cd ${TOOLS_DIR}
${SCRIPT_DIR}/scp_same.sh ${START_NODE} ${END_NODE} protobuf-2.5.0.tar.gz
${SCRIPT_DIR}/same_command.sh ${START_NODE} ${END_NODE} "apt install -y openjdk-8-jdk maven"
${SCRIPT_DIR}/same_command.sh ${START_NODE} ${END_NODE} "cd ~ && tar -zxvf protobuf-2.5.0.tar.gz && cd protobuf-2.5.0 && sudo mkdir /usr/local/protoc-2.5.0/ && ./configure --prefix=/usr/local/protoc-2.5.0/ && make && sudo make install"


# Add RAN integration code to HDFS source code
cp -r ${BASE_DIR}/RAN/HDFS_integrate/hadoop-common-project/ ${HADOOP_SRC_DIR}/
cp -r ${BASE_DIR}/RAN/HDFS_integrate/hadoop-hdfs-project/ ${HADOOP_SRC_DIR}/

# install docker
apt install -y docker.io
systemctl start docker
systemctl enable docker

# Return to the original directory
cd ${CURRENT_DIR}