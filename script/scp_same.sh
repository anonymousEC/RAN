#!/bin/bash

if [ $# -ne 3 ]; then
    echo "Usage: $0 <start_node> <end_node> <file>"
    exit 1
fi

start_node="$1"
end_node="$2"
file_to_copy="$3"

for ((i=start_node; i<=end_node; i++)); do
    dest="192.168.7.$i:$(dirname "$file_to_copy")"  
    echo "Copying $file_to_copy to $dest"
    scp -r "$file_to_copy" "$dest" &
done

wait

echo "Files copied to all nodes successfully"
