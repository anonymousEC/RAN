#!/bin/bash

echo "stop HDFS"
stop-dfs.sh

echo "delete data and format Namenode"
/home/ecRepair/RAN/script/same_command.sh 101 121 "rm -r /home/ecRepair/hadoopData/dfs"
hdfs namenode -format

echo "start HDFS"
start-dfs.sh

echo "check whether the startup is normal"
hdfs dfsadmin -report | grep slave

echo "create the /ec directory and enable an EC policy"
hdfs dfs -mkdir /ec
hdfs ec -addPolicies -policyFile /home/ecRepair/hadoop-3.1.4/etc/hadoop/user_ec_policies.xml
hdfs ec -enablePolicy -policy RS-2-2-1024k
hdfs ec -setPolicy -policy RS-2-2-1024k -path /ec

echo "Upload files to the /ec directory"
cd /home/ecRepair
for (( i=1; i<=160; i++ ))
do
    hdfs dfs -put 128MB_src /ec/128MB_dst${i}
    echo "hdfs dfs -put 128MB_src /ec/128MB_dst${i}"
done

echo "check the file block information"
hdfs fsck /ec -files -blocks -locations
