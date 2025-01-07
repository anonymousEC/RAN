#!/bin/bash
if [ "$#" -lt 3 ]; then
    echo "Usage: $0 <START_NODE_SUFFIX> <END_NODE_SUFFIX> <PND_NODE_SUFFIX>"
    exit 1
fi

./make_run_x86.sh $1 $2 $3
#./make_run_arm.sh $1 $2 $3