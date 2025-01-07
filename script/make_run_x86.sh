#!/bin/bash
if [ "$#" -lt 3 ]; then
    echo "Usage: $0 <START_NODE_SUFFIX> <END_NODE_SUFFIX> <PND_NODE_SUFFIX>"
    exit 1
fi

cd /home/ecRepair/RAN/
make clean
cmake . && make #PND is x86
cd /home/ecRepair/RAN/script
./kill_all_storagenodes.sh $1 $2 && ./kill_all_pnetdevice.sh $3 && ./kill_client.sh #if have PND
./scp_storagenodes.sh $1 $2 && ./scp_pnetdevice.sh $3 #if have PND
./start_all_storagenodes.sh $1 $2 && ./start_all_pnetdevice.sh $3  #if have PND