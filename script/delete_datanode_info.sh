#!/bin/bash
if [ $# -ne 2 ]; then
    echo "Usage: $0 <start_node> <end_node>"
    exit 1
fi
start_node="$1"
end_node="$2"
dfs_directory="/home/ecRepair/hadoopData/dfs"

for ((i=start_node; i<=end_node; i++)); do
    host="192.168.7.$i"
    ssh root@"$host" "rm -r $dfs_directory"
done

echo "Directories deleted on all nodes successfully"
