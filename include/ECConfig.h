#ifndef ECCONFIG_H
#define ECCONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <isa-l.h>

#include "ShareFunc.h"
#include "FileOpt.h"
#include "NetTransfer.h"
#include "BandwidthWeight.h"
#include "HDFS.h"
#include "Exp.h"

/* experimente*/
#define EXP_TIMES 1

/* EC configure */
#define EC_K 12
#define EC_M 4
#define EC_N (EC_K + EC_M) // n of k+m EC, n>k
#define EC_X 4
#define EC_A (EC_N + EC_X) // number of all storage nodes
#define EC_W 8             // finite field 2^w
#define CHUNK_SIZE 67108864
#define SLICE_SIZE 1048576

/* bandwidth */
#define NUM_BAND_MAX 40           // Number of heterogeneous network environments in bandwidth.cpp
#define BAND_LOCATION 16
#define EC_A_MAX 20          //  Maximum EC_A
#define MAX_LINE_LENGTH 1000 // MAX line length in bandwidth file


/* repair */
#define NUM_ENCODING_CPU 1
#define NUM_ENCODING_CPU_MAX 64
#define FAIL_NODE_INDEX 0 // Position relative to STORAGENODES_START_IP_ADDR: [0,EC_N-1]
#define FLAG_WRITE_REPAIR 0
#define FULL_RECOVERY_STRIPE_MAX_NUM 50
#define MUL_FAIL_NUM 1
#define MUL_FAIL_START 0 // failed node [MUL_FAIL_START...MUL_FAIL_NUM]

/* file */
#define SCRIPT_PATH "script"                                             // saved executable file
#define BUILD_PATH "build"                                               // saved executable file
#define WRITE_PATH "test_file/write/"                                    // src_file and dst_file saved path
#define READ_PATH "test_file/read/"                                      // src_file and dst_file saved path
#define WRITE_RAND "test_file/write/rand_"                               // src_file and dst_file saved path
#define FILE_SIZE_PATH "test_file/file_size/file_size_"                  // file_size file saved path
#define FILE_SIZE_RAND "test_file/file_size/file_size_r_"                // file_size file saved path
#define FILE_STRIPE_BITMAP "test_file/stripe_bitmap/file_stripe_bitmap_" // bitmap file saved path
#define BANDWIDTH_PATH "test_file/bandwidth/"                            // nodes bandwidth saved path
#define REPAIR_PATH "test_file/repair/"                                  // repair data saved path
#define REPAIR_RAND "test_file/repair/rand_"                             // repair data saved path
#define MAX_PATH_LEN 256                                                 // Max length of file path

/* network */
#define IP_PREFIX "192.168.7."                  // storages ip prefix
#define STORAGENODES_START_IP_ADDR 102
#define CLIENT_COORDINATOR_NAMENODE_IP_ADDR 101 // client/coordinator/namenode ip
#define PNETDEVICE_IP_ADDR 100
#define EC_CLIENT_STORAGENODES_PORT 8000        // EC network port between client and nodes
#define EC_STORAGENODES_PORT 8100               // EC network port between nodes and nodes
#define EC_PND_STORAGENODES_PORT 8200           // EC network port between programmable network device and nodes
#define EC_PND_CLIENT_PORT 8300                 // EC network port between programmable network device and client
#define MUL_PORT_NUM 1                          // To achieve higher network bandwidth on EC2, such as 25Gbps

/* HDFS integrate */
#define HDFS_FLAG 0
#define HDFS_DEBUG 0
#define EC_HDFS_COORDINATOR_PORT 8400 // EC network port between hdfs and coordinator
#define HDFS_MAX_BP_COUNT FULL_RECOVERY_STRIPE_MAX_NUM
#define MAX_LINES (HDFS_MAX_BP_COUNT * 10)
#define HDFS_PATH1 "/home/ecRepair/hadoopData/dfs/data/current/"
#define HDFS_PATH2 "/current/finalized/subdir0/subdir0/"
#define HDFS_DEBUG_PATH "/home/ecRepair/HDFS_info"
#define HDFS_DEBUG_BLOCK "blk_-9223372036854775775"

/* just flag */
#define SEND 0
#define RECV 1
#define ENCODE 1
#define NOENCODE 0


/* other */
#define INT_MAX 2147483647

#define EC_ERROR -1
#define EC_OK 0

#define ERROR_RETURN_VALUE(message)                                   \
    do                                                                \
    {                                                                 \
        fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, message); \
        perror("OS_errorcode_information");                           \
        fflush(stdout);                                               \
        fflush(stderr);                                               \
        return EC_ERROR;                                              \
    } while (0)

#define ERROR_RETURN_NULL(message)                                    \
    do                                                                \
    {                                                                 \
        fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, message); \
        perror("OS_errorcode_information");                           \
        fflush(stdout);                                               \
        fflush(stderr);                                               \
        return NULL;                                                  \
    } while (0)

#define PRINT_FLUSH     \
    do                  \
    {                   \
        fflush(stdout); \
        fflush(stderr); \
    } while (0)

#define CALCULATE_TIME(start, end, result)                                                          \
    do                                                                                              \
    {                                                                                               \
        clock_gettime(CLOCK_MONOTONIC, &end);                                                       \
        result = ((end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec)) / 1000 / 1000; \
    } while (0)

#define ADD_TIME(start, end, result)                                                                 \
    do                                                                                               \
    {                                                                                                \
        clock_gettime(CLOCK_MONOTONIC, &end);                                                        \
        result += ((end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec)) / 1000 / 1000; \
    } while (0)

typedef struct clientCmd_s
{
    char *name;
    int (*func)(int argc, char *argv[]);
    char *msg;
} clientCmd_t;

typedef struct netMeta_s // chunk netMeta and data
{
    int sockfd;       // network sockfd fd
    int cmdType;      // 0=ecWrite, 1=ecRead,
                      // 2=rodt_fail/redt_fail, 3=rodt_heal/redt_heal,
                      // 4=rodp_fail/redp_fail, 5=rodp_heal/redp_heal
                      // 6=rode_fail/rede_fail/pnd_netec, 7=rode_heal/rede_heal/pnd_netec
                      // 8=rodn_fail/redn_fail/pnd_new, 9=rodn_heal/redn_fail/pnd_new
                      // 10=roft_fail/reft_fail, 11=roft_heal/reft_heal,
                      // 12=rofr_fail/refr_fail, 13=rofr_heal/refr_heal,
                      // 14=rofe_fail/refe_fail/pnd_netec, 15=rofe_heal/refe_heal/pnd_netec
                      // 16=rofn_fail/refn_fail/pnd_new, 17=rofn_heal/refn_heal/pnd_new
    int mulFailFlag;  // 0-df,1-cs
    int dstNodeIndex; // destination node.  for HDFS, is repair node
    int srcNodeIndex; // source node.

    int expCount;

    char *data;                     // chunk data or block data
    int size;                       // data size
    char dstFileName[MAX_PATH_LEN]; // dst filename on storages

} netMeta_t;

typedef struct mulThreadEncode_s
{
    int chunksize;
    int k;
    int needRepairDataNum;
    unsigned char *g_tbls;
    char **data;
    char **coding;
} mulThreadEncode_t;

typedef struct sglPortNet_s
{
    int nodeNum;    // Number of nodes requested
    int *sockfdArr; // Specify port
    char **data;    // The data location that each node needs to save
    int *size;      // The size each node needs to transfer
    int mode;       // mode: 0-send, 1-recv
} sglPortNet_t;

typedef struct degradedReadNetRecordN_s
{
    int (*twoBucketsSliceIndexInfo)[6];
    int divisionNum;
    int *divisionLines;
    unsigned char ***g_tbls2Arr;
} degradedReadNetRecordN_t;

typedef struct fullRecoverNetRecordN_s
{
    int stripeNum;
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N - 1];
    int (*twoBucketsSliceIndexInfo)[EC_N - 1][6];
    int(*divisionNum);
    int (*divisionLines)[2 * EC_N];
    int eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM];
    unsigned char ***g_tbls2Arr;
    unsigned char ****g_tbls3Arr;
} fullRecoverNetRecordN_t;

typedef struct mulPortNetMeta_s
{
    int nodeNum;            // Number of nodes requested
    int mode;               // mode: 0-send, 1-recv
    int totalSliceNum;      // total slice_num
    int flagEncode;         // 0-no,1 yes
    int **mulPortSockfdArr; // mul port
    char **dataRecv;        // The data location that each node needs to save
    char **dataSend;        // The data location that each node needs to save
    int *sliceIndexP;       // cur recv slice_index
    unsigned char *g_tbls;  // for decoding

    /* mul node */
    char ***recvDataBucket;       // recv
    queue_t *sliceIndexRecvQueue; // slice index for recv k
    int **sliceBitmap;            // record

    /* Not commonly used */
    int curNodeIndex;
    int curStripe;
    int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M];
    int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM];
    int *mergeNR;

    int startFailNodeIndex; // FAIL_NODE_INDEX for df, MUL_FAIL_START for cn, failNodeIndex for HDFS
    int failNodeNum;
    int *failNodeIndex;
} mulPortNetMeta_t;

/* HDFS integrate */
typedef struct hdfs_info_s
{
    int ecK;
    int ecM;
    int cellSize;
    char bpName[MAX_PATH_LEN];
    int bpNum;
    char bName[HDFS_MAX_BP_COUNT][EC_N][MAX_PATH_LEN];
    int bNodeIndex[HDFS_MAX_BP_COUNT][EC_N];
    int len[HDFS_MAX_BP_COUNT];

    int failNodeIndex;
    /* fail single chunk */
    int failBp;
    int failChunkIndex;
    int nodeIndexToChunkIndex[EC_N];
    int chunkIndexToNodeIndex[EC_N];

    /* fail single node */
    int failBpNum;
    int failFullRecoverBp[HDFS_MAX_BP_COUNT];
    int failFullRecoverChunkIndex[HDFS_MAX_BP_COUNT];
    int fullRecoverNodeIndexToChunkIndex[HDFS_MAX_BP_COUNT][EC_N];
    int fullRecoverChunkIndexToNodeIndex[HDFS_MAX_BP_COUNT][EC_N];
    char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN];
} hdfs_info_t;

typedef void (*type_handle_func)(netMeta_t *, int);

#endif