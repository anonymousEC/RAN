#include "MulPortTransfer.h"

degradedReadNetRecordN_t *degradedReadNetRecordN;
fullRecoverNetRecordN_t *fullRecoverNetRecordN;
static pthread_mutex_t mutex_bitmap[CHUNK_SIZE / SLICE_SIZE];
static pthread_mutex_t mutex_bitmap_f[FULL_RECOVERY_STRIPE_MAX_NUM * (CHUNK_SIZE / SLICE_SIZE)];

typedef struct mulPortNetTransfer_s
{
    char **data;
    int **mulPortSockfdArr;
    int data_size;
    int nodeNum;
    int mode;
} mulPortNetTransfer_t;

void *sglPortNetTransfer(void *arg)
{
    sglPortNet_t *sgl_port_net = (sglPortNet_t *)arg;

    int nodeNum = sgl_port_net->nodeNum;
    pthread_t tid[nodeNum];
    netMeta_t netMetaArr[nodeNum];

    /* Recv data sizes and locations */
    for (int i = 0; i < nodeNum; i++)
    {
        netMetaArr[i].data = sgl_port_net->data[i];
        netMetaArr[i].size = sgl_port_net->size[i];
        netMetaArr[i].sockfd = sgl_port_net->sockfdArr[i];

        if (sgl_port_net->mode == 1)
            if (nodeNum == 1)
                RecvDataSize((void *)&netMetaArr[i]);
            else
                pthreadCreate(&tid[i], NULL, RecvDataSize, (void *)&netMetaArr[i]);
        if (sgl_port_net->mode == 0)
            if (nodeNum == 1)
                SendDataSize((void *)&netMetaArr[i]);
            else
                pthreadCreate(&tid[i], NULL, SendDataSize, (void *)&netMetaArr[i]);
    }

    if (nodeNum != 1)
        for (int i = 0; i < nodeNum; i++)
            pthreadJoin(tid[i], NULL);
    return NULL;
}

int mulPortNetTransfer(char **data, int **mulPortSockfdArr, int data_size, int nodeNum, int mode)
{
    sglPortNet_t sgl_port_net[MUL_PORT_NUM];
    pthread_t tid[MUL_PORT_NUM];

    /* mul port init */
    for (int i = 0; i < MUL_PORT_NUM; i++)
    {
        sgl_port_net[i].nodeNum = nodeNum;
        sgl_port_net[i].sockfdArr = mulPortSockfdArr[i];
        sgl_port_net[i].size = (int *)Malloc(sizeof(int) * sgl_port_net[i].nodeNum);
        sgl_port_net[i].data = (char **)Malloc(sizeof(char *) * sgl_port_net[i].nodeNum);
        sgl_port_net[i].mode = mode;

        for (int j = 0; j < sgl_port_net[i].nodeNum; j++)
        {
            if (i == MUL_PORT_NUM - 1)
                sgl_port_net[i].size[j] = data_size / MUL_PORT_NUM + data_size % MUL_PORT_NUM;
            else
                sgl_port_net[i].size[j] = data_size / MUL_PORT_NUM;
            sgl_port_net[i].data[j] = data[j] + data_size / MUL_PORT_NUM * i;
        }
    }

    /* single port  */
    if (MUL_PORT_NUM == 1)
        sglPortNetTransfer((void *)&sgl_port_net[0]);
    else
    {
        for (int i = 0; i < MUL_PORT_NUM; i++)
            pthreadCreate(&tid[i], NULL, sglPortNetTransfer, (void *)&sgl_port_net[i]);
        for (int i = 0; i < MUL_PORT_NUM; i++)
            pthreadJoin(tid[i], NULL);
    }

    for (int i = 0; i < MUL_PORT_NUM; i++)
    {
        free(sgl_port_net[i].size);
        free(sgl_port_net[i].data);
    }
    return EC_OK;
}

void *mulPortNetTransferP(void *arg)
{
    mulPortNetTransfer_t *transf = (mulPortNetTransfer_t *)arg;
    if ((mulPortNetTransfer(transf->data, transf->mulPortSockfdArr, transf->data_size, transf->nodeNum, transf->mode)) == EC_ERROR)
        ERROR_RETURN_NULL("Fail send 1 slice");
    return NULL;
}

void mulPortNetMetaInitBasic(mulPortNetMeta_t *mulPortNetMetaP, int nodeNum, int mode, int totalSliceNum, int flagEncode,
                             int **mulPortSockfdArr, char **dataRecv, char **dataSend, int *sliceIndexP, unsigned char *g_tbls)
{

    mulPortNetMetaP->nodeNum = nodeNum;
    mulPortNetMetaP->mode = mode;
    mulPortNetMetaP->totalSliceNum = totalSliceNum;
    mulPortNetMetaP->flagEncode = flagEncode;
    mulPortNetMetaP->mulPortSockfdArr = mulPortSockfdArr;
    mulPortNetMetaP->dataRecv = dataRecv;
    mulPortNetMetaP->dataSend = dataSend;
    mulPortNetMetaP->sliceIndexP = sliceIndexP;
    mulPortNetMetaP->g_tbls = g_tbls;
}

void mulPortNetMetaInit3Arr(mulPortNetMeta_t *mulPortNetMetaP, int nodeNum, int mode, int totalSliceNum, int flagEncode,
                            int **mulPortSockfdArr, char ***recvDataBucket, queue_t *sliceIndexRecvQueue, int **sliceBitmap, unsigned char *g_tbls)
{

    mulPortNetMetaP->nodeNum = nodeNum;
    mulPortNetMetaP->mode = mode;
    mulPortNetMetaP->totalSliceNum = totalSliceNum;
    mulPortNetMetaP->flagEncode = flagEncode;
    mulPortNetMetaP->mulPortSockfdArr = mulPortSockfdArr;
    mulPortNetMetaP->recvDataBucket = recvDataBucket;
    mulPortNetMetaP->sliceIndexRecvQueue = sliceIndexRecvQueue;
    mulPortNetMetaP->sliceBitmap = sliceBitmap;
    mulPortNetMetaP->g_tbls = g_tbls;
}

void mulPortNetMetaInitSend2(mulPortNetMeta_t *mulPortNetMetaP, int startFailNodeIndex, int failNodeNum, int *failNodeIndex)
{
    mulPortNetMetaP->failNodeIndex = failNodeIndex;
    mulPortNetMetaP->startFailNodeIndex = startFailNodeIndex;
    mulPortNetMetaP->failNodeNum = failNodeNum;
}

void degradedReadNetRecordNInit(int (*twoBucketsSliceIndexInfo)[6], int divisionNum, int *divisionLines, unsigned char ***g_tbls2Arr)
{
    degradedReadNetRecordN->twoBucketsSliceIndexInfo = twoBucketsSliceIndexInfo;
    degradedReadNetRecordN->divisionNum = divisionNum;
    degradedReadNetRecordN->divisionLines = divisionLines;
    degradedReadNetRecordN->g_tbls2Arr = g_tbls2Arr;
}

void fullRecoverNetRecordNInit(int stripeNum, int increasingNodesIndexArr[][EC_N - 1], int (*twoBucketsSliceIndexInfo)[EC_N - 1][6], int(*divisionNum), int (*divisionLines)[2 * EC_N],
                               int eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char ***g_tbls2Arr, unsigned char ****g_tbls3Arr)
{
    fullRecoverNetRecordN->stripeNum = stripeNum;
    memcpy(fullRecoverNetRecordN->increasingNodesIndexArr, increasingNodesIndexArr, (EC_N - 1) * FULL_RECOVERY_STRIPE_MAX_NUM * sizeof(int));
    fullRecoverNetRecordN->twoBucketsSliceIndexInfo = twoBucketsSliceIndexInfo;
    fullRecoverNetRecordN->divisionNum = divisionNum;
    fullRecoverNetRecordN->divisionLines = divisionLines;
    memcpy(fullRecoverNetRecordN->eachNodeInfo, eachNodeInfo, 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM * sizeof(int));
    fullRecoverNetRecordN->g_tbls2Arr = g_tbls2Arr;
    fullRecoverNetRecordN->g_tbls3Arr = g_tbls3Arr;
}

/* recv 1 node, send 1 node, 1 failure */
void *netSlice1Node1Fail(void *arg)
{
    mulPortNetMeta_t *mulPortNetMeta = (mulPortNetMeta_t *)arg;
    if (mulPortNetMeta->nodeNum != 1)
        ERROR_RETURN_NULL("mulPortNetMeta->nodeNum!=1");
    if (mulPortNetMeta->mode == RECV)
    {
        char **curRecvData = (char **)Malloc(sizeof(char *) * 1);
        curRecvData[0] = mulPortNetMeta->dataRecv[0];
        for (int i = 0; i < mulPortNetMeta->totalSliceNum; i++)
        {
            /* recv slice */
            if ((mulPortNetTransfer(curRecvData, mulPortNetMeta->mulPortSockfdArr, SLICE_SIZE, mulPortNetMeta->nodeNum, mulPortNetMeta->mode)) == EC_ERROR)
                ERROR_RETURN_NULL("Fail recv 1 slice");
            curRecvData[0] += SLICE_SIZE;
            (*(mulPortNetMeta->sliceIndexP))++;
        }
        free(curRecvData);
    }
    else if (mulPortNetMeta->mode == SEND)
    {
        char **curRecvData = (char **)Malloc(sizeof(char *) * 1);
        if (mulPortNetMeta->flagEncode == 1)
            curRecvData[0] = mulPortNetMeta->dataRecv[0];
        char **cur_send_data = (char **)Malloc(sizeof(char *) * 1);
        cur_send_data[0] = mulPortNetMeta->dataSend[0];
        int curSliceNum = 0;
        char **data_tmp = (char **)Malloc(sizeof(char *) * 2); // tmp data pointer
        char *slice_tmp = (char *)Calloc(SLICE_SIZE, sizeof(char));
        char **send_tmp;
        while (1)
        {
            if ((*(mulPortNetMeta->sliceIndexP)) > curSliceNum) // There is received slice data
            {
                /* multiplication and XOR */
                if (mulPortNetMeta->flagEncode == 1)
                {
                    data_tmp[0] = curRecvData[0];
                    data_tmp[1] = cur_send_data[0];
                    mulThreadEncode(SLICE_SIZE, 2, 1, mulPortNetMeta->g_tbls, (unsigned char **)data_tmp, (unsigned char **)&slice_tmp);
                    send_tmp = &slice_tmp;
                    // send_tmp = &cur_send_data[0];
                }
                else
                    send_tmp = &cur_send_data[0];

                /* send slice */
                if ((mulPortNetTransfer(send_tmp, mulPortNetMeta->mulPortSockfdArr, SLICE_SIZE, mulPortNetMeta->nodeNum, mulPortNetMeta->mode)) == EC_ERROR)
                    ERROR_RETURN_NULL("Fail send 1 slice");

                if (mulPortNetMeta->flagEncode == 1)
                    curRecvData[0] += SLICE_SIZE;
                cur_send_data[0] += SLICE_SIZE;
                curSliceNum++;
            }
            if (curSliceNum == mulPortNetMeta->totalSliceNum)
                break;
        }
        free4(slice_tmp, data_tmp, cur_send_data, curRecvData);
    }
    else
        ERROR_RETURN_NULL("error mulPortNetMeta->mode");

    return NULL;
}

void *netSliceKNode1FailRecv(void *arg)
{
    mulPortNetMeta_t *mulPortNetMeta = (mulPortNetMeta_t *)arg;
    if (mulPortNetMeta->nodeNum != 1)
        ERROR_RETURN_NULL("mulPortNetMeta->nodeNum!=1");
    if (mulPortNetMeta->mode == RECV)
    {
        char **curRecvData = (char **)Malloc(sizeof(char *) * 1);
        curRecvData[0] = mulPortNetMeta->dataRecv[0];
        for (int i = 0; i < mulPortNetMeta->totalSliceNum; i++)
        {
            /* recv slice */
            if ((mulPortNetTransfer(curRecvData, mulPortNetMeta->mulPortSockfdArr, SLICE_SIZE, mulPortNetMeta->nodeNum, mulPortNetMeta->mode)) == EC_ERROR)
                ERROR_RETURN_NULL("Fail recv 1 slice");
            curRecvData[0] += SLICE_SIZE;

            /* update slice bitmap and check it if k slice recv  */
            int sum_tmp = 0;
            pthread_mutex_lock(&mutex_bitmap[i]);
            mulPortNetMeta->sliceBitmap[mulPortNetMeta->curNodeIndex][i] = 1;
            for (int j = 0; j < EC_K; j++)
                sum_tmp += mulPortNetMeta->sliceBitmap[j][i];
            pthread_mutex_unlock(&mutex_bitmap[i]);
            if (sum_tmp == EC_K)
                enqueue(mulPortNetMeta->sliceIndexRecvQueue, i);
        }
        free(curRecvData);
    }
    else
        ERROR_RETURN_NULL("error mulPortNetMeta->mode");
    return NULL;
}

void *netSliceKNode(void *arg)
{
    mulPortNetMeta_t *mulPortNetMeta = (mulPortNetMeta_t *)arg;
    if (mulPortNetMeta->mode == RECV)
    {
        if (mulPortNetMeta->nodeNum != EC_K)
            ERROR_RETURN_NULL("mulPortNetMeta->nodeNum!=EC_K");
        for (int i = 0; i < CHUNK_SIZE / SLICE_SIZE; i++)
            pthread_mutex_init(&mutex_bitmap[i], NULL);

        mulPortNetMeta_t mul_port_net_1_recv[EC_K];
        pthread_t tid[EC_K];
        for (int i = 0; i < EC_K; i++)
        {
            mul_port_net_1_recv[i].nodeNum = 1;
            mul_port_net_1_recv[i].mulPortSockfdArr = (int **)Malloc(sizeof(int *) * MUL_PORT_NUM);
            for (int j = 0; j < MUL_PORT_NUM; j++)
                mul_port_net_1_recv[i].mulPortSockfdArr[j] = &(mulPortNetMeta->mulPortSockfdArr[j][i]);
            mul_port_net_1_recv[i].dataRecv = mulPortNetMeta->recvDataBucket[i];
            mul_port_net_1_recv[i].totalSliceNum = mulPortNetMeta->totalSliceNum;
            mul_port_net_1_recv[i].mode = RECV;
            mul_port_net_1_recv[i].sliceBitmap = mulPortNetMeta->sliceBitmap;
            mul_port_net_1_recv[i].sliceIndexRecvQueue = mulPortNetMeta->sliceIndexRecvQueue;
            mul_port_net_1_recv[i].curNodeIndex = i;
            pthreadCreate(&tid[i], NULL, netSliceKNode1FailRecv, (void *)&mul_port_net_1_recv[i]); // Create thread to recv slice
        }
        for (int i = 0; i < EC_K; i++)
            pthreadJoin(tid[i], NULL);
        for (int i = 0; i < EC_K; i++)
            free(mul_port_net_1_recv[i].mulPortSockfdArr);
        for (int i = 0; i < CHUNK_SIZE / SLICE_SIZE; i++)
            pthread_mutex_destroy(&mutex_bitmap[i]);
    }
    else if (mulPortNetMeta->mode == SEND)
    {
        if (mulPortNetMeta->nodeNum != 1)
            ERROR_RETURN_NULL("mulPortNetMeta->nodeNum!=1");
        char **data_tmp = (char **)Malloc(sizeof(char *) * EC_K); // tmp data pointer
        char *slice_tmp = (char *)Calloc(mulPortNetMeta->failNodeNum * SLICE_SIZE, sizeof(char));
        char **send_tmp, *pointer_tmp;
        int slice_loc[CHUNK_SIZE / SLICE_SIZE];
        int slice_loc_conunt = 0, curSliceNum = 0;

        int *tmpSockfdBuf = NULL, **mulPortMulNodeSendTmpSockfd = NULL, ***tmpSockfd = NULL;
        create3PointerArrINT(&tmpSockfdBuf, &mulPortMulNodeSendTmpSockfd, &tmpSockfd, MUL_FAIL_NUM, MUL_PORT_NUM, 1);
        for (int i = 0; i < MUL_PORT_NUM; i++)
            for (int j = 0; j < MUL_FAIL_NUM; j++)
                tmpSockfd[j][i][0] = mulPortNetMeta->mulPortSockfdArr[i][j];
        while (1)
        {
            if (!queueIsEmpty(mulPortNetMeta->sliceIndexRecvQueue)) // There is received k slices
            {
                /* encoding */
                int curSlice = dequeue(mulPortNetMeta->sliceIndexRecvQueue);
                slice_loc[slice_loc_conunt++] = curSlice;
                for (int i = 0; i < EC_K; i++)
                    data_tmp[i] = mulPortNetMeta->recvDataBucket[i][curSlice];
                for (int i = 0; i < mulPortNetMeta->failNodeNum; i++)
                {
                    pointer_tmp = slice_tmp + i * SLICE_SIZE;
                    mulThreadEncode(SLICE_SIZE, EC_K, 1, mulPortNetMeta->g_tbls + i * EC_K * 32, (unsigned char **)data_tmp, (unsigned char **)&pointer_tmp);
                }
                /* send slice */
                pthread_t tid[mulPortNetMeta->failNodeNum];
                mulPortNetTransfer_t transf[mulPortNetMeta->failNodeNum];
                for (int i = 0; i < mulPortNetMeta->failNodeNum; i++)
                {
                    int curOffset = mulPortNetMeta->failNodeIndex[i] - mulPortNetMeta->startFailNodeIndex;
                    pointer_tmp = slice_tmp + i * SLICE_SIZE;
                    send_tmp = &pointer_tmp;
                    transf[i].data = send_tmp;
                    transf[i].mulPortSockfdArr = tmpSockfd[curOffset];
                    transf[i].data_size = SLICE_SIZE;
                    transf[i].nodeNum = mulPortNetMeta->nodeNum;
                    transf[i].mode = mulPortNetMeta->mode;
                    pthreadCreate(&tid[i], NULL, mulPortNetTransferP, (void *)&transf[i]); // Create thread to send slice
                }
                for (int i = 0; i < mulPortNetMeta->failNodeNum; i++)
                    pthreadJoin(tid[i], NULL);
                curSliceNum++;
            }
            if (curSliceNum == mulPortNetMeta->totalSliceNum)
                break;
        }

        for (int i = 0; i < mulPortNetMeta->failNodeNum; i++)
        {
            int curOffset = mulPortNetMeta->failNodeIndex[i] - mulPortNetMeta->startFailNodeIndex;
            Send(tmpSockfd[curOffset][0][0], slice_loc, sizeof(int) * (CHUNK_SIZE / SLICE_SIZE), "send slice loc");
        }
        delete3ArrINT(tmpSockfdBuf, mulPortMulNodeSendTmpSockfd, tmpSockfd);
        free2(slice_tmp, data_tmp);
    }
    else
        ERROR_RETURN_NULL("error mulPortNetMeta->mode");
    return NULL;
}

void *netSliceNNode1FailRecv(void *arg)
{
    mulPortNetMeta_t *mulPortNetMeta = (mulPortNetMeta_t *)arg;
    if (mulPortNetMeta->nodeNum != 1)
        ERROR_RETURN_NULL("mulPortNetMeta->nodeNum!=1");
    int curNodeIndex = mulPortNetMeta->curNodeIndex;
    int start_index, data_num, cur_bucket;
    if (mulPortNetMeta->mode == RECV)
    {
        char **curRecvData = (char **)Malloc(sizeof(char *) * 1);
        for (int j = 0; j < 6; j += 3) // each node at most have data of two bucket
            if (degradedReadNetRecordN->twoBucketsSliceIndexInfo[curNodeIndex][j] != -1)
            {
                start_index = degradedReadNetRecordN->twoBucketsSliceIndexInfo[curNodeIndex][j];
                cur_bucket = degradedReadNetRecordN->twoBucketsSliceIndexInfo[curNodeIndex][j + 2];
                curRecvData[0] = mulPortNetMeta->recvDataBucket[cur_bucket][start_index];
                data_num = degradedReadNetRecordN->twoBucketsSliceIndexInfo[curNodeIndex][j + 1];
                for (int i = 0; i < data_num; i++)
                {
                    /* recv slice */
                    if ((mulPortNetTransfer(curRecvData, mulPortNetMeta->mulPortSockfdArr, SLICE_SIZE, mulPortNetMeta->nodeNum, mulPortNetMeta->mode)) == EC_ERROR)
                        ERROR_RETURN_NULL("Fail recv 1 slice");
                    curRecvData[0] += SLICE_SIZE;

                    /* update slice bitmap and check it if k slice recv  */
                    int sum_tmp = 0;
                    int curSlice = i + start_index;
                    pthread_mutex_lock(&mutex_bitmap[curSlice]);
                    mulPortNetMeta->sliceBitmap[cur_bucket][curSlice] = 1;
                    for (int j = 0; j < EC_K; j++)
                        sum_tmp += mulPortNetMeta->sliceBitmap[j][curSlice];
                    pthread_mutex_unlock(&mutex_bitmap[curSlice]);
                    if (sum_tmp == EC_K)
                        enqueue(mulPortNetMeta->sliceIndexRecvQueue, curSlice);
                }
            }
        free(curRecvData);
    }
    else
        ERROR_RETURN_NULL("error mulPortNetMeta->mode");
    return NULL;
}

void *netSliceNNode(void *arg)
{
    mulPortNetMeta_t *mulPortNetMeta = (mulPortNetMeta_t *)arg;
    if (mulPortNetMeta->mode == RECV)
    {
        for (int i = 0; i < CHUNK_SIZE / SLICE_SIZE; i++)
            pthread_mutex_init(&mutex_bitmap[i], NULL);

        mulPortNetMeta_t mul_port_net_1_recv[EC_N - 1];
        pthread_t tid[EC_N - 1];
        for (int i = 0; i < EC_N - mulPortNetMeta->failNodeNum; i++)
        {
            mul_port_net_1_recv[i].nodeNum = 1;
            mul_port_net_1_recv[i].mulPortSockfdArr = (int **)Malloc(sizeof(int *) * MUL_PORT_NUM);
            for (int j = 0; j < MUL_PORT_NUM; j++)
                mul_port_net_1_recv[i].mulPortSockfdArr[j] = &(mulPortNetMeta->mulPortSockfdArr[j][i]);
            mul_port_net_1_recv[i].recvDataBucket = mulPortNetMeta->recvDataBucket;
            mul_port_net_1_recv[i].mode = RECV;
            mul_port_net_1_recv[i].sliceBitmap = mulPortNetMeta->sliceBitmap;
            mul_port_net_1_recv[i].sliceIndexRecvQueue = mulPortNetMeta->sliceIndexRecvQueue;
            mul_port_net_1_recv[i].curNodeIndex = i;
            pthreadCreate(&tid[i], NULL, netSliceNNode1FailRecv, (void *)&mul_port_net_1_recv[i]); // Create thread to recv slice
        }
        for (int i = 0; i < EC_N - mulPortNetMeta->failNodeNum; i++)
            pthreadJoin(tid[i], NULL);
        for (int i = 0; i < EC_N - mulPortNetMeta->failNodeNum; i++)
            free(mul_port_net_1_recv[i].mulPortSockfdArr);
        for (int i = 0; i < CHUNK_SIZE / SLICE_SIZE; i++)
            pthread_mutex_destroy(&mutex_bitmap[i]);
    }
    else if (mulPortNetMeta->mode == SEND)
    {
        if (mulPortNetMeta->nodeNum != 1)
            ERROR_RETURN_NULL("mulPortNetMeta->nodeNum!=1");
        char **data_tmp = (char **)Malloc(sizeof(char *) * EC_K); // tmp data pointer
        char *slice_tmp = (char *)Calloc(mulPortNetMeta->failNodeNum * SLICE_SIZE, sizeof(char));
        char **send_tmp, *pointer_tmp;
        int slice_loc[CHUNK_SIZE / SLICE_SIZE];
        int slice_loc_conunt = 0, curSliceNum = 0;

        int *tmpSockfdBuf = NULL, **mulPortMulNodeSendTmpSockfd = NULL, ***tmpSockfd = NULL;
        create3PointerArrINT(&tmpSockfdBuf, &mulPortMulNodeSendTmpSockfd, &tmpSockfd, MUL_FAIL_NUM, MUL_PORT_NUM, 1);
        for (int i = 0; i < MUL_PORT_NUM; i++)
            for (int j = 0; j < MUL_FAIL_NUM; j++)
                tmpSockfd[j][i][0] = mulPortNetMeta->mulPortSockfdArr[i][j];
        unsigned char *g_tblsTmpBuf = NULL, **g_tblsTmpArr = NULL;
        create2PointerArrUC(&g_tblsTmpBuf, &g_tblsTmpArr, MUL_FAIL_NUM, EC_K * 32);
        while (1)
        {
            if (!queueIsEmpty(mulPortNetMeta->sliceIndexRecvQueue)) // There is received k slices
            {
                /* encoding */
                int curSlice = dequeue(mulPortNetMeta->sliceIndexRecvQueue);
                slice_loc[slice_loc_conunt++] = curSlice;
                for (int i = 0; i < EC_K; i++)
                    data_tmp[i] = mulPortNetMeta->recvDataBucket[i][curSlice];
                for (int i = 0; i < degradedReadNetRecordN->divisionNum; i++)
                    if (degradedReadNetRecordN->divisionLines[i] > curSlice)
                    {
                        for (int j = 0; j < mulPortNetMeta->failNodeNum; j++)
                            g_tblsTmpArr[j] = degradedReadNetRecordN->g_tbls2Arr[j][i];
                        break;
                    }
                for (int i = 0; i < mulPortNetMeta->failNodeNum; i++)
                {
                    pointer_tmp = slice_tmp + i * SLICE_SIZE;
                    mulThreadEncode(SLICE_SIZE, EC_K, 1, g_tblsTmpArr[i], (unsigned char **)data_tmp, (unsigned char **)&pointer_tmp);
                }

                /* send slice */
                pthread_t tid[mulPortNetMeta->failNodeNum];
                mulPortNetTransfer_t transf[mulPortNetMeta->failNodeNum];
                for (int i = 0; i < mulPortNetMeta->failNodeNum; i++)
                {
                    pointer_tmp = slice_tmp + i * SLICE_SIZE;
                    send_tmp = &pointer_tmp;
                    transf[i].data = send_tmp;
                    transf[i].mulPortSockfdArr = tmpSockfd[i];
                    transf[i].data_size = SLICE_SIZE;
                    transf[i].nodeNum = mulPortNetMeta->nodeNum;
                    transf[i].mode = mulPortNetMeta->mode;
                    pthreadCreate(&tid[i], NULL, mulPortNetTransferP, (void *)&transf[i]); // Create thread to send slice
                }
                for (int i = 0; i < mulPortNetMeta->failNodeNum; i++)
                    pthreadJoin(tid[i], NULL);

                curSliceNum++;
            }
            if (curSliceNum == mulPortNetMeta->totalSliceNum)
                break;
        }
        for (int i = 0; i < mulPortNetMeta->failNodeNum; i++)
            Send(mulPortNetMeta->mulPortSockfdArr[0][i], slice_loc, sizeof(int) * (CHUNK_SIZE / SLICE_SIZE), "send slice loc");

        delete2ArrUC(g_tblsTmpBuf, g_tblsTmpArr);
        delete3ArrINT(tmpSockfdBuf, mulPortMulNodeSendTmpSockfd, tmpSockfd);
        free2(slice_tmp, data_tmp);
    }
    else
        ERROR_RETURN_NULL("error mulPortNetMeta->mode");
    return NULL;
}

void *netSliceANode1FailRecv(void *arg)
{
    mulPortNetMeta_t *mulPortNetMeta = (mulPortNetMeta_t *)arg;
    if (mulPortNetMeta->nodeNum != 1)
        ERROR_RETURN_NULL("mulPortNetMeta->nodeNum!=1");
    if (mulPortNetMeta->mode != RECV)
        ERROR_RETURN_NULL("mulPortNetMeta->mode != RECV");

    int curNodeIndex = mulPortNetMeta->curNodeIndex, recvStripeNum = 0, increasingNodeIndex;
    int totalSliceNum[FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, recv_slice_num[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    int cur_bucket[FULL_RECOVERY_STRIPE_MAX_NUM][2] = {0}, start_index[FULL_RECOVERY_STRIPE_MAX_NUM][2], bucket_slice_thresh[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    int curStripe[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    memset(cur_bucket, -1, 2 * FULL_RECOVERY_STRIPE_MAX_NUM * sizeof(int));

    for (int i = 0; i < fullRecoverNetRecordN->stripeNum; i++)
    {
        if ((increasingNodeIndex = getArrIndex(fullRecoverNetRecordN->increasingNodesIndexArr[i], EC_N - 1, curNodeIndex)) == -1)
            continue; // no exist
        if (fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][0] != -1)
        {
            start_index[recvStripeNum][0] = fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][0];
            totalSliceNum[recvStripeNum] += fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][1];
            cur_bucket[recvStripeNum][0] = fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][2];
            bucket_slice_thresh[recvStripeNum] = fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][1];
        }
        if (fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][3] != -1)
        {
            start_index[recvStripeNum][1] = fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][3];
            totalSliceNum[recvStripeNum] += fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][4];
            cur_bucket[recvStripeNum][1] = fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][5];
        }
        curStripe[recvStripeNum] = i;
        recvStripeNum++;
    }

    int startIndexTmp, curBucketTmp;
    char **curRecvData = (char **)Malloc(sizeof(char *) * 1);
    for (int j = 0; j < CHUNK_SIZE / SLICE_SIZE; j++)
        for (int i = 0; i < recvStripeNum; i++)
        {
            if (totalSliceNum[i] == recv_slice_num[i])
                continue;
            if (recv_slice_num[i]++ >= bucket_slice_thresh[i] && cur_bucket[i][1] != -1)
            {
                curBucketTmp = cur_bucket[i][1] + curStripe[i] * EC_K;
                startIndexTmp = start_index[i][1]++;
            }
            else
            {
                curBucketTmp = cur_bucket[i][0] + curStripe[i] * EC_K;
                startIndexTmp = start_index[i][0]++;
            }
            curRecvData[0] = mulPortNetMeta->recvDataBucket[curBucketTmp][startIndexTmp];

            if ((mulPortNetTransfer(curRecvData, mulPortNetMeta->mulPortSockfdArr, SLICE_SIZE, mulPortNetMeta->nodeNum, mulPortNetMeta->mode)) == EC_ERROR)
                ERROR_RETURN_NULL("Fail recv 1 slice");

            /* update slice bitmap and check it if k slice recv  */
            int sum_tmp = 0;
            int curSlice = curStripe[i] * (CHUNK_SIZE / SLICE_SIZE) + startIndexTmp; // stripe+slice
            pthread_mutex_lock(&mutex_bitmap_f[curSlice]);
            mulPortNetMeta->sliceBitmap[curBucketTmp][startIndexTmp] = 1; //[stripe+bucket][slice]
            for (int j = 0; j < EC_K; j++)
                sum_tmp += mulPortNetMeta->sliceBitmap[curStripe[i] * EC_K + j][startIndexTmp];
            pthread_mutex_unlock(&mutex_bitmap_f[curSlice]);
            if (sum_tmp == EC_K)
                enqueue(mulPortNetMeta->sliceIndexRecvQueue, curSlice);
        }
    free(curRecvData);
    return NULL;
}

void *netSliceANode1Fail(void *arg)
{
    mulPortNetMeta_t *mulPortNetMeta = (mulPortNetMeta_t *)arg;
    if (mulPortNetMeta->mode == RECV)
    {
        for (int i = 0; i < mulPortNetMeta->totalSliceNum; i++)
            pthread_mutex_init(&mutex_bitmap_f[i], NULL);
        mulPortNetMeta_t mul_port_net_1_recv[EC_A - 1];
        pthread_t tid[EC_A - 1];
        for (int i = 0; i < EC_A - 1; i++)
        {
            mul_port_net_1_recv[i].nodeNum = 1;
            mul_port_net_1_recv[i].mulPortSockfdArr = (int **)Malloc(sizeof(int *) * MUL_PORT_NUM);
            for (int j = 0; j < MUL_PORT_NUM; j++)
                mul_port_net_1_recv[i].mulPortSockfdArr[j] = &(mulPortNetMeta->mulPortSockfdArr[j][i]);
            mul_port_net_1_recv[i].recvDataBucket = mulPortNetMeta->recvDataBucket;
            mul_port_net_1_recv[i].mode = RECV;
            mul_port_net_1_recv[i].sliceBitmap = mulPortNetMeta->sliceBitmap;
            mul_port_net_1_recv[i].sliceIndexRecvQueue = mulPortNetMeta->sliceIndexRecvQueue;
            mul_port_net_1_recv[i].curNodeIndex = i < mulPortNetMeta->startFailNodeIndex ? i : i + 1;
            pthreadCreate(&tid[i], NULL, netSliceANode1FailRecv, (void *)&mul_port_net_1_recv[i]); // Create thread to recv slice
        }
        for (int i = 0; i < EC_A - 1; i++)
            pthreadJoin(tid[i], NULL);
        enqueue(mulPortNetMeta->sliceIndexRecvQueue, -1); // mark node recv end
        for (int i = 0; i < EC_A - 1; i++)
            free(mul_port_net_1_recv[i].mulPortSockfdArr);
        for (int i = 0; i < mulPortNetMeta->totalSliceNum; i++)
            pthread_mutex_destroy(&mutex_bitmap_f[i]);
    }
    else if (mulPortNetMeta->mode == SEND)
    {
        if (mulPortNetMeta->nodeNum != 1)
            ERROR_RETURN_NULL("mulPortNetMeta->nodeNum!=1");
        int curSliceNum = 0;
        char **data_tmp = (char **)Malloc(sizeof(char *) * EC_K); // tmp data pointer
        char *slice_tmp = (char *)Calloc(SLICE_SIZE, sizeof(char));
        char **send_tmp;
        int slice_loc[fullRecoverNetRecordN->stripeNum * (CHUNK_SIZE / SLICE_SIZE)];
        int slice_send_conunt = 0;

        while (1)
        {
            if (!queueIsEmpty(mulPortNetMeta->sliceIndexRecvQueue)) // There is received k slices
            {
                /* encoding */
                int curSlice = dequeue(mulPortNetMeta->sliceIndexRecvQueue); //[stripe][slice]
                if (curSlice == -1)
                    break;
                int curSliceTmp = curSlice % (CHUNK_SIZE / SLICE_SIZE); //[slice]
                int curStripe = curSlice / (CHUNK_SIZE / SLICE_SIZE);   //[stripe]
                int cur_bucket = curStripe * EC_K;                      // [stripe]*EC_K
                slice_loc[curSlice] = slice_send_conunt++;              // it is totally different for d repair,where p_slice_loc[loc]=send_count, d is p_slice_loc[send_count]=loc

                for (int i = 0; i < EC_K; i++)
                    data_tmp[i] = mulPortNetMeta->recvDataBucket[cur_bucket + i][curSliceTmp]; //[stripe+bucket][slice]
                for (int i = 0; i < fullRecoverNetRecordN->divisionNum[curStripe]; i++)
                    if (fullRecoverNetRecordN->divisionLines[curStripe][i] > curSliceTmp)
                    {
                        mulPortNetMeta->g_tbls = fullRecoverNetRecordN->g_tbls2Arr[curStripe][i];
                        mulThreadEncode(SLICE_SIZE, EC_K, 1, mulPortNetMeta->g_tbls, (unsigned char **)data_tmp, (unsigned char **)&slice_tmp);
                        break;
                    }
                send_tmp = &slice_tmp;

                /* send slice */
                if ((mulPortNetTransfer(send_tmp, mulPortNetMeta->mulPortSockfdArr, SLICE_SIZE, mulPortNetMeta->nodeNum, mulPortNetMeta->mode)) == EC_ERROR)
                    ERROR_RETURN_NULL("Fail send 1 slice");

                curSliceNum++;
            }
        }
        Send(mulPortNetMeta->mulPortSockfdArr[0][0], slice_loc, sizeof(int) * fullRecoverNetRecordN->stripeNum * (CHUNK_SIZE / SLICE_SIZE), "send slice loc");
        free2(slice_tmp, data_tmp);
    }
    else
        ERROR_RETURN_NULL("error mulPortNetMeta->mode");

    return NULL;
}

void *netSliceANodeMulFailRecv(void *arg)
{
    mulPortNetMeta_t *mulPortNetMeta = (mulPortNetMeta_t *)arg;
    if (mulPortNetMeta->mode != RECV)
        ERROR_RETURN_NULL("mulPortNetMeta->mode != RECV");

    int curNodeIndex = mulPortNetMeta->curNodeIndex;
    int recvStripeNum = 0, increasingNodeIndex;
    int totalSliceNum[FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, recv_slice_num[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    int cur_bucket[FULL_RECOVERY_STRIPE_MAX_NUM][2] = {0}, start_index[FULL_RECOVERY_STRIPE_MAX_NUM][2], bucket_slice_thresh[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    int curStripe[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    memset(cur_bucket, -1, 2 * FULL_RECOVERY_STRIPE_MAX_NUM * sizeof(int));

    for (int i = 0; i < fullRecoverNetRecordN->stripeNum; i++)
    {
        if ((increasingNodeIndex = getArrIndex(fullRecoverNetRecordN->increasingNodesIndexArr[i], EC_N - 1, curNodeIndex)) == -1)
            continue; // no exist

        if (fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][0] != -1)
        {
            start_index[recvStripeNum][0] = fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][0];
            totalSliceNum[recvStripeNum] += fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][1];
            cur_bucket[recvStripeNum][0] = fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][2];
            bucket_slice_thresh[recvStripeNum] = fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][1];
        }
        if (fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][3] != -1)
        {
            start_index[recvStripeNum][1] = fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][3];
            totalSliceNum[recvStripeNum] += fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][4];
            cur_bucket[recvStripeNum][1] = fullRecoverNetRecordN->twoBucketsSliceIndexInfo[i][increasingNodeIndex][5];
        }
        curStripe[recvStripeNum] = i;
        recvStripeNum++;
    }

    int startIndexTmp, curBucketTmp;
    char **curRecvData = (char **)Malloc(sizeof(char *) * 1);
    for (int j = 0; j < CHUNK_SIZE / SLICE_SIZE; j++)
        for (int i = 0; i < recvStripeNum; i++)
        {
            if (totalSliceNum[i] == recv_slice_num[i])
                continue;
            if (recv_slice_num[i]++ >= bucket_slice_thresh[i] && cur_bucket[i][1] != -1)
            {
                curBucketTmp = cur_bucket[i][1] + curStripe[i] * EC_K;
                startIndexTmp = start_index[i][1]++;
            }
            else
            {
                curBucketTmp = cur_bucket[i][0] + curStripe[i] * EC_K;
                startIndexTmp = start_index[i][0]++;
            }
            curRecvData[0] = mulPortNetMeta->recvDataBucket[curBucketTmp][startIndexTmp];

            if ((mulPortNetTransfer(curRecvData, mulPortNetMeta->mulPortSockfdArr, SLICE_SIZE, mulPortNetMeta->nodeNum, mulPortNetMeta->mode)) == EC_ERROR)
                ERROR_RETURN_NULL("Fail recv 1 slice");

            /* update slice bitmap and check it if k slice recv  */
            int sum_tmp = 0;
            int curSlice = curStripe[i] * (CHUNK_SIZE / SLICE_SIZE) + startIndexTmp; // stripe+slice
            pthread_mutex_lock(&mutex_bitmap_f[curSlice]);
            mulPortNetMeta->sliceBitmap[curBucketTmp][startIndexTmp] = 1; //[stripe+bucket][slice]
            for (int j = 0; j < EC_K; j++)
                sum_tmp += mulPortNetMeta->sliceBitmap[curStripe[i] * EC_K + j][startIndexTmp];
            pthread_mutex_unlock(&mutex_bitmap_f[curSlice]);
            if (sum_tmp == EC_K)
                enqueue(mulPortNetMeta->sliceIndexRecvQueue, curSlice);
        }

    free(curRecvData);
    return NULL;
}

void *netSliceANodeMulFail(void *arg)
{
    mulPortNetMeta_t *mulPortNetMeta = (mulPortNetMeta_t *)arg;

    int mergeNR[fullRecoverNetRecordN->stripeNum];
    if (MUL_FAIL_NUM == 1)
        for (int i = 0; i < fullRecoverNetRecordN->stripeNum; i++)
            mergeNR[i] = fullRecoverNetRecordN->eachNodeInfo[0][MUL_FAIL_START][i];
    else
        for (int n = 1; n < MUL_FAIL_NUM; n++)
            if (n == 1)
                mergeFail(fullRecoverNetRecordN->eachNodeInfo[0][0 + MUL_FAIL_START], fullRecoverNetRecordN->eachNodeInfo[0][1 + MUL_FAIL_START], mergeNR, fullRecoverNetRecordN->stripeNum);
            else
                mergeFail(fullRecoverNetRecordN->eachNodeInfo[0][n + MUL_FAIL_START], mergeNR, mergeNR, fullRecoverNetRecordN->stripeNum);

    if (mulPortNetMeta->mode == RECV)
    {
        for (int i = 0; i < mulPortNetMeta->totalSliceNum; i++)
            pthread_mutex_init(&mutex_bitmap_f[i], NULL);

        mulPortNetMeta_t mul_port_net_1_recv[EC_A - MUL_FAIL_NUM];
        pthread_t tid[EC_A - MUL_FAIL_NUM];
        for (int i = 0; i < EC_A - MUL_FAIL_NUM; i++)
        {
            mul_port_net_1_recv[i].nodeNum = 1;
            mul_port_net_1_recv[i].mulPortSockfdArr = (int **)Malloc(sizeof(int *) * MUL_PORT_NUM);
            for (int j = 0; j < MUL_PORT_NUM; j++)
                mul_port_net_1_recv[i].mulPortSockfdArr[j] = &(mulPortNetMeta->mulPortSockfdArr[j][i]);
            mul_port_net_1_recv[i].recvDataBucket = mulPortNetMeta->recvDataBucket;
            mul_port_net_1_recv[i].mode = RECV;
            mul_port_net_1_recv[i].sliceBitmap = mulPortNetMeta->sliceBitmap;
            mul_port_net_1_recv[i].sliceIndexRecvQueue = mulPortNetMeta->sliceIndexRecvQueue;
            mul_port_net_1_recv[i].curNodeIndex = i < MUL_FAIL_START ? i : i + MUL_FAIL_NUM;
            mul_port_net_1_recv[i].mergeNR = mergeNR;
            pthreadCreate(&tid[i], NULL, netSliceANodeMulFailRecv, (void *)&mul_port_net_1_recv[i]); // Create thread to recv slice
        }
        for (int i = 0; i < EC_A - MUL_FAIL_NUM; i++)
            pthreadJoin(tid[i], NULL);
        enqueue(mulPortNetMeta->sliceIndexRecvQueue, -1);                                                                             // mark node recv end
        for (int i = 0; i < EC_A - MUL_FAIL_NUM; i++)
            free(mul_port_net_1_recv[i].mulPortSockfdArr);
        for (int i = 0; i < mulPortNetMeta->totalSliceNum; i++)
            pthread_mutex_destroy(&mutex_bitmap_f[i]);
    }
    else if (mulPortNetMeta->mode == SEND)
    {
        if (mulPortNetMeta->nodeNum != 1)
            ERROR_RETURN_NULL("mulPortNetMeta->nodeNum!=1");
        int curSliceNum = 0, curSlice;
        char **data_tmp = (char **)Malloc(sizeof(char *) * EC_K); // tmp data pointer
        char *slice_tmp = (char *)Calloc(MUL_FAIL_NUM * SLICE_SIZE, sizeof(char));
        char *pointer_tmp;
        char **send_tmp;

        int **slice_loc = (int **)Malloc(MUL_FAIL_NUM * sizeof(int *));
        for (int i = 0; i < MUL_FAIL_NUM; i++)
            slice_loc[i] = (int *)Malloc(fullRecoverNetRecordN->stripeNum * (CHUNK_SIZE / SLICE_SIZE) * sizeof(int));
        // int slice_loc[MUL_FAIL_NUM * fullRecoverNetRecordN->stripeNum * (CHUNK_SIZE / SLICE_SIZE)];

        int slice_send_conunt[MUL_FAIL_NUM] = {0};
        int *tmpSockfdBuf = NULL, **mulPortMulNodeSendTmpSockfd = NULL, ***tmpSockfd = NULL;
        create3PointerArrINT(&tmpSockfdBuf, &mulPortMulNodeSendTmpSockfd, &tmpSockfd, MUL_FAIL_NUM, MUL_PORT_NUM, 1);
        for (int i = 0; i < MUL_PORT_NUM; i++)
            for (int j = 0; j < MUL_FAIL_NUM; j++)
                tmpSockfd[j][i][0] = mulPortNetMeta->mulPortSockfdArr[i][j];

        int map_stripe2actual[MUL_FAIL_NUM][FULL_RECOVERY_STRIPE_MAX_NUM];
        for (int n = 0; n < MUL_FAIL_NUM; n++)
        {
            int tmp_count = 0;
            for (int i = 0; i < fullRecoverNetRecordN->stripeNum; i++)
                if (mergeNR[i] != fullRecoverNetRecordN->eachNodeInfo[0][n + MUL_FAIL_START][tmp_count])
                    continue;
                else
                    map_stripe2actual[n][i] = tmp_count++;
        }

        while (1)
        {
            if (!queueIsEmpty(mulPortNetMeta->sliceIndexRecvQueue)) // There is received k slices
            {
                /* encoding */
                curSlice = dequeue(mulPortNetMeta->sliceIndexRecvQueue); //[stripe][slice]
                if (curSlice == -1)
                    break;
                int curSliceTmp = curSlice % (CHUNK_SIZE / SLICE_SIZE); //[slice]
                int curStripe = curSlice / (CHUNK_SIZE / SLICE_SIZE);   //[stripe]
                int cur_bucket = curStripe * EC_K;                      // [stripe]*EC_K

                for (int i = 0; i < EC_K; i++)
                    data_tmp[i] = mulPortNetMeta->recvDataBucket[cur_bucket + i][curSliceTmp]; //[stripe+bucket][slice]

                for (int i = 0; i < fullRecoverNetRecordN->divisionNum[curStripe]; i++)
                    if (fullRecoverNetRecordN->divisionLines[curStripe][i] > curSliceTmp)
                    {
                        for (int j = 0; j < mulPortNetMeta->eachStripeFailNodeNum[curStripe]; j++)
                        {
                            pointer_tmp = slice_tmp + j * SLICE_SIZE;
                            mulPortNetMeta->g_tbls = fullRecoverNetRecordN->g_tbls3Arr[curStripe][j][i];
                            mulThreadEncode(SLICE_SIZE, EC_K, 1, mulPortNetMeta->g_tbls, (unsigned char **)data_tmp, (unsigned char **)&pointer_tmp);
                        }
                        pthread_t tid[mulPortNetMeta->eachStripeFailNodeNum[curStripe]];
                        mulPortNetTransfer_t transf[mulPortNetMeta->eachStripeFailNodeNum[curStripe]];
                        for (int j = 0; j < mulPortNetMeta->eachStripeFailNodeNum[curStripe]; j++)
                        {
                            pointer_tmp = slice_tmp + j * SLICE_SIZE;
                            send_tmp = &pointer_tmp;
                            int curOffset = mulPortNetMeta->eachStripeFailNodeIndex[curStripe][j] - MUL_FAIL_START;
                            transf[j].data = send_tmp;
                            transf[j].mulPortSockfdArr = tmpSockfd[curOffset];
                            transf[j].data_size = SLICE_SIZE;
                            transf[j].nodeNum = mulPortNetMeta->nodeNum;
                            transf[j].mode = mulPortNetMeta->mode;
                            pthreadCreate(&tid[j], NULL, mulPortNetTransferP, (void *)&transf[j]); // Create thread to send slice
                        }
                        for (int j = 0; j < mulPortNetMeta->eachStripeFailNodeNum[curStripe]; j++)
                            pthreadJoin(tid[j], NULL);

                        for (int j = 0; j < mulPortNetMeta->eachStripeFailNodeNum[curStripe]; j++)
                        {
                            int curOffset = mulPortNetMeta->eachStripeFailNodeIndex[curStripe][j] - MUL_FAIL_START;
                            /* curSlice =  curStripe * (CHUNK_SIZE / SLICE_SIZE) + curSliceTmp */
                            int actual_cur_slice = map_stripe2actual[curOffset][curStripe] * (CHUNK_SIZE / SLICE_SIZE) + curSliceTmp;
                            slice_loc[curOffset][actual_cur_slice] = slice_send_conunt[curOffset]; // it is totally different for d repair,where p_slice_loc[loc]=send_count, d is p_slice_loc[send_count]=loc
                            slice_send_conunt[curOffset]++;
                        }
                        break;
                    }
                curSliceNum++;
                PRINT_FLUSH;
            }
        }
        PRINT_FLUSH;
        for (int curOffset = 0; curOffset < MUL_FAIL_NUM; curOffset++)
            Send(tmpSockfd[curOffset][0][0], slice_loc[curOffset], sizeof(int) * fullRecoverNetRecordN->stripeNum * (CHUNK_SIZE / SLICE_SIZE), "send slice loc");

        for (int i = 0; i < MUL_FAIL_NUM; i++)
            free(slice_loc[i]);
        free(slice_loc);
        delete3ArrINT(tmpSockfdBuf, mulPortMulNodeSendTmpSockfd, tmpSockfd);
        free2(slice_tmp, data_tmp);
    }
    else
        ERROR_RETURN_NULL("error mulPortNetMeta->mode");
    return NULL;
}
