

前置条件：21个EC2 m6i.large实例和1个c7gn.8xlarge实例，Each instance runs Ubuntu 22.04 and includes 40 GiB of EBS storage. One of the m6i.large instances acts as a metadata node and the rest serve as storage nodes. Storage nodes with data failures also act as request nodes for convenience. The c7gn.8xlarge instance acts as a programmable network device. PND的IP地址为192.168.7.100, metadata node的IP地址为192.168.7.101，存储节点的ip地址为192.168.7.102-121. 如果没有足够的节点，请修改所涉及的参数. 后续所有命令默认client的root用户下执行

Example: 7 m6i.large instances and 1 c7gn.8xlarge instance on Amazon EC2. Each instance runs Ubuntu 22.04 and includes 40 GiB of EBS storage. One of the m6i.large instances acts as a metadata node and the rest serve as storage nodes. Storage nodes with data failures also act as request nodes for convenience. The c7gn.8xlarge instance acts as a programmable network device. 



# RAN

This is the implementation of the RAN prototype described in our paper, "RAN: Accelerating Data Repair Using Available Nodes in Erasure-Coded Storage," currently under review at IEEE INFOCOM 2025. RAN is a repair algorithm for erasure coding, designed to significantly reduce the repair performance penalties caused by network transfers.



## 0. Example Deployment


Instances: 7 `m6i.large` instances and 1 `c7gn.8xlarge` instance on Amazon EC2.

| **Component** | **Instance Type** | **Quantity** | **IP Address**                     | **Role**                                  |
| ------------- | ----------------- | ------------ | ---------------------------------- | ----------------------------------------- |
| Client        | `m6i.large`       | 1            | `192.168.7.101`                    | Client or Metadata node                   |
| Storage Nodes | `m6i.large`       | 6            | `192.168.7.102` to `192.168.7.107` | Storage nodes (also act as request nodes) |
| PND           | `c7gn.8xlarge`    | 1            | `192.168.7.100`                    | Programmable network device               |

## 1. Install

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

## 2. Run

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
########
######## ecRepair修改为erasureCoding 后续添加write和update更加封边
##修改HDFS的install.sh
########
# Download hadoop-3.1.4-src on available mirror
cd /home/ecRepair/ && wget https://archive.apache.org/dist/hadoop/common/hadoop-3.1.4/hadoop-3.1.4-src.tar.gz
tar -xzvf hadoop-3.1.4-src.tar.gz

# Integrating RAN to HDFS
/home/ecRepair/RAN/HDFS_integrate/RAN_install_HDFS.sh 101 107

# Prepare the compilation environment in docker
cp /home/ecRepair/RAN/HDFS_integrate/compile/* /home/ecRepair/hadoop-3.1.4-src/
cd hadoop-3.1.4-src &&  chmod a+x *.sh
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
# Setting the repair algorithm：没必要
vim /home/ych/hadoop-3.1.4/etc/hadoop/hdfs-site.xml 
/home/ecRepair/RAN/script/scp_same.sh 102 121 /home/ecRepair/hadoop-3.1.4/etc/hadoop/hdfs-site.xml

# repair prepare
/home/ecRepair/RAN/HDFS_integrate/data_prepare_HDFS.sh 101 107

# Use SSH to log in to any storage node and erase data (we recommend creating a new command window named SW, the original window is CW)
ssh root@192.168.7.102
cd /home/ecRepair/hadoopData && find -type f -name "blk_*" ! -name "*meta" -exec md5sum {} \;
cd /home/ecRepair/hadoopData && find -type f -name "blk_*" ! -name "*meta" -exec rm {} \; 



#HDFS Client/OR端
修改OptimalRepair的 HDFS_FLAG 1
替换OptimalRepair的HDFS_DEBUG_BLOCK为任意被删除的Block名，且 HDFS_DEBUG 1 #如果仅调试
./make.sh 102 106 120 #启动
./start_client.sh -hdfs #执行, 调试结束
 
#登录故障节点-监听
tail -f /home/ych/hadoop-3.1.4/logs/hadoop-root-datanode-ych.log | grep OptimalRepair
#tail -f /home/ych/hadoop-3.1.4/logs/hadoop-root-datanode-slave.log | grep OptimalRepair
cat /home/ych/hadoop-3.1.4/logs/hadoop-root-datanode-ych.log | grep OptimalRepair
#HDFS Client/OR端测试，这里需要等待心跳触发
stop-dfs.sh && start-dfs.sh && hdfs fsck /ec -files -blocks -locations #重新检测，等待触发修复，测试结束

#再次启动检测或直接故障节点查找该块
stop-dfs.sh && start-dfs.sh && hdfs fsck /ec -files -blocks -locations #重新检测，等待触发修复，测试结束

#修改方法-重复测试[仅能单独测试全节点和降级读，因为节点数目不一致]
vim /home/ych/hadoop-3.1.4/etc/hadoop/hdfs-site.xml #修改配置调试[设置修复方法]
vim /home/ych/hadoop-3.1.4/etc/hadoop/workers #修改节点
/home/ych/OptimalRepair/script/scp_same.sh 102 106 /home/ych/hadoop-3.1.4/etc/hadoop/hdfs-site.xml
/home/ych/OptimalRepair/script/scp_same.sh 102 106 /home/ych/hadoop-3.1.4/etc/hadoop/workers

```

```shell
# Configure environment variables and hostname
cp /home/ecRepair/RAN/tools/dfs_tools_scripts/etc/* /etc/ 
/home/ecRepair/RAN/script/scp_same.sh 101 107 /etc/hosts
/home/ecRepair/RAN/script/scp_same.sh 101 107 /etc/profile
/home/ecRepair/RAN/script/same_command.sh 101 107 "source /etc/profile" 
source /etc/profile

# Add RAN integration code to HDFS source code
cp -r /home/ecRepair/RAN/HDFS_integrate/hadoop-common-project/ ${HADOOP_SRC_DIR}/
cp -r /home/ecRepair/RAN/HDFS_integrate/hadoop-hdfs-project/ ${HADOOP_SRC_DIR}/


# Install the required tools and libraries 
apt-get -y install build-essential autoconf automake libtool cmake zlib1g-dev pkg-config libssl-dev expect
cd /home/ecRepair/RAN/tools/dfs_tools_scripts
/home/ecRepair/RAN/script/scp_same.sh 101 107 protobuf-2.5.0.tar.gz
/home/ecRepair/RAN/script/same_command.sh 101 107 "apt install -y openjdk-8-jdk maven" 
/home/ecRepair/RAN/script/same_command.sh 101 107 "cd ~ && tar -zxvf protobuf-2.5.0.tar.gz  && cd protobuf-2.5.0 && sudo mkdir /usr/local/protoc-2.5.0/ && ./configure --prefix=/usr/local/protoc-2.5.0/ && make && sudo make install" 
```



## 3-OptimalRepair测试



```shell
环境搭建-测试命令-调试命令-代码协助

----------------------------------------------------------------------

#1. 准备测试运行环境：创建环境和删除环境（仅首次搭建使用，后续无需运行）
./create_env.sh 101 121
#2. 编译命令 + 发送可执行文件 + 运行可执行文件
./make.sh 102 121 100
./microtest.sh | tee test.txt
./test.sh | tee finalTestResult.txt
cat test.txt | grep MAX | grep ms  #查询异常结果
cat finalTestResult.txt | grep MAX | grep ms  #查询异常结果
cat finalTestResult-CR.txt | grep MAX | grep ms
cat finalTestResult-EN.txt | grep MAX | grep ms
curl -F "file=@/path/to/your/file" https://file.io #EC2的文件中转，1MB/s左右
#3. 测试命令
./kill_all_storagenodes.sh 102 121 && ./kill_all_pnetdevice.sh 100 
./start_all_storagenodes.sh 102 121 && ./start_all_pnetdevice.sh 100  #if have PND
./kill_all_storagenodes.sh 102 110
./start_all_storagenodes.sh 102 110
./start_client.sh -rdre 192MB_src 192MB_dst
./start_client.sh -rdee 384MB_repair 384MB_dst
./start_client.sh -rdne 128MB_repair 128MB_dst
./start_client.sh -rdne 192MB_repair 192MB_dst
./start_client.sh -w 192MB_src 192MB_dst
./start_client.sh -rdte 384MB_repair 384MB_dst

./start_client.sh -rdre 384MB_repair 384MB_dst

./scp_same.sh 100 105 "/home/ych/OptimalRepair/src"
./start_client.sh -rdte 16MB_repair 16MB_dst	
./start_client.sh -rdre 16MB_repair 16MB_dst
./start_client.sh -rdee 16MB_repair 16MB_dst
./start_client.sh -rdne 16MB_repair 16MB_dst
./start_client.sh -rfne 80MB_repair 80MB_dst
./start_client.sh -rfne 96MB_repair 96MB_dst
./start_client.sh -rfte 48MB_repair 48MB_dst
./start_client.sh -rfte 16MB_repair 16MB_dst	
./start_client.sh -rfre 16MB_repair 16MB_dst
./start_client.sh -rfee 16MB_repair 16MB_dst
./start_client.sh -rfne 16MB_repair 16MB_dst


登录到故障节点
md5sum /home/ych/OptimalRepair/test_file/write/* 
md5sum /home/ych/OptimalRepair/test_file/repair/*  && rm /home/ych/OptimalRepair/test_file/repair/* 

#可选1.make.sh实际执行
cd /home/ych/OptimalRepair/ && cmake .  && make
cd /home/ych/OptimalRepair/script
./kill_all_storagenodes.sh 102 121 && ./kill_all_pnetdevice.sh 100 #if have PND
./scp_storagenodes.sh 102 108 && ./scp_pnetdevice.sh 120 #if have PND
./start_all_storagenodes.sh 102 121 && ./start_all_pnetdevice.sh 100  #if have PND

sudo /home/ych/OptimalRepair/tools/wondershaper-master/wondershaper staus
#可选3.网络带宽限制
./limit_network.sh 102 108 enp0s9 100000     	//ens5需要查看网卡名确定100Mbps
./unlimit_network.sh 102 108 enp0s9 
./unlimit_network.sh 102 110 ens5 
./limit_network.sh 102 110 ens5 1000 1000
./limit_network.sh 102 121 enps5 10000     	//ens5需要查看网卡名确定100Mbps
./unlimit_network.sh 102 108 enp0s9 

#可选4.网络带宽测试
./start_close_iperf3_server.sh 102 121 1
./start_close_iperf3_server.sh 102 121 0  关闭服务器
 ./start_iperf3_test.sh 102 121 1 
 
#其他脚本-发送相同文件和执行相同命令
./scp_same.sh <start_node> <end_node> <file>
./same_command.sh <start_node> <end_node> <"command">
----------------------------------------------------------------------
#可选6：网络流量查看
iftop -i wlan0 -BnP -f 'port 8800'  //http://www.360doc.com/content/23/0630/18/21693298_1086847598.shtml 按T查看单次数据量
rates：分别表示过去 2s 10s 40s 的平均流量
nload -devices enp0s9
----------------------------------------------------------------------
```

## 4-HDFS集成

### Hadoop编译

```shell
#JDK自动安装(推荐)jdk安装位置/usr/lib/jvm/java-8-openjdk-amd64（readlink -f $(which java)）
#Maven自动安装(推荐)Maven安装位置/usr/share/maven
cd /home/ych/OptimalRepair/tools/dfs_tools_scripts
/home/ych/OptimalRepair/script/same_command.sh 101 121 "apt install -y openjdk-8-jdk maven" 
#protobuf安装
/home/ych/OptimalRepair/script/scp_same.sh 101 121 protobuf-2.5.0.tar.gz
/home/ych/OptimalRepair/script/same_command.sh 101 121 "cd ~ && tar -zxvf protobuf-2.5.0.tar.gz  && cd protobuf-2.5.0 && sudo mkdir /usr/local/protoc-2.5.0/ && ./configure --prefix=/usr/local/protoc-2.5.0/ && make && sudo make install" 
#配置设置hosts和profile[后续涉及的所有，EC需要适当修改]
cp /home/ych/OptimalRepair/tools/dfs_tools_scripts/etc/* /etc/ #根据实际路径调整
/home/ych/OptimalRepair/script/scp_same.sh 102 121 /etc/hosts
/home/ych/OptimalRepair/script/scp_same.sh 102 121 /etc/profile
/home/ych/OptimalRepair/script/same_command.sh 101 121 "source /etc/profile" #可以登录检查
source /etc/profile #当前端口还要执行

#其他本地库安装
apt-get -y install build-essential autoconf automake libtool cmake zlib1g-dev pkg-config libssl-dev expect

#修改的代码
cp -r /home/ych/OptimalRepair/HDFS_integrate/hadoop-common-project/ ${HADOOP_SRC_DIR}/
cp -r /home/ych/OptimalRepair/HDFS_integrate/hadoop-hdfs-project/ ${HADOOP_SRC_DIR}/

#Hadoop编译：注意编译时最好关闭vscode，仅用命令行-加速编译，否则半个小时+都可能，直接编译就4-5分钟
tar -xzvf hadoop-3.1.4-src.tar.gz #自行下载 #hadoop-3.1.4-src.zip已备份初次编译成功的，避免长时间下载
cd ${HADOOP_SRC_DIR}; mvn package -DskipTests -Dtar -Dmaven.javadoc.skip=true -Drequire.isal -Disal.lib=/home/ych/OptimalRepair/tools/isa-l/ -Dbundle.isal=true -Pdist,native -DskipShade -e

出现[ERROR] Failed to execute goal org.apache.hadoop:hadoop-maven-plugins:3.1.4:cmake-compile (cmake-compile) on project hadoop-common: make failed with error code 2
protobuf安装

#复制到常用目录
cp -rf /home/ych/hadoop-3.1.4-src/hadoop-dist/target/hadoop-3.1.4/ /home/ych

#配置
cp /home/ych/OptimalRepair/HDFS_integrate/hadoop-4node3worker/* /home/ych/hadoop-3.1.4/etc/hadoop/ #根据需求修改workers，可以不修改hadoop-env.sh,core-site.xml,mapred-site.xml,hdfs-site.xml,yarn-site.xml.也可以看着改，如副本需要写入测试设置为3


#发送编译版本
/home/ych/OptimalRepair/script/scp_same.sh 102 121 /home/ych/hadoop-3.1.4/ 

/home/ych/OptimalRepair/script/scp_same.sh 102 121 /home/ych/hadoop-3.1.4/etc/hadoop/workers
vim /home/ych/hadoop-3.1.4/etc/hadoop/workers #修改合适的worker[单块为EC_K,全节点为EC_A]
vim /home/ych/hadoop-3.1.4/etc/hadoop/hdfs-site.xml #修改配置调试[设置修复方法]


```

### HDFS启动

```shell
#获取数据
cd /home/ych/ && dd if=/dev/urandom iflag=fullblock of=192MB_src bs=64M count=3 
cd /home/ych/ && dd if=/dev/urandom iflag=fullblock of=384MB_src bs=64M count=6
#格式化 NameNode 以及启动 HDFS 系统[101节点][hdfs dfs 是对 hadoop fs别名]
/home/ych/OptimalRepair/script/same_command.sh 101 121 "rm -r /home/ych/hadoopData/dfs"; hdfs namenode -format
/home/ych/OptimalRepair/script/same_command.sh 101 102 "rm -r /home/ych/hadoopData/dfs"; hdfs namenode -format
#启动一套
start-dfs.sh; hdfs dfsadmin -report | grep slave #检查woker是否匹配
hdfs dfs -mkdir /ec; hdfs ec -enablePolicy -policy RS-3-2-1024k; hdfs ec -setPolicy -policy RS-3-2-1024k -path /ec
hdfs dfs -mkdir /ec; hdfs ec -enablePolicy -policy RS-6-3-1024k; hdfs ec -setPolicy -policy RS-6-3-1024k -path /ec
#单个修复
cd /home/ych; hdfs dfs -put 192MB_src /ec/192MB_dst
#全节点修复
cd /home/ych; hdfs dfs -put 128MB_src /ec/128MB_dst1; hdfs dfs -put 128MB_src /ec/128MB_dst2; hdfs dfs -put 128MB_src /ec/128MB_dst3; hdfs dfs -put 128MB_src /ec/128MB_dst4

#启动OptimalRepair

hdfs fsck /ec -files -blocks -locations | tee /home/ych/HDFS_info

hdfs dfs -rm /ec/128MB_dst && hdfs dfs -put 128MB_src /ec/128MB_dst

hdfs fsck /ec -files -blocks -locations > /home/ych/HDFS_info

#Optimal测试
./make.sh 102 104 120
./start_client.sh -hdfs #必须在写入后执行

stop-dfs.sh; start-dfs.sh #关闭

#登录到模拟故障的节点，根据fsck的BP值，复制替换。必须是数据块位置，通过fsck看顺序
truncate -s 0 /home/ych/hadoop-3.1.4/logs/hadoop-root-datanode-ych.log
cd /home/ych/hadoopData/dfs/data/current && find -name "blk_*" && find -type f -name "blk_*" -exec rm {} \; #查看删除
#md5sum存储的数据
cd /home/ych/hadoopData/dfs/data/current && find -type f -name "blk_*" ! -name "*meta" -exec md5sum {} \;
#删除数据数据
cd /home/ych/hadoopData/dfs/data/current && find -type f -name "blk_*" ! -name "*meta" -exec rm {} \;
#j
/home/ych/OptimalRepair/script/same_command.sh 102 106 "cd /home/ych/hadoopData/dfs/data/current && find -type f -name "blk_*" ! -name "*meta" -exec md5sum {} \

tail -f /home/ych/hadoop-3.1.4/logs/hadoop-root-datanode-ych.log | grep OptimalRepair
cat /home/ych/hadoop-3.1.4/logs/hadoop-root-datanode-ych.log | grep OptimalRepair #登录到其他节点查看修复数据
cat /home/ych/hadoop-3.1.4/logs/hadoop-root-namenode-ych.log | grep OptimalRepair

cp ./BP-1806768031-127.0.1.1-1718847969903/current/finalized/subdir0/subdir0/* /home/ych/test/
4d9176ee32ff5df8ace8630823f80b3c  /home/ych/test/blk_-9223372036854775791
2db01df3ff6a3b01f793dc4343c42c54  /home/ych/test/blk_-9223372036854775791_1001.meta

start-dfs.sh; hdfs fsck /ec -files -blocks -locations #重新启动触发修复，并查看是否已修复


#再执行一次检查修复结果
stop-dfs.sh
start-dfs.sh; hdfs fsck /ec -files -blocks -locations 

#查看NameNode日志
cat /home/ych/hadoop-3.1.4/logs/hadoop-root-namenode-ych.log | grep OptimalRepair
truncate -s 0 /home/ych/hadoop-3.1.4/logs/hadoop-root-namenode-ych.log

```

### HDFS启动全节点整理版

```shell
#全节点测试：前置条件1.workers被修改为EC_A并被发送到每个节点 2.hdfs-site.xml被修改为全节点方法
#降级读测试(支持单节点多块)：前置条件1.workers被修改为EC_N 且修改hdfs-site.xml为降级读
#测试时打开四个终端：HDFS Client，OR Client，故障节点检查，故障节点监听

#HDFS Client/OR端
./prepare_hdfs.sh

#登录故障节点
cd /home/ych/hadoopData && find -type f -name "blk_*" ! -name "*meta" -exec md5sum {} \; #检查
cd /home/ych/hadoopData && find -type f -name "blk_*" ! -name "*meta" -exec rm {} \; #删除

cd /home/ych/hadoopData && find -type f -name "blk_-9223372036854774240" ! -name "*meta" -exec md5sum {} \; #检查


#HDFS Client/OR端
修改OptimalRepair的 HDFS_FLAG 1
替换OptimalRepair的HDFS_DEBUG_BLOCK为任意被删除的Block名，且 HDFS_DEBUG 1 #如果仅调试
./make.sh 102 106 120 #启动
./start_client.sh -hdfs #执行, 调试结束
 
#登录故障节点-监听
tail -f /home/ych/hadoop-3.1.4/logs/hadoop-root-datanode-ych.log | grep OptimalRepair
#tail -f /home/ych/hadoop-3.1.4/logs/hadoop-root-datanode-slave.log | grep OptimalRepair
cat /home/ych/hadoop-3.1.4/logs/hadoop-root-datanode-ych.log | grep OptimalRepair
#HDFS Client/OR端测试，这里需要等待心跳触发
stop-dfs.sh && start-dfs.sh && hdfs fsck /ec -files -blocks -locations #重新检测，等待触发修复，测试结束

#再次启动检测或直接故障节点查找该块
stop-dfs.sh && start-dfs.sh && hdfs fsck /ec -files -blocks -locations #重新检测，等待触发修复，测试结束

#修改方法-重复测试[仅能单独测试全节点和降级读，因为节点数目不一致]
vim /home/ych/hadoop-3.1.4/etc/hadoop/hdfs-site.xml #修改配置调试[设置修复方法]
vim /home/ych/hadoop-3.1.4/etc/hadoop/workers #修改节点
/home/ych/OptimalRepair/script/scp_same.sh 102 106 /home/ych/hadoop-3.1.4/etc/hadoop/hdfs-site.xml
/home/ych/OptimalRepair/script/scp_same.sh 102 106 /home/ych/hadoop-3.1.4/etc/hadoop/workers
```

# 