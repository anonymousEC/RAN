# RAN

This is the implementation of the RAN prototype described in our paper, "RAN: Accelerating Data Repair with Available Nodes in  Erasure-Coded Storage", currently under review at ACM ICS 2025. RAN is a repair algorithm for erasure coding, designed to significantly reduce the repair performance penalties caused by network transfers.

## Status

![Ubuntu 24.04](https://img.shields.io/badge/Ubuntu%2024.04-black?style=flat-square) ![Test Passed](https://img.shields.io/badge/Test-Passing-brightgreen?style=flat-square)

## 1. Example Deployment

Instances: 7 `m6i.large` instances and 1 `c7gn.8xlarge` instance on Amazon EC2. If there are not enough nodes, please modify the relevant parameters. All commands are executed as root user in the client node.

| **Component** | **Instance Type** | **Quantity** | **IP Address**                     | **Role**                                  |
| ------------- | ----------------- | ------------ | ---------------------------------- | ----------------------------------------- |
| Client        | `m6i.large`       | 1            | `192.168.7.101`                    | Client or Metadata node                   |
| Storage Nodes | `m6i.large`       | 6            | `192.168.7.102` to `192.168.7.107` | Storage nodes (also act as request nodes) |
| PND           | `c7gn.8xlarge`    | 1            | `192.168.7.100`                    | Programmable network device               |

## 2. Install

```Shell
# Install the required tools and libraries for metadata node
apt update && apt install -y gcc g++ cmake make git unzip libtool autoconf python3 expect gcc-aarch64-linux-gnu

# Download RAN
mkdir /home/ecRepair && cd /home/ecRepair
git clone https://github.com/anonymousEC/RAN.git

# Configure SSH password-free access between nodes
ssh-keygen 
cd /home/ecRepair/RAN/script && chmod a+x *
./ssh-auto-send.exp <root_password> 100 107

# Install the required tools and libraries for other nodes
./same_command.sh 100 107 "apt update && apt install -y unzip libtool autoconf python3 libisal2" 

# Create the running environment for each node
./create_env.sh 100 107
```

## 3. Run

```Shell
# Compile and run: If the programmable network device is of x86 architecture, modify make_run.sh

# Modify the test configuration and add tests to the test script
vim test.sh
BASE_DIR="/home/ecRepair/RAN"       # Base directory for the project
NETINTERFACE="ens5"                 # Network interface to use
EXP_TIMES=5                         # Number of experiment repetitions
PND_NODE=100                        # IP suffix for the programmable network device
START_NODE=102                      # IP suffix for starting storage node
END_NODE=107                        # IP suffix for ending storage node
EC_K=2                              # Number of data chunks
EC_M=2                              # Number of parity chunks
CHUNK_SIZE_MB=64                    # Chunk size in MB
SLICE_SIZE_KB=1024                  # Slice size in KB
WANT_REPAIRED_NUM=5                 # Number of repaired chunks for full-node recovery  
MUL_FAIL_NUM=1                      # Number of data failures  for muti-failure recovery

# Batch Testing 
./test.sh | tee TestResult.log
```

## 3. HDFS Integration

### 3-1. Prerequisites

| **HDFS Component** | **Instance Type** | **Quantity** | **IP Address**                     | **Role**                    |
| ------------------ | ----------------- | ------------ | ---------------------------------- | --------------------------- |
| Client/NameNode    | `m6i.large`       | 1            | `192.168.7.101`                    | Client or NameNode          |
| DataNodes          | `m6i.large`       | 6            | `192.168.7.102` to `192.168.7.107` | DataNodes                   |
| PND                | `c7gn.8xlarge`    | 1            | `192.168.7.100`                    | Programmable network device |

```Shell
# Download hadoop-3.1.4-src on available mirror
cd /home/ecRepair/ && wget https://archive.apache.org/dist/hadoop/common/hadoop-3.1.4/hadoop-3.1.4-src.tar.gz
tar -xzvf hadoop-3.1.4-src.tar.gz

# Integrating RAN to HDFS
cd /home/ecRepair/RAN/HDFS_integrate/ &&  chmod a+x *.sh
/home/ecRepair/RAN/HDFS_integrate/RAN_install_HDFS.sh 101 107
source /etc/profile

# Prepare the compilation environment in docker
cp /home/ecRepair/RAN/HDFS_integrate/compile/* /home/ecRepair/hadoop-3.1.4-src/
cd /home/ecRepair/hadoop-3.1.4-src &&  chmod a+x *.sh
./docker_build.sh
./docker_run.sh

# Compile HDFS source code in docker
mvn package -e -DskipTests -Dtar -Dmaven.javadoc.skip=true -Drequire.isal -Disal.lib=/home/root/isa-l/ -Dbundle.isal=true -Pdist,native -DskipShade

# Copy the HDFS program to the specified folder
cp -rf /home/ecRepair/hadoop-3.1.4-src/hadoop-dist/target/hadoop-3.1.4/ /home/ecRepair/

# HDFS Configuration: core-site.xml, hadoop-env.sh, hdfs-site.xml, user_ec_policies.xml, workers
cp /home/ecRepair/RAN/HDFS_integrate/hadoop/* /home/ecRepair/hadoop-3.1.4/etc/hadoop/

# Copy the HDFS program to other nodes
/home/ecRepair/RAN/script/scp_same.sh 102 107 /home/ecRepair/hadoop-3.1.4/ 

```

### 3-2  Run

```shell
# data prepare: 1 stripes
/home/ecRepair/RAN/HDFS_integrate/data_prepare_HDFS.sh 101 107 1

#start RAN coordinator 
/home/ecRepair/RAN/script/test_for_HDFS.sh

#Create two new terminals, named ST and CT
# Use SSH to log in to any storage node and erase data [ST]
ssh root@192.168.7.102
cd /home/ecRepair/hadoopData && find -type f -name "blk_*" ! -name "*meta" -exec md5sum {} \;
#Delete specified blocks [ST]
cd /home/ecRepair/hadoopData && find -type f -name "blk_*" ! -name "*meta" -exec rm {} \; 
#Monitor repair results [ST]
tail -f /home/ecRepair/hadoop-3.1.4/logs/*.log | grep RAN

# Restart and wait for the heartbeat to trigger [CT]
# Trigger can be viewed in the original window and ST window
stop-dfs.sh && start-dfs.sh && hdfs fsck /ec -files -blocks -locations

# Restart and check repair result [CT]
stop-dfs.sh && start-dfs.sh && hdfs fsck /ec -files -blocks -locations 
```
