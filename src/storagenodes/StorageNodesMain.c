#include "ECConfig.h"
#include "MulPortTransfer.h"
#include "PackedData.h"
#include "ECEncode.h"

static int serverFdArr[MUL_PORT_NUM], nodeIP;
static int **mulPortRecvSockfd = NULL, *mulPortRecvSockfdBuf = NULL;
static int **mulPortSendSockfd = NULL, *mulPortSendSockfdBuf = NULL;

/* HDFS */
static hdfs_info_t *HDFSInfo = NULL;

/* mul use variable */
static struct timespec totalTimeStart, ioTimeStart, calTimeStart, netTimeStart;
static struct timespec totalTimeEnd, ioTimeEnd, calTimeEnd, netTimeEnd;
static long long totalExpTime, ioExpTime, calExpTime, netExpTime;
static exp_t *exp = NULL;

static mulPortNetMeta_t mulPortNetMetaSend, mulPortNetMetaRecv;
static pthread_t mulPortNetSendTid, mulPortNetRecvTid;

/* mul use func */

void *acceptECAStoragenodes(void *arg)
{
    netMeta_t *netMeta = (netMeta_t *)arg;
    int tmpSockfdArr[EC_A] = {0}, cur_storagenode_fail = netMeta->dstNodeIndex;

    for (int i = 0; i < MUL_PORT_NUM; i++)
    {
        for (int j = 0; j < EC_A; j++)
        {
            if ((netMeta->mulFailFlag == 0 && j != nodeIP - STORAGENODES_START_IP_ADDR && j != cur_storagenode_fail) ||
                (netMeta->mulFailFlag == 1 && j != nodeIP - STORAGENODES_START_IP_ADDR && (j < MUL_FAIL_START || j > MUL_FAIL_START + MUL_FAIL_NUM - 1)))
            {
                int tmpSockfd = -1, tmpNodeindex = -1;
                tmpSockfd = AcceptAndGetNodeIndex(serverFdArr[i], &tmpNodeindex);
                tmpSockfdArr[tmpNodeindex] = tmpSockfd;
            }
        }
        for (int j = 0; j < EC_A; j++)
            mulPortRecvSockfd[i][j] = tmpSockfdArr[j];
    }

    return NULL;
}

void *connectECAStoragenodes(void *arg)
{
    for (int i = 0; i < MUL_PORT_NUM; i++)
        for (int j = 0; j < EC_A; j++)
        {
            mulPortSendSockfd[i][j] = 0;
            if (j == nodeIP - STORAGENODES_START_IP_ADDR)
                continue;
            if ((clientInitNetwork(&mulPortSendSockfd[i][j], EC_STORAGENODES_PORT + i, j)) == EC_ERROR) // only the func close sockfd
                ERROR_RETURN_NULL("Fail init network");
        }
    return NULL;
}

void *handleReadChunks(netMeta_t *netMeta, int clientFd)
{
    printf("[handleReadChunks begin]\n");

    char *sendChunkBuf = (char *)Malloc(sizeof(char) * CHUNK_SIZE); // buffer for EC chunk
    /* file IO */
    if (openReadFile(netMeta->dstFileName, "rb", sendChunkBuf, CHUNK_SIZE) == EC_ERROR)
        ERROR_RETURN_NULL("openReadFile");

    /* send chunk data to client*/
    Send(clientFd, sendChunkBuf, CHUNK_SIZE, "send chunk");
    RecvResponse(clientFd);
    free(sendChunkBuf);
    printf("[handleReadChunks end]\n");
    return NULL;
}

void *handleWriteChunks(netMeta_t *netMeta, int clientFd)
{
    printf("[handleWriteChunks begin]\n");

    /* recv chunk data and send response */
    char *sendChunkBuf = (char *)Malloc(sizeof(char) * CHUNK_SIZE); // buffer for EC chunk
    Recv(clientFd, sendChunkBuf, CHUNK_SIZE, "recv chunk");
    SendResponse(clientFd);

    /* file IO */
    if (openWriteFile(netMeta->dstFileName, "wb", sendChunkBuf, CHUNK_SIZE) == EC_ERROR)
        ERROR_RETURN_NULL("openWriteFile");
    free(sendChunkBuf);
    printf("[handleWriteChunks end]\n");
    return NULL;
}

void *handleHealNodeDegradedReadT(netMeta_t *netMeta, int clientFd)
{
    printf("[handleHealNodeDegradedReadT begin]\n");
    SendResponse(clientFd);

    int startFailNodeIndex = netMeta->mulFailFlag == 0 ? netMeta->dstNodeIndex : MUL_FAIL_START;
    int failNodeNum = netMeta->mulFailFlag == 0 ? 1 : MUL_FAIL_NUM;
    /* init network for request nodes */
    if (netMeta->expCount == 0)
        for (int i = 0; i < MUL_PORT_NUM; i++)
            for (int j = 0; j < failNodeNum; j++)
                if ((clientInitNetwork(&mulPortSendSockfd[i][j], EC_STORAGENODES_PORT + i, startFailNodeIndex + j)) == EC_ERROR) // only the func close sockfd
                    ERROR_RETURN_NULL("Fail init network");

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* read chunk from disk */
    unsigned char *sendChunkBuf = NULL, **sendChunk = NULL;
    create2PointerArrUC(&sendChunkBuf, &sendChunk, failNodeNum, CHUNK_SIZE);
    clock_gettime(CLOCK_MONOTONIC, &ioTimeStart);
    if (openReadFile(netMeta->dstFileName, "rb", sendChunkBuf, CHUNK_SIZE) == EC_ERROR)
        ERROR_RETURN_NULL("openReadFile");
    ADD_TIME(ioTimeStart, ioTimeEnd, exp->ioTime);

    /* SEND: send chunks to new nodes */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    for (int i = 0; i < failNodeNum; i++)
        sendChunk[i] = sendChunkBuf;
    if ((mulPortNetTransfer(sendChunk, mulPortSendSockfd, CHUNK_SIZE, failNodeNum, SEND)) == EC_ERROR)
        ERROR_RETURN_NULL("Fail send MUL_FAIL_NUM chunks");

    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);

    SendExp(exp); // send exp data
    delete2ArrUC(sendChunkBuf, sendChunk);
    /* END: send chunks to new nodes */
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortSendSockfd, MUL_PORT_NUM, EC_M);
    printf("[handleHealNodeDegradedReadT end]\n");
    return NULL;
}

void *handleFailNodeDegradedReadT(netMeta_t *netMeta, int clientFd)
{
    printf("[handleFailNodeDegradedReadT begin]\n");
    unsigned char *g_tbls = (unsigned char *)Calloc(MUL_FAIL_NUM * EC_K * 32, sizeof(unsigned char));
    Recv(clientFd, g_tbls, netMeta->size, "recv data");
    SendResponse(clientFd);

    /* INIT: recv chunk from k nodes on mul port */
    if (netMeta->expCount == 0)
        for (int i = 0; i < MUL_PORT_NUM; i++)
        {
            int tmpSockfdArr[EC_N] = {0};
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

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* EC arguments */
    unsigned char *sendChunkBuf = NULL, **sendChunk = NULL, *codingChunkBuf = NULL, **codingChunk = NULL;
    create2PointerArrUC(&sendChunkBuf, &sendChunk, EC_K, CHUNK_SIZE);
    create2PointerArrUC(&codingChunkBuf, &codingChunk, 1, CHUNK_SIZE);
    /* RECV: recv chunk from k nodes on mul port */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    if ((mulPortNetTransfer(sendChunk, mulPortRecvSockfd, CHUNK_SIZE, EC_K, RECV)) == EC_ERROR)
        ERROR_RETURN_NULL("Fail recv k chunks");
    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);

    /* cal data chunks */
    clock_gettime(CLOCK_MONOTONIC, &calTimeStart);
    mulThreadEncode(CHUNK_SIZE, EC_K, 1, g_tbls, (unsigned char **)sendChunk, (unsigned char **)codingChunk);
    ADD_TIME(calTimeStart, calTimeEnd, exp->calTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);
    SendExp(exp); // send exp data

    /* writ data chunks, cannot increase exp time */
    if (netMeta->expCount == EXP_TIMES - 1 && FLAG_WRITE_REPAIR == 1)
        if (openWriteFile(netMeta->dstFileName, "wb", codingChunk[0], CHUNK_SIZE) == EC_ERROR)
            ERROR_RETURN_NULL("openWriteMulFile");

    delete2ArrUC(sendChunkBuf, sendChunk);
    delete2ArrUC(codingChunkBuf, codingChunk);
    free(g_tbls);
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_K);
    printf("[handleFailNodeDegradedReadT end]\n");
    return NULL;
}

void *handleHealNodeDegradedReadR(netMeta_t *netMeta, int clientFd)
{
    printf("[handleHealNodeDegradedReadR begin]\n");
    unsigned char *g_tbls = (unsigned char *)Calloc(MUL_FAIL_NUM * 2 * 32, sizeof(unsigned char));
    Recv(clientFd, g_tbls, netMeta->size, "recv g_tbls for decoding");
    SendResponse(clientFd);

    /* INIT: recv chunk from 1 node on mul port */
    int startFailNodeIndex = netMeta->mulFailFlag == 0 || netMeta->dstNodeIndex != MUL_FAIL_START + MUL_FAIL_NUM - 1 ? netMeta->dstNodeIndex : MUL_FAIL_START;
    int failNodeNum = netMeta->mulFailFlag == 0 || netMeta->dstNodeIndex != MUL_FAIL_START + MUL_FAIL_NUM - 1 ? 1 : MUL_FAIL_NUM;
    if (netMeta->expCount == 0)
    {
        if (netMeta->srcNodeIndex != -1)
            for (int i = 0; i < MUL_PORT_NUM; i++)
                mulPortRecvSockfd[i][0] = Accept(serverFdArr[i]);

        for (int i = 0; i < MUL_PORT_NUM; i++)
            for (int j = 0; j < failNodeNum; j++)
                if ((clientInitNetwork(&mulPortSendSockfd[i][j], EC_STORAGENODES_PORT + i, startFailNodeIndex + j)) == EC_ERROR) // only the func close sockfd
                    ERROR_RETURN_NULL("Fail init network");
    }

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* read chunk from disk */
    char *sendChunkBuf = NULL, **dataSend = NULL, *bufRecv = NULL, **dataRecv = NULL;
    create2PointerArrCHAR(&sendChunkBuf, &dataSend, 1, CHUNK_SIZE);
    create2PointerArrCHAR(&bufRecv, &dataRecv, 1, CHUNK_SIZE);
    memset(bufRecv, 0, CHUNK_SIZE); // must set 0, since first src node need encode into sendData with it
    clock_gettime(CLOCK_MONOTONIC, &ioTimeStart);
    if (openReadFile(netMeta->dstFileName, "rb", sendChunkBuf, CHUNK_SIZE) == EC_ERROR)
        ERROR_RETURN_NULL("openReadFile");
    ADD_TIME(ioTimeStart, ioTimeEnd, exp->ioTime);

    /* send and recv slice data */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    int actualFailNodeNum = netMeta->mulFailFlag == 0 ? 1 : MUL_FAIL_NUM;
    int *tmpSockfdBuf = NULL, **mulPortMulNodeSendTmpSockfd = NULL, ***tmpSockfd = NULL;
    create3PointerArrINT(&tmpSockfdBuf, &mulPortMulNodeSendTmpSockfd, &tmpSockfd, actualFailNodeNum, MUL_PORT_NUM, 1);
    for (int i = 0; i < MUL_PORT_NUM; i++)
        for (int j = 0; j < actualFailNodeNum; j++)
            tmpSockfd[j][i][0] = mulPortSendSockfd[i][j];

    for (int i = 0; i < actualFailNodeNum; i++)
    {
        int totalSliceNum = CHUNK_SIZE / SLICE_SIZE;
        int recvSliceIndex = netMeta->srcNodeIndex == -1 ? totalSliceNum : 0;
        mulPortNetMetaInitBasic(&mulPortNetMetaRecv, 1, RECV, totalSliceNum, NOENCODE, mulPortRecvSockfd, dataRecv, NULL, &recvSliceIndex, NULL);
        if (actualFailNodeNum != failNodeNum)
            mulPortNetMetaInitBasic(&mulPortNetMetaSend, 1, SEND, totalSliceNum, ENCODE, tmpSockfd[0], dataRecv, dataSend, &recvSliceIndex, g_tbls + i * 2 * 32);
        else
            mulPortNetMetaInitBasic(&mulPortNetMetaSend, 1, SEND, totalSliceNum, ENCODE, tmpSockfd[i], dataRecv, dataSend, &recvSliceIndex, g_tbls + i * 2 * 32);
        if (netMeta->srcNodeIndex != -1)
            pthreadCreate(&mulPortNetRecvTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaRecv); // Create thread to recv slice
        pthreadCreate(&mulPortNetSendTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaSend);     // Create thread to send slice
        pthreadJoin(mulPortNetSendTid, NULL);
        if (netMeta->srcNodeIndex != -1)
            pthreadJoin(mulPortNetRecvTid, NULL);
    }
    delete3ArrINT(tmpSockfdBuf, mulPortMulNodeSendTmpSockfd, tmpSockfd);

    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);
    SendExp(exp); // send exp data

    delete2ArrCHAR(sendChunkBuf, dataSend);
    delete2ArrCHAR(bufRecv, dataRecv);
    free(g_tbls);
    /* END: recv/send chunks from/to node */
    if (netMeta->expCount == EXP_TIMES - 1)
    {
        if (netMeta->srcNodeIndex != -1)
            shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, 1);
        shutdown2Arr(mulPortSendSockfd, MUL_PORT_NUM, EC_M);
    }

    printf("[handleHealNodeDegradedReadR end]\n");
    return NULL;
}

void *handleFailNodeDegradedReadR(netMeta_t *netMeta, int clientFd)
{
    printf("[handleFailNodeDegradedReadR begin]\n");
    SendResponse(clientFd);

    /* INIT: recv chunk from 1 node on mul port */
    if (netMeta->expCount == 0)
        for (int i = 0; i < MUL_PORT_NUM; i++)
            mulPortRecvSockfd[i][0] = Accept(serverFdArr[i]);

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* recv buffer */
    char *bufRecv = NULL, **dataRecv = NULL;
    create2PointerArrCHAR(&bufRecv, &dataRecv, 1, CHUNK_SIZE);

    /* recv slice data */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    int totalSliceNum = CHUNK_SIZE / SLICE_SIZE, recvSliceIndex = 0;
    mulPortNetMetaInitBasic(&mulPortNetMetaRecv, 1, RECV, totalSliceNum, NOENCODE, mulPortRecvSockfd, dataRecv, NULL, &recvSliceIndex, NULL);

    pthreadCreate(&mulPortNetRecvTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaRecv); // Create thread to recv slice
    pthreadJoin(mulPortNetRecvTid, NULL);
    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);
    SendExp(exp); // send exp data

    /* writ data chunks, cannot increase exp time */
    if (netMeta->expCount == EXP_TIMES - 1 && FLAG_WRITE_REPAIR == 1)
        if (openWriteFile(netMeta->dstFileName, "wb", bufRecv, CHUNK_SIZE) == EC_ERROR)
            ERROR_RETURN_NULL("openWriteMulFile");
    delete2ArrCHAR(bufRecv, dataRecv);
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_K);
    printf("[handleFailNodeDegradedReadR end]\n");
    return NULL;
}

void *handleHealNodeDegradedReadE(netMeta_t *netMeta, int clientFd)
{
    printf("[handleHealNodeDegradedReadE begin]\n");
    SendResponse(clientFd);

    /* INIT: send chunks to PND */
    if (netMeta->expCount == 0)
        for (int i = 0; i < MUL_PORT_NUM; i++)
            if ((clientInitNetwork(&mulPortSendSockfd[i][0], EC_PND_STORAGENODES_PORT + i, PNETDEVICE_IP_ADDR - STORAGENODES_START_IP_ADDR)) == EC_ERROR) // only the func close sockfd
                ERROR_RETURN_NULL("Fail init network");

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* read chunk from disk */
    char *sendChunkBuf = NULL, **dataSend = NULL;
    create2PointerArrCHAR(&sendChunkBuf, &dataSend, 1, CHUNK_SIZE);
    clock_gettime(CLOCK_MONOTONIC, &ioTimeStart);
    if (openReadFile(netMeta->dstFileName, "rb", sendChunkBuf, CHUNK_SIZE) == EC_ERROR)
        ERROR_RETURN_NULL("openReadFile");
    ADD_TIME(ioTimeStart, ioTimeEnd, exp->ioTime);

    /* send and recv slice data */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    int totalSliceNum = CHUNK_SIZE / SLICE_SIZE, recvSliceIndex = totalSliceNum;
    mulPortNetMetaInitBasic(&mulPortNetMetaSend, 1, SEND, totalSliceNum, NOENCODE, mulPortSendSockfd, NULL, dataSend, &recvSliceIndex, NULL);
    pthreadCreate(&mulPortNetSendTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaSend); // Create thread to send slice
    pthreadJoin(mulPortNetSendTid, NULL);
    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);
    SendExp(exp); // send exp data
    delete2ArrCHAR(sendChunkBuf, dataSend);

    /* END: send chunks to node */
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortSendSockfd, MUL_PORT_NUM, 1);
    printf("[handleHealNodeDegradedReadE end]\n");
    return NULL;
}

void *handleFailNodeDegradedReadE(netMeta_t *netMeta, int clientFd)
{
    printf("[handleFailNodeDegradedReadE begin]\n");
    SendResponse(clientFd);

    /* INIT: recv chunk from 1 node on mul port */
    if (netMeta->expCount == 0)
        for (int i = 0; i < MUL_PORT_NUM; i++)
            mulPortRecvSockfd[i][0] = Accept(serverFdArr[i]);

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* recv buffer */
    char *bufRecv = NULL, **dataRecv = NULL;
    create2PointerArrCHAR(&bufRecv, &dataRecv, 1, CHUNK_SIZE);

    /* recv slice data */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    int totalSliceNum = CHUNK_SIZE / SLICE_SIZE, recvSliceIndex = totalSliceNum, p_slice_loc[CHUNK_SIZE / SLICE_SIZE];
    mulPortNetMetaInitBasic(&mulPortNetMetaRecv, 1, RECV, totalSliceNum, NOENCODE, mulPortRecvSockfd, dataRecv, NULL, &recvSliceIndex, NULL);
    pthreadCreate(&mulPortNetRecvTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaRecv); // Create thread to recv slice
    pthreadJoin(mulPortNetRecvTid, NULL);
    Recv(mulPortNetMetaRecv.mulPortSockfdArr[0][0], p_slice_loc, sizeof(int) * (CHUNK_SIZE / SLICE_SIZE), "recv slice loc");
    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);
    SendExp(exp); // send exp data

    /* writ data chunks, cannot increase exp time */
    if (netMeta->expCount == EXP_TIMES - 1 && FLAG_WRITE_REPAIR == 1)
    {
        char *tmp_buffer = (char *)Calloc(CHUNK_SIZE, sizeof(char));
        for (int i = 0; i < CHUNK_SIZE / SLICE_SIZE; i++)
            memcpy(tmp_buffer + p_slice_loc[i] * SLICE_SIZE, bufRecv + i * SLICE_SIZE, sizeof(char) * SLICE_SIZE);
        if (openWriteFile(netMeta->dstFileName, "wb", tmp_buffer, CHUNK_SIZE) == EC_ERROR)
            ERROR_RETURN_NULL("openWriteMulFile");
        free(tmp_buffer);
    }

    delete2ArrCHAR(bufRecv, dataRecv);
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_K);
    printf("[handleFailNodeDegradedReadE end]\n");
    return NULL;
}

void *handleHealNodeDegradedReadN(netMeta_t *netMeta, int clientFd)
{
    printf("[handleHealNodeDegradedReadN begin]\n");
    int *twoBucketsSliceIndexInfo = (int *)Calloc(6, sizeof(int));
    Recv(clientFd, twoBucketsSliceIndexInfo, netMeta->size, "recv twoBucketsSliceIndexInfo");
    SendResponse(clientFd);

    /* INIT: send chunks to PND */
    if (netMeta->expCount == 0)
        for (int i = 0; i < MUL_PORT_NUM; i++)
            if ((clientInitNetwork(&mulPortSendSockfd[i][0], EC_PND_STORAGENODES_PORT + i, PNETDEVICE_IP_ADDR - STORAGENODES_START_IP_ADDR)) == EC_ERROR) // only the func close sockfd
                ERROR_RETURN_NULL("Fail init network");

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* read chunk from disk: first read min_index */
    char *sendChunkBuf = NULL, **dataSend = NULL;
    create2PointerArrCHAR(&sendChunkBuf, &dataSend, 1, CHUNK_SIZE);
    clock_gettime(CLOCK_MONOTONIC, &ioTimeStart);
    if (twoBucketsSliceIndexInfo[0] != -1)
        if (openReadOffsetFile(netMeta->dstFileName, "rb", twoBucketsSliceIndexInfo[0] * SLICE_SIZE, sendChunkBuf, twoBucketsSliceIndexInfo[1] * SLICE_SIZE) == EC_ERROR)
            ERROR_RETURN_NULL("openReadOffsetFile 1");
    if (twoBucketsSliceIndexInfo[3] != -1)
    {
        int lastIndex = twoBucketsSliceIndexInfo[1] == -1 ? 0 : twoBucketsSliceIndexInfo[1];
        if (openReadOffsetFile(netMeta->dstFileName, "rb", twoBucketsSliceIndexInfo[3] * SLICE_SIZE, sendChunkBuf + twoBucketsSliceIndexInfo[1] * SLICE_SIZE, twoBucketsSliceIndexInfo[4] * SLICE_SIZE) == EC_ERROR)
            ERROR_RETURN_NULL("openReadOffsetFile 2");
    }
    ADD_TIME(ioTimeStart, ioTimeEnd, exp->ioTime);
    /* send and recv slice data */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    int totalSliceNum = 0;
    totalSliceNum += twoBucketsSliceIndexInfo[0] != -1 ? twoBucketsSliceIndexInfo[1] : 0;
    totalSliceNum += twoBucketsSliceIndexInfo[3] != -1 ? twoBucketsSliceIndexInfo[4] : 0;
    int recvSliceIndex = totalSliceNum;
    mulPortNetMetaInitBasic(&mulPortNetMetaSend, 1, SEND, totalSliceNum, NOENCODE, mulPortSendSockfd, NULL, dataSend, &recvSliceIndex, NULL);
    pthreadCreate(&mulPortNetSendTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaSend); // Create thread to send slice
    pthreadJoin(mulPortNetSendTid, NULL);
    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);
    SendExp(exp); // send exp data
    delete2ArrCHAR(sendChunkBuf, dataSend);
    free(twoBucketsSliceIndexInfo);

    /* END: send chunks to node */
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortSendSockfd, MUL_PORT_NUM, 1);
    printf("[handleHealNodeDegradedReadN end]\n");
    return NULL;
}

void *handleFailNodeDegradedReadN(netMeta_t *netMeta, int clientFd)
{
    printf("[handleFailNodeDegradedReadN begin]\n");
    SendResponse(clientFd);

    /* INIT: recv chunk from 1 node on mul port */
    if (netMeta->expCount == 0)
        for (int i = 0; i < MUL_PORT_NUM; i++)
            mulPortRecvSockfd[i][0] = Accept(serverFdArr[i]);

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* recv buffer */
    char *bufRecv = NULL, **dataRecv = NULL;
    create2PointerArrCHAR(&bufRecv, &dataRecv, 1, CHUNK_SIZE);

    /* recv slice data */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    int totalSliceNum = CHUNK_SIZE / SLICE_SIZE, recvSliceIndex = 0, p_slice_loc[CHUNK_SIZE / SLICE_SIZE];
    mulPortNetMetaInitBasic(&mulPortNetMetaRecv, 1, RECV, totalSliceNum, NOENCODE, mulPortRecvSockfd, dataRecv, NULL, &recvSliceIndex, NULL);
    pthreadCreate(&mulPortNetRecvTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaRecv); // Create thread to recv slice
    pthreadJoin(mulPortNetRecvTid, NULL);
    Recv(mulPortNetMetaRecv.mulPortSockfdArr[0][0], p_slice_loc, sizeof(int) * (CHUNK_SIZE / SLICE_SIZE), "recv slice loc");
    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);
    SendExp(exp); // send exp data

    /* writ data chunks, cannot increase exp time */
    if (netMeta->expCount == EXP_TIMES - 1 && FLAG_WRITE_REPAIR == 1)
    {
        char *tmp_buffer = (char *)Calloc(CHUNK_SIZE, sizeof(char));
        for (int i = 0; i < CHUNK_SIZE / SLICE_SIZE; i++)
            memcpy(tmp_buffer + p_slice_loc[i] * SLICE_SIZE, bufRecv + i * SLICE_SIZE, sizeof(char) * SLICE_SIZE);
        if (openWriteFile(netMeta->dstFileName, "wb", tmp_buffer, CHUNK_SIZE) == EC_ERROR)
            ERROR_RETURN_NULL("openWriteMulFile");
        free(tmp_buffer);
    }

    delete2ArrCHAR(bufRecv, dataRecv);
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_K);
    printf("[handleFailNodeDegradedReadN end]\n");
    return NULL;
}

void *handleHealNodeFullRecoverT(netMeta_t *netMeta, int clientFd)
{
    printf("[handleHealNodeFullRecoverT begin]\n");
    char *netDataBuf = (char *)Malloc(sizeof(char) * netMeta->size);
    Recv(clientFd, netDataBuf, netMeta->size, "recv data");
    SendResponse(clientFd);

    /* parse data */
    unsigned char *g_tblsBuf, **g_tblsArr, ***g_tbls2Arr; // must free
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K] = {0}, eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], stripeNum;
    char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN];
    int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M] = {0}, eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    if (netMeta->mulFailFlag == 0)
    {
        int curOffset = unpackedDataFullRecoverTE1(netDataBuf, increasingNodesIndexArr, eachNodeInfo, &stripeNum, fullRecoverFailBlockName);
        create3PointerArrUC(&g_tblsBuf, &g_tblsArr, &g_tbls2Arr, 1, stripeNum, EC_K * 32);
        memcpyMove(g_tblsArr[0], netDataBuf + curOffset, stripeNum * EC_K * 1 * 32 * sizeof(char), &curOffset);
    }
    else if (netMeta->mulFailFlag == 1) // mul failure
    {
        int curOffset = unpackedDataMulFullRecoverTE1(netDataBuf, increasingNodesIndexArr, eachNodeInfo, &stripeNum);
        create3PointerArrUC(&g_tblsBuf, &g_tblsArr, &g_tbls2Arr, stripeNum, MUL_FAIL_NUM, EC_K * 32);
        curOffset = unpackedDataMulFullRecoverTE2(netDataBuf + curOffset, stripeNum, eachStripeFailNodeNum, eachStripeFailNodeIndex, g_tbls2Arr[0][0]);
    }
    /* init network for request nodes */
    int startFailNodeIndex = netMeta->mulFailFlag == 0 ? netMeta->dstNodeIndex : MUL_FAIL_START;
    int failNodeNum = netMeta->mulFailFlag == 0 ? 1 : MUL_FAIL_NUM;
    if (netMeta->expCount == 0)
        for (int i = 0; i < MUL_PORT_NUM; i++)
            for (int j = 0; j < failNodeNum; j++)
                if ((clientInitNetwork(&mulPortSendSockfd[i][j], EC_STORAGENODES_PORT + i, startFailNodeIndex + j)) == EC_ERROR) // only the func close sockfd
                    ERROR_RETURN_NULL("Fail init network");

    /* get filename */
    int curNodeIndex = nodeIP - STORAGENODES_START_IP_ADDR;
    char dstFileNameArr[FULL_RECOVERY_STRIPE_MAX_NUM][MAX_PATH_LEN] = {0};

    for (int i = 0; i < stripeNum && eachNodeInfo[0][curNodeIndex][i] != -1; i++)
        if (!HDFS_FLAG)
            sprintf(dstFileNameArr[i], "%s%s%d_%d", netMeta->dstFileName, "fr", eachNodeInfo[0][curNodeIndex][i], eachNodeInfo[1][curNodeIndex][i] + 1);
        else
            strcpy(dstFileNameArr[i], fullRecoverFailBlockName[curNodeIndex][eachNodeInfo[0][curNodeIndex][i]]);

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    char *sendChunkBuf = NULL, **dataSend = NULL;
    create2PointerArrCHAR(&sendChunkBuf, &dataSend, 1, CHUNK_SIZE);
    if (netMeta->mulFailFlag == 0)
        for (int i = 0; i < stripeNum && eachNodeInfo[0][curNodeIndex][i] != -1; i++)
        {
            /* read chunk from disk */
            clock_gettime(CLOCK_MONOTONIC, &ioTimeStart);
            if (openReadFile(dstFileNameArr[i], "rb", sendChunkBuf, CHUNK_SIZE) == EC_ERROR)
                ERROR_RETURN_NULL("openReadFile");
            ADD_TIME(ioTimeStart, ioTimeEnd, exp->ioTime);

            /* SEND: send chunks to new nodes */
            clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
            if ((mulPortNetTransfer(dataSend, mulPortSendSockfd, CHUNK_SIZE, 1, SEND)) == EC_ERROR)
                ERROR_RETURN_NULL("Fail send 1 chunks");
            ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
        }

    if (netMeta->mulFailFlag == 1)
    {
        int *tmpSockfdBuf = NULL, **mulPortMulNodeSendTmpSockfd = NULL, ***tmpSockfd = NULL;
        create3PointerArrINT(&tmpSockfdBuf, &mulPortMulNodeSendTmpSockfd, &tmpSockfd, MUL_FAIL_NUM, MUL_PORT_NUM, 1);
        for (int i = 0; i < MUL_PORT_NUM; i++)
            for (int j = 0; j < MUL_FAIL_NUM; j++)
                tmpSockfd[j][i][0] = mulPortSendSockfd[i][j];

        int curRecordStripe[FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, curStripeNum = 0;
        for (int k = 0; k < stripeNum; k++)
            for (int i = 0; i < EC_K; i++)
                if (increasingNodesIndexArr[k][i] == curNodeIndex)
                {
                    curRecordStripe[curStripeNum] = k;
                    curStripeNum++;
                }
        for (int i = 0; i < curStripeNum; i++)
        {
            /* read chunk from disk */
            clock_gettime(CLOCK_MONOTONIC, &ioTimeStart);
            if (openReadFile(dstFileNameArr[i], "rb", sendChunkBuf, CHUNK_SIZE) == EC_ERROR)
                ERROR_RETURN_NULL("openReadFile");
            ADD_TIME(ioTimeStart, ioTimeEnd, exp->ioTime);

            /* SEND: send chunks to new nodes */
            clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
            for (int j = 0; j < eachStripeFailNodeNum[curRecordStripe[i]]; j++)
                if ((mulPortNetTransfer(dataSend, tmpSockfd[eachStripeFailNodeIndex[curRecordStripe[i]][j] - MUL_FAIL_START], CHUNK_SIZE, 1, SEND)) == EC_ERROR)
                    ERROR_RETURN_NULL("Fail send 1 chunks");
            ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
        }
        delete3ArrINT(tmpSockfdBuf, mulPortMulNodeSendTmpSockfd, tmpSockfd);
    }
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);

    SendExp(exp); // send exp dataSend
    delete3ArrUC(g_tblsBuf, g_tblsArr, g_tbls2Arr);
    free(netDataBuf);
    delete2ArrCHAR(sendChunkBuf, dataSend);
    /* END: send chunks to new nodes */
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortSendSockfd, MUL_PORT_NUM, EC_M);
    printf("[handleHealNodeFullRecoverT end]\n");
    return NULL;
}

void *handleFailNodeFullRecoverT(netMeta_t *netMeta, int clientFd)
{
    printf("[handleFailNodeFullRecoverT begin]\n");
    char *netDataBuf = (char *)Malloc(sizeof(char) * netMeta->size);
    Recv(clientFd, netDataBuf, netMeta->size, "recv data");
    SendResponse(clientFd);

    /* parse data same heal */
    unsigned char *g_tblsBuf, **g_tblsArr, ***g_tbls2Arr; // must free
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K] = {0}, eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], stripeNum;
    char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN];
    int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M] = {0}, eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    if (netMeta->mulFailFlag == 0)
    {
        int curOffset = unpackedDataFullRecoverTE1(netDataBuf, increasingNodesIndexArr, eachNodeInfo, &stripeNum, fullRecoverFailBlockName);
        create3PointerArrUC(&g_tblsBuf, &g_tblsArr, &g_tbls2Arr, 1, stripeNum, EC_K * 32);
        memcpyMove(g_tblsArr[0], netDataBuf + curOffset, stripeNum * EC_K * 1 * 32 * sizeof(char), &curOffset);
    }
    else if (netMeta->mulFailFlag == 1) // mul failure
    {
        int curOffset = unpackedDataMulFullRecoverTE1(netDataBuf, increasingNodesIndexArr, eachNodeInfo, &stripeNum);
        create3PointerArrUC(&g_tblsBuf, &g_tblsArr, &g_tbls2Arr, stripeNum, MUL_FAIL_NUM, EC_K * 32);
        curOffset = unpackedDataMulFullRecoverTE2(netDataBuf + curOffset, stripeNum, eachStripeFailNodeNum, eachStripeFailNodeIndex, g_tbls2Arr[0][0]);
    }

    /* init network for ec_a-1 nodes */
    if (netMeta->expCount == 0)
    {
        int tmpSockfdArr[EC_A] = {0};
        for (int i = 0; i < MUL_PORT_NUM; i++)
        {
            int heal_node;
            if (netMeta->mulFailFlag == 0)
                heal_node = EC_A - 1;
            if (netMeta->mulFailFlag == 1)
                heal_node = EC_A - MUL_FAIL_NUM;
            for (int j = 0; j < heal_node; j++)
            {
                int tmpSockfd = -1, tmpNodeindex = -1;
                tmpSockfd = AcceptAndGetNodeIndex(serverFdArr[i], &tmpNodeindex);
                tmpSockfdArr[tmpNodeindex] = tmpSockfd;
            }
            for (int j = 0; j < EC_A; j++)
                mulPortRecvSockfd[i][j] = tmpSockfdArr[j];
        }
    }

    /* get filename */
    int curNodeIndex = nodeIP - STORAGENODES_START_IP_ADDR;
    char dstFileNameArr[FULL_RECOVERY_STRIPE_MAX_NUM][MAX_PATH_LEN] = {0};
    for (int i = 0; i < stripeNum && eachNodeInfo[0][curNodeIndex][i] != -1; i++)
        if (!HDFS_FLAG)
            sprintf(dstFileNameArr[i], "%s%s%d_%d", netMeta->dstFileName, "fr", eachNodeInfo[0][curNodeIndex][i], eachNodeInfo[1][curNodeIndex][i] + 1);
        else
            strcpy(dstFileNameArr[i], fullRecoverFailBlockName[curNodeIndex][eachNodeInfo[0][curNodeIndex][i]]);

    /* modify mulPortRecvSockfd */
    int *sockFdBuf = NULL, **mulPortRecvSockfdArrBuf = NULL, ***mulPortRecvSockfdArr = NULL;
    create3PointerArrINT(&sockFdBuf, &mulPortRecvSockfdArrBuf, &mulPortRecvSockfdArr, stripeNum, MUL_PORT_NUM, EC_K);
    if (netMeta->mulFailFlag == 0)
    {
        for (int k = 0; k < stripeNum; k++)
            for (int j = 0; j < MUL_PORT_NUM; j++)
                for (int i = 0; i < EC_K; i++)
                    mulPortRecvSockfdArr[k][j][i] = mulPortRecvSockfd[j][increasingNodesIndexArr[k][i]];
    }
    int curRecordStripe[FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, curStripeNum = 0, curFailOffset[FULL_RECOVERY_STRIPE_MAX_NUM];
    if (netMeta->mulFailFlag == 1)
    {
        for (int k = 0; k < stripeNum; k++)
            for (int i = 0; i < eachStripeFailNodeNum[k]; i++)
                if (eachStripeFailNodeIndex[k][i] == curNodeIndex)
                {
                    curRecordStripe[curStripeNum] = k;
                    curFailOffset[curStripeNum] = i;
                    curStripeNum++;
                }
        for (int k = 0; k < curStripeNum; k++)
            for (int j = 0; j < MUL_PORT_NUM; j++)
                for (int i = 0; i < EC_K; i++)
                    mulPortRecvSockfdArr[k][j][i] = mulPortRecvSockfd[j][increasingNodesIndexArr[curRecordStripe[k]][i]];
    }

    /* test time */
    initExp(exp, clientFd, nodeIP);

    /* EC arguments */
    unsigned char *sendChunkBuf = NULL, **sendChunk = NULL, *codingChunkBuf = NULL, **codingChunk = NULL;
    create2PointerArrUC(&sendChunkBuf, &sendChunk, EC_K, CHUNK_SIZE);
    create2PointerArrUC(&codingChunkBuf, &codingChunk, 1, CHUNK_SIZE);

    int actualStripeNum = netMeta->mulFailFlag == 0 ? stripeNum : curStripeNum;
    for (int i = 0; i < actualStripeNum; i++)
    {
        clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

        /* RECV: recv chunk from k nodes on mul port */
        clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
        if ((mulPortNetTransfer(sendChunk, mulPortRecvSockfdArr[i], CHUNK_SIZE, EC_K, RECV)) == EC_ERROR)
            ERROR_RETURN_NULL("Fail recv k chunks");
        ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);

        /* cal data chunks */
        clock_gettime(CLOCK_MONOTONIC, &calTimeStart);
        if (netMeta->mulFailFlag == 0)
            mulThreadEncode(CHUNK_SIZE, EC_K, 1, g_tblsArr[i], (unsigned char **)sendChunk, (unsigned char **)codingChunk);
        else
            mulThreadEncode(CHUNK_SIZE, EC_K, 1, g_tbls2Arr[curRecordStripe[i]][curFailOffset[i]], (unsigned char **)sendChunk, (unsigned char **)codingChunk);
        ADD_TIME(calTimeStart, calTimeEnd, exp->calTime);
        ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);

        /* writ data chunks, cannot increase exp time */
        if (netMeta->expCount == EXP_TIMES - 1 && FLAG_WRITE_REPAIR == 1)
            if (openWriteFile(dstFileNameArr[i], "wb", codingChunk[0], CHUNK_SIZE) == EC_ERROR)
                ERROR_RETURN_NULL("openWriteMulFile");
    }
    SendExp(exp); // send exp data

    delete3ArrINT(sockFdBuf, mulPortRecvSockfdArrBuf, mulPortRecvSockfdArr);
    delete2ArrUC(sendChunkBuf, sendChunk);
    delete2ArrUC(codingChunkBuf, codingChunk);
    delete3ArrUC(g_tblsBuf, g_tblsArr, g_tbls2Arr);
    free(netDataBuf);
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_A);
    printf("[handleFailNodeFullRecoverT end]\n");
    return NULL;
}

void *handleHealNodeFullRecoverR(netMeta_t *netMeta, int clientFd)
{
    printf("[handleHealNodeFullRecoverR begin]\n");
    char *netDataBuf = (char *)Malloc(sizeof(char) * netMeta->size);
    Recv(clientFd, netDataBuf, netMeta->size, "recv data");
    SendResponse(clientFd);

    if (netMeta->expCount == 0)
    {
        pthread_t tid[2];
        /* INIT: recv chunk from nodes on mul port */
        pthreadCreate(&tid[0], NULL, acceptECAStoragenodes, netMeta);
        /* INIT: send chunks to nodes */
        pthreadCreate(&tid[1], NULL, connectECAStoragenodes, NULL);
        pthreadJoin(tid[0], NULL);
        pthreadJoin(tid[1], NULL);
    }

    /* parse data */
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K], eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], stripeNum;
    int dstNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], srcNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM];
    unsigned char *g_tblsBuf = NULL, **g_tblsArr = NULL, ***g_tbls2Arr = NULL, ****g_tbls3Arr = NULL; // must free
    char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN];
    int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M] = {0}, eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    if (netMeta->mulFailFlag == 0)
    {
        int curOffset = unpackedDataFullRecoverR1(netDataBuf, increasingNodesIndexArr, eachNodeInfo, &stripeNum, dstNodeArr, srcNodeArr, fullRecoverFailBlockName);
        create4PointerArrUC(&g_tblsBuf, &g_tblsArr, &g_tbls2Arr, &g_tbls3Arr, 1, stripeNum, EC_K, 2 * 32);
        memcpyMove(g_tbls2Arr[0][0], netDataBuf + curOffset, stripeNum * EC_K * 2 * 1 * 32 * sizeof(char), &curOffset);
    }
    else if (netMeta->mulFailFlag == 1) // mul failur
    {
        int curOffset = unpackedDataMulFullRecoverR1(netDataBuf, increasingNodesIndexArr, eachNodeInfo, &stripeNum, dstNodeArr, srcNodeArr);
        create4PointerArrUC(&g_tblsBuf, &g_tblsArr, &g_tbls2Arr, &g_tbls3Arr, stripeNum, MUL_FAIL_NUM, EC_K, 2 * 32);
        curOffset = unpackedDataMulFullRecoverR2(netDataBuf + curOffset, stripeNum, eachStripeFailNodeNum, eachStripeFailNodeIndex, g_tbls3Arr[0][0][0]);
    }

    /* get filename */
    int curNodeIndex = nodeIP - STORAGENODES_START_IP_ADDR;
    char dstFileNameArr[FULL_RECOVERY_STRIPE_MAX_NUM][MAX_PATH_LEN];
    for (int i = 0; i < stripeNum && eachNodeInfo[0][curNodeIndex][i] != -1; i++)
        if (!HDFS_FLAG)
            sprintf(dstFileNameArr[i], "%s%s%d_%d", netMeta->dstFileName, "fr", eachNodeInfo[0][curNodeIndex][i], eachNodeInfo[1][curNodeIndex][i] + 1);
        else
            strcpy(dstFileNameArr[i], fullRecoverFailBlockName[curNodeIndex][eachNodeInfo[0][curNodeIndex][i]]);

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* curStripeO is Number of stripes owned by the current node instead of stripeNum */
    /* stripeNumLoc is stripeNum */
    int stripeNumLoc[FULL_RECOVERY_STRIPE_MAX_NUM], stripeNumLoc_count = 0;
    for (int i = 0; i < stripeNum; i++)
        if (srcNodeArr[curNodeIndex][i] != -1 || dstNodeArr[curNodeIndex][i] != -1)
            stripeNumLoc[stripeNumLoc_count++] = i;

    /* begin */
    char *sendChunkBuf = NULL, **dataSend = NULL, *bufRecv = NULL, **dataRecv = NULL;
    create2PointerArrCHAR(&sendChunkBuf, &dataSend, 1, CHUNK_SIZE);
    create2PointerArrCHAR(&bufRecv, &dataRecv, 1, CHUNK_SIZE);

    int **mulPortMulNodeRecvTmpSockfd = NULL, *mulPortMulNodeRecvTmpSockfdBuf = NULL;
    create2PointerArrINT(&mulPortMulNodeRecvTmpSockfdBuf, &mulPortMulNodeRecvTmpSockfd, MUL_PORT_NUM, 1);
    int **mulPortMulNodeSendTmpSockfd = NULL, *mulPortMulNodeSendTmpSockfdBuf = NULL;
    create2PointerArrINT(&mulPortMulNodeSendTmpSockfdBuf, &mulPortMulNodeSendTmpSockfd, MUL_PORT_NUM, 1);
    int totalSliceNum = CHUNK_SIZE / SLICE_SIZE, recvSliceIndex = 0;

    for (int curStripeO = 0; curStripeO < stripeNumLoc_count && eachNodeInfo[0][curNodeIndex][curStripeO] != -1; curStripeO++)
    {

        int curStripe = stripeNumLoc[curStripeO];
        int failNodeNum = netMeta->mulFailFlag == 0 ? 1 : eachStripeFailNodeNum[curStripe];
        for (int n = 0; n < failNodeNum; n++)
        {
            /* read chunk from disk */
            clock_gettime(CLOCK_MONOTONIC, &ioTimeStart);
            if (openReadFile(dstFileNameArr[curStripeO], "rb", sendChunkBuf, CHUNK_SIZE) == EC_ERROR)
                ERROR_RETURN_VALUE("openReadFile");
            ADD_TIME(ioTimeStart, ioTimeEnd, exp->ioTime);

            memset(bufRecv, 0, CHUNK_SIZE * sizeof(char));

            /* send and recv slice data */
            clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
            recvSliceIndex = 0;
            if (srcNodeArr[curNodeIndex][curStripe] != -1)
            {
                for (int i = 0; i < MUL_PORT_NUM; i++)
                    mulPortMulNodeRecvTmpSockfd[i][0] = mulPortRecvSockfd[i][srcNodeArr[curNodeIndex][curStripe]];
                mulPortNetMetaInitBasic(&mulPortNetMetaRecv, 1, RECV, totalSliceNum, NOENCODE, mulPortMulNodeRecvTmpSockfd, dataRecv, NULL, &recvSliceIndex, NULL);
                pthreadCreate(&mulPortNetRecvTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaRecv); // Create thread to recv slice
            }

            if (dstNodeArr[curNodeIndex][curStripe] != -1)
            {
                if (netMeta->mulFailFlag == 0 || dstNodeArr[curNodeIndex][curStripe] != MUL_FAIL_START)
                    for (int i = 0; i < MUL_PORT_NUM; i++)
                        mulPortMulNodeSendTmpSockfd[i][0] = mulPortSendSockfd[i][dstNodeArr[curNodeIndex][curStripe]];
                else
                    for (int i = 0; i < MUL_PORT_NUM; i++)
                        mulPortMulNodeSendTmpSockfd[i][0] = mulPortSendSockfd[i][eachStripeFailNodeIndex[curStripe][n]];
                char *g_tblsTmp = netMeta->mulFailFlag == 0 ? g_tbls2Arr[curStripe][getArrIndex(increasingNodesIndexArr[curStripe], EC_K, curNodeIndex)] : g_tbls3Arr[curStripe][n][getArrIndex(increasingNodesIndexArr[curStripe], EC_K, curNodeIndex)];
                if (srcNodeArr[curNodeIndex][curStripe] == -1)
                    recvSliceIndex = totalSliceNum;
                mulPortNetMetaInitBasic(&mulPortNetMetaSend, 1, SEND, totalSliceNum, ENCODE, mulPortMulNodeSendTmpSockfd, dataRecv, dataSend, &recvSliceIndex, g_tblsTmp);
                pthreadCreate(&mulPortNetSendTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaSend); // Create thread to send slice
            }
            if (srcNodeArr[curNodeIndex][curStripe] != -1)
                pthreadJoin(mulPortNetRecvTid, NULL);
            if (dstNodeArr[curNodeIndex][curStripe] != -1)
                pthreadJoin(mulPortNetSendTid, NULL);
            ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
        }
    }
    delete2ArrINT(mulPortMulNodeRecvTmpSockfdBuf, mulPortMulNodeRecvTmpSockfd);
    delete2ArrINT(mulPortMulNodeSendTmpSockfdBuf, mulPortMulNodeSendTmpSockfd);
    delete2ArrCHAR(sendChunkBuf, dataSend);
    delete2ArrCHAR(bufRecv, dataRecv);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);

    SendExp(exp); // send exp data
    delete4ArrUC(g_tblsBuf, g_tblsArr, g_tbls2Arr, g_tbls3Arr);
    free(netDataBuf);
    if (netMeta->expCount == EXP_TIMES - 1)
    {
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_A);
        shutdown2Arr(mulPortSendSockfd, MUL_PORT_NUM, EC_A);
    }
    printf("[handleHealNodeFullRecoverR end]\n");
    return NULL;
}

void *handleFailNodeFullRecoverR(netMeta_t *netMeta, int clientFd)
{
    printf("[handleFailNodeFullRecoverR begin]\n");
    char *netDataBuf = (char *)Malloc(sizeof(char) * netMeta->size);
    Recv(clientFd, netDataBuf, netMeta->size, "recv data");
    SendResponse(clientFd);

    if (netMeta->expCount == 0)
        acceptECAStoragenodes(netMeta);

    /* parse data */
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K], eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], stripeNum;
    int dstNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], srcNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM];
    char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN];
    int curOffset = unpackedDataFullRecoverR1(netDataBuf, increasingNodesIndexArr, eachNodeInfo, &stripeNum, dstNodeArr, srcNodeArr, fullRecoverFailBlockName);

    /* get filename */
    int curNodeIndex = nodeIP - STORAGENODES_START_IP_ADDR;
    char dstFileNameArr[FULL_RECOVERY_STRIPE_MAX_NUM][MAX_PATH_LEN];
    for (int i = 0; i < stripeNum && eachNodeInfo[0][curNodeIndex][i] != -1; i++)
        if (!HDFS_FLAG)
            sprintf(dstFileNameArr[i], "%s%s%d_%d", netMeta->dstFileName, "fr", eachNodeInfo[0][curNodeIndex][i], eachNodeInfo[1][curNodeIndex][i] + 1);
        else
            strcpy(dstFileNameArr[i], fullRecoverFailBlockName[curNodeIndex][eachNodeInfo[0][curNodeIndex][i]]);

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* curStripeO is Number of stripes owned by the current node instead of stripeNum */
    /* stripeNumLoc is stripeNum */
    int stripeNumLoc[FULL_RECOVERY_STRIPE_MAX_NUM], stripeNumLoc_count = 0;
    for (int i = 0; i < stripeNum; i++)
        if (srcNodeArr[curNodeIndex][i] != -1 || dstNodeArr[curNodeIndex][i] != -1)
            stripeNumLoc[stripeNumLoc_count++] = i;

    /* begin */
    char *bufRecv = NULL, **dataRecv = NULL;
    create2PointerArrCHAR(&bufRecv, &dataRecv, 1, CHUNK_SIZE);

    int **mulPortMulNodeRecvTmpSockfd = NULL, *mulPortMulNodeRecvTmpSockfdBuf = NULL;
    create2PointerArrINT(&mulPortMulNodeRecvTmpSockfdBuf, &mulPortMulNodeRecvTmpSockfd, MUL_PORT_NUM, 1);

    for (int curStripeO = 0; curStripeO < stripeNumLoc_count && eachNodeInfo[0][curNodeIndex][curStripeO] != -1; curStripeO++)
    {
        int curStripe = stripeNumLoc[curStripeO];
        /* recv slice data */
        clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
        int totalSliceNum = CHUNK_SIZE / SLICE_SIZE, recvSliceIndex = 0;
        if (srcNodeArr[curNodeIndex][curStripe] != -1)
        {
            for (int i = 0; i < MUL_PORT_NUM; i++)
                mulPortMulNodeRecvTmpSockfd[i][0] = mulPortRecvSockfd[i][srcNodeArr[curNodeIndex][curStripe]];
            mulPortNetMetaInitBasic(&mulPortNetMetaRecv, 1, RECV, totalSliceNum, NOENCODE, mulPortMulNodeRecvTmpSockfd, dataRecv, NULL, &recvSliceIndex, NULL);
            pthreadCreate(&mulPortNetRecvTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaRecv); // Create thread to recv slice
            pthreadJoin(mulPortNetRecvTid, NULL);
        }
        ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);

        /* writ data chunks, increase exp time */
        clock_gettime(CLOCK_MONOTONIC, &ioTimeStart);
        if (netMeta->expCount == EXP_TIMES - 1 && FLAG_WRITE_REPAIR == 1)
            if (openWriteFile(dstFileNameArr[curStripeO], "wb", bufRecv, CHUNK_SIZE) == EC_ERROR)
                ERROR_RETURN_NULL("openWriteMulFile");
        ADD_TIME(ioTimeStart, ioTimeEnd, exp->ioTime);
    }
    delete2ArrINT(mulPortMulNodeRecvTmpSockfdBuf, mulPortMulNodeRecvTmpSockfd);
    delete2ArrCHAR(bufRecv, dataRecv);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);

    SendExp(exp); // send exp data
    free(netDataBuf);
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_A);
    printf("[handleFailNodeFullRecoverR end]\n");
    return NULL;
}

void *handleHealNodeFullRecoverE(netMeta_t *netMeta, int clientFd)
{
    printf("[handleHealNodeFullRecoverE begin]\n");
    char *netDataBuf = (char *)Malloc(sizeof(char) * netMeta->size);
    Recv(clientFd, netDataBuf, netMeta->size, "recv data");
    SendResponse(clientFd);

    /* init network for request nodes */
    if (netMeta->expCount == 0)
        for (int i = 0; i < MUL_PORT_NUM; i++)
            if ((clientInitNetwork(&mulPortSendSockfd[i][0], EC_PND_STORAGENODES_PORT + i, PNETDEVICE_IP_ADDR - STORAGENODES_START_IP_ADDR)) == EC_ERROR) // only the func close sockfd
                ERROR_RETURN_NULL("Fail init network");

    /* parse data */
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K] = {0}, eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], stripeNum;
    char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN];
    int curOffset = unpackedDataFullRecoverTE1(netDataBuf, increasingNodesIndexArr, eachNodeInfo, &stripeNum, fullRecoverFailBlockName);

    /* get filename */
    int curNodeIndex = nodeIP - STORAGENODES_START_IP_ADDR;
    char dstFileNameArr[FULL_RECOVERY_STRIPE_MAX_NUM][MAX_PATH_LEN] = {0};
    for (int i = 0; i < stripeNum && eachNodeInfo[0][curNodeIndex][i] != -1; i++)
        if (!HDFS_FLAG)
            sprintf(dstFileNameArr[i], "%s%s%d_%d", netMeta->dstFileName, "fr", eachNodeInfo[0][curNodeIndex][i], eachNodeInfo[1][curNodeIndex][i] + 1);
        else
            strcpy(dstFileNameArr[i], fullRecoverFailBlockName[curNodeIndex][eachNodeInfo[0][curNodeIndex][i]]);

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    char *sendChunkBuf = NULL, **dataSend = NULL;
    create2PointerArrCHAR(&sendChunkBuf, &dataSend, 1, CHUNK_SIZE);
    for (int i = 0; i < stripeNum && eachNodeInfo[0][curNodeIndex][i] != -1; i++)
    {
        /* read chunk from disk */
        clock_gettime(CLOCK_MONOTONIC, &ioTimeStart);
        if (openReadFile(dstFileNameArr[i], "rb", sendChunkBuf, CHUNK_SIZE) == EC_ERROR)
            ERROR_RETURN_NULL("openReadFile");
        ADD_TIME(ioTimeStart, ioTimeEnd, exp->ioTime);

        /* send and recv slice data */
        clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
        int totalSliceNum = CHUNK_SIZE / SLICE_SIZE, recvSliceIndex = totalSliceNum;
        mulPortNetMetaInitBasic(&mulPortNetMetaSend, 1, SEND, totalSliceNum, NOENCODE, mulPortSendSockfd, NULL, dataSend, &recvSliceIndex, NULL);
        pthreadCreate(&mulPortNetSendTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaSend); // Create thread to send slice
        pthreadJoin(mulPortNetSendTid, NULL);
        ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    }
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);
    SendExp(exp); // send exp data

    delete2ArrCHAR(sendChunkBuf, dataSend);
    free(netDataBuf);
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortSendSockfd, MUL_PORT_NUM, 1);
    printf("[handleHealNodeFullRecoverE end]\n");
    return NULL;
}

void *handleFailNodeFullRecoverE(netMeta_t *netMeta, int clientFd)
{
    printf("[handleFailNodeFullRecoverE begin]\n");
    char *netDataBuf = (char *)Malloc(sizeof(char) * netMeta->size);
    Recv(clientFd, netDataBuf, netMeta->size, "recv data");
    SendResponse(clientFd);

    /* init network for PND */
    if (netMeta->expCount == 0)
        for (int i = 0; i < MUL_PORT_NUM; i++)
            mulPortRecvSockfd[i][0] = Accept(serverFdArr[i]);

    /* parse data */
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K] = {0}, eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], stripeNum;
    char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN]; // HDFS
    int curOffset = unpackedDataFullRecoverTE1(netDataBuf, increasingNodesIndexArr, eachNodeInfo, &stripeNum, fullRecoverFailBlockName);

    /* get filename */
    int curNodeIndex = nodeIP - STORAGENODES_START_IP_ADDR;
    char dstFileNameArr[FULL_RECOVERY_STRIPE_MAX_NUM][MAX_PATH_LEN] = {0};
    for (int i = 0; i < stripeNum && eachNodeInfo[0][curNodeIndex][i] != -1; i++)
        if (!HDFS_FLAG)
            sprintf(dstFileNameArr[i], "%s%s%d_%d", netMeta->dstFileName, "fr", eachNodeInfo[0][curNodeIndex][i], eachNodeInfo[1][curNodeIndex][i] + 1);
        else
            strcpy(dstFileNameArr[i], fullRecoverFailBlockName[curNodeIndex][eachNodeInfo[0][curNodeIndex][i]]);

    /* test time */
    initExp(exp, clientFd, nodeIP);

    /* recv buffer */
    char *bufRecv = NULL, **dataRecv = NULL;
    create2PointerArrCHAR(&bufRecv, &dataRecv, 1, CHUNK_SIZE);
    for (int i = 0; i < stripeNum && eachNodeInfo[0][curNodeIndex][i] != -1; i++)
    {
        clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);
        /* recv slice data */
        clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
        int totalSliceNum = CHUNK_SIZE / SLICE_SIZE, recvSliceIndex = 0, p_slice_loc[CHUNK_SIZE / SLICE_SIZE];
        mulPortNetMetaInitBasic(&mulPortNetMetaRecv, 1, RECV, totalSliceNum, NOENCODE, mulPortRecvSockfd, dataRecv, NULL, &recvSliceIndex, NULL);
        pthreadCreate(&mulPortNetRecvTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaRecv); // Create thread to recv slice
        pthreadJoin(mulPortNetRecvTid, NULL);
        Recv(mulPortNetMetaRecv.mulPortSockfdArr[0][0], p_slice_loc, sizeof(int) * (CHUNK_SIZE / SLICE_SIZE), "recv slice loc");
        ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
        ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);

        /* writ data chunks, cannot increase exp time */
        if (netMeta->expCount == EXP_TIMES - 1 && FLAG_WRITE_REPAIR == 1)
        {
            char *tmp_buffer = (char *)Calloc(CHUNK_SIZE, sizeof(char));
            for (int i = 0; i < CHUNK_SIZE / SLICE_SIZE; i++)
                memcpy(tmp_buffer + p_slice_loc[i] * SLICE_SIZE, bufRecv + i * SLICE_SIZE, sizeof(char) * SLICE_SIZE);
            if (openWriteFile(dstFileNameArr[i], "wb", tmp_buffer, CHUNK_SIZE) == EC_ERROR)
                ERROR_RETURN_NULL("openWriteMulFile");
            free(tmp_buffer);
        }
    }
    SendExp(exp); // send exp data
    delete2ArrCHAR(bufRecv, dataRecv);
    free(netDataBuf);
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_K);
    printf("[handleFailNodeFullRecoverE end]\n");
    return NULL;
}

void *handleHealNodeFullRecoverN(netMeta_t *netMeta, int clientFd)
{
    printf("[handleHealNodeFullRecoverN begin]\n");
    char *netDataBuf = (char *)Malloc(sizeof(char) * netMeta->size);
    Recv(clientFd, netDataBuf, netMeta->size, "recv data");
    SendResponse(clientFd);

    /* init network for request nodes */
    if (netMeta->expCount == 0)
        for (int i = 0; i < MUL_PORT_NUM; i++)
            if ((clientInitNetwork(&mulPortSendSockfd[i][0], EC_PND_STORAGENODES_PORT + i, PNETDEVICE_IP_ADDR - STORAGENODES_START_IP_ADDR)) == EC_ERROR) // only the func close sockfd
                ERROR_RETURN_NULL("Fail init network");

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
    /* get filename and send_stripe_num */
    int curNodeIndex = nodeIP - STORAGENODES_START_IP_ADDR, send_stripe_num = 0, increasingNodeIndex;
    char dstFileNameArr[FULL_RECOVERY_STRIPE_MAX_NUM][MAX_PATH_LEN] = {0};

    int mergeNR[stripeNum];
    if (netMeta->mulFailFlag == 1)
        if (MUL_FAIL_NUM == 1)
            for (int i = 0; i < stripeNum; i++)
                mergeNR[i] = eachNodeInfo[0][MUL_FAIL_START][i];
        else
            for (int n = 1; n < MUL_FAIL_NUM; n++)
                if (n == 1)
                    mergeFail(eachNodeInfo[0][0 + MUL_FAIL_START], eachNodeInfo[0][1 + MUL_FAIL_START], mergeNR, stripeNum);
                else
                    mergeFail(eachNodeInfo[0][n + MUL_FAIL_START], mergeNR, mergeNR, stripeNum);

    int tmp_count = 0;
    int cur_storagenode_fail = netMeta->dstNodeIndex;
    for (int i = 0; i < stripeNum; i++)
    {
        if ((increasingNodeIndex = getArrIndex(increasingNodesIndexArr[i], EC_N - 1, curNodeIndex)) == -1)
            continue; // no exist
        if (twoBucketsSliceIndexInfo[i][increasingNodeIndex][0] != -1 || twoBucketsSliceIndexInfo[i][increasingNodeIndex][3] != -1)
            send_stripe_num++;
        if (!HDFS_FLAG)
            sprintf(dstFileNameArr[tmp_count], "%s%s%d_%d", netMeta->dstFileName, "fr", eachNodeInfo[0][curNodeIndex][tmp_count], eachNodeInfo[1][curNodeIndex][tmp_count] + 1);
        else
            strcpy(dstFileNameArr[tmp_count], fullRecoverFailBlockName[curNodeIndex][eachNodeInfo[0][curNodeIndex][tmp_count]]);
        tmp_count++;
    }

    /* test time */
    initExp(exp, clientFd, nodeIP);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* read SendChunk from disk: Just for coding convenience, used large memory space to avoid high IO latency and achieve pipeline transmission*/
    char **sendChunkBuf = (char **)Malloc(sizeof(char *) * send_stripe_num); // buffer for EC chunk * send_stripe_num
    int *totalSliceNum = (int *)Calloc(send_stripe_num, sizeof(int)), total_slice_num_count = 0;

    tmp_count = 0;
    for (int i = 0; i < stripeNum; i++)
    {
        if ((increasingNodeIndex = getArrIndex(increasingNodesIndexArr[i], EC_N - 1, curNodeIndex)) == -1)
            continue; // no exist
        clock_gettime(CLOCK_MONOTONIC, &ioTimeStart);
        if (twoBucketsSliceIndexInfo[i][increasingNodeIndex][0] != -1 || twoBucketsSliceIndexInfo[i][increasingNodeIndex][3] != -1)
            sendChunkBuf[total_slice_num_count] = (char *)Malloc(sizeof(char) * CHUNK_SIZE);
        if (twoBucketsSliceIndexInfo[i][increasingNodeIndex][0] != -1)
        {
            if (openReadOffsetFile(dstFileNameArr[tmp_count], "rb", twoBucketsSliceIndexInfo[i][increasingNodeIndex][0] * SLICE_SIZE,
                                   sendChunkBuf[total_slice_num_count], twoBucketsSliceIndexInfo[i][increasingNodeIndex][1] * SLICE_SIZE) == EC_ERROR)
                ERROR_RETURN_NULL("openReadOffsetFile 1");
            totalSliceNum[total_slice_num_count] += twoBucketsSliceIndexInfo[i][increasingNodeIndex][1];
        }
        if (twoBucketsSliceIndexInfo[i][increasingNodeIndex][3] != -1)
        {
            int lastIndex = twoBucketsSliceIndexInfo[i][increasingNodeIndex][1] == -1 ? 0 : twoBucketsSliceIndexInfo[i][increasingNodeIndex][1];
            if (openReadOffsetFile(dstFileNameArr[tmp_count], "rb", twoBucketsSliceIndexInfo[i][increasingNodeIndex][3] * SLICE_SIZE,
                                   sendChunkBuf[total_slice_num_count] + lastIndex * SLICE_SIZE, twoBucketsSliceIndexInfo[i][increasingNodeIndex][4] * SLICE_SIZE) == EC_ERROR)
                ERROR_RETURN_NULL("openReadOffsetFile 2");
            totalSliceNum[total_slice_num_count] += twoBucketsSliceIndexInfo[i][increasingNodeIndex][4];
        }
        if (twoBucketsSliceIndexInfo[i][increasingNodeIndex][0] != -1 || twoBucketsSliceIndexInfo[i][increasingNodeIndex][3] != -1)
            total_slice_num_count++;

        tmp_count++;
        ADD_TIME(ioTimeStart, ioTimeEnd, exp->ioTime);
    }
    if (total_slice_num_count != send_stripe_num)
        ERROR_RETURN_NULL("error totalSliceNum");

    /* send remain first slice data for each stripe for achieve pipeline transmission */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    int send_slice_num[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    char **sendChunk = (char **)Malloc(sizeof(char *) * 1);

    for (int j = 0; j < CHUNK_SIZE / SLICE_SIZE; j++)
        for (int i = 0; i < send_stripe_num; i++)
        {
            if (totalSliceNum[i] == send_slice_num[i])
                continue;
            sendChunk[0] = sendChunkBuf[i] + send_slice_num[i] * SLICE_SIZE;
            if ((mulPortNetTransfer(sendChunk, mulPortSendSockfd, SLICE_SIZE, 1, SEND)) == EC_ERROR)
                ERROR_RETURN_NULL("Fail send 1 slice");
            send_slice_num[i]++;
        }
    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);

    SendExp(exp); // send exp data
    for (int i = 0; i < send_stripe_num; i++)
        free(sendChunkBuf[i]);
    delete4ArrUC(g_tblsBuf, g_tblsArr, g_tbls2Arr, g_tbls3Arr);
    free3(sendChunk, sendChunkBuf, totalSliceNum);
    free4(twoBucketsSliceIndexInfo, divisionNum, divisionLines, netDataBuf);
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortSendSockfd, MUL_PORT_NUM, 1);
    printf("[handleHealNodeFullRecoverN end]\n");
    return NULL;
}

void *handleFailNodeFullRecoverN(netMeta_t *netMeta, int clientFd)
{
    printf("[handleFailNodeFullRecoverN begin]\n");
    char *netDataBuf = (char *)Malloc(sizeof(char) * netMeta->size);
    Recv(clientFd, netDataBuf, netMeta->size, "recv data");
    SendResponse(clientFd);

    /* init network for PND */
    if (netMeta->expCount == 0)
        for (int i = 0; i < MUL_PORT_NUM; i++)
            mulPortRecvSockfd[i][0] = Accept(serverFdArr[i]);

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
    
    /* get filename */
    int curNodeIndex = nodeIP - STORAGENODES_START_IP_ADDR;
    char dstFileNameArr[FULL_RECOVERY_STRIPE_MAX_NUM][MAX_PATH_LEN] = {0};
    for (int i = 0; i < stripeNum && eachNodeInfo[0][curNodeIndex][i] != -1; i++)
        if (!HDFS_FLAG)
            sprintf(dstFileNameArr[i], "%s%s%d_%d", netMeta->dstFileName, "fr", eachNodeInfo[0][curNodeIndex][i], eachNodeInfo[1][curNodeIndex][i] + 1);
        else
            strcpy(dstFileNameArr[i], fullRecoverFailBlockName[curNodeIndex][eachNodeInfo[0][curNodeIndex][i]]);

    /* test time */
    initExp(exp, clientFd, nodeIP);

    /* recv RecvChunk from PND: Just for coding convenience, used large memory space to avoid high IO latency and achieve pipeline transmission*/
    char *bufRecv = NULL, **dataRecv = NULL;
    create2PointerArrCHAR(&bufRecv, &dataRecv, 1, (size_t)stripeNum * CHUNK_SIZE);
    clock_gettime(CLOCK_MONOTONIC, &totalTimeStart);

    /* recv slice data */
    clock_gettime(CLOCK_MONOTONIC, &netTimeStart);
    int curStripeNum = 0;
    for (int i = 0; i < stripeNum && eachNodeInfo[0][curNodeIndex][i] != -1; i++)
        curStripeNum++;
    int totalSliceNum = curStripeNum * (CHUNK_SIZE / SLICE_SIZE), recvSliceIndex = 0, p_slice_loc[stripeNum * (CHUNK_SIZE / SLICE_SIZE)]; // it is totally different for d repair,where p_slice_loc[loc]=send_count, d is p_slice_loc[send_count]=loc
    mulPortNetMetaInitBasic(&mulPortNetMetaRecv, 1, RECV, totalSliceNum, NOENCODE, mulPortRecvSockfd, dataRecv, NULL, &recvSliceIndex, NULL);
    pthreadCreate(&mulPortNetRecvTid, NULL, netSlice1Node1Fail, (void *)&mulPortNetMetaRecv); // Create thread to recv slice
    pthreadJoin(mulPortNetRecvTid, NULL);
    Recv(mulPortNetMetaRecv.mulPortSockfdArr[0][0], p_slice_loc, sizeof(int) * stripeNum * (CHUNK_SIZE / SLICE_SIZE), "recv slice loc");
    ADD_TIME(netTimeStart, netTimeEnd, exp->netTime);
    ADD_TIME(totalTimeStart, totalTimeEnd, exp->totalTime);

    /* writ data chunks, cannot increase exp time */
    if (netMeta->expCount == EXP_TIMES - 1 && FLAG_WRITE_REPAIR == 1)
    {
        char *tmp_buffer = (char *)Calloc(CHUNK_SIZE, sizeof(char));
        for (int i = 0; i < stripeNum && eachNodeInfo[0][curNodeIndex][i] != -1; i++)
        {
            int start_stripe_slice = i * (CHUNK_SIZE / SLICE_SIZE);
            for (int j = 0; j < CHUNK_SIZE / SLICE_SIZE; j++)
                memcpy(tmp_buffer + j * SLICE_SIZE, bufRecv + p_slice_loc[start_stripe_slice + j] * SLICE_SIZE, sizeof(char) * SLICE_SIZE);
            if (openWriteFile(dstFileNameArr[i], "wb", tmp_buffer, CHUNK_SIZE) == EC_ERROR)
                ERROR_RETURN_NULL("openWriteMulFile");
        }
        free(tmp_buffer);
    }

    SendExp(exp); // send exp data
    free4(twoBucketsSliceIndexInfo, divisionNum, divisionLines, netDataBuf);
    delete2ArrCHAR(bufRecv, dataRecv);
    delete4ArrUC(g_tblsBuf, g_tblsArr, g_tbls2Arr, g_tbls3Arr);

    /* END: recv chunk from pnd on mul port  */
    if (netMeta->expCount == EXP_TIMES - 1)
        shutdown2Arr(mulPortRecvSockfd, MUL_PORT_NUM, EC_K);
    printf("[handleFailNodeFullRecoverN end]\n");
    return NULL;
}

void *handleClient(void *arg)
{
    printf("[ec_storagenodes_handle_client begin]\n");

    int clientFd = *((int *)arg);                                // only the func close
    netMeta_t *netMeta = (netMeta_t *)Malloc(sizeof(netMeta_t)); // only the func free
    /* EXP START */
    for (int i = 0; i < EXP_TIMES; i++)
    {
        /* recv netMeta and send response */
        Recv(clientFd, netMeta, sizeof(netMeta_t), "recv netMeta");

        type_handle_func handle_func[] = {
            handleWriteChunks, handleReadChunks,
            handleFailNodeDegradedReadT, handleHealNodeDegradedReadT,
            handleFailNodeDegradedReadR, handleHealNodeDegradedReadR,
            handleFailNodeDegradedReadE, handleHealNodeDegradedReadE,
            handleFailNodeDegradedReadN, handleHealNodeDegradedReadN,
            handleFailNodeFullRecoverT, handleHealNodeFullRecoverT,
            handleFailNodeFullRecoverR, handleHealNodeFullRecoverR,
            handleFailNodeFullRecoverE, handleHealNodeFullRecoverE,
            handleFailNodeFullRecoverN, handleHealNodeFullRecoverN};
        handle_func[netMeta->cmdType](netMeta, clientFd);

        if (netMeta->cmdType < 2)
            break; // non - exp function
    }
    /* EXP END */

    free(netMeta);
    shutdown(clientFd, SHUT_RDWR);
    printf("[ec_storagenodes_handle_client end]\n");
    return NULL;
}

int main()
{
    printf("[ec_storagenodes begin]\n");

    getLocalIPLastNum(&nodeIP);
    exp = (exp_t *)Malloc(sizeof(exp_t));
    create2PointerArrINT(&mulPortRecvSockfdBuf, &mulPortRecvSockfd, MUL_PORT_NUM, EC_A);
    create2PointerArrINT(&mulPortSendSockfdBuf, &mulPortSendSockfd, MUL_PORT_NUM, EC_A);

    int serverFd;
    if ((serverInitNetwork(&serverFd, EC_CLIENT_STORAGENODES_PORT)) == EC_ERROR) //  init server network between client and node
        ERROR_RETURN_VALUE("error serverInitNetwork");
    for (int i = 0; i < MUL_PORT_NUM; i++)
        if ((serverInitNetwork(&serverFdArr[i], EC_STORAGENODES_PORT + i)) == EC_ERROR) //  //  init server network between nodes
            ERROR_RETURN_NULL("error serverInitNetwork for mul port");
    pthread_t handleClientTid;
    while (1)
    {
        int clientFd = Accept(serverFd);
        pthreadCreate(&handleClientTid, NULL, handleClient, (void *)&clientFd);
        pthreadJoin(handleClientTid, NULL);
    }

    delete2ArrINT(mulPortRecvSockfdBuf, mulPortRecvSockfd);
    delete2ArrINT(mulPortSendSockfdBuf, mulPortSendSockfd);
    return EC_OK;
}