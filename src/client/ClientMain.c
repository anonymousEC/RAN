#include "ECConfig.h"
/* cleint include */
#include "SelectNodes.h"
#include "RepairChunkIndex.h"
#include "ClientRequest.h"
#include "ECEncode.h"
#include "HDFS.h"

int sockfdArr[EC_A_MAX + 1] = {0}, FailNodeIndex = FAIL_NODE_INDEX;
char curDir[MAX_PATH_LEN] = {0}, ecDir[MAX_PATH_LEN] = {0};
char dstFailFileName[MAX_PATH_LEN] = {0}, dstHealFileName[MAX_PATH_LEN] = {0};

/* full-node recovery */
int stripeNum = 0, eachStripeNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N];
int failStripeCountToAllStripeCount[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};

/* mul full-node recovery */
int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M] = {0};

/* HDFS */
hdfs_info_t *HDFSInfo = NULL;
pthread_mutex_t HDFSFullRecoveLock;
pthread_mutex_t HDFSDegradedReadLock;
int HDFSFullRecoveFlag = 0;

/* mul use variable */
static exp_t *exp2Arr[EXP_TIMES];
static char srcFileName[MAX_PATH_LEN] = {0}, dstFileName[MAX_PATH_LEN] = {0}, fileSizeFileName[MAX_PATH_LEN] = {0};
static char fileSizeStr[MAX_PATH_LEN] = {0};
static char eachStripeNodeIndexFileName[MAX_PATH_LEN] = {0};
static char netMode[16] = {0};
static int selectNodesIndex[EC_N] = {0};                                                       // selectNodesIndex[0] is FailNodeIndex, Degraded Read
static int selectChunksIndex[EC_N] = {0};                                                      // selectNodesIndex[0] is FailChunkIndex, Degraded Read
static int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N] = {0};                      // selectNodesIndex[0] is FailNodeIndex, Full Recover except N
static int selectChunksIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N] = {0};                     // selectNodesIndex[0] is FailChunkIndex, Full Recover except N
static int selectNodesIndexArrN[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N + MUL_FAIL_NUM - 1] = {0};  // selectNodesIndex[0] is FailNodeIndex, Full Recover  N
static int selectChunksIndexArrN[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N + MUL_FAIL_NUM - 1] = {0}; // selectNodesIndex[0] is FailChunkIndex, Full Recover  N
static int eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM] = {0};                          // each node need send: init value -1,where // 0-recordstripe,1-chunkindex

static void getFullRecoverInfo(char **argv)
{
    if (!HDFS_FLAG)
        for (int k = 0; k < FULL_RECOVERY_STRIPE_MAX_NUM; k++)
        {
            getEachStripeNodeIndexFileName(argv, ecDir, k, eachStripeNodeIndexFileName);
            int nodesIndexArr[EC_A] = {0}, nodeNum = 0;
            FILE *eachStripeNodeIndexFile = Fopen(eachStripeNodeIndexFileName, "r");
            while (fscanf(eachStripeNodeIndexFile, "%d", &nodesIndexArr[nodeNum]) != EOF && nodeNum < EC_N)
                nodeNum++;
            fclose(eachStripeNodeIndexFile);
            if (getArrIndex(nodesIndexArr, EC_N, FailNodeIndex) == EC_ERROR)
                continue;
            for (int i = 0; i < EC_N; i++)
                eachStripeNodeIndex[stripeNum][i] = nodesIndexArr[i];
            failStripeCountToAllStripeCount[stripeNum++] = k;
        }
    else
    {
        stripeNum = HDFSInfo->failBpNum;
        for (int k = 0; k < stripeNum; k++)
        {
            for (int i = 0; i < EC_N; i++)
                eachStripeNodeIndex[k][i] = HDFSInfo->bNodeIndex[HDFSInfo->failFullRecoverBp[k]][i] - STORAGENODES_START_IP_ADDR;
            failStripeCountToAllStripeCount[k] = k;
        }
    }
    return;
}

static void getMulFullRecoverInfo(char **argv)
{
    char tmpArgv3[MAX_PATH_LEN] = {0};
    for (int k = 0; k < FULL_RECOVERY_STRIPE_MAX_NUM; k++)
    {
        getEachStripeNodeIndexFileName(argv, ecDir, k, eachStripeNodeIndexFileName);
        int nodesIndexArr[EC_A] = {0};
        FILE *eachStripeNodeIndexFile = Fopen(eachStripeNodeIndexFileName, "r");
        int nodeNum = 0;
        while (fscanf(eachStripeNodeIndexFile, "%d", &nodesIndexArr[nodeNum]) != EOF && nodeNum < EC_N)
            nodeNum++;
        fclose(eachStripeNodeIndexFile);
        for (int n = 0; n < MUL_FAIL_NUM; n++)
            if (getArrIndex(nodesIndexArr, EC_N, n + MUL_FAIL_START) != EC_ERROR)
            {
                eachStripeFailNodeIndex[stripeNum][eachStripeFailNodeNum[stripeNum]] = n + MUL_FAIL_START;
                eachStripeFailNodeNum[stripeNum]++;
            }
        if (eachStripeFailNodeNum[stripeNum] == 0)
            continue;
        for (int i = 0; i < EC_N; i++)
            eachStripeNodeIndex[stripeNum][i] = nodesIndexArr[i];
        failStripeCountToAllStripeCount[stripeNum++] = k;
    }
    return;
}

/* note: In this program, the chunkindex and nodeindex of the data written are in increasing order,
 * so sometimes you don't actually need to call them. But they are not related in HDFS.
 */
static void g_tblsSortbyIncreasingNodeIndexDR_TE(int *selectNodesIndex, int *selectChunksIndex, int *healChunkIndex, unsigned char *g_tblsChunk)
{
    unsigned char *g_tblsNode = (unsigned char *)Calloc(1 * EC_K * 32, sizeof(unsigned char));
    int healNodesIndex[EC_K], count = 0;
    for (int i = 0; i < EC_K; i++)
        healNodesIndex[i] = selectNodesIndex[i + 1]; // 0 is failNode, other is healNode
    sortMinBubble(healNodesIndex, EC_K);
    for (int j = 0; count != EC_K; j++)
        if (healChunkIndex[count] == selectChunksIndex[j])
        {
            int index = getArrIndex(healNodesIndex, EC_K, selectNodesIndex[j]); // increasing node index order
            memcpy(g_tblsNode + index * 32, g_tblsChunk + (count++) * 32, 32);
            j = 0;
        }
    memcpy(g_tblsChunk, g_tblsNode, EC_K * 32); // gtbls chunks to be gtbls node
    free(g_tblsNode);
}

static void g_tblsSortbyIncreasingNodeIndexDR_R(int *selectNodesIndex, int *selectChunksIndex, int *healChunkIndex, unsigned char **g_tblsArrChunk)
{
    unsigned char *g_tblsBufNode = NULL, **g_tblsArrNode = NULL;
    create2PointerArrUC(&g_tblsBufNode, &g_tblsArrNode, EC_K, 2 * 1 * 32);

    int healNodesIndex[EC_K], count = 0;
    for (int i = 0; i < EC_K; i++)
        healNodesIndex[i] = selectNodesIndex[i + 1]; // 0 is failNode, other is healNode
    sortMinBubble(healNodesIndex, EC_K);
    for (int j = 0; count != EC_K; j++)
        if (healChunkIndex[count] == selectChunksIndex[j])
        {
            int index = getArrIndex(healNodesIndex, EC_K, selectNodesIndex[j]); // increasing node index order
            memcpy(g_tblsArrNode[index], g_tblsArrChunk[count++], 2 * 32);
            j = 0;
        }
    for (int i = 0; i < EC_K; i++)
        memcpy(g_tblsArrChunk[i], g_tblsArrNode[i], 2 * 32); // gtbls chunks to be gtbls node
    delete2ArrUC(g_tblsBufNode, g_tblsArrNode);
}

static void g_tblsSortbyIncreasingNodeIndexDR_N(int *selectNodesIndex, int *selectChunksIndex, int healChunkIndex[][EC_N], int divisionNum, unsigned char **g_tblsArrChunk)
{
    unsigned char *g_tblsBufNode = NULL, **g_tblsArrNode = NULL;
    create2PointerArrUC(&g_tblsBufNode, &g_tblsArrNode, divisionNum, EC_K * 32);

    int lowb, upb;
    for (int k = 0; k < divisionNum; k++)
    {

        int selectNodesIndexTmp[EC_K], count = 0;
        for (int j = 0; count != EC_K; j++)
            if (healChunkIndex[k][count] == selectChunksIndex[j])
            {
                selectNodesIndexTmp[count++] = selectNodesIndex[j]; // still mix order,just get actual selectnodes
                j = 0;
            }

        int healNodesIndex[EC_K];
        for (int i = 0; i < EC_K; i++)
            healNodesIndex[i] = selectNodesIndexTmp[i]; // all is healNode
        sortMinBubble(healNodesIndex, EC_K);
        count = 0;
        for (int j = 0; count != EC_K; j++)
        {
            if (healChunkIndex[k][count] == selectChunksIndex[j])
            {
                int index = getArrIndex(healNodesIndex, EC_K, selectNodesIndex[j]); // increasing node index order
                memcpy(g_tblsArrNode[k] + index * 32, g_tblsArrChunk[k] + (count++) * 32, 32);
                j = 0;
            }
        }
        memcpy(g_tblsArrChunk[k], g_tblsArrNode[k], EC_K * 32); // gtbls chunks to be gtbls node
    }
    delete2ArrUC(g_tblsBufNode, g_tblsArrNode);
}

static void g_tblsSortbyIncreasingNodeIndexDR_MR(int *selectNodesIndex, int *selectChunksIndex, int *healChunkIndex, unsigned char **g_tblsArrChunk)
{
    unsigned char *g_tblsBufNode = NULL, **g_tblsArrNode = NULL;
    create2PointerArrUC(&g_tblsBufNode, &g_tblsArrNode, EC_K, 2 * 1 * 32);

    int healNodesIndex[EC_K], count = 0;
    for (int i = 0; i < EC_K; i++)
        healNodesIndex[i] = selectNodesIndex[i + MUL_FAIL_NUM]; // 0 is failNode, other is healNode
    sortMinBubble(healNodesIndex, EC_K);
    for (int j = 0; count != EC_K; j++)
        if (healChunkIndex[count] == selectChunksIndex[j])
        {
            int index = getArrIndex(healNodesIndex, EC_K, selectNodesIndex[j]); // increasing node index order
            memcpy(g_tblsArrNode[index], g_tblsArrChunk[count++], 2 * 32);
            j = 0;
        }
    for (int i = 0; i < EC_K; i++)
        memcpy(g_tblsArrChunk[i], g_tblsArrNode[i], 2 * 32); // gtbls chunks to be gtbls node
    delete2ArrUC(g_tblsBufNode, g_tblsArrNode);
}

/* *******************************************************
**********************************************************
**********************************************************
**********************************************************
**********************************************************
**/
/* mul use func */
static void repairStart(int mode, char **argv)
{
    /* check */
    if (CHUNK_SIZE % SLICE_SIZE != 0)
        ERROR_RETURN_NULL("error chunksize or splicesize");
    if (MUL_FAIL_START + MUL_FAIL_NUM > EC_N || MUL_FAIL_NUM > EC_M)
        ERROR_RETURN_VALUE("MUL_FAIL_START + MUL_FAIL_NUM > EC_N || MUL_FAIL_NUM > EC_M");

    printConfigure();
    strcpy(netMode, argv[1]);
    if (mode == 0) // degraded Read
        getDegradedReadExpFilename(argv, ecDir, dstFailFileName, dstHealFileName);
    else if (mode == 1) // full recover
    {
        getFullRecoverExpFilename(argv, ecDir, dstFailFileName, dstHealFileName);
        getFullRecoverInfo(argv);
    }
    else if (mode == 2) // mul full recover
    {
        getFullRecoverExpFilename(argv, ecDir, dstFailFileName, dstHealFileName);
        getMulFullRecoverInfo(argv);
    }
    PRINT_FLUSH;
}

static void repairRecvExp(int nodeNum, int curTimes)
{
    exp_t *expArr = (exp_t *)Malloc(sizeof(exp_t) * (nodeNum)); // only the func free
    recvExp(expArr, sockfdArr, nodeNum);
    printExp(expArr, nodeNum, curTimes);
    exp2Arr[curTimes] = expArr;
}

static void repairExpEnd(int nodeNum)
{
    printAvgExp(exp2Arr, EXP_TIMES, nodeNum);
    for (int j = 0; j < EXP_TIMES; j++)
        free(exp2Arr[j]);
    for (int i = 0; i < nodeNum; i++)
        shutdown(sockfdArr[i], SHUT_RDWR);
    PRINT_FLUSH;
}

/* *******************************************************
**********************************************************
**********************************************************
**********************************************************
**********************************************************
**/
/* Debug */

static void printDegradedReadInfoTRE()
{
    printArrINT(EC_N, selectNodesIndex, "selectNodesIndex");
    printArrINT(EC_N, selectChunksIndex, "selectChunksIndex");
}

static void printDegradedReadInfoN(int twoBucketsSliceIndexInfo[EC_N - 1][6], int divisionLines[2 * EC_N], int divisionNum)
{
    printArrINT(EC_N, selectNodesIndex, "selectNodesIndex");
    printArrINT(EC_N, selectChunksIndex, "selectChunksIndex");
    print2ArrINT(EC_N - 1, 6, twoBucketsSliceIndexInfo, "twoBucketsSliceIndexInfo");
    printArrINT(divisionNum, divisionLines, "divisionLines");
}

static void printFullRecoveryInfoTRE()
{
    print2ArrINT(stripeNum, EC_N, eachStripeNodeIndex, "eachStripeNodeIndex");
    printArrINT(stripeNum, failStripeCountToAllStripeCount, "failStripeCountToAllStripeCount");
    print2ArrINT(stripeNum, EC_N, selectNodesIndexArr, "selectNodesIndexArr");
    print2ArrINT(stripeNum, EC_N, selectChunksIndexArr, "selectChunksIndexArr");
    print3ArrINT(2, EC_A, FULL_RECOVERY_STRIPE_MAX_NUM, eachNodeInfo, "eachNodeInfo");
}

static void printFullRecoveryInfoN(int (*twoBucketsSliceSizeInfo)[EC_N - 1][6], int (*twoBucketsSliceIndexInfo)[EC_N - 1][6],
                                   int (*divisionLines)[2 * EC_N], int(*divisionNum))
{
    print2ArrINT(stripeNum, EC_N, eachStripeNodeIndex, "eachStripeNodeIndex");
    printArrINT(stripeNum, failStripeCountToAllStripeCount, "failStripeCountToAllStripeCount");
    print2ArrINT(stripeNum, EC_N + MUL_FAIL_NUM - 1, selectNodesIndexArrN, "selectNodesIndexArrN");
    print2ArrINT(stripeNum, EC_N + MUL_FAIL_NUM - 1, selectChunksIndexArrN, "selectChunksIndexArrN");
    print3ArrINT(2, EC_A, FULL_RECOVERY_STRIPE_MAX_NUM, eachNodeInfo, "eachNodeInfo");

    for (int k = 0; k < stripeNum; k++)
    {
        printf("curStripe = %d\n", k);
        // print2ArrINT(EC_N - 1, 6, twoBucketsSliceSizeInfo[k], "twoBucketsSliceSizeInfo");
        print2ArrINT(EC_N - 1, 6, twoBucketsSliceIndexInfo[k], "twoBucketsSliceIndexInfo");
        printf("divisionNum = %d\n", divisionNum[k]);
        printArrINT(2 * EC_N, divisionLines[k], "division");
    }
}
static void printMulFailInfo()
{
    printArrINT(stripeNum, eachStripeFailNodeNum, "eachStripeFailNodeNum");
    print2ArrINT(stripeNum, EC_M, eachStripeFailNodeIndex, "eachStripeFailNodeIndex");
}

/* *******************************************************
**********************************************************
**********************************************************
**********************************************************
**********************************************************
**/
/* Client Command */
static int handleClientMain(int argc, char *argv[]);
static int ecRead(int argc, char **argv)
{
    printf("[ecRead begin]\n");
    PRINT_FLUSH;
    FILE *srcFp;
    int fileSize;
    getECFileName(ecDir, READ_PATH, argv[2], srcFileName);
    getECFileName(ecDir, WRITE_PATH, argv[3], dstFileName);
    getECFileName(ecDir, FILE_SIZE_PATH, argv[3], fileSizeFileName);

    /* EC arguments */
    unsigned char *dataChunkBuf = NULL, **dataChunk = NULL, *codingChunkBuf = NULL, **codingChunk = NULL;
    create2PointerArrUC(&dataChunkBuf, &dataChunk, EC_K, CHUNK_SIZE);
    create2PointerArrUC(&codingChunkBuf, &codingChunk, EC_M, CHUNK_SIZE);
    char **repairData = (char **)Malloc(sizeof(char *) * EC_N); // repair data chunk

    /* check fileSize */
    openReadFile(fileSizeFileName, "rb", fileSizeStr, sizeof(fileSizeStr));
    if ((fileSize = atoi(fileSizeStr)) != EC_K * CHUNK_SIZE) // check readsize
        ERROR_RETURN_VALUE("error readsize: different chunksize or ecK+ecM");

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex[EC_N], healChunkIndex[EC_N], failChunkNum = 0, healChunkNum = 0;
    for (int i = 0; i < EC_N; i++)
    {
        sockfdArr[i] = -1;
        if ((clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, i)) == EC_ERROR) // only the func close sockfd
            failChunkIndex[failChunkNum++] = i;
        else
            healChunkIndex[healChunkNum++] = i;
        if (healChunkNum == EC_K)
            break;
    }
    if (healChunkNum < EC_K)
        ERROR_RETURN_VALUE("error num_nodes more than EC_M");

    /* read chunk from k nodes */
    clientRequestReadKChunks(dataChunk, dstFileName, healChunkIndex);
    for (int i = 0; i < healChunkNum; i++)
        shutdown(sockfdArr[healChunkIndex[i]], SHUT_RDWR);

    /* repair data chunks */
    repairDataChunks(healChunkIndex, healChunkNum, failChunkIndex, failChunkNum, dataChunk, codingChunk, repairData);

    /* clear file and write data */
    openWriteMulFile(srcFileName, "wb", repairData, CHUNK_SIZE, EC_K);
    free(repairData);
    delete2ArrUC(dataChunkBuf, dataChunk);
    delete2ArrUC(codingChunkBuf, codingChunk);
    printf("[ecRead end]\n");
    return EC_OK;
}

static int ecWrite(int argc, char **argv)
{
    printf("[ecWrite begin]\n");
    PRINT_FLUSH;
    /* File arguments */
    FILE *srcFp;             // src_file pointers
    int fileSize;            // size of file
    struct stat file_status; // finding file size
    getECFileName(ecDir, WRITE_PATH, argv[2], srcFileName);
    getECFileName(ecDir, WRITE_PATH, argv[3], dstFileName);
    getECFileName(ecDir, FILE_SIZE_PATH, argv[3], fileSizeFileName);

    /* EC arguments */
    unsigned char *dataChunkBuf = NULL, **dataChunk = NULL, *codingChunkBuf = NULL, **codingChunk = NULL;
    create2PointerArrUC(&dataChunkBuf, &dataChunk, EC_K, CHUNK_SIZE);
    create2PointerArrUC(&codingChunkBuf, &codingChunk, EC_M, CHUNK_SIZE);
    /*One IO read file to dataChunkBuf*/
    if ((fileSize = getFileSize(srcFileName)) != EC_K * CHUNK_SIZE)
        ERROR_RETURN_VALUE("fileSize != EC_K * CHUNK_SIZE");
    readFileToBuffer(srcFileName, dataChunkBuf, EC_K * CHUNK_SIZE);

    /* Encode */
    struct timespec time_start, time_end; // test time
    long long elapsed_time;
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    encodeDataChunks(dataChunk, codingChunk);
    clock_gettime(CLOCK_MONOTONIC, &time_end);                                                            // Get the start time
    elapsed_time = (time_end.tv_sec - time_start.tv_sec) * 1e9 + (time_end.tv_nsec - time_start.tv_nsec); // Calculate execution time (in milliseconds)
    // printf("ecWrite: encoding time =  %lld ms\n", elapsed_time / 1000 / 1000);                           // Print execution time

    /* write chunks to each storages */
    for (int i = 0; i < EC_N; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, i);
    clientRequestWriteNChunks(dataChunk, codingChunk, dstFileName, sockfdArr);
    for (int i = 0; i < EC_N; i++)
        shutdown(sockfdArr[i], SHUT_RDWR);

    /* write file size to fileSize filename */
    sprintf(fileSizeStr, "%d", fileSize);
    openWriteFile(fileSizeFileName, "wb", fileSizeStr, sizeof(fileSizeStr));
    delete2ArrUC(dataChunkBuf, dataChunk);
    delete2ArrUC(codingChunkBuf, codingChunk);
    printf("[ecWrite end]\n");
    return EC_OK;
}

/* Matching ecRandWrite, ecRandMulWrite, ecMulWrite */
static int ecAnyRead(int argc, char **argv)
{
    printf("[ecAnyRead begin]\n");
    FILE *srcFp;
    int fileSize;
    getECFileName(ecDir, READ_PATH, argv[2], srcFileName);
    getECFileName(ecDir, WRITE_RAND, argv[3], dstFileName);
    getECFileName(ecDir, FILE_SIZE_RAND, argv[3], fileSizeFileName);
    getECFileName(ecDir, FILE_STRIPE_BITMAP, argv[3], eachStripeNodeIndexFileName);

    /* EC arguments */
    unsigned char *dataChunkBuf = NULL, **dataChunk = NULL, *codingChunkBuf = NULL, **codingChunk = NULL;
    create2PointerArrUC(&dataChunkBuf, &dataChunk, EC_K, CHUNK_SIZE);
    create2PointerArrUC(&codingChunkBuf, &codingChunk, EC_M, CHUNK_SIZE);
    char **repairData = (char **)Malloc(sizeof(char *) * EC_N); // repair data chunk

    /* check fileSize */
    openReadFile(fileSizeFileName, "rb", fileSizeStr, sizeof(fileSizeStr));
    if ((fileSize = atoi(fileSizeStr)) != EC_K * CHUNK_SIZE) // check readsize
        ERROR_RETURN_VALUE("error readsize: different chunksize or ecK+ecM");

    /* get stripe bitmap distribution */
    int nodesIndexArr[EC_A] = {0};
    FILE *eachStripeNodeIndexFile = Fopen(eachStripeNodeIndexFileName, "r");
    int nodeNum = 0;
    while (fscanf(eachStripeNodeIndexFile, "%d", &nodesIndexArr[nodeNum]) != EOF && nodeNum < EC_N)
        nodeNum++;
    fclose(eachStripeNodeIndexFile);

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex[EC_N], healChunkIndex[EC_N], failChunkNum = 0, healChunkNum = 0;
    for (int i = 0; i < EC_N; i++)
    {
        sockfdArr[i] = -1;
        if ((clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, nodesIndexArr[i])) == EC_ERROR) // only the func close sockfd
            failChunkIndex[failChunkNum++] = i;
        else
            healChunkIndex[healChunkNum++] = i;
        if (healChunkNum == EC_K)
            break;
    }
    if (healChunkNum < EC_K)
        ERROR_RETURN_VALUE("error num_nodes more than EC_M");

    /* read chunk from k nodes */
    clientRequestReadKChunks(dataChunk, dstFileName, healChunkIndex);
    for (int i = 0; i < healChunkNum; i++)
        shutdown(sockfdArr[healChunkIndex[i]], SHUT_RDWR);

    /* repair data chunks */
    repairDataChunks(healChunkIndex, healChunkNum, failChunkIndex, failChunkNum, dataChunk, codingChunk, repairData);

    /* clear file and write data */
    openWriteMulFile(srcFileName, "wb", repairData, CHUNK_SIZE, EC_K);
    free(repairData);
    delete2ArrUC(dataChunkBuf, dataChunk);
    delete2ArrUC(codingChunkBuf, codingChunk);
    printf("[ecAnyRead end]\n");
    return EC_OK;
}

static int ecRandWrite(int argc, char **argv)
{
    printf("[ecRandWrite begin]\n");
    FILE *srcFp;
    int fileSize;
    struct stat file_status;
    getECFileName(ecDir, WRITE_PATH, argv[2], srcFileName);
    getECFileName(ecDir, WRITE_RAND, argv[3], dstFileName);
    getECFileName(ecDir, FILE_SIZE_RAND, argv[3], fileSizeFileName);
    getECFileName(ecDir, FILE_STRIPE_BITMAP, argv[3], eachStripeNodeIndexFileName);

    /* check- not support same name */
    FILE *eachStripeNodeIndexFile = fopen(eachStripeNodeIndexFileName, "r");
    if (eachStripeNodeIndexFile != NULL)
    {
        fclose(eachStripeNodeIndexFile);
        ERROR_RETURN_VALUE("exist same file name: rand write not support overwrite");
    }

    /* EC arguments */
    unsigned char *dataChunkBuf = NULL, **dataChunk = NULL, *codingChunkBuf = NULL, **codingChunk = NULL;
    create2PointerArrUC(&dataChunkBuf, &dataChunk, EC_K, CHUNK_SIZE);
    create2PointerArrUC(&codingChunkBuf, &codingChunk, EC_M, CHUNK_SIZE);

    /*One IO read file to dataChunkBuf*/
    if ((fileSize = getFileSize(srcFileName)) != EC_K * CHUNK_SIZE)
        ERROR_RETURN_VALUE("fileSize != EC_K * CHUNK_SIZE");
    readFileToBuffer(srcFileName, dataChunkBuf, EC_K * CHUNK_SIZE);

    /* Encode */
    struct timespec time_start, time_end; // test time
    long long elapsed_time;
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    encodeDataChunks(dataChunk, codingChunk);
    clock_gettime(CLOCK_MONOTONIC, &time_end);                                                            // Get the start time
    elapsed_time = (time_end.tv_sec - time_start.tv_sec) * 1e9 + (time_end.tv_nsec - time_start.tv_nsec); // Calculate execution time (in milliseconds)
    // printf("ecWrite: encoding time =  %lld ms\n", elapsed_time / 1000 / 1000);                           // Print execution time

    /* rand n nodes */
    int nodesIndexArr[EC_A] = {0};
    selectNodesRand(nodesIndexArr);

    /* write chunks to each storages */
    for (int i = 0; i < EC_N; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, nodesIndexArr[i]);
    clientRequestWriteNChunks(dataChunk, codingChunk, dstFileName, sockfdArr);
    for (int i = 0; i < EC_N; i++)
        shutdown(sockfdArr[i], SHUT_RDWR);

    /* write file size to fileSize filename */
    sprintf(fileSizeStr, "%d", fileSize);
    openWriteFile(fileSizeFileName, "wb", fileSizeStr, sizeof(fileSizeStr));

    /* write stripe bitmap */
    eachStripeNodeIndexFile = Fopen(eachStripeNodeIndexFileName, "w");
    for (int i = 0; i < EC_N; i++)
        fprintf(eachStripeNodeIndexFile, "%d ", nodesIndexArr[i]);
    fclose(eachStripeNodeIndexFile);
    delete2ArrUC(dataChunkBuf, dataChunk);
    delete2ArrUC(codingChunkBuf, codingChunk);
    printf("[ecRandWrite end]\n");
    return EC_OK;
}

/* just for full-node recovey exp */
static int ecMulRandWrite(int argc, char **argv)
{
    printf("[ecMulRandWrite begin]\n");

    /* modify argv */
    char **modifiedArgv = (char **)Malloc(argc * sizeof(char *));
    for (int j = 0; j < argc; j++)
    {
        int length = strlen(argv[j]);
        modifiedArgv[j] = (char *)Malloc((length + 5) * sizeof(char *));
        strcpy(modifiedArgv[j], argv[j]);
    }
    for (int i = 0; i < FULL_RECOVERY_STRIPE_MAX_NUM; i++)
    {
        sprintf(modifiedArgv[3], "%s%s%d", argv[3], "fr", i);
        /* signle rand_write */
        ecRandWrite(argc, modifiedArgv);
        printf("[%d-OK] rand write file [%s]\n", i, modifiedArgv[3]);
    }
    for (int j = 0; j < argc; j++)
        free(modifiedArgv[j]);
    free(modifiedArgv);
    return EC_OK;
}

static int ecMulWriteImp(int argc, char **argv, int *nodesIndexArr)
{
    FILE *srcFp;
    int fileSize;
    struct stat file_status;
    getECFileName(ecDir, WRITE_PATH, argv[2], srcFileName);
    getECFileName(ecDir, WRITE_RAND, argv[3], dstFileName);
    getECFileName(ecDir, FILE_SIZE_RAND, argv[3], fileSizeFileName);
    getECFileName(ecDir, FILE_STRIPE_BITMAP, argv[3], eachStripeNodeIndexFileName);

    /* check- not support same name */
    FILE *eachStripeNodeIndexFile = fopen(eachStripeNodeIndexFileName, "r");
    if (eachStripeNodeIndexFile != NULL)
    {
        fclose(eachStripeNodeIndexFile);
        ERROR_RETURN_VALUE("exist same file name: rand write not support overwrite");
    }
    /* EC arguments */
    unsigned char *dataChunkBuf = NULL, **dataChunk = NULL, *codingChunkBuf = NULL, **codingChunk = NULL;
    create2PointerArrUC(&dataChunkBuf, &dataChunk, EC_K, CHUNK_SIZE);
    create2PointerArrUC(&codingChunkBuf, &codingChunk, EC_M, CHUNK_SIZE);

    /*One IO read file to dataChunkBuf*/
    if ((fileSize = getFileSize(srcFileName)) != EC_K * CHUNK_SIZE)
        ERROR_RETURN_VALUE("fileSize != EC_K * CHUNK_SIZE");
    readFileToBuffer(srcFileName, dataChunkBuf, EC_K * CHUNK_SIZE);

    /* Encode */
    struct timespec time_start, time_end; // test time
    long long elapsed_time;
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    encodeDataChunks(dataChunk, codingChunk);
    clock_gettime(CLOCK_MONOTONIC, &time_end);                                                            // Get the start time
    elapsed_time = (time_end.tv_sec - time_start.tv_sec) * 1e9 + (time_end.tv_nsec - time_start.tv_nsec); // Calculate execution time (in milliseconds)
    // printf("ecWrite: encoding time =  %lld ms\n", elapsed_time / 1000 / 1000);                           // Print execution time

    /* write chunks to each storages */
    for (int i = 0; i < EC_N; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, nodesIndexArr[i]);
    clientRequestWriteNChunks(dataChunk, codingChunk, dstFileName, sockfdArr);
    for (int i = 0; i < EC_N; i++)
        shutdown(sockfdArr[i], SHUT_RDWR);

    /* write file size to fileSize filename */
    sprintf(fileSizeStr, "%d", fileSize);
    openWriteFile(fileSizeFileName, "wb", fileSizeStr, sizeof(fileSizeStr));

    /* write stripe bitmap */
    eachStripeNodeIndexFile = Fopen(eachStripeNodeIndexFileName, "w");
    for (int i = 0; i < EC_N; i++)
        fprintf(eachStripeNodeIndexFile, "%d ", nodesIndexArr[i]);
    fclose(eachStripeNodeIndexFile);
    delete2ArrUC(dataChunkBuf, dataChunk);
    delete2ArrUC(codingChunkBuf, codingChunk);
    return EC_OK;
}

/* just for full-node recovey exp */
static int ecMulWrite(int argc, char **argv)
{
    printf("[ecMulWrite begin]\n");

    /* modify argv */
    char **modifiedArgv = (char **)Malloc(argc * sizeof(char *));
    for (int j = 0; j < argc; j++)
    {
        int length = strlen(argv[j]);
        modifiedArgv[j] = (char *)Malloc((length + 5) * sizeof(char *));
        strcpy(modifiedArgv[j], argv[j]);
    }

    int nodesIndexArr[EC_N] = {0};
    int nodesPool[EC_A];
    for (int i = 0; i < EC_A; i++) // reset node pool
        nodesPool[i] = i;
    srand(time(NULL));
    int totalNodes = EC_A;
    for (int i = 0; i < FULL_RECOVERY_STRIPE_MAX_NUM; i++)
    {
        sprintf(modifiedArgv[3], "%s%s%d", argv[3], "fr", i);

        /* select nodes */

        for (int j = 0; j < EC_N; j++) // initSelectNodes
            nodesIndexArr[j] = -1;
        int selectCount = 0;
        for (int j = 0; j < EC_N && totalNodes > 0; j++)
        {

            int randIndex = rand() % totalNodes;
            nodesIndexArr[selectCount++] = nodesPool[randIndex];
            for (int k = randIndex; k < totalNodes - 1; k++)
                nodesPool[k] = nodesPool[k + 1];
            totalNodes--;
        }
        if (selectCount < EC_N) // if Not enough nodes left, continue
        {
            for (int j = 0; j < EC_A; j++) // reset node pool
                nodesPool[j] = j;
            totalNodes = EC_A;
            int needSelectCount = EC_N - selectCount;
            for (int j = 0; j < needSelectCount && totalNodes > 0; j++)
            {

                int randIndex = rand() % totalNodes;
                if (getArrIndex(nodesIndexArr, EC_N, nodesPool[randIndex]) != -1) // have same node
                {
                    j--; // reselect nodes
                    continue;
                }
                nodesIndexArr[selectCount++] = nodesPool[randIndex];

                for (int k = randIndex; k < totalNodes - 1; k++)
                    nodesPool[k] = nodesPool[k + 1];
                totalNodes--;
            }
        }
        sortMinBubble(nodesIndexArr, EC_N);
        ecMulWriteImp(argc, modifiedArgv, nodesIndexArr);
    }
    for (int j = 0; j < argc; j++)
        free(modifiedArgv[j]);
    free(modifiedArgv);
    return EC_OK;
}

static int degradedReadTra(int argc, char **argv)
{
    printf("[degradedReadTra begin]\n");
    repairStart(0, argv);

    /* select nodes */
    if (netMode[4] == 'o')
        selectNodesDegradedReadTRENNetO(selectNodesIndex, selectChunksIndex);
    else if (netMode[4] == 'e')
        selectNodesDegradedReadTENetE(selectNodesIndex, selectChunksIndex);

    /* init Net */
    for (int i = 0; i < EC_K + 1; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, selectNodesIndex[i]); // only the func close sockfd

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex[EC_N], healChunkIndex[EC_N], failChunkNum = 0, healChunkNum = 0;
    repairChunkIndexDegradedReadTRE(selectNodesIndex, selectChunksIndex, healChunkIndex, &healChunkNum, failChunkIndex, &failChunkNum);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsChunk = (unsigned char *)Calloc(1 * EC_K * 32, sizeof(unsigned char));
    repairMatrixTE(healChunkIndex, healChunkNum, failChunkIndex, failChunkNum, g_tblsChunk);
    g_tblsSortbyIncreasingNodeIndexDR_TE(selectNodesIndex, selectChunksIndex, healChunkIndex, g_tblsChunk);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestDegradedReadT(selectNodesIndex, g_tblsChunk, j);
        repairRecvExp(EC_K + 1, j);
    }
    /* EXP END */

    free(g_tblsChunk);
    repairExpEnd(EC_K + 1);
    printf("[degradedReadTra end]\n");
    return EC_OK;
}

static int degradedReadRP(int argc, char **argv)
{
    printf("[degradedReadRP begin]\n");
    repairStart(0, argv);

    /* select nodes */
    if (netMode[4] == 'o')
        selectNodesDegradedReadTRENNetO(selectNodesIndex, selectChunksIndex);
    else if (netMode[4] == 'e')
        selectNodesDegradedReadRNetE(selectNodesIndex, selectChunksIndex);

    /* init Net */
    for (int i = 0; i < EC_K + 1; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, selectNodesIndex[i]); // only the func close sockfd

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex[EC_N], healChunkIndex[EC_N], failChunkNum = 0, healChunkNum = 0;
    repairChunkIndexDegradedReadTRE(selectNodesIndex, selectChunksIndex, healChunkIndex, &healChunkNum, failChunkIndex, &failChunkNum);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL;
    create2PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, EC_K, 2 * 1 * 32);
    repairMatrixR(healChunkIndex, healChunkNum, failChunkIndex, failChunkNum, g_tblsArrChunk);
    g_tblsSortbyIncreasingNodeIndexDR_R(selectNodesIndex, selectChunksIndex, healChunkIndex, g_tblsArrChunk);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestDegradedReadR(selectNodesIndex, g_tblsArrChunk, j);
        repairRecvExp(EC_K + 1, j);
    }
    /* EXP END */

    delete2ArrUC(g_tblsBufChunk, g_tblsArrChunk);
    repairExpEnd(EC_K + 1);
    printf("[degradedReadRP end]\n");
    return EC_OK;
}

static int degradedReadNetEC(int argc, char **argv)
{
    printf("[degradedReadNetEC begin]\n");
    repairStart(0, argv);

    /* select nodes */
    if (netMode[4] == 'o')
        selectNodesDegradedReadTRENNetO(selectNodesIndex, selectChunksIndex);
    else if (netMode[4] == 'e')
        selectNodesDegradedReadTENetE(selectNodesIndex, selectChunksIndex);

    /* init Net */
    for (int i = 0; i < EC_K + 1; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, selectNodesIndex[i]); // only the func close sockfd
    clientInitNetwork(&sockfdArr[EC_K + 1], EC_PND_CLIENT_PORT, PNETDEVICE_IP_ADDR - STORAGENODES_START_IP_ADDR);

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex[EC_N], healChunkIndex[EC_N], failChunkNum = 0, healChunkNum = 0;
    repairChunkIndexDegradedReadTRE(selectNodesIndex, selectChunksIndex, healChunkIndex, &healChunkNum, failChunkIndex, &failChunkNum);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsChunk = (unsigned char *)Calloc(1 * EC_K * 32, sizeof(unsigned char));
    repairMatrixTE(healChunkIndex, healChunkNum, failChunkIndex, failChunkNum, g_tblsChunk);
    g_tblsSortbyIncreasingNodeIndexDR_TE(selectNodesIndex, selectChunksIndex, healChunkIndex, g_tblsChunk);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestDegradedReadE(selectNodesIndex, g_tblsChunk, j);
        repairRecvExp(EC_K + 2, j);
    }
    /* EXP END */

    free(g_tblsChunk);
    repairExpEnd(EC_K + 2);
    printf("[degradedReadNetEC end]\n");
    return EC_OK;
}
static int degradedReadNew(int argc, char **argv)
{
    printf("[degradedReadNew begin]\n");
    repairStart(0, argv);

    /* select nodes by increasing node index order except failNode */
    selectNodesDegradedReadTRENNetO(selectNodesIndex, selectChunksIndex); // homogeneous or heterogeneous network

    /* init Net */
    for (int i = 0; i < EC_N; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, selectNodesIndex[i]); // only the func close sockfd
    clientInitNetwork(&sockfdArr[EC_N], EC_PND_CLIENT_PORT, PNETDEVICE_IP_ADDR - STORAGENODES_START_IP_ADDR);

    /* NEW0: Get available uplink bandwidth, where skip the failed node */
    int Ba[EC_N - 1], Br[EC_N - 1], Tmax;
    if (netMode[4] == 'o')
        RANGetBaNetO(Ba, EC_N - 1);
    else if (netMode[4] == 'e')
        RANGetBaNetE(Ba, EC_N); // by increasing node index order except failNode

    /* NEW1: calculate Tmax and Br */
    RANDegradedReadCalBrTmax(Ba, Br, &Tmax, EC_N - 1);

    /* NEW2: allocation data size for each node */
    int Ds[EC_N - 1];
    RANDegradedReadAllocateDataSize(Ds, Br, Tmax, EC_N - 1);

    /* NEW3: data position for each node  */
    int twoBucketsSliceSizeInfo[EC_N - 1][6], twoBucketsSliceIndexInfo[EC_N - 1][6], divisionLines[2 * EC_N];
    RANDegradedReadFillDataBuckets(twoBucketsSliceSizeInfo, Ds, EC_N - 1);
    RANDegradedReadSliceAlig(twoBucketsSliceIndexInfo, twoBucketsSliceSizeInfo, EC_N - 1); // RANDegradedReadSliceAlig: // twoBucketsSliceIndexInfo[]={position_start_index1,slice_num1,databuket_index1,position_start_index2,slice_num2,databuket_index2}
    int divisionNum = RANDegradedReadDivisionLines(divisionLines, twoBucketsSliceIndexInfo, EC_N - 1);
    RANDegradedReadSortPosIndex(twoBucketsSliceIndexInfo, EC_N - 1); // by increasing node index order except failNode

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex[divisionNum][EC_N], healChunkIndex[divisionNum][EC_N], failChunkNum[divisionNum], healChunkNum[divisionNum];
    repairChunkIndexDegradedReadN(selectNodesIndex, selectChunksIndex, healChunkIndex, healChunkNum, failChunkIndex, failChunkNum,
                                  twoBucketsSliceIndexInfo, divisionLines, divisionNum);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL;
    create2PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, divisionNum, 1 * EC_K * 32);
    repairMatrixN(healChunkIndex, healChunkNum, failChunkIndex, failChunkNum, divisionNum, g_tblsArrChunk);

    g_tblsSortbyIncreasingNodeIndexDR_N(selectNodesIndex, selectChunksIndex, healChunkIndex, divisionNum, g_tblsArrChunk);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestDegradedReadN(selectNodesIndex, twoBucketsSliceIndexInfo, divisionLines, divisionNum, g_tblsArrChunk, j);
        repairRecvExp(EC_N + 1, j);
    }
    /* EXP END */

    delete2ArrUC(g_tblsBufChunk, g_tblsArrChunk);
    repairExpEnd(EC_N + 1);
    printf("[degradedReadNew end]\n");
    return EC_OK;
}

static int fullRecoverTra(int argc, char **argv)
{
    printf("[fullRecoverTra begin]\n");
    repairStart(1, argv);

    /* select nodes */
    if (netMode[4] == 'o')
        selectNodesFullRecoverTENetO(selectNodesIndexArr, selectChunksIndexArr, eachNodeInfo);
    else if (netMode[4] == 'e')
        selectNodesFullRecoverTENetE(selectNodesIndexArr, selectChunksIndexArr, eachNodeInfo);

    /* init Net */
    for (int i = 0; i < EC_A; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, i);

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex2Arr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N], healChunkIndex2Arr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N];
    int failChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, healChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    for (int i = 0; i < stripeNum; i++)
        repairChunkIndexDegradedReadTRE(selectNodesIndexArr[i], selectChunksIndexArr[i], healChunkIndex2Arr[i], &healChunkNumArr[i], failChunkIndex2Arr[i], &failChunkNumArr[i]);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL;
    create2PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, stripeNum, EC_K * 32);
    for (int i = 0; i < stripeNum; i++)
        repairMatrixTE(healChunkIndex2Arr[i], healChunkNumArr[i], failChunkIndex2Arr[i], failChunkNumArr[i], g_tblsArrChunk[i]);
    for (int i = 0; i < stripeNum; i++)
        g_tblsSortbyIncreasingNodeIndexDR_TE(selectNodesIndexArr[i], selectChunksIndexArr[i], healChunkIndex2Arr[i], g_tblsArrChunk[i]);
    // printFullRecoveryInfoTRE();
    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestFullRecoverT(selectNodesIndexArr, eachNodeInfo, g_tblsArrChunk, j);
        repairRecvExp(EC_A, j);
    }
    /* EXP END */

    delete2ArrUC(g_tblsBufChunk, g_tblsArrChunk);
    repairExpEnd(EC_A);
    printf("[fullRecoverTra end]\n");
    return EC_OK;
}

static int fullRecoverRP(int argc, char **argv)
{
    printf("[fullRecoverRP begin]\n");
    repairStart(1, argv);

    /* select nodes */
    if (netMode[4] == 'o')
        selectNodesFullRecoverRNetO(selectNodesIndexArr, selectChunksIndexArr, eachNodeInfo);
    else if (netMode[4] == 'e')
        selectNodesFullRecoverRNetE(selectNodesIndexArr, selectChunksIndexArr, eachNodeInfo);

    // printFullRecoveryInfoTRE();

    /* init Net */
    for (int i = 0; i < EC_A; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, i);

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex2Arr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N], healChunkIndex2Arr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N];
    int failChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, healChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    for (int i = 0; i < stripeNum; i++)
        repairChunkIndexDegradedReadTRE(selectNodesIndexArr[i], selectChunksIndexArr[i], healChunkIndex2Arr[i], &healChunkNumArr[i], failChunkIndex2Arr[i], &failChunkNumArr[i]);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL, ***g_tbls2ArrChunk = NULL;
    create3PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, &g_tbls2ArrChunk, stripeNum, EC_K, 2 * 32);
    for (int i = 0; i < stripeNum; i++)
        repairMatrixR(healChunkIndex2Arr[i], healChunkNumArr[i], failChunkIndex2Arr[i], failChunkNumArr[i], g_tbls2ArrChunk[i]);
    for (int i = 0; i < stripeNum; i++)
        g_tblsSortbyIncreasingNodeIndexDR_R(selectNodesIndexArr[i], selectChunksIndexArr[i], healChunkIndex2Arr[i], g_tbls2ArrChunk[i]);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestFullRecoverR(selectNodesIndexArr, eachNodeInfo, g_tbls2ArrChunk, j);
        repairRecvExp(EC_A, j);
    }
    /* EXP END */

    delete3ArrUC(g_tblsBufChunk, g_tblsArrChunk, g_tbls2ArrChunk);
    repairExpEnd(EC_A);
    printf("[fullRecoverRP end]\n");
    return EC_OK;
}

static int fullRecoverNetEC(int argc, char **argv)
{
    printf("[fullRecoverNetEC begin]\n");
    repairStart(1, argv);

    /* select nodes */

    int eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM] = {0}; // each node need send: init value -1,where // 0-recordstripe,1-loc
    if (netMode[4] == 'o')
        selectNodesFullRecoverTENetO(selectNodesIndexArr, selectChunksIndexArr, eachNodeInfo);
    else if (netMode[4] == 'e')
        selectNodesFullRecoverTENetE(selectNodesIndexArr, selectChunksIndexArr, eachNodeInfo);
    for (int k = 0; k < stripeNum; k++)
        sortMin2ArrBubble(&selectChunksIndexArr[k][1], &selectNodesIndexArr[k][1], EC_K);

    /* init Net */
    for (int i = 0; i < EC_A; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, i);
    clientInitNetwork(&sockfdArr[EC_A], EC_PND_CLIENT_PORT, PNETDEVICE_IP_ADDR - STORAGENODES_START_IP_ADDR);

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex2Arr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N], healChunkIndex2Arr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N];
    int failChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, healChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    for (int i = 0; i < stripeNum; i++)
        repairChunkIndexDegradedReadTRE(selectNodesIndexArr[i], selectChunksIndexArr[i], healChunkIndex2Arr[i], &healChunkNumArr[i], failChunkIndex2Arr[i], &failChunkNumArr[i]);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL;
    create2PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, stripeNum, EC_K * 32);
    for (int i = 0; i < stripeNum; i++)
        repairMatrixTE(healChunkIndex2Arr[i], healChunkNumArr[i], failChunkIndex2Arr[i], failChunkNumArr[i], g_tblsArrChunk[i]);
    for (int i = 0; i < stripeNum; i++)
        g_tblsSortbyIncreasingNodeIndexDR_TE(selectNodesIndexArr[i], selectChunksIndexArr[i], healChunkIndex2Arr[i], g_tblsArrChunk[i]);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestFullRecoverE(selectNodesIndexArr, eachNodeInfo, g_tblsArrChunk, j);
        repairRecvExp(EC_A + 1, j);
    }
    /* EXP END */

    delete2ArrUC(g_tblsBufChunk, g_tblsArrChunk);
    repairExpEnd(EC_A + 1);
    printf("[fullRecoverNetEC end]\n");
    return EC_OK;
}

static int fullRecoverNew(int argc, char **argv)
{
    printf("[fullRecoverNew begin]\n");
    repairStart(1, argv);

    /* select nodes */
    selectNodesFullRecoverNNetO(selectNodesIndexArrN, selectChunksIndexArrN, eachNodeInfo);

    /* init Net */
    for (int i = 0; i < EC_A; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, i);
    clientInitNetwork(&sockfdArr[EC_A], EC_PND_CLIENT_PORT, PNETDEVICE_IP_ADDR - STORAGENODES_START_IP_ADDR);

    /* NEW0: Get available uplink bandwidth, where skip the failed node */
    int Ba[EC_A - 1];
    if (netMode[4] == 'o')
        RANGetBaNetO(Ba, EC_A - 1);
    else if (netMode[4] == 'e')
        RANGetBaNetE(Ba, EC_A); // increasing node index sort

    /* NEW1: calculate Tmax and Dij */
    int(*Dij)[EC_N - 1] = (int(*)[EC_N - 1]) Calloc(stripeNum, sizeof(*Dij)), Tmax;
    RANFullRecoverSolveLP(selectNodesIndexArrN, Ba, Dij, &Tmax);

    // /* NEW2: data position for each node */
    int(*twoBucketsSliceSizeInfo)[EC_N - 1][6] = (int(*)[EC_N - 1][6]) Calloc(stripeNum, sizeof(*twoBucketsSliceSizeInfo));
    int(*twoBucketsSliceIndexInfo)[EC_N - 1][6] = (int(*)[EC_N - 1][6]) Calloc(stripeNum, sizeof(*twoBucketsSliceIndexInfo));
    int(*divisionLines)[2 * EC_N] = (int(*)[2 * EC_N]) Calloc(stripeNum, sizeof(*divisionLines));
    int(*divisionNum) = Calloc(stripeNum, sizeof(*divisionNum)); // selct node index sort
    RANFullRecoverFillDataBuckets(twoBucketsSliceSizeInfo, Dij);
    RANFullRecoverSliceAlig(twoBucketsSliceIndexInfo, twoBucketsSliceSizeInfo); // RANDegradedReadSliceAlig: // twoBucketsSliceIndexInfo[]={position_start_index1,slice_num1,databuket_index1,position_start_index2,slice_num2,databuket_index2}
    RANFullRecoverDivisionLines(divisionLines, twoBucketsSliceIndexInfo, divisionNum);
    RANFullRecoverSortPosIndex(twoBucketsSliceIndexInfo); // increasing node index sort

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex2Arr[stripeNum][2 * EC_N][EC_N], healChunkIndex2Arr[stripeNum][2 * EC_N][EC_N];
    int failChunkNumArr[stripeNum][2 * EC_N], healChunkNumArr[stripeNum][2 * EC_N];
    for (int i = 0; i < stripeNum; i++)
        repairChunkIndexDegradedReadN(selectNodesIndexArrN[i], selectChunksIndexArrN[i], healChunkIndex2Arr[i], healChunkNumArr[i],
                                      failChunkIndex2Arr[i], failChunkNumArr[i], twoBucketsSliceIndexInfo[i], divisionLines[i], divisionNum[i]);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    int maxDivisionNum = getArrMaxValue(divisionNum, stripeNum);
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL, ***g_tbls2ArrChunk = NULL;
    create3PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, &g_tbls2ArrChunk, stripeNum, maxDivisionNum, EC_K * 32);
    for (int i = 0; i < stripeNum; i++)
        repairMatrixN(healChunkIndex2Arr[i], healChunkNumArr[i], failChunkIndex2Arr[i], failChunkNumArr[i], divisionNum[i], g_tbls2ArrChunk[i]);
    for (int i = 0; i < stripeNum; i++)
        g_tblsSortbyIncreasingNodeIndexDR_N(selectNodesIndexArrN[i], selectChunksIndexArrN[i], healChunkIndex2Arr[i], divisionNum[i], g_tbls2ArrChunk[i]);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestFullRecoverN(selectNodesIndexArrN, eachNodeInfo, maxDivisionNum,
                                  twoBucketsSliceIndexInfo, divisionLines, divisionNum, g_tbls2ArrChunk, j);
        repairRecvExp(EC_A + 1, j);
    }
    /* EXP END */

    free4(divisionLines, twoBucketsSliceIndexInfo, twoBucketsSliceSizeInfo, Dij);
    delete3ArrUC(g_tblsBufChunk, g_tblsArrChunk, g_tbls2ArrChunk);
    repairExpEnd(EC_A + 1);
    printf("[fullRecoverNew end]\n");
    return EC_OK;
}

static int mulDegradedReadTra(int argc, char **argv)
{
    printf("[mulDegradedReadTra begin]\n");
    repairStart(0, argv);

    /* select nodes */
    if (netMode[4] == 'o')
        selectNodesMulDegradedReadTRENNetO(selectNodesIndex, selectChunksIndex);
    else if (netMode[4] == 'e')
        selectNodesMulDegradedReadTENetE(selectNodesIndex, selectChunksIndex);

    /* init Net */
    for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, selectNodesIndex[i]);

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex2Arr[MUL_FAIL_NUM][EC_N], healChunkIndex[EC_N], failChunkNum = 0, healChunkNum = 0;
    repairChunkIndexMulDegradedReadTRE(selectNodesIndex, selectChunksIndex, healChunkIndex, &healChunkNum, failChunkIndex2Arr, &failChunkNum);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL;
    create2PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, MUL_FAIL_NUM, 1 * EC_K * 32);
    for (int i = 0; i < MUL_FAIL_NUM; i++)
        repairMatrixTE(healChunkIndex, healChunkNum, failChunkIndex2Arr[i], failChunkNum, g_tblsArrChunk[i]);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestMulDegradedReadT(selectNodesIndex, g_tblsArrChunk, j);
        repairRecvExp(EC_K + MUL_FAIL_NUM, j);
    }
    /* EXP END */

    delete2ArrUC(g_tblsBufChunk, g_tblsArrChunk);
    repairExpEnd(EC_K + MUL_FAIL_NUM);
    printf("[mulDegradedReadTra end]\n");
    return EC_OK;
}

static int mulDegradedReadRP(int argc, char **argv)
{
    printf("[mulDegradedReadRP begin]\n");
    repairStart(0, argv);

    /* select nodes */
    if (netMode[4] == 'o')
        selectNodesMulDegradedReadTRENNetO(selectNodesIndex, selectChunksIndex);
    else if (netMode[4] == 'e')
        selectNodesMulDegradedReadRNetE(selectNodesIndex, selectChunksIndex);

    /* init Net */
    for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, selectNodesIndex[i]);

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex2Arr[MUL_FAIL_NUM][EC_N], healChunkIndex[EC_N], failChunkNum = 0, healChunkNum = 0;
    repairChunkIndexMulDegradedReadTRE(selectNodesIndex, selectChunksIndex, healChunkIndex, &healChunkNum, failChunkIndex2Arr, &failChunkNum);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL, ***g_tbls2ArrChunk = NULL;
    create3PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, &g_tbls2ArrChunk, MUL_FAIL_NUM, EC_K, 2 * 1 * 32);
    for (int i = 0; i < MUL_FAIL_NUM; i++)
        repairMatrixR(healChunkIndex, healChunkNum, failChunkIndex2Arr[i], failChunkNum, g_tbls2ArrChunk[i]);
    for (int i = 0; i < MUL_FAIL_NUM; i++)
        g_tblsSortbyIncreasingNodeIndexDR_MR(selectNodesIndex, selectChunksIndex, healChunkIndex, g_tbls2ArrChunk[i]);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestMulDegradedReadR(selectNodesIndex, g_tbls2ArrChunk, j);
        repairRecvExp(EC_K + MUL_FAIL_NUM, j);
    }
    /* EXP END */

    delete3ArrUC(g_tblsBufChunk, g_tblsArrChunk, g_tbls2ArrChunk);
    repairExpEnd(EC_K + MUL_FAIL_NUM);
    printf("[mulDegradedReadRP end]\n");
    return EC_OK;
}

static int mulDegradedReadNetEC(int argc, char **argv)
{
    printf("[mulDegradedReadNetEC begin]\n");
    repairStart(0, argv);

    /* select nodes */
    if (netMode[4] == 'o')
        selectNodesMulDegradedReadTRENNetO(selectNodesIndex, selectChunksIndex);
    else if (netMode[4] == 'e')
        selectNodesMulDegradedReadTENetE(selectNodesIndex, selectChunksIndex);

    /* init Net */
    for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, selectNodesIndex[i]);
    clientInitNetwork(&sockfdArr[EC_K + MUL_FAIL_NUM], EC_PND_CLIENT_PORT, PNETDEVICE_IP_ADDR - STORAGENODES_START_IP_ADDR);

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex2Arr[MUL_FAIL_NUM][EC_N], healChunkIndex[EC_N], failChunkNum = 0, healChunkNum = 0;
    repairChunkIndexMulDegradedReadTRE(selectNodesIndex, selectChunksIndex, healChunkIndex, &healChunkNum, failChunkIndex2Arr, &failChunkNum);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL;
    create2PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, MUL_FAIL_NUM, 1 * EC_K * 32);
    for (int i = 0; i < MUL_FAIL_NUM; i++)
        repairMatrixTE(healChunkIndex, healChunkNum, failChunkIndex2Arr[i], failChunkNum, g_tblsArrChunk[i]);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestMulDegradedReadE(selectNodesIndex, g_tblsArrChunk, j);
        repairRecvExp(EC_K + MUL_FAIL_NUM + 1, j);
    }
    /* EXP END */

    delete2ArrUC(g_tblsBufChunk, g_tblsArrChunk);
    repairExpEnd(EC_K + MUL_FAIL_NUM + 1);
    printf("[mulDegradedReadNetEC end]\n");
    return EC_OK;
}
static int mulDegradedReadNew(int argc, char **argv)
{
    printf("[mulDegradedReadNew begin]\n");
    repairStart(0, argv);

    /* select nodes */
    selectNodesMulDegradedReadTRENNetO(selectNodesIndex, selectChunksIndex); // homogeneous or heterogeneous network

    /* init Net */
    for (int i = 0; i < EC_N; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, selectNodesIndex[i]);
    clientInitNetwork(&sockfdArr[EC_N], EC_PND_CLIENT_PORT, PNETDEVICE_IP_ADDR - STORAGENODES_START_IP_ADDR);

    /* NEW0: Get available uplink bandwidth, where skip the failed node */
    int Ba[EC_N - MUL_FAIL_NUM], Br[EC_N - MUL_FAIL_NUM], Tmax;
    if (netMode[4] == 'o')
        RANGetBaNetO(Ba, EC_N - MUL_FAIL_NUM);
    else if (netMode[4] == 'e')
        RANMulDegradedReadGetBaNetE(Ba, EC_N);

    /* NEW1: calculate Tmax and Br */
    RANDegradedReadCalBrTmax(Ba, Br, &Tmax, EC_N - MUL_FAIL_NUM);

    /* NEW2: allocation data size for each node */
    int Ds[EC_N - MUL_FAIL_NUM];
    RANDegradedReadAllocateDataSize(Ds, Br, Tmax, EC_N - MUL_FAIL_NUM);

    /* NEW3: data position for each node  */
    int twoBucketsSliceSizeInfo[EC_N - MUL_FAIL_NUM][6], twoBucketsSliceIndexInfo[EC_N - MUL_FAIL_NUM][6], divisionLines[2 * EC_N];
    RANDegradedReadFillDataBuckets(twoBucketsSliceSizeInfo, Ds, EC_N - MUL_FAIL_NUM);
    RANDegradedReadSliceAlig(twoBucketsSliceIndexInfo, twoBucketsSliceSizeInfo, EC_N - MUL_FAIL_NUM); // RANDegradedReadSliceAlig: // twoBucketsSliceIndexInfo[]={position_start_index1,slice_num1,databuket_index1,position_start_index2,slice_num2,databuket_index2}
    int divisionNum = RANDegradedReadDivisionLines(divisionLines, twoBucketsSliceIndexInfo, EC_N - MUL_FAIL_NUM);
    RANDegradedReadSortPosIndex(twoBucketsSliceIndexInfo, EC_N - MUL_FAIL_NUM);

    /* get number&location of nodes_error and nodes_ok */
    int failChunk2ArrIndex[MUL_FAIL_NUM][2 * EC_N][EC_N] = {0}, healChunkIndex[divisionNum][EC_N], failChunkNum[MUL_FAIL_NUM][2 * EC_N] = {0}, healChunkNum[divisionNum];
    repairChunkIndexMulDegradedReadN(selectNodesIndex, selectChunksIndex, healChunkIndex, healChunkNum, failChunk2ArrIndex, failChunkNum,
                                     twoBucketsSliceIndexInfo, divisionLines, divisionNum);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL, ***g_tbls2ArrChunk = NULL;
    create3PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, &g_tbls2ArrChunk, MUL_FAIL_NUM, divisionNum, 1 * EC_K * 32);
    for (int i = 0; i < MUL_FAIL_NUM; i++)
        repairMatrixN(healChunkIndex, healChunkNum, failChunk2ArrIndex[i], failChunkNum, divisionNum, g_tbls2ArrChunk[i]);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestMulDegradedReadN(selectNodesIndex, twoBucketsSliceIndexInfo, divisionLines, divisionNum, g_tbls2ArrChunk, j);
        repairRecvExp(EC_N + 1, j);
    }
    /* EXP END */

    delete3ArrUC(g_tblsBufChunk, g_tblsArrChunk, g_tbls2ArrChunk);
    repairExpEnd(EC_N + 1);
    printf("[mulDegradedReadNew end]\n");
    return EC_OK;
}

static int mulFullRecoverTra(int argc, char **argv)
{
    printf("[mulFullRecoverTra begin]\n");
    repairStart(2, argv);

    /* select nodes */
    if (netMode[4] == 'o')
        selectNodesMulFullRecoverTENetO(selectNodesIndexArr, selectChunksIndexArr, eachNodeInfo);
    else if (netMode[4] == 'e')
        selectNodesMulFullRecoverTENetE(selectNodesIndexArr, selectChunksIndexArr, eachNodeInfo);
    for (int k = 0; k < stripeNum; k++)
        sortMin2ArrBubble(&selectChunksIndexArr[k][MUL_FAIL_NUM], &selectNodesIndexArr[k][MUL_FAIL_NUM], EC_K);

    /* init Net */
    for (int i = 0; i < EC_A; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, i);

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex3Arr[FULL_RECOVERY_STRIPE_MAX_NUM][MUL_FAIL_NUM][EC_N] = {0}, healChunkIndex2Arr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N] = {0};
    int failChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, healChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    for (int i = 0; i < stripeNum; i++)
        repairChunkIndexMulDegradedReadTRE(selectNodesIndexArr[i], selectChunksIndexArr[i], healChunkIndex2Arr[i], &healChunkNumArr[i], failChunkIndex3Arr[i], &failChunkNumArr[i]);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL, ***g_tbls2ArrChunk = NULL;
    create3PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, &g_tbls2ArrChunk, stripeNum, MUL_FAIL_NUM, EC_K * 32);
    for (int i = 0; i < stripeNum; i++)
        for (int n = 0; n < eachStripeFailNodeNum[i]; n++)
            repairMatrixTE(healChunkIndex2Arr[i], healChunkNumArr[i], failChunkIndex3Arr[i][n], failChunkNumArr[i], g_tbls2ArrChunk[i][n]);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestMulFullRecoverT(selectNodesIndexArr, eachNodeInfo, g_tbls2ArrChunk, j);
        repairRecvExp(EC_A, j);
    }

    /* EXP END */
    delete3ArrUC(g_tblsBufChunk, g_tblsArrChunk, g_tbls2ArrChunk);
    repairExpEnd(EC_A);
    printf("[mulFullRecoverTra end]\n");
    return EC_OK;
}

static int mulFullRecoverRP(int argc, char **argv)
{
    printf("[mulFullRecoverRP begin]\n");
    repairStart(2, argv);

    /* select nodes */
    if (netMode[4] == 'o')
        selectNodesMulFullRecoverRNetO(selectNodesIndexArr, selectChunksIndexArr, eachNodeInfo);
    else if (netMode[4] == 'e')
        selectNodesMulFullRecoverRNetE(selectNodesIndexArr, selectChunksIndexArr, eachNodeInfo);

    /* init Net */
    for (int i = 0; i < EC_A; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, i);

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex3Arr[FULL_RECOVERY_STRIPE_MAX_NUM][MUL_FAIL_NUM][EC_N], healChunkIndex2Arr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N];
    int failChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, healChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    for (int i = 0; i < stripeNum; i++)
        repairChunkIndexMulDegradedReadTRE(selectNodesIndexArr[i], selectChunksIndexArr[i], healChunkIndex2Arr[i], &healChunkNumArr[i], failChunkIndex3Arr[i], &failChunkNumArr[i]);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL, ***g_tbls2ArrChunk = NULL, ****g_tbls3ArrChunk = NULL;
    create4PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, &g_tbls2ArrChunk, &g_tbls3ArrChunk, stripeNum, MUL_FAIL_NUM, EC_K, 2 * 32);
    for (int i = 0; i < stripeNum; i++)
        for (int n = 0; n < eachStripeFailNodeNum[i]; n++)
            repairMatrixR(healChunkIndex2Arr[i], healChunkNumArr[i], failChunkIndex3Arr[i][n], failChunkNumArr[i], g_tbls3ArrChunk[i][n]);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestMulFullRecoverR(selectNodesIndexArr, eachNodeInfo, g_tbls3ArrChunk, j);
        repairRecvExp(EC_A, j);
    }
    /* EXP END */

    delete4ArrUC(g_tblsBufChunk, g_tblsArrChunk, g_tbls2ArrChunk, g_tbls3ArrChunk);
    repairExpEnd(EC_A);
    return EC_OK;
}

static int mulFullRecoverNetEC(int argc, char **argv)
{
    printf("[mulFullRecoverNetEC begin]\n");
    repairStart(2, argv);

    /* select nodes */
    if (netMode[4] == 'o')
        selectNodesMulFullRecoverTENetO(selectNodesIndexArr, selectChunksIndexArr, eachNodeInfo);
    else if (netMode[4] == 'e')
        selectNodesMulFullRecoverTENetE(selectNodesIndexArr, selectChunksIndexArr, eachNodeInfo);
    for (int k = 0; k < stripeNum; k++)
        sortMin2ArrBubble(&selectChunksIndexArr[k][MUL_FAIL_NUM], &selectNodesIndexArr[k][MUL_FAIL_NUM], EC_K);

    /* init Net */
    for (int i = 0; i < EC_A; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, i);
    clientInitNetwork(&sockfdArr[EC_A], EC_PND_CLIENT_PORT, PNETDEVICE_IP_ADDR - STORAGENODES_START_IP_ADDR);

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex3Arr[FULL_RECOVERY_STRIPE_MAX_NUM][MUL_FAIL_NUM][EC_N], healChunkIndex2Arr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N];
    int failChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, healChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    for (int i = 0; i < stripeNum; i++)
        repairChunkIndexMulDegradedReadTRE(selectNodesIndexArr[i], selectChunksIndexArr[i], healChunkIndex2Arr[i], &healChunkNumArr[i], failChunkIndex3Arr[i], &failChunkNumArr[i]);

    /* Generate decoded g_tblsChunk in increasing healthy chunk index order */
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL, ***g_tbls2ArrChunk = NULL;
    create3PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, &g_tbls2ArrChunk, stripeNum, MUL_FAIL_NUM, EC_K * 32);
    for (int i = 0; i < stripeNum; i++)
        for (int n = 0; n < eachStripeFailNodeNum[i]; n++)
            repairMatrixTE(healChunkIndex2Arr[i], healChunkNumArr[i], failChunkIndex3Arr[i][n], failChunkNumArr[i], g_tbls2ArrChunk[i][n]);

    // printFullRecoveryInfoTRE();

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestMulFullRecoverE(selectNodesIndexArr, eachNodeInfo, g_tbls2ArrChunk, j);
        repairRecvExp(EC_A + 1, j);
    }
    /* EXP END */

    delete3ArrUC(g_tblsBufChunk, g_tblsArrChunk, g_tbls2ArrChunk);
    repairExpEnd(EC_A + 1);
    printf("[mulFullRecoverNetEC end]\n");
    return EC_OK;
}

/* note: failNum is 1-MUL_FAIL_NUM for each stripe!!! */
static int mulFullRecoverNew(int argc, char **argv)
{
    printf("[mulFullRecoverNew begin]\n");
    repairStart(2, argv);

    /* select nodes */
    selectNodesMulFullRecoverNNetO(selectNodesIndexArrN, selectChunksIndexArrN, eachNodeInfo);

    /* init Net */
    for (int i = 0; i < EC_A; i++)
        clientInitNetwork(&sockfdArr[i], EC_CLIENT_STORAGENODES_PORT, i);
    clientInitNetwork(&sockfdArr[EC_A], EC_PND_CLIENT_PORT, PNETDEVICE_IP_ADDR - STORAGENODES_START_IP_ADDR);

    /* NEW0: Get available uplink bandwidth, where skip the failed node */
    int Ba[EC_A - MUL_FAIL_NUM];
    if (netMode[4] == 'o')
        RANGetBaNetO(Ba, EC_A - MUL_FAIL_NUM);
    else if (netMode[4] == 'e')
        RANMulDegradedReadGetBaNetE(Ba, EC_A); // increasing node index sort

    /* NEW1: calculate Tmax and Dij */
    int(*Dij)[EC_N - 1] = (int(*)[EC_N - 1]) Calloc(stripeNum, sizeof(*Dij)), Tmax;
    RANMulFullRecoverSolveLP(selectNodesIndexArrN, Ba, Dij, &Tmax); // increasing node index sort

    // /* NEW2: data position for each node */
    int(*twoBucketsSliceSizeInfo)[EC_N - 1][6] = (int(*)[EC_N - 1][6]) Calloc(stripeNum, sizeof(*twoBucketsSliceSizeInfo)); // for convenience, actual EC_N-MUL_FAIL_NUM
    int(*twoBucketsSliceIndexInfo)[EC_N - 1][6] = (int(*)[EC_N - 1][6]) Calloc(stripeNum, sizeof(*twoBucketsSliceIndexInfo));
    int(*divisionLines)[2 * EC_N] = (int(*)[2 * EC_N]) Calloc(stripeNum, sizeof(*divisionLines));
    int(*divisionNum) = Calloc(stripeNum, sizeof(*divisionNum));
    RANMulFullRecoverFillDataBuckets(twoBucketsSliceSizeInfo, Dij);
    RANMulFullRecoverSliceAlig(twoBucketsSliceIndexInfo, twoBucketsSliceSizeInfo); // RANDegradedReadSliceAlig: // twoBucketsSliceIndexInfo[]={position_start_index1,slice_num1,databuket_index1,position_start_index2,slice_num2,databuket_index2}
    RANMulFullRecoverDivisionLines(divisionLines, twoBucketsSliceIndexInfo, divisionNum);
    RANMulFullRecoverSortPosIndex(twoBucketsSliceIndexInfo);

    /* get number&location of nodes_error and nodes_ok */
    int failChunkIndex3Arr[FULL_RECOVERY_STRIPE_MAX_NUM][MUL_FAIL_NUM][2 * EC_N][EC_N], healChunkIndex2Arr[FULL_RECOVERY_STRIPE_MAX_NUM][2 * EC_N][EC_N];
    int failChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM][MUL_FAIL_NUM][2 * EC_N] = {0}, healChunkNumArr[FULL_RECOVERY_STRIPE_MAX_NUM][2 * EC_N] = {0};
    for (int i = 0; i < stripeNum; i++)
        repairChunkIndexMulFullRecoverN(selectNodesIndexArrN[i], selectChunksIndexArrN[i], healChunkIndex2Arr[i], healChunkNumArr[i],
                                        failChunkIndex3Arr[i], failChunkNumArr[i], twoBucketsSliceIndexInfo[i], divisionLines[i], divisionNum[i]);

    int maxDivisionNum = getArrMaxValue(divisionNum, stripeNum);
    unsigned char *g_tblsBufChunk = NULL, **g_tblsArrChunk = NULL, ***g_tbls2ArrChunk = NULL, ****g_tbls3ArrChunk = NULL;
    create4PointerArrUC(&g_tblsBufChunk, &g_tblsArrChunk, &g_tbls2ArrChunk, &g_tbls3ArrChunk, stripeNum, MUL_FAIL_NUM, maxDivisionNum, EC_K * 32);
    for (int i = 0; i < stripeNum; i++)
        for (int n = 0; n < eachStripeFailNodeNum[i]; n++)
            repairMatrixN(healChunkIndex2Arr[i], healChunkNumArr[i], failChunkIndex3Arr[i][n],
                          failChunkNumArr[i][n], divisionNum[i], g_tbls3ArrChunk[i][n]);

    /* EXP START */
    for (int j = 0; j < EXP_TIMES; j++)
    {
        clientRequestMulFullRecoverN(selectNodesIndexArrN, eachNodeInfo, maxDivisionNum,
                                     twoBucketsSliceIndexInfo, divisionLines, divisionNum, g_tbls3ArrChunk, j);
        repairRecvExp(EC_A + 1, j);
    }
    /* EXP END */

    free4(divisionLines, twoBucketsSliceIndexInfo, twoBucketsSliceSizeInfo, Dij);
    delete4ArrUC(g_tblsBufChunk, g_tblsArrChunk, g_tbls2ArrChunk, g_tbls3ArrChunk);
    repairExpEnd(EC_A + 1);
    printf("[mulFullRecoverNew end]\n");
    return EC_OK;
}

#if (!HDFS_DEBUG)
void *handleHDFS(void *arg)
{
    printf("[ec_coordinator_handle_hdfs begin]\n");
    int clientFd = *((int *)arg); // only the func close

    int cmd = -2;
    Recv(clientFd, &cmd, sizeof(int), "recv hdfs-cmd");
    cmd = ntohl(cmd);
    printf("cmd = %d \n", cmd); // if cmd>0, cmd is reconstructor worker - no use now

    if (cmd > 0) // sycn cmd - no use now
    {
        printf("Please waiting hdfs information synchronization! (need start-dfs, otherwise rerun) \n");

        hdfs_info_t *HDFSInfo = (hdfs_info_t *)Malloc(sizeof(hdfs_info_t));
        getHDFSInfo();
        printf("Information synchronization completed!\n");
    }
    if (cmd == -1) // reconstruct  cmd
    {
        char blockFileName[MAX_PATH_LEN] = {0};
        int chunk_node_flag = -1, method_flag = -1, file_name_length = -1;
        Recv(clientFd, &chunk_node_flag, sizeof(int), "recv chunk_node_flag");
        chunk_node_flag = ntohl(chunk_node_flag);
        Recv(clientFd, &method_flag, sizeof(int), "recv method_flag");
        method_flag = ntohl(method_flag);
        Recv(clientFd, &file_name_length, sizeof(int), "file_name_length");
        file_name_length = ntohl(file_name_length); // must 24
        Recv(clientFd, &blockFileName, sizeof(char) * file_name_length, "recv blockFileName");

        /* print basic info */
        printf("chunk_node_flag = %d \n", chunk_node_flag);
        printf("method_flag= %d \n", method_flag);
        printf("file_name_length = %d \n", file_name_length);
        printf("blockFileName = %s \n", blockFileName);
        PRINT_FLUSH;

        int argc = 3;
        char *argv[argc];
        for (int i = 0; i < argc; i++)
            argv[i] = (char *)Calloc(10, sizeof(char));

        if (chunk_node_flag == 1) // single chunk repair
        {
            pthread_mutex_lock(&HDFSDegradedReadLock);
            if (getHDFSDegradedReadInfo(blockFileName) == EC_ERROR)
                ERROR_RETURN_NULL("fail hdfs repair info");
            FailNodeIndex = HDFSInfo->failNodeIndex; // chunk index
            int argc = 3;

            if (method_flag == 0)
                strcpy(argv[1], "-rdte");
            else if (method_flag == 1)
                strcpy(argv[1], "-rdre");
            else if (method_flag == 2)
                strcpy(argv[1], "-rdee");
            else if (method_flag == 3)
                strcpy(argv[1], "-rdne");
            if (handleClientMain(argc, argv) == EC_ERROR)
                ERROR_RETURN_VALUE("handle main");
            pthread_mutex_unlock(&HDFSDegradedReadLock);
        }
        else if (chunk_node_flag == 2)
        {
            pthread_mutex_lock(&HDFSFullRecoveLock);
            HDFSFullRecoveFlag++;
            if (HDFSFullRecoveFlag == 1)
            {
                printf("**************************\n");
                printf("chunk_node_flag1 = %d \n", chunk_node_flag);
                printf("method_flag1= %d \n", method_flag);
                printf("file_name_length1 = %d \n", file_name_length);
                printf("blockFileName1 = %s \n", blockFileName);
                printf("**************************\n");
                pthread_mutex_unlock(&HDFSFullRecoveLock);
                if (getHDFSFullRecoverInfo(blockFileName) == EC_ERROR)
                    ERROR_RETURN_NULL("fail hdfs node repair info");
                FailNodeIndex = HDFSInfo->failNodeIndex; // failNodeIndex

                if (method_flag == 0)
                    strcpy(argv[1], "-rfte");
                else if (method_flag == 1)
                    strcpy(argv[1], "-rfre");
                else if (method_flag == 2)
                    strcpy(argv[1], "-rfee");
                else if (method_flag == 3)
                    strcpy(argv[1], "-rfne");
                if (handleClientMain(argc, argv) == EC_ERROR)
                    ERROR_RETURN_VALUE("handle main");
            }
            else
                pthread_mutex_unlock(&HDFSFullRecoveLock); // direct return

            if (HDFSFullRecoveFlag == HDFSInfo->failBpNum)
            {
                pthread_mutex_lock(&HDFSFullRecoveLock);
                HDFSFullRecoveFlag = 0;
                pthread_mutex_unlock(&HDFSFullRecoveLock);
            }
        }
        else
            printf("error chunk node flag");

        for (int i = 0; i < argc; i++)
            free(argv[i]);
    }

    int complete_flag = 1;
    complete_flag = ntohl(complete_flag);
    Send(clientFd, &complete_flag, sizeof(int), "send complete_flag");
    shutdown(clientFd, SHUT_RDWR);
    printf("[ec_coordinator_handle_hdfs end]\n");

    free(arg);
    pthread_exit(NULL);
    return NULL;
}
#else

/* it need:
 * 1. Get HDFS_DEBUG_INFO: hdfs fsck / -files -blocks -locations > HDFS_DEBUG_INFO (a file)
 * 2. find failed block name: HDFS_DEBUG_BLOCK
 */
void *handleHDFSTest(void *arg)
{
    printf("[ec_coordinator_handle_hdfs begin]\n");

    char blockFileName[MAX_PATH_LEN] = HDFS_DEBUG_BLOCK;
    int chunk_node_flag = 2, method_flag = 0; // change them
    int argc = 3;
    char *argv[argc];
    for (int i = 0; i < argc; i++)
        argv[i] = (char *)Calloc(10, sizeof(char));
    if (chunk_node_flag == 1) // single chunk repair
    {
        if (getHDFSDegradedReadInfo(blockFileName) == EC_ERROR)
            ERROR_RETURN_NULL("fail hdfs repair info");
        FailNodeIndex = HDFSInfo->failNodeIndex; // chunk index
        int argc = 3;
        if (method_flag == 0)
            strcpy(argv[1], "-rdte");
        else if (method_flag == 1)
            strcpy(argv[1], "-rdre");
        else if (method_flag == 2)
            strcpy(argv[1], "-rdee");
        else if (method_flag == 3)
            strcpy(argv[1], "-rdne");
        if (handleClientMain(argc, argv) == EC_ERROR)
            ERROR_RETURN_VALUE("handle main");
    }
    else if (chunk_node_flag == 2)
    {
        pthread_mutex_lock(&HDFSFullRecoveLock);
        if (HDFSFullRecoveFlag == 0)
        {
            HDFSFullRecoveFlag = 1;
            pthread_mutex_unlock(&HDFSFullRecoveLock);
            if (getHDFSFullRecoverInfo(blockFileName) == EC_ERROR)
                ERROR_RETURN_NULL("fail hdfs node repair info");
            FailNodeIndex = HDFSInfo->failNodeIndex; // failNodeIndex

            if (method_flag == 0)
                strcpy(argv[1], "-rfte");
            else if (method_flag == 1)
                strcpy(argv[1], "-rfre");
            else if (method_flag == 2)
                strcpy(argv[1], "-rfee");
            else if (method_flag == 3)
                strcpy(argv[1], "-rfne");
            if (handleClientMain(argc, argv) == EC_ERROR)
                ERROR_RETURN_VALUE("handle main");
        }
        else
            pthread_mutex_unlock(&HDFSFullRecoveLock); // direct return
    }
    else
        printf("error chunk node flag");
    for (int i = 0; i < argc; i++)
        free(argv[i]);
    printf("[ec_coordinator_handle_hdfs end]\n");

    pthread_exit(NULL);
    return NULL;
}
#endif
static int HDFSCoordinator(int argc, char **argv)
{
    printf("[HDFSCoordinator begin]\n");

    if (!HDFS_FLAG)
        ERROR_RETURN_VALUE("HDFS_FLAG is false");
    pthread_mutex_init(&HDFSDegradedReadLock, NULL);
    pthread_mutex_init(&HDFSFullRecoveLock, NULL);
    printf("Please waiting hdfs information synchronization! (need start-dfs, otherwise rerun) \n");
    PRINT_FLUSH;
    getHDFSInfo();
    printf("Information synchronization completed!\n");
    PRINT_FLUSH;

    pthread_t HDFStid;

#if (!HDFS_DEBUG)
    /* handle hdfs*/
    int serverFd, clientFd;
    if ((serverInitNetwork(&serverFd, EC_HDFS_COORDINATOR_PORT)) == EC_ERROR) //  init server network between client and node
        ERROR_RETURN_VALUE("error serverInitNetwork");

    while (1)
    {
        int *clientFdP = (int *)malloc(sizeof(int));
        (*clientFdP) = Accept(serverFd);
        pthreadCreate(&HDFStid, NULL, handleHDFS, (void *)clientFdP);
    }
#else
    pthreadCreate(&HDFStid, NULL, handleHDFSTest, NULL);
    pthreadJoin(HDFStid, NULL);
#endif
    pthread_mutex_destroy(&HDFSDegradedReadLock);
    pthread_mutex_destroy(&HDFSFullRecoveLock);
    return EC_OK;
}

static void help(int argc, char **argv)
{
    fprintf(stderr, "Usage introduction: %s cmd\n"
                    "\t [help]\t\t-h \n"
                    "\t [write]\t-w <srcFileName> <dstFileName> \n"
                    "\t [read]\t\t-r <srcFileName> <dstFileName> \n"
                    "\t *********************************** \n"
                    "\t Example1: -w 192MB_src 192MB_dst \n"
                    "\t <srcFileName> is saved on RAN/test_file/write/ [client]\n"
                    "\t <dstFileName> is saved on RAN/test_file/write/ [nodes] \n"
                    "\t Example2: -r 192MB_src 192MB_dst  \n"
                    "\t <srcFileName> is saved on RAN/test_file/read/ [client]\n"
                    "\t <dstFileName> is saved on RAN/test_file/write/ [nodes]\n"
                    "\t -wr rand write data into rand n node"
                    "\t -rr read data for rand write"
                    "\t wf  mul write for full-node recovery"
                    "\t Tip: saved filename on storages actually is dst_filenameX_Y, \n"
                    "\t the X is Xth stripe and the Y is Yth chunk.\n"
                    "\t *********************************** \n"
                    "\t  \n"
                    "\t  \n"
                    "\t Experiment command usage:-<exp_cmd> <srcFileName> <dstFileName>\n"
                    "\t <exp_cmd> = <exp_type>+<environment>+<repair_type>+<repair_paradigm>\n"
                    "\t <exp_type> as follows:\n"
                    "\t r: repair experiment \n"
                    "\t <environment> as follows:\n"
                    "\t o: homogeneous network (network bandwidth not specified) \n"
                    "\t e: heterogeneous network (network bandwidth specified)\n"
                    "\t <repair_type> as follows:\n"
                    "\t d: For degraded read of a single failed chunk \n"
                    "\t f: For full-node recovery of a single failed node\n"
                    "\t c: For degraded reads of multiple failed chunks \n"
                    "\t n: For full-node recoveryof multiple failed node\n"
                    "\t <repair_paradigm> as follows:\n"
                    "\t t: traditional repair\n"
                    "\t r: RP (repiar pipeline)\n"
                    "\t e: NetEC \n"
                    "\t n: NEW\n"
                    "\t *********************************** \n"
                    "\t Example1: -rodt 192MB_repair 192MB_dst \n"
                    "\t repair experiment + homogeneous network + For degraded read of a single failed chunk + traditional repair\n"
                    "\t <srcFileName> is saved on RAN/test_file/repair/ [node]\n"
                    "\t <dstFileName> is saved on RAN/test_file/write/ [nodes] \n"
                    "\t Example2: -refe 192MB_repair 192MB_dst  \n"
                    "\t repair experiment + heterogeneous network + For full-node recovery of a single failed node + NetEC\n"
                    "\t *********************************** \n"
                    "\t  \n",
            argv[0]);
}

/**
 * argv[1] cmd
 * argv[2] srcFileName
 * argv[3] dstFileName
 */

clientCmd_t clientCommands[] = {
    {"-h", help, "Show help information"},
    {"-w", ecWrite, "Write"},
    {"-wr", ecRandWrite, "Rand write"},
    {"-wmr", ecMulRandWrite, "mul Rand write"},
    {"-wm", ecMulWrite, "Multiple evenly distributed writes, For Full-node recovery write"},
    {"-r", ecRead, "Read"},
    {"-ra", ecAnyRead, "Any read"},
    {"-rdt", degradedReadTra, "Degraded read traditional"},
    {"-rdr", degradedReadRP, "Degraded read rp"},
    {"-rde", degradedReadNetEC, "Degraded read netec"},
    {"-rdn", degradedReadNew, "Degraded read new"},
    {"-rft", fullRecoverTra, "Full-node recovery traditional"},
    {"-rfr", fullRecoverRP, "Full-node recovery rp"},
    {"-rfe", fullRecoverNetEC, "Full-node recovery netec"},
    {"-rfn", fullRecoverNew, "Full-node recovery new"},
    {"-rct", mulDegradedReadTra, "Multiple failed chunks traditional"},
    {"-rcr", mulDegradedReadRP, "Multiple failed chunks rp"},
    {"-rce", mulDegradedReadNetEC, "Multiple failed chunks netec"},
    {"-rcn", mulDegradedReadNew, "Multiple failed chunks new"},
    {"-rnt", mulFullRecoverTra, "Multiple failed nodes traditional"},
    {"-rnr", mulFullRecoverRP, "Multiple failed nodes rp"},
    {"-rne", mulFullRecoverNetEC, "Multiple failed nodes netec"},
    {"-rnn", mulFullRecoverNew, "Multiple failed nodes new"},
    {"-hdfs", HDFSCoordinator, "HDFS Coordinator"},
    {NULL, NULL, NULL}};

static int handleClientMain(int argc, char *argv[])
{

    getECDir(ecDir);
    printf("[ec_client begin]\n");
    if (argc < 2)
    {
        help(argc, argv);
        return EC_OK;
    }

    /* clientCommand */
    char *cmd = argv[1];
    int found = 0;
    for (int i = 0; clientCommands[i].name != NULL; i++)
        if (strncmp(cmd, clientCommands[i].name, 4) == 0)
        {
            if (clientCommands[i].func(argc, argv) == EC_ERROR)
                printf("Fail %s\n", clientCommands[i].name);
            found = 1;
            break;
        }
    if (!found)
    {
        printf("Unknown command: %s\n", cmd);
        help(argc, argv);
    }
    PRINT_FLUSH;
    return EC_OK;
}

int main(int argc, char *argv[])
{
    HDFSInfo = (hdfs_info_t *)Malloc(sizeof(hdfs_info_t));
    return handleClientMain(argc, argv);
}