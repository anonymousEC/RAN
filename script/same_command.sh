#!/bin/sh
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <start_ip_suffix> <end_ip_suffix> <command>"
    exit 1
fi

echo "same command: $3, not support * !"
IP_PREFIX="192.168.7."

for i in $(seq $1 $2)
do
{
    SSH_COMMAND="ssh root@$IP_PREFIX$i $3"
    printf "\n"
    echo "Executing command: $SSH_COMMAND ******************************"
    $SSH_COMMAND &
}
done

# Wait for all background jobs to complete
wait

echo "All SSH commands executed."
