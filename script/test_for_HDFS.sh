#!/bin/bash
set -e # command fail will make scripe exit: add [false] Breakpoints

# ===============================================PATH===========================================
BASE_DIR="/home/ecRepair/RAN"       # Base directory for the project
NETINTERFACE="eth0"                 # Network interface to use
EXP_TIMES=1                         # Number of experiment repetitions
PND_NODE=100                        # IP suffix for the programmable network device
START_NODE=102                      # IP suffix for starting storage node
END_NODE=107                        # IP suffix for ending storage node
EC_K=2                              # Number of data chunks
EC_M=2                              # Number of parity chunks
CHUNK_SIZE_MB=64                    # Chunk size in MB
SLICE_SIZE_KB=1024                  # Slice size in KB
WANT_REPAIRED_NUM=40                 # Number of repaired chunks
MUL_FAIL_NUM=1                      # Number of data failures

NODE_NUM=$((END_NODE - START_NODE + 1))
EC_X=$((NODE_NUM - EC_K - EC_M))
FULL_RECOVERY_STRIPE_MAX_NUM=$(( ($WANT_REPAIRED_NUM * $NODE_NUM) / ($EC_K + $EC_M) ))
BAND_LOCATION=6
FLAG_WRITE_REPAIR=1
FILE_SIZE=$(($EC_K * $CHUNK_SIZE_MB))

# ===============================================PATH===========================================
RESULT_DIR="${BASE_DIR}/test_result"
SCRIPT_DIR="${BASE_DIR}/script"
RESULT_SAVE_DIR=" "
INCLUDE_DIR="${BASE_DIR}/include"
CONFIG_FILE="${INCLUDE_DIR}/ECConfig.h"
SRC_DIR="${BASE_DIR}/src"
LOG_DIR="${BASE_DIR}/log"
FILE_DIR="${BASE_DIR}/test_file"
WRITE_FILE_DIR="${FILE_DIR}/write"
READ_FILE_DIR="${FILE_DIR}/read"
REPAIR_FILE_DIR="${FILE_DIR}/repair"

# ===============================================FUNC===========================================
limit_network_speed(){
    ./unlimit_network.sh $START_NODE $END_NODE $NETINTERFACE
    local -n nodes_suffix_ref=$1
    local -n upload_speeds_Mbps_ref=$2
    local -n download_speeds_Mbps_ref=$3
    local network_interface=$NETINTERFACE

    if [ ${#nodes_suffix_ref[@]} -ne ${#upload_speeds_Mbps_ref[@]} ] || [ ${#nodes_suffix_ref[@]} -ne ${#download_speeds_Mbps_ref[@]} ]; then
        echo "Error: Arrays lengths do not match."
        exit 1
    fi
    for i in "${!nodes_suffix_ref[@]}"; do
        SUFFIX=$(( ${nodes_suffix_ref[$i]} + $START_NODE ))
        UP_SPEED_Mbps=${upload_speeds_Mbps_ref[$i]}
        DW_SPEED_Mbps=${download_speeds_Mbps_ref[$i]}
        ./limit_network.sh "$SUFFIX" "$SUFFIX" "$network_interface" "$UP_SPEED_Mbps" "$DW_SPEED_Mbps"
    done
}


config_run() {
    #config: [defalut] 3 2 2 1 64 1 30 2 1
    local EC_K=$1
    local EC_M=$2
    local EC_X=$3
    local CHUNK_SIZE_MB=$4
    local CHUNK_SIZE=$((CHUNK_SIZE_MB * 1024 * 1024))
    local SLICE_SIZE_KB=$5
    local SLICE_SIZE=$((SLICE_SIZE_KB * 1024))
    local BAND_LOCATION=$6
    local FULL_RECOVERY_STRIPE_MAX_NUM=$7
    local MUL_FAIL_NUM=$8
    local FLAG_WRITE_REPAIR=$9
    local HDFS_FLAG=1
    local HDFS_DEBUG=0

    sed -i "s/^#define EC_K .*/#define EC_K ${EC_K}/" "$CONFIG_FILE"
    sed -i "s/^#define EC_M .*/#define EC_M ${EC_M}/" "$CONFIG_FILE"
    sed -i "s/^#define EC_X .*/#define EC_X ${EC_X}/" "$CONFIG_FILE"
    sed -i "s/^#define CHUNK_SIZE .*/#define CHUNK_SIZE ${CHUNK_SIZE}/" "$CONFIG_FILE"
    sed -i "s/^#define SLICE_SIZE .*/#define SLICE_SIZE ${SLICE_SIZE}/" "$CONFIG_FILE"
    sed -i "s/^#define BAND_LOCATION .*/#define BAND_LOCATION ${BAND_LOCATION}/" "$CONFIG_FILE"
    sed -i "s/^#define FULL_RECOVERY_STRIPE_MAX_NUM .*/#define FULL_RECOVERY_STRIPE_MAX_NUM ${FULL_RECOVERY_STRIPE_MAX_NUM}/" "$CONFIG_FILE"
    sed -i "s/^#define MUL_FAIL_NUM .*/#define MUL_FAIL_NUM ${MUL_FAIL_NUM}/" "$CONFIG_FILE"
    sed -i "s/^#define FLAG_WRITE_REPAIR .*/#define FLAG_WRITE_REPAIR ${FLAG_WRITE_REPAIR}/" "$CONFIG_FILE"
    sed -i "s/^#define EXP_TIMES .*/#define EXP_TIMES ${EXP_TIMES}/" "$CONFIG_FILE"
    sed -i "s/^#define HDFS_FLAG .*/#define HDFS_FLAG ${HDFS_FLAG}/" "$CONFIG_FILE"
    sed -i "s/^#define HDFS_DEBUG .*/#define HDFS_DEBUG ${HDFS_DEBUG}/" "$CONFIG_FILE"
    
    sed -i "s/^#define STORAGENODES_START_IP_ADDR .*/#define STORAGENODES_START_IP_ADDR ${START_NODE}/" "$CONFIG_FILE"
    sed -i "s/^#define PNETDEVICE_IP_ADDR .*/#define PNETDEVICE_IP_ADDR ${PND_NODE}/" "$CONFIG_FILE"
    echo "Configuration has been updated."

    #run
    echo "start run..."
    RESULT_SAVE_DIR="${RESULT_DIR}/${EC_K}_${EC_M}_${EC_X}_${CHUNK_SIZE_MB}_${SLICE_SIZE_KB}_${BAND_LOCATION}_${FULL_RECOVERY_STRIPE_MAX_NUM}_${MUL_FAIL_NUM}_${FLAG_WRITE_REPAIR}_${EXP_TIMES}"
    mkdir -p "${RESULT_SAVE_DIR}"
    ./make_run.sh $START_NODE $((START_NODE + EC_K+ EC_M + EC_X -1)) $PND_NODE
}

multiply_array() {
    local array=("${!1}")
    local multiplier=$2
    local result=()
    for value in "${array[@]}"; do
        result+=( $(($value * $multiplier)) )
    done
    echo "${result[@]}"
}

limit_e_net_onefailnode()
{
    local node_index=$1
    local multiplier=$2
    local node0_num=$((NODE_NUM - 1))
    declare -a nodes_suffix=( $(seq 0 $node0_num) )
    declare -a upload_speeds_Mbps=(958 139 589 207 155 735)
    declare -a download_speeds_Mbps=(214 110 168 307 134 501)

    if [ $node_index -ge 0 ] && [ $node_index -lt ${#nodes_suffix[@]} ]; then
        upload_speeds_Mbps[$node_index]=1000
        download_speeds_Mbps[$node_index]=1000
    else
        echo "Error: Invalid node index."
        return 1
    fi

    upload_speeds_Mbps=($(multiply_array upload_speeds_Mbps[@] $multiplier))
    download_speeds_Mbps=($(multiply_array download_speeds_Mbps[@] $multiplier))

    limit_network_speed nodes_suffix upload_speeds_Mbps download_speeds_Mbps
}

# ===============================================TEST===========================================
cd $SCRIPT_DIR

limit_e_net_onefailnode 0 1
EC_K=2
EC_M=2
EC_X=$((NODE_NUM - EC_K - EC_M))
FULL_RECOVERY_STRIPE_MAX_NUM=$(( ($WANT_REPAIRED_NUM * $NODE_NUM) / ($EC_K + $EC_M) ))
FILE_SIZE=$(($EC_K*$CHUNK_SIZE_MB))
config_run $EC_K $EC_M $EC_X $CHUNK_SIZE_MB $SLICE_SIZE_KB $BAND_LOCATION $FULL_RECOVERY_STRIPE_MAX_NUM $MUL_FAIL_NUM $FLAG_WRITE_REPAIR
./start_client.sh -hdfs

echo "========================================HDFS RAN run...!========================================"