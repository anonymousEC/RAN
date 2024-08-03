cd /home/ecRepair/RAN/
make clean
cmake . && make #PND is x86
cd /home/ecRepair/RAN/script
./kill_all_storagenodes.sh $1 $2 && ./kill_all_pnetdevice.sh $3 && ./kill_client.sh #if have PND
./scp_storagenodes.sh $1 $2 && ./scp_pnetdevice.sh $3 #if have PND
./start_all_storagenodes.sh $1 $2 && ./start_all_pnetdevice.sh $3  #if have PND