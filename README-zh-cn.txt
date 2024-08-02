环境搭建-测试命令-调试命令-代码协助

----------------------------------------------------------------------
环境搭建

#1. 必备软件安装
sudo apt update && sudo apt install -y gcc g++ cmake make net-tools gdb git iperf openssh-client openssh-server

#2. SSH免密登录：每个机器都要轮流分发到对应机器
ssh-keygen 						//一直回车，在当前用户~下生成.ssh文件夹
ssh-copy-id -i ~/.ssh/id_rsa.pub root@192.168.7.101	//给需要免密登录的节点执行

#3. 创建ych文件夹并上传测试文档
cd /home && mkdir ych
winscp上传文件


#可选1. iperf网速测试节点之间网络
iperf3 -s			//在服务器节点192.168.7.102运行
iperf3 -c 192.168.7.102		//在非服务器节点运行

#可选2：通过切分文件准备测试文件
cd /home/ych/OptimalRepair/test_file/write && gcc -o cut_file_test cut_file_test.c && ./cut_file_test 1.58GB.mp4
#可选3：直接生成，无需准备测试文件64*3MB的文件生成
dd if=/dev/urandom iflag=fullblock of=file.txt bs=64M count=3 

#可选3：发送相同文件 101 108 start_storagenodes.sh
/home/ych/dfs_tools_scripts/scripts/scp_same.sh 101 108 start_storagenodes.sh 
/home/ych/dfs_tools_scripts/scripts/scp_same.sh 101 108 /home/ych/OptimalRepai
r/script/start_storagenodes.sh

#md5sum快速比较
md5sum /home/ych/OptimalRepair/test_file/write/3MB_dst_4
全部为49d8fa2bef26b5bc9fd074e8b13273a0
修复chunk为07bae8dcfe175709f5d770b19bbe07f9

#tcpdump包查看
tcpdump -n -i enp0s9 port 8000 or port 8001 or port 8002 or port 8004
tcpdump -n -i enp0s9 port 8100 or port 8101 or port 8102 or port 8104
三次握手[S][S.][.]
数据发送[P.]和确认[.]和关闭[F.]和重置[R.]

#其他脚本-发送相同文件和执行相同命令
./scp_same.sh <start_node> <end_node> <file>
./same_command.sh <start_node> <end_node> <"command">
----------------------------------------------------------------------
测试命令
!!!多端口是为了充分利用EC2的网络，当有足够的socket则无需多端口

#1. 准备测试运行环境：创建环境和删除环境（仅首次搭建使用，后续无需运行）
cd /home/ych/OptimalRepair/test_file/write && dd if=/dev/urandom iflag=fullblock of=3MB_src bs=1M count=3 
cd /home/ych/OptimalRepair/test_file/write && dd if=/dev/urandom iflag=fullblock of=3MB_src bs=1M count=3  #virtualbox全节点专用
cd /home/ych/OptimalRepair/test_file/write && dd if=/dev/urandom iflag=fullblock of=3MB_src bs=8M count=3  #virtualbox全节点专用
cd /home/ych/OptimalRepair && cd script && chmod a+x *
./create_env_storagenodes.sh 101 108
./create_env_storagenodes.sh 120 120
./delete_env_storagenodes.sh 102 108
./delete_env_storagenodes.sh 120 120


#2. 编译命令 + 发送可执行文件 + 运行可执行文件
cd /home/ych/OptimalRepair && cd script && chmod a+x * && ./make.sh

#3. 测试命令
./start_client.sh -w 3MB_src 3MB_dst 		//3MB_src是client本地保存文件，3MB_dst是存储在分布式存储的文件名
./start_client.sh -r 3MB_src 3MB_dst 
./start_client.sh -wf 3MB_src 3MB_dst 		//全节点写入，固定配置
#同构-单块降级读
./start_client.sh -rodt 3MB_repair 3MB_dst	//3MB_repair是新节点或故障节点修复得到的文件，3MB_dst是存储在分布式存储的文件名:传统修复
./start_client.sh -rodr 3MB_repair 3MB_dst	//RP修复
./start_client.sh -rode 3MB_repair 3MB_dst	//NetEC修复
./start_client.sh -rodn 3MB_repair 3MB_dst	//新修复
#异构网络-单块降级读
./start_client.sh -redt 3MB_repair 3MB_dst	//3MB_repair是新节点或故障节点修复得到的文件，3MB_dst是存储在分布式存储的文件名:传统修复
./start_client.sh -redr 3MB_repair 3MB_dst	//RP修复
./start_client.sh -rede 3MB_repair 3MB_dst	//NetEC修复
./start_client.sh -redn 3MB_repair 3MB_dst	//新修复

#全节点修复
./start_client.sh -roft 3MB_repair 3MB_dst 
./start_client.sh -rofr 3MB_repair 3MB_dst 
./start_client.sh -rofe 3MB_repair 3MB_dst 
./start_client.sh -rofn 3MB_repair 3MB_dst 
./start_client.sh -reft 3MB_repair 3MB_dst 
./start_client.sh -refr 3MB_repair 3MB_dst 
./start_client.sh -refe 3MB_repair 3MB_dst 
./start_client.sh -refn 3MB_repair 3MB_dst 

#多块修复
./start_client.sh -roct 3MB_repair 3MB_dst	
./start_client.sh -rocr 3MB_repair 3MB_dst	
./start_client.sh -roce 3MB_repair 3MB_dst	
./start_client.sh -rocn 3MB_repair 3MB_dst	
./start_client.sh -rect 3MB_repair 3MB_dst	
./start_client.sh -recr 3MB_repair 3MB_dst	
./start_client.sh -rece 3MB_repair 3MB_dst
./start_client.sh -recn 3MB_repair 3MB_dst	

#多节点修复
./start_client.sh -ront 3MB_repair 3MB_dst	
./start_client.sh -ronr 3MB_repair 3MB_dst	
./start_client.sh -rone 3MB_repair 3MB_dst	
./start_client.sh -ronn 3MB_repair 3MB_dst	
./start_client.sh -rent 3MB_repair 3MB_dst	
./start_client.sh -renr 3MB_repair 3MB_dst	
./start_client.sh -rene 3MB_repair 3MB_dst
./start_client.sh -renn 3MB_repair 3MB_dst	


登录到故障节点
md5sum /home/ych/OptimalRepair/test_file/write/* 
 md5sum /home/ych/OptimalRepair/test_file/repair/*  && rm /home/ych/OptimalRepair/test_file/repair/* 

#可选1.make.sh实际执行
cd /home/ych/OptimalRepair/
cmake . 
make
cd /home/ych/OptimalRepair/script
./kill_all_storagenodes.sh 102 108 && ./kill_all_pnetdevice.sh 120 #if have PND
./scp_storagenodes.sh 102 108 && ./scp_pnetdevice.sh 120 #if have PND
./start_all_storagenodes.sh 102 108 && ./start_all_pnetdevice.sh 120  #if have PND

#可选2.单点故障
./kill_ip_storagenodes.sh 103


#可选3.网络带宽限制
./limit_network.sh 102 108 enp0s9 100000     	//ens5需要查看网卡名确定100Mbps
./unlimit_network.sh 102 108 enp0s9 

#可选4.网络带宽测试
./start_close_iperf3_server.sh 102 108 1
./start_close_iperf3_server.sh 102 108 0  关闭服务器
 ./start_iperf3_test.sh 102 108 1 

----------------------------------------------------------------------
调试命令

#可选1.GDB手动调试
gdb --args  ../build/client -r 3MB_src 3MB_dst

#可选2：GDB+VSCODE远程调试
优点:远程直接修改文件，添加print log和打断点以及图形化的调试，直到问题解决。避免反复传输执行文件
操作：修改task.json和launch.json

#可选3：内存调试：查看报错前面的变化
./valgrind.sh client -r 3MB_src 3MB_dst 

#可选4：实时查看文档信息
	tail -f ../log/log.txt

#可选5：网络端口查看
	netstat -tuln

#可选6：网络流量查看
iftop -i wlan0 -BnP -f 'port 8800'  //http://www.360doc.com/content/23/0630/18/21693298_1086847598.shtml 按T查看单次数据量
rates：分别表示过去 2s 10s 40s 的平均流量
nload -devices enp0s9
----------------------------------------------------------------------


