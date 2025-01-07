#!/bin/bash
set -e # command fail will make scripe exit: add [false] Breakpoints

# ===============================================PATH===========================================
BASE_DIR="/home/ecRepair/RAN"       # Base directory for the project
NETINTERFACE="eth0"                 # Network interface to use
EXP_TIMES=5                         # Number of experiment repetitions
PND_NODE=100                        # IP suffix for the programmable network device
START_NODE=102                      # IP suffix for starting storage node
END_NODE=107                        # IP suffix for ending storage node
EC_K=2                              # Number of data chunks
EC_M=2                              # Number of parity chunks
CHUNK_SIZE_MB=64                    # Chunk size in MB
SLICE_SIZE_KB=1024                  # Slice size in KB
WANT_REPAIRED_NUM=5                # Number of repaired chunks
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

rm_write_file(){
    ./same_command.sh $START_NODE $END_NODE "rm -r $WRITE_FILE_DIR"
    ./same_command.sh $START_NODE $END_NODE "mkdir $WRITE_FILE_DIR"
}

# Function to generate a file with a given size using dd:
# generate_file 128 # Generate file: file name-128MB_src, file size-128MB
generate_file() {
    local bs_size=$1   # Block size
    local count_num=1 # Count
    local size_mb=$((bs_size * count_num)) # Calculate the size in MB
    local file_name="${WRITE_FILE_DIR}/${size_mb}MB_src" # Construct the file name based on size
    dd if=/dev/urandom of="$file_name" bs=${bs_size}M count=$count_num
}

clear_metadata() {
    rm -f "$FILE_DIR/stripe_bitmap/"* #for full recover
    rm -f "$FILE_DIR/file_size/"*     #metadata of all write operation
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
    local HDFS_FLAG=0
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
    echo "generate_file..."
    generate_file $(($1*CHUNK_SIZE_MB))

    #run
    echo "start run..."
    RESULT_SAVE_DIR="${RESULT_DIR}/${EC_K}_${EC_M}_${EC_X}_${CHUNK_SIZE_MB}_${SLICE_SIZE_KB}_${BAND_LOCATION}_${FULL_RECOVERY_STRIPE_MAX_NUM}_${MUL_FAIL_NUM}_${FLAG_WRITE_REPAIR}_${EXP_TIMES}"
    mkdir -p "${RESULT_SAVE_DIR}"
    ./make_run.sh $START_NODE $((START_NODE + EC_K+ EC_M + EC_X -1)) $PND_NODE
}

test_all_e()
{
    #./start_client.sh -w "${1}MB_src" "${1}MB_dst" | tee "${2}/w"
    #./start_client.sh -wm "${1}MB_src" "${1}MB_dst" | tee "${2}/wf"
    #Heterogeneous Network and Degraded Rpair
    ./start_client.sh -rdte "${1}MB_repair" "${1}MB_dst" | tee "${2}/edt"
    ./start_client.sh -rdre "${1}MB_repair" "${1}MB_dst" | tee "${2}/edr"
    ./start_client.sh -rdee "${1}MB_repair" "${1}MB_dst" | tee "${2}/ede"
    ./start_client.sh -rdne "${1}MB_repair" "${1}MB_dst" | tee "${2}/edn"
    #Heterogeneous Network and Full-node Recovery
    ./start_client.sh -rfte "${1}MB_repair" "${1}MB_dst" | tee "${2}/eft"
    ./start_client.sh -rfre "${1}MB_repair" "${1}MB_dst" | tee "${2}/efr"
    ./start_client.sh -rfee "${1}MB_repair" "${1}MB_dst" | tee "${2}/efe"
    sleep 3
    ./start_client.sh -rfne "${1}MB_repair" "${1}MB_dst" | tee "${2}/efn"
}

test_all_ed()
{
    echo "ecRepair-testdd: ${1}MB_src"
    #./start_client.sh -w "${1}MB_src" "${1}MB_dst"

    #Heterogeneous Network and Degraded Rpair
    ./start_client.sh -rdte "${1}MB_repair" "${1}MB_dst" | tee "${2}/edt"
    ./start_client.sh -rdre "${1}MB_repair" "${1}MB_dst" | tee "${2}/edr"
    ./start_client.sh -rdee "${1}MB_repair" "${1}MB_dst" | tee "${2}/ede"
    ./start_client.sh -rdne "${1}MB_repair" "${1}MB_dst" | tee "${2}/edn"
}

test_all_ef()
{
    #./start_client.sh -wm "${1}MB_src" "${1}MB_dst" | tee "${2}/wf"
    #Heterogeneous Network and Full-node Recovery
    ./start_client.sh -rfte "${1}MB_repair" "${1}MB_dst" | tee "${2}/eft"
    ./start_client.sh -rfre "${1}MB_repair" "${1}MB_dst" | tee "${2}/efr"
    ./start_client.sh -rfee "${1}MB_repair" "${1}MB_dst" | tee "${2}/efe"
    ./start_client.sh -rfne "${1}MB_repair" "${1}MB_dst" | tee "${2}/efn"
}

test_all_m()
{
    #./start_client.sh -w "${1}MB_src" "${1}MB_dst" | tee "${2}/w"
    #./start_client.sh -wm "${1}MB_src" "${1}MB_dst" | tee "${2}/wf"
    #Heterogeneous Network and Mul-chunk Rpair
    ./start_client.sh -rcte "${1}MB_repair" "${1}MB_dst" | tee "${2}/ect"
    ./start_client.sh -rcre "${1}MB_repair" "${1}MB_dst" | tee "${2}/ecr"
    ./start_client.sh -rcee "${1}MB_repair" "${1}MB_dst" | tee "${2}/ece"
    ./start_client.sh -rcne "${1}MB_repair" "${1}MB_dst" | tee "${2}/ecn"
    #Heterogeneous Network and Mul-node Recovery
    ./start_client.sh -rnte "${1}MB_repair" "${1}MB_dst" | tee "${2}/ent"
    ./start_client.sh -rnre "${1}MB_repair" "${1}MB_dst" | tee "${2}/enr"
    ./start_client.sh -rnee "${1}MB_repair" "${1}MB_dst" | tee "${2}/ene"
    ./start_client.sh -rnne "${1}MB_repair" "${1}MB_dst" | tee "${2}/enn"
}

test_all_w()
{
    ./start_client.sh -w "${1}MB_src" "${1}MB_dst"
    ./start_client.sh -wm "${1}MB_src" "${1}MB_dst"
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
./unlimit_network.sh $START_NODE $END_NODE $NETINTERFACE

echo "=======================================PREPARE FILE for accelerate: Only once ================================"
clear_metadata
rm_write_file
EC_K=2
EC_M=2
EC_X=$((NODE_NUM - EC_K - EC_M))
FILE_SIZE=$(($EC_K*$CHUNK_SIZE_MB))
FULL_RECOVERY_STRIPE_MAX_NUM=$(( ($WANT_REPAIRED_NUM * $NODE_NUM) / ($EC_K + $EC_M) ))
config_run $EC_K $EC_M $EC_X $CHUNK_SIZE_MB $SLICE_SIZE_KB $BAND_LOCATION $FULL_RECOVERY_STRIPE_MAX_NUM $MUL_FAIL_NUM $FLAG_WRITE_REPAIR
test_all_w $FILE_SIZE
# exit


limit_e_net_onefailnode 0 1
echo "=======================================TEST 1-1 BEGIN=======================================2+2"
EC_K=2
EC_M=2
EC_X=$((NODE_NUM - EC_K - EC_M))
FULL_RECOVERY_STRIPE_MAX_NUM=$(( ($WANT_REPAIRED_NUM * $NODE_NUM) / ($EC_K + $EC_M) ))
FILE_SIZE=$(($EC_K*$CHUNK_SIZE_MB))
config_run $EC_K $EC_M $EC_X $CHUNK_SIZE_MB $SLICE_SIZE_KB $BAND_LOCATION $FULL_RECOVERY_STRIPE_MAX_NUM $MUL_FAIL_NUM $FLAG_WRITE_REPAIR
test_all_e $FILE_SIZE $RESULT_SAVE_DIR
EC_K=2
EC_M=2
EC_X=$((NODE_NUM - EC_K - EC_M))
FULL_RECOVERY_STRIPE_MAX_NUM=$(( ($WANT_REPAIRED_NUM * $NODE_NUM) / ($EC_K + $EC_M) ))
FILE_SIZE=$(($EC_K*$CHUNK_SIZE_MB)) #end recover default
echo "========================================TEST 1-1 END========================================"

echo "========================================ALL TEST END!========================================"