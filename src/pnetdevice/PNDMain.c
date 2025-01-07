#include "ECConfig.h"
#include "MulPortTransfer.h"
#include "PackedData.h"
#include "ECEncode.h"

static int serverFdArr[MUL_PORT_NUM], nodeIP;
static int **mulPortRecvSockfd = NULL, *mulPortRecvSockfdBuf = NULL;
static int **mulPortSendSockfd = NULL, *mulPortSendSockfdBuf = NULL;

/* from MulPortTransfer.c */
extern degradedReadNetRecordN_t *degradedReadNetRecordN;
extern fullRecoverNetRecordN_t *fullRecoverNetRecordN;

/* HDFS */
static hdfs_info_t *HDFSInfo = NULL;

/* mul use variable */
static struct timespec totalTimeStart, ioTimeStart, calTimeStart, netTimeStart;
static struct timespec totalTimeEnd, ioTimeEnd, calTimeEnd, netTimeEnd;
static long long totalExpTime, ioExpTime, calExpTime, netExpTime;
static exp_t *exp = NULL;
static mulPortNetMeta_t mulPortNetMetaSend, mulPortNetMetaRecv;
static pthread_t mulPortNetSendTid, mulPortNetRecvTid;

void *handleDegradedReadE(netMeta_t *netMeta, int clientFd)
{
    printf("[handleDegradedReadE begin]\n");
    unsigned char *g_tbls = (unsigned char *)Calloc(MUL_FAIL_NUM * EC_K * 32, sizeof(unsigned char));
    Recv(clientFd, g_tbls, netMeta->size, "recv data");
    SendResponse(clientFd);

    /* init network for request nodes */
    int startFailNodeIndex = netMeta->mulFailFlag == 0 ? netMeta->dstNodeIndex : MUL_FAIL_START;
    int failNodeNum = netMeta->mulFailFlag == 0 ? 1 : MUL_FAIL_NUM;
    if (netMeta->expCount == 0)
    {
        int tmpSockfdArr[EC_N] = {0};
        for (int i = 0; i < MUL_PORT_NUM; i++)
        {
            for (int j = 0; j < EC_K; j++)
            {
                int tmpSockfd = -1, tmpNodeindex = -1;
                tmpSockfd = AcceptAndGetNodeIndex(serverFdArr[i], &tmpNodeindex);
                tmpSockfdArr[tmpNodeindex] = tmpSockfd;
            }
            int loc = 0;
            for (int j = 0; j < EC_N; j++)
                if (tmpSockfdArr[j] != 0)
                    mulPortRecvSockfd[i][loc++] = tmpSockfdArr[j];
        }
        for (int i = 0; i < MUL_PORT_NUM; i++)
            for (int j = 0; j < failNodeNum; j++)
                if ((clientInitNetwork(&mulPortSendSockfd[i][j], EC_STORAGENODES_PORT + i, startFailNodeIndex + j)) == EC_ERROR) // only the func close sockfd
                    ERROR_RETURN_NULL("Fail init network");
    }

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* send and recv buffer */
    int totalSliceNum = CHUNK_SIZE / SLICE_SIZE, *slice_bitmap_buffer = NULL, **sliceBitmap = NULL;
    char *bufRecv = NULL, **dataRecv = NULL, ***recvDataBucket = NULL;
    create3PointerArrCHAR(&bufRecv, &dataRecv, &recvDataBucket, EC_K, totalSliceNum, SLICE_SIZE);
    create2PointerArrINT(&slice_bitmap_buffer, &sliceBitmap, EC_K, totalSliceNum);
    for (int j = 0; j < EC_K * totalSliceNum; j++)
        slice_bitmap_buffer[j] = 0; // No slices recv

    /* send and recv slice data */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    queue_t sliceIndexRecvQueue[1];
    queueInit(sliceIndexRecvQueue, CHUNK_SIZE / SLICE_SIZE);
    int failNodeIndex[failNodeNum];
    for (int j = 0; j < failNodeNum; j++)
        failNodeIndex[j] = startFailNodeIndex + j;

    mulPortNetMetaInit3Arr(&mulPortNetMetaRecv, EC_K, RECV, totalSliceNum, NOENCODE, mulPortRecvSockfd, recvDataBucket, sliceIndexRecvQueue, sliceBitmap, NULL);
    mulPortNetMetaInit3Arr(&mulPortNetMetaSend, 1, SEND, totalSliceNum, ENCODE, mulPortSendSockfd, recvDataBucket, sliceIndexRecvQueue, NULL, g_tbls);
    mulPortNetMetaInitSend2(&mulPortNetMetaSend, startFailNodeIndex, failNodeNum, failNodeIndex);
    pthreadCreate(&mulPortNetRecvTid, NULL, netSliceKNode, (void *)&mulPortNetMetaRecv); // Create thread to recv slice
    pthreadCreate(&mulPortNetSendTid, NULL, netSliceKNode, (void *)&mulPortNetMetaSend); // Create thread to send slice

    pthreadJoin(mulPortNetRecvTid, NULL);
    pthreadJoin(mulPortNetSendTid, NULL);
    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);
    SendExp(exp); // send exp data

    queueFree(sliceIndexRecvQueue);
    delete2ArrINT(slice_bitmap_buffer, sliceBitmap);
    delete3ArrCHAR(bufRecv, dataRecv, recvDataBucket);
    free(g_tbls);

    /* END: recv/send chunks from/to node */
    if (netMeta->expCount == EXP_TIMES - 1)
    {
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_K);
        shutdown2Arr(mulPortSendSockfd, MUL_PORT_NUM, EC_M);
    }
    printf("[handleDegradedReadE end]\n");
    return NULL;
}

void *handleDegradedReadN(netMeta_t *netMeta, int clientFd)
{
    printf("[handleDegradedReadN begin]\n");
    char *netDataBuf = (char *)Malloc(sizeof(char) * netMeta->size);
    Recv(clientFd, netDataBuf, netMeta->size, "recv data");
    SendResponse(clientFd);

    /* parse data */
    int startFailNodeIndex = netMeta->mulFailFlag == 0 ? netMeta->dstNodeIndex : MUL_FAIL_START;
    int failNodeNum = netMeta->mulFailFlag == 0 ? 1 : MUL_FAIL_NUM;
    int twoBucketsSliceIndexInfo[EC_N - 1][6], divisionNum, divisionLines[2 * EC_N], curOffset = 0;
    unsigned char *g_tblsBuf, **g_tblsArr, ***g_tbls2Arr;
    if (netMeta->mulFailFlag == 0)
        curOffset = unpackedDataDegradedReadN1(netDataBuf, twoBucketsSliceIndexInfo, divisionLines, &divisionNum);
    else
        curOffset = unpackedDataMulDegradedReadN1(netDataBuf, twoBucketsSliceIndexInfo, divisionLines, &divisionNum);
    create3PointerArrUC(&g_tblsBuf, &g_tblsArr, &g_tbls2Arr, failNodeNum, divisionNum, EC_K * 32);
    memcpyMove(g_tbls2Arr[0][0], netDataBuf + curOffset, failNodeNum * divisionNum * EC_K * 32 * sizeof(char), &curOffset);

    /* INIT: recv chunk from n-1 nodes on mul port */
    if (netMeta->expCount == 0)
    {
        int tmpSockfdArr[EC_N] = {0};
        int accept_node_num = netMeta->mulFailFlag == 0 ? EC_N - 1 : EC_N - MUL_FAIL_NUM;
        for (int i = 0; i < MUL_PORT_NUM; i++)
        {
            for (int j = 0; j < accept_node_num; j++)
            {
                int tmpSockfd = -1, tmpNodeindex = -1;
                tmpSockfd = AcceptAndGetNodeIndex(serverFdArr[i], &tmpNodeindex);
                tmpSockfdArr[tmpNodeindex] = tmpSockfd;
            }
            int loc = 0;
            for (int j = 0; j < EC_N; j++)
                if (tmpSockfdArr[j] != 0)
                    mulPortRecvSockfd[i][loc++] = tmpSockfdArr[j];
        }
        /* INIT: send chunks to fail/request node */
        for (int i = 0; i < MUL_PORT_NUM; i++)
            for (int j = 0; j < failNodeNum; j++)
                if ((clientInitNetwork(&mulPortSendSockfd[i][j], EC_STORAGENODES_PORT + i, startFailNodeIndex + j)) == EC_ERROR) // only the func close sockfd
                    ERROR_RETURN_NULL("Fail init network");
    }

    degradedReadNetRecordN = (degradedReadNetRecordN_t *)Malloc(sizeof(degradedReadNetRecordN_t) * 1);
    degradedReadNetRecordNInit(twoBucketsSliceIndexInfo, divisionNum, divisionLines, g_tbls2Arr);

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* send and recv buffer */
    int totalSliceNum = CHUNK_SIZE / SLICE_SIZE, *slice_bitmap_buffer = NULL, **sliceBitmap = NULL;
    char *bufRecv = NULL, **dataRecv = NULL, ***recvDataBucket = NULL;
    create3PointerArrCHAR(&bufRecv, &dataRecv, &recvDataBucket, EC_K, totalSliceNum, SLICE_SIZE);
    create2PointerArrINT(&slice_bitmap_buffer, &sliceBitmap, EC_K, totalSliceNum);
    for (int j = 0; j < EC_K * totalSliceNum; j++)
        slice_bitmap_buffer[j] = 0; // No slices recv

    /* send and recv slice data */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    queue_t sliceIndexRecvQueue[1];
    queueInit(sliceIndexRecvQueue, CHUNK_SIZE / SLICE_SIZE);
    int nodeNumTmp = netMeta->mulFailFlag == 0 ? EC_N - 1 : EC_N - MUL_FAIL_NUM;
    mulPortNetMetaInit3Arr(&mulPortNetMetaRecv, nodeNumTmp, RECV, totalSliceNum, NOENCODE, mulPortRecvSockfd, recvDataBucket, sliceIndexRecvQueue, sliceBitmap, NULL);
    mulPortNetMetaInit3Arr(&mulPortNetMetaSend, 1, SEND, totalSliceNum, ENCODE, mulPortSendSockfd, recvDataBucket, sliceIndexRecvQueue, NULL, NULL);
    mulPortNetMetaRecv.failNodeNum = failNodeNum;
    mulPortNetMetaSend.failNodeNum = failNodeNum;
    pthreadCreate(&mulPortNetRecvTid, NULL, netSliceNNode, (void *)&mulPortNetMetaRecv); // Create thread to recv slice
    pthreadCreate(&mulPortNetSendTid, NULL, netSliceNNode, (void *)&mulPortNetMetaSend); // Create thread to send slice
    pthreadJoin(mulPortNetRecvTid, NULL);
    pthreadJoin(mulPortNetSendTid, NULL);
    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);
    SendExp(exp); // send exp data

    queueFree(sliceIndexRecvQueue);
    delete2ArrINT(slice_bitmap_buffer, sliceBitmap);
    delete3ArrCHAR(bufRecv, dataRecv, recvDataBucket);
    delete3ArrUC(g_tblsBuf, g_tblsArr, g_tbls2Arr);
    free2(degradedReadNetRecordN, netDataBuf);
    if (netMeta->expCount == EXP_TIMES - 1)
    {
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_N);
        shutdown2Arr(mulPortSendSockfd, MUL_PORT_NUM, EC_M);
    }
    printf("[handleDegradedReadN end]\n");
    return NULL;
}

void *handleFullRecoverE(netMeta_t *netMeta, int clientFd)
{
    printf("[handleFullRecoverE begin]\n");
    char *netDataBuf = (char *)Malloc(sizeof(char) * netMeta->size);
    Recv(clientFd, netDataBuf, netMeta->size, "recv data");
    SendResponse(clientFd);

    int startFailNodeIndex = netMeta->mulFailFlag == 0 ? netMeta->dstNodeIndex : MUL_FAIL_START;
    int failNodeNum = netMeta->mulFailFlag == 0 ? 1 : MUL_FAIL_NUM;
    /* init network for ec_a nodes */
    if (netMeta->expCount == 0)
    {
        int tmpSockfdArr[EC_A] = {0};
        int recv_num = netMeta->mulFailFlag == 0 ? EC_A - 1 : EC_A - MUL_FAIL_NUM;
        /* INIT: recv*/
        for (int i = 0; i < MUL_PORT_NUM; i++)
        {
            for (int j = 0; j < recv_num; j++)
            {
                int tmpSockfd = -1, tmpNodeindex = -1;
                tmpSockfd = AcceptAndGetNodeIndex(serverFdArr[i], &tmpNodeindex);
                tmpSockfdArr[tmpNodeindex] = tmpSockfd;
            }
            for (int j = 0; j < EC_A; j++)
                mulPortRecvSockfd[i][j] = tmpSockfdArr[j];
        }
        /* INIT: send */
        for (int i = 0; i < MUL_PORT_NUM; i++)
            for (int j = 0; j < failNodeNum; j++)
                if ((clientInitNetwork(&mulPortSendSockfd[i][j], EC_STORAGENODES_PORT + i, startFailNodeIndex + j)) == EC_ERROR) // only the func close sockfd
                    ERROR_RETURN_NULL("Fail init network");
    }

    /* parse data */
    unsigned char *g_tblsBuf, **g_tblsArr, ***g_tbls2Arr; // must free
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K] = {0}, eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], stripeNum;
    char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN];
    int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M] = {0}, eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    if (netMeta->mulFailFlag == 0)
    {
        int curOffset = unpackedDataFullRecoverTE1(netDataBuf, increasingNodesIndexArr, eachNodeInfo, &stripeNum, fullRecoverFailBlockName);
        create2PointerArrUC(&g_tblsBuf, &g_tblsArr, stripeNum, EC_K * 32);
        memcpyMove(g_tblsArr[0], netDataBuf + curOffset, stripeNum * EC_K * 1 * 32 * sizeof(char), &curOffset);
    }
    else if (netMeta->mulFailFlag == 1) // mul failure
    {
        int curOffset = unpackedDataMulFullRecoverTE1(netDataBuf, increasingNodesIndexArr, eachNodeInfo, &stripeNum);
        create2PointerArrUC(&g_tblsBuf, &g_tblsArr, stripeNum, MUL_FAIL_NUM * EC_K * 32);
        curOffset = unpackedDataMulFullRecoverTE2(netDataBuf + curOffset, stripeNum, eachStripeFailNodeNum, eachStripeFailNodeIndex, g_tblsArr[0]);
    }
    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* send and recv buffer */
    int totalSliceNum = CHUNK_SIZE / SLICE_SIZE, *slice_bitmap_buffer = NULL, **sliceBitmap = NULL;
    char *bufRecv = NULL, **dataRecv = NULL, ***recvDataBucket = NULL;
    create3PointerArrCHAR(&bufRecv, &dataRecv, &recvDataBucket, EC_K, totalSliceNum, SLICE_SIZE);
    create2PointerArrINT(&slice_bitmap_buffer, &sliceBitmap, EC_K, totalSliceNum);

    /* modify mulPortRecvSockfd */
    int *sockFdBuf = NULL, **mulPortRecvSockfdArrBuf = NULL, ***mulPortRecvSockfdArr = NULL;
    create3PointerArrINT(&sockFdBuf, &mulPortRecvSockfdArrBuf, &mulPortRecvSockfdArr, stripeNum, MUL_PORT_NUM, EC_K);
    for (int k = 0; k < stripeNum; k++)
        for (int j = 0; j < MUL_PORT_NUM; j++)
            for (int i = 0; i < EC_K; i++)
                if (netMeta->mulFailFlag == 0)
                    mulPortRecvSockfdArr[k][j][i] = mulPortRecvSockfd[j][increasingNodesIndexArr[k][i]];
                else if (netMeta->mulFailFlag == 1) // mul failure
                    mulPortRecvSockfdArr[k][j][i] = mulPortRecvSockfd[j][increasingNodesIndexArr[k][i]];

    for (int i = 0; i < stripeNum; i++)
    {
        for (int j = 0; j < EC_K * totalSliceNum; j++)
            slice_bitmap_buffer[j] = 0; // No slices recv

        /* send and recv slice data */
        clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
        queue_t sliceIndexRecvQueue[1];
        queueInit(sliceIndexRecvQueue, CHUNK_SIZE / SLICE_SIZE);
        if (netMeta->mulFailFlag == 1)
            failNodeNum = eachStripeFailNodeNum[i];
        int failNodeIndex[failNodeNum];
        for (int j = 0; j < failNodeNum; j++)
            failNodeIndex[j] = netMeta->mulFailFlag == 0 ? startFailNodeIndex + j : eachStripeFailNodeIndex[i][j];
        mulPortNetMetaInit3Arr(&mulPortNetMetaRecv, EC_K, RECV, totalSliceNum, NOENCODE, mulPortRecvSockfdArr[i], recvDataBucket, sliceIndexRecvQueue, sliceBitmap, NULL);
        mulPortNetMetaInit3Arr(&mulPortNetMetaSend, 1, SEND, totalSliceNum, ENCODE, mulPortSendSockfd, recvDataBucket, sliceIndexRecvQueue, NULL, g_tblsArr[i]);
        mulPortNetMetaInitSend2(&mulPortNetMetaSend, startFailNodeIndex, failNodeNum, failNodeIndex);
        pthreadCreate(&mulPortNetRecvTid, NULL, netSliceKNode, (void *)&mulPortNetMetaRecv); // Create thread to recv slice
        pthreadCreate(&mulPortNetSendTid, NULL, netSliceKNode, (void *)&mulPortNetMetaSend); // Create thread to send slice
        pthreadJoin(mulPortNetSendTid, NULL);
        pthreadJoin(mulPortNetRecvTid, NULL);
        queueFree(sliceIndexRecvQueue);
        ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    }
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);
    SendExp(exp); // send exp data

    delete3ArrINT(sockFdBuf, mulPortRecvSockfdArrBuf, mulPortRecvSockfdArr);
    delete2ArrINT(slice_bitmap_buffer, sliceBitmap);
    delete3ArrCHAR(bufRecv, dataRecv, recvDataBucket);
    delete2ArrUC(g_tblsBuf, g_tblsArr);
    free(netDataBuf);
    if (netMeta->expCount == EXP_TIMES - 1)
    {
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_A);
        shutdown2Arr(mulPortSendSockfd, MUL_PORT_NUM, EC_M);
    }
    printf("[handleDegradedReadE end]\n");
    return NULL;
}

void *handleFullRecoverN(netMeta_t *netMeta, int clientFd)
{
    printf("[handleFullRecoverN begin]\n");
    char *netDataBuf = (char *)Malloc(sizeof(char) * netMeta->size);
    Recv(clientFd, netDataBuf, netMeta->size, "recv data");
    SendResponse(clientFd);

    int startFailNodeIndex = netMeta->mulFailFlag == 0 ? netMeta->dstNodeIndex : MUL_FAIL_START;
    int failNodeNum = netMeta->mulFailFlag == 0 ? 1 : MUL_FAIL_NUM;
    /* parse data */
    unsigned char *g_tblsBuf, **g_tblsArr, ***g_tbls2Arr, ****g_tbls3Arr;
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N - 1] = {0}, eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM];
    int stripeNum, maxDivisionNum;
    int curOffset = unpackedDataFullRecoverN1(netDataBuf, increasingNodesIndexArr, eachNodeInfo, &stripeNum, &maxDivisionNum);
    int(*twoBucketsSliceIndexInfo)[EC_N - 1][6] = (int(*)[EC_N - 1][6]) Calloc(stripeNum, sizeof(*twoBucketsSliceIndexInfo));
    int(*divisionLines)[2 * EC_N] = (int(*)[2 * EC_N]) Calloc(stripeNum, sizeof(*divisionLines));
    int(*divisionNum) = Calloc(stripeNum, sizeof(*divisionNum));
    int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M] = {0};
    char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN];
    if (netMeta->mulFailFlag == 0)
    {
        create4PointerArrUC(&g_tblsBuf, &g_tblsArr, &g_tbls2Arr,&g_tbls3Arr,1, stripeNum, maxDivisionNum, EC_K * 32);
        curOffset = +unpackedDataFullRecoverN2(netDataBuf + curOffset, stripeNum, maxDivisionNum, twoBucketsSliceIndexInfo, divisionLines,
                                               divisionNum, fullRecoverFailBlockName, g_tbls2Arr[0][0]);
    }
    if (netMeta->mulFailFlag == 1) // mul failure
    {
        create4PointerArrUC(&g_tblsBuf, &g_tblsArr, &g_tbls2Arr, &g_tbls3Arr, stripeNum, MUL_FAIL_NUM, maxDivisionNum, EC_K * 32);
        curOffset = +unpackedDataMulFullRecoverN2(netDataBuf + curOffset, stripeNum, maxDivisionNum, twoBucketsSliceIndexInfo, divisionLines,
                                                  divisionNum, eachStripeFailNodeNum, eachStripeFailNodeIndex, g_tbls3Arr[0][0][0]);
    }

    /* init network for ec_a nodes */
    if (netMeta->expCount == 0)
    {
        int tmpSockfdArr[EC_A] = {0};
        int recv_num = netMeta->mulFailFlag == 0 ? EC_A - 1 : EC_A - MUL_FAIL_NUM;
        /* INIT: recv*/
        for (int i = 0; i < MUL_PORT_NUM; i++)
        {
            for (int j = 0; j < recv_num; j++)
            {
                int tmpSockfd = -1, tmpNodeindex = -1;
                tmpSockfd = AcceptAndGetNodeIndex(serverFdArr[i], &tmpNodeindex);
                tmpSockfdArr[tmpNodeindex] = tmpSockfd;
            }
            int loc = 0;
            for (int j = 0; j < EC_A; j++)
                if (tmpSockfdArr[j] != 0)
                    mulPortRecvSockfd[i][loc++] = tmpSockfdArr[j];
        }
        /* INIT: send */
        for (int i = 0; i < MUL_PORT_NUM; i++)
            for (int j = 0; j < failNodeNum; j++)
                if ((clientInitNetwork(&mulPortSendSockfd[i][j], EC_STORAGENODES_PORT + i, startFailNodeIndex + j)) == EC_ERROR) // only the func close sockfd
                    ERROR_RETURN_NULL("Fail init network");
    }

    /* para */
    fullRecoverNetRecordN = (fullRecoverNetRecordN_t *)Malloc(sizeof(fullRecoverNetRecordN_t) * 1);
    fullRecoverNetRecordNInit(stripeNum, increasingNodesIndexArr, twoBucketsSliceIndexInfo, divisionNum, divisionLines, eachNodeInfo, g_tbls2Arr, g_tbls3Arr);

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* send and recv buffer */
    int totalSliceNum = stripeNum * (CHUNK_SIZE / SLICE_SIZE), *slice_bitmap_buffer = NULL, **sliceBitmap = NULL;
    char *bufRecv = NULL, **dataRecv = NULL, ***recvDataBucket = NULL;
    create3PointerArrCHAR(&bufRecv, &dataRecv, &recvDataBucket, stripeNum * EC_K, CHUNK_SIZE / SLICE_SIZE, SLICE_SIZE);
    create2PointerArrINT(&slice_bitmap_buffer, &sliceBitmap, EC_K * stripeNum, CHUNK_SIZE / SLICE_SIZE);
    for (int i = 0; i < EC_K * totalSliceNum; i++)
        slice_bitmap_buffer[i] = -1;
    for (int j = 0; j < EC_K * totalSliceNum; j++)
        slice_bitmap_buffer[j] = 0; // No slices recv

    /* send and recv slice data */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    queue_t sliceIndexRecvQueue[1];
    queueInit(sliceIndexRecvQueue, totalSliceNum);

    mulPortNetMetaRecv.startFailNodeIndex = netMeta->dstNodeIndex;
    mulPortNetMetaInit3Arr(&mulPortNetMetaRecv, EC_A - 1, RECV, totalSliceNum, NOENCODE, mulPortRecvSockfd, recvDataBucket, sliceIndexRecvQueue, sliceBitmap, NULL);
    mulPortNetMetaInit3Arr(&mulPortNetMetaSend, 1, SEND, totalSliceNum, ENCODE, mulPortSendSockfd, recvDataBucket, sliceIndexRecvQueue, NULL, NULL);
    if (netMeta->mulFailFlag == 0)
    {
        pthreadCreate(&mulPortNetRecvTid, NULL, netSliceANode1Fail, (void *)&mulPortNetMetaRecv); // Create thread to recv slice
        pthreadCreate(&mulPortNetSendTid, NULL, netSliceANode1Fail, (void *)&mulPortNetMetaSend); // Create thread to send slice
    }
    else
    {
        memcpy(mulPortNetMetaRecv.eachStripeFailNodeIndex, eachStripeFailNodeIndex, stripeNum * EC_M * sizeof(int));
        memcpy(mulPortNetMetaRecv.eachStripeFailNodeNum, eachStripeFailNodeNum, stripeNum * sizeof(int));
        memcpy(mulPortNetMetaSend.eachStripeFailNodeIndex, eachStripeFailNodeIndex, stripeNum * EC_M * sizeof(int));
        memcpy(mulPortNetMetaSend.eachStripeFailNodeNum, eachStripeFailNodeNum, stripeNum * sizeof(int));
        pthreadCreate(&mulPortNetRecvTid, NULL, netSliceANodeMulFail, (void *)&mulPortNetMetaRecv); // Create thread to send slice
        pthreadCreate(&mulPortNetSendTid, NULL, netSliceANodeMulFail, (void *)&mulPortNetMetaSend); // Create thread to send slice
    }
    pthreadJoin(mulPortNetRecvTid, NULL);
    pthreadJoin(mulPortNetSendTid, NULL);
    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);
    SendExp(exp); // send exp data

    queueFree(sliceIndexRecvQueue);
    delete3ArrCHAR(bufRecv, dataRecv, recvDataBucket);
    delete4ArrUC(g_tblsBuf, g_tblsArr, g_tbls2Arr, g_tbls3Arr);
    free4(fullRecoverNetRecordN, divisionLines, twoBucketsSliceIndexInfo, netDataBuf);
    if (netMeta->expCount == EXP_TIMES - 1)
    {
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_A);
        shutdown2Arr(mulPortSendSockfd, MUL_PORT_NUM, EC_M);
    }
    printf("[handleFullRecoverN end]\n");
    return NULL;
}

void *no_handle_func(netMeta_t *netMeta, int clientFd)
{
    printf("no handle func\n");
    return NULL;
}

void *handleClient(void *arg)
{
    printf("[ec_pnetdevice_handle_client begin]\n");

    int clientFd = *((int *)arg);                                // only the func close
    netMeta_t *netMeta = (netMeta_t *)Malloc(sizeof(netMeta_t)); // only the func free
    /* EXP START */
    for (int i = 0; i < EXP_TIMES; i++)
    {
        /* recv netMeta and send response */
        Recv(clientFd, netMeta, sizeof(netMeta_t), "recv netMeta");

        type_handle_func handle_func[] = {
            no_handle_func, no_handle_func,
            no_handle_func, no_handle_func,
            no_handle_func, no_handle_func,
            handleDegradedReadE, handleDegradedReadE,
            handleDegradedReadN, handleDegradedReadE,
            no_handle_func, no_handle_func,
            no_handle_func, no_handle_func,
            handleFullRecoverE, handleFullRecoverE,
            handleFullRecoverN, handleFullRecoverN};
        handle_func[netMeta->cmdType](netMeta, clientFd);

        if (netMeta->cmdType < 2)
            break; // non - exp function
    }
    /* EXP END */

    free(netMeta);
    shutdown(clientFd, SHUT_RDWR);
    printf("[ec_pnetdevice_handle_client end]\n");
    pthread_exit(NULL);
    return NULL;
}

int main()
{
    printf("[ec_pnetdevice begin]\n");

    getLocalIPLastNum(&nodeIP);
    exp = (exp_t *)Malloc(sizeof(exp_t));
    create2PointerArrINT(&mulPortRecvSockfdBuf, &mulPortRecvSockfd, MUL_PORT_NUM, EC_A);
    create2PointerArrINT(&mulPortSendSockfdBuf, &mulPortSendSockfd, MUL_PORT_NUM, EC_A);

    int serverFd;
    if ((serverInitNetwork(&serverFd, EC_PND_CLIENT_PORT)) == EC_ERROR) //  init server network between client and node
        ERROR_RETURN_VALUE("error serverInitNetwork");

    for (int i = 0; i < MUL_PORT_NUM; i++)
        if ((serverInitNetwork(&serverFdArr[i], EC_PND_STORAGENODES_PORT + i)) == EC_ERROR) // init server network between nodes
            ERROR_RETURN_NULL("error serverInitNetwork for mul port");
    pthread_t handleClientTid;
    while (1)
    {
        int clientFd= Accept(serverFd);
        pthreadCreate(&handleClientTid, NULL, handleClient, (void *)&clientFd);
        pthreadJoin(handleClientTid, NULL);
    }
    delete2ArrINT(mulPortRecvSockfdBuf, mulPortRecvSockfd);
    delete2ArrINT(mulPortSendSockfdBuf, mulPortSendSockfd);
    return EC_OK;
}
