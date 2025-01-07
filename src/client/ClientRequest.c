#include "ClientRequest.h"
#include "PackedData.h"

/* from ClientMain.c */
extern int FailNodeIndex;
extern int stripeNum;
extern int sockfdArr[EC_A_MAX + 1];
extern char dstFailFileName[MAX_PATH_LEN];
extern char dstHealFileName[MAX_PATH_LEN];
extern int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM];
extern int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M];
extern hdfs_info_t *HDFSInfo;

/* mul use variables */
static netMeta_t netMetaArr[EC_A + 1];
static pthread_t clientNetTid[EC_A + 1];
static int bufSize;
static char *netDataBuf;
static int curOffset;

static void getIncreasingNodesIndexArr(int selectNodesIndexArr[][EC_N], int increasingNodesIndexArr[][EC_K], int stripeNum, int failNodeNum)
{
    for (int i = 0; i < stripeNum; i++)
    {
        for (int j = 0; j < EC_K; j++)
            increasingNodesIndexArr[i][j] = selectNodesIndexArr[i][j + failNodeNum];
        sortMinBubble(increasingNodesIndexArr[i], EC_K);
    }
}

static void getIncreasingNodesIndexArrN(int selectNodesIndexArr[][EC_N + MUL_FAIL_NUM - 1], int increasingNodesIndexArr[][EC_N - 1], int stripeNum, int failNodeNum)
{
    for (int i = 0; i < stripeNum; i++)
    {
        int count = 0;
        for (int j = 0; j < EC_N - 1; j++)
        {
            increasingNodesIndexArr[i][j] = selectNodesIndexArr[i][j + failNodeNum]; // for mulFullRecover,  increasing has -1
            if (selectNodesIndexArr[i][j + failNodeNum] != -1)
                count++;
        }
        sortMinBubble(increasingNodesIndexArr[i], count);
    }
}
static void netMetaArrInit(int i, int expCount, int mulFailFlag)
{
    netMetaArr[i].size = curOffset;
    netMetaArr[i].data = netDataBuf;
    netMetaArr[i].expCount = expCount;
    netMetaArr[i].sockfd = sockfdArr[i];
    memset(netMetaArr[i].dstFileName, 0, sizeof(netMetaArr[i].dstFileName));
    netMetaArr[i].mulFailFlag = mulFailFlag;
    netMetaArr[i].dstNodeIndex = FailNodeIndex;
}

void getDstFileNameDstNode(netMeta_t *netMeta, char *origin, int nodeIndex)
{
    if (!HDFS_FLAG)
        sprintf(netMeta->dstFileName, "%s_%d", origin, nodeIndex + 1);
    else
    {
        char tmp[MAX_PATH_LEN] = {0}, chunkIndex = HDFSInfo->nodeIndexToChunkIndex[nodeIndex];
        getHDFSFilename(HDFSInfo->bpName, HDFSInfo->bName[HDFSInfo->failBp][chunkIndex], tmp);
        sprintf(netMeta->dstFileName, "%s", tmp);
    }
}

int clientRequestReadKChunks(char **dataChunk, char *dstFileName, int healChunkIndex[])
{
    /* Recv to k storages that saved data chunk */
    for (int i = 0; i < EC_K; i++)
    {
        int j = healChunkIndex[i];
        netMetaArr[i].sockfd = sockfdArr[j];
        netMetaArr[i].cmdType = 1;
        netMetaArr[i].data = dataChunk[i];
        sprintf(netMetaArr[i].dstFileName, "%s_%d", dstFileName, j + 1);
        pthreadCreate(&clientNetTid[i], NULL, RecvNetMetaChunk, (void *)&netMetaArr[i]);
    }
    for (int i = 0; i < EC_K; i++)
        pthreadJoin(clientNetTid[i], NULL);
    return EC_OK;
}

int clientRequestWriteNChunks(char **dataChunk, char **codingChunk, char *dstFileName, int *sockfdArr)
{
    /* Send to each nodes */
    for (int i = 0; i < EC_N; i++)
    {
        netMetaArr[i].sockfd = sockfdArr[i];
        netMetaArr[i].cmdType = 0;
        netMetaArr[i].data = i < EC_K ? dataChunk[i] : codingChunk[i - EC_K];
        sprintf(netMetaArr[i].dstFileName, "%s_%d", dstFileName, i + 1);
        pthreadCreate(&clientNetTid[i], NULL, SendNetMetaChunk, (void *)&netMetaArr[i]);
    }
    for (int i = 0; i < EC_N; i++)
        pthreadJoin(clientNetTid[i], NULL);
    return EC_OK;
}

int clientRequestDegradedReadT(int selectNodesIndex[], unsigned char *g_tbls, int expCount)
{
    /* Send to each nodes */
    for (int i = 0; i < EC_K + 1; i++)
    {
        netMetaArrInit(i, expCount, 0);
        int tmpNode = selectNodesIndex[i];
        if (tmpNode == FailNodeIndex)
        {
            netMetaArr[i].cmdType = 2;
            netMetaArr[i].size = EC_K * 32;
            netMetaArr[i].data = (char *)g_tbls;
            getDstFileNameDstNode(&netMetaArr[i], dstFailFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
        }
        else
        {
            netMetaArr[i].cmdType = 3;
            getDstFileNameDstNode(&netMetaArr[i], dstHealFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMeta, (void *)&netMetaArr[i]);
        }
    }
    for (int i = 0; i < EC_K + 1; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}

int clientRequestDegradedReadR(int selectNodesIndex[], unsigned char **g_tblsArr, int expCount)
{
    int increasingNodesIndexArr[EC_K];
    for (int i = 0; i < EC_K; i++)
        increasingNodesIndexArr[i] = selectNodesIndex[i + 1];
    sortMinBubble(increasingNodesIndexArr, EC_K);
    /* Send to each nodes */
    for (int i = 0; i < EC_K + 1; i++)
    {
        netMetaArrInit(i, expCount, 0);
        int tmpNode = selectNodesIndex[i];
        if (tmpNode == FailNodeIndex)
        {
            netMetaArr[i].cmdType = 4;
            getDstFileNameDstNode(&netMetaArr[i], dstFailFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMeta, (void *)&netMetaArr[i]);
        }
        else
        {
            netMetaArr[i].cmdType = 5;
            int index = i;
            netMetaArr[i].dstNodeIndex = selectNodesIndex[index - 1];
            if (index == 0)
                netMetaArr[i].dstNodeIndex = -1;
            else
                netMetaArr[i].dstNodeIndex = selectNodesIndex[index - 1];
            if (index == EC_K)
                netMetaArr[i].srcNodeIndex = -1;
            else
                netMetaArr[i].srcNodeIndex = selectNodesIndex[index + 1];
            netMetaArr[i].size = 2 * 32;
            netMetaArr[i].data = (char *)g_tblsArr[getArrIndex(increasingNodesIndexArr, EC_K, tmpNode)]; // sort by increasing node index
            getDstFileNameDstNode(&netMetaArr[i], dstHealFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
        }
    }

    for (int i = 0; i < EC_K + 1; i++)
        pthreadJoin(clientNetTid[i], NULL);

    return EC_OK;
}

int clientRequestDegradedReadE(int selectNodesIndex[], unsigned char *g_tbls, int expCount)
{
    /* Send to each nodes */
    for (int i = 0; i < EC_K + 2; i++)
    {
        netMetaArrInit(i, expCount, 0);
        int tmpNode = selectNodesIndex[i];
        if (i == EC_K + 1) // PND
        {
            netMetaArr[i].cmdType = 6; // or 7
            netMetaArr[i].size = EC_K * 32;
            netMetaArr[i].data = (char *)g_tbls;
            getDstFileNameDstNode(&netMetaArr[i], dstFailFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
            continue;
        }
        else if (tmpNode == FailNodeIndex)
        {
            netMetaArr[i].cmdType = 6;
            getDstFileNameDstNode(&netMetaArr[i], dstFailFileName, tmpNode);
        }
        else
        {
            netMetaArr[i].cmdType = 7;
            getDstFileNameDstNode(&netMetaArr[i], dstHealFileName, tmpNode);
        }
        pthreadCreate(&clientNetTid[i], NULL, SendNetMeta, (void *)&netMetaArr[i]);
    }

    for (int i = 0; i < EC_K + 2; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}

int clientRequestDegradedReadN(int selectNodesIndex[], int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum, unsigned char **g_tblsArr, int expCount)
{
    curOffset = packedDataDegradedReadN(&netDataBuf, twoBucketsSliceIndexInfo, divisionLines, divisionNum, g_tblsArr[0]);

    /* Send to each nodes */
    int count = 0;
    for (int i = 0; i < EC_N + 1; i++)
    {
        netMetaArrInit(i, expCount, 0);
        int tmpNode;
        if (i != EC_N)
            tmpNode = selectNodesIndex[i];

        if (i == EC_N) // PND
        {
            netMetaArr[i].cmdType = 8; // or 9
            pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
        }
        else if (tmpNode == FailNodeIndex)
        {
            netMetaArr[i].cmdType = 8;
            getDstFileNameDstNode(&netMetaArr[i], dstFailFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMeta, (void *)&netMetaArr[i]);
        }
        else
        {
            netMetaArr[i].cmdType = 9;
            netMetaArr[i].size = 6 * sizeof(int);
            netMetaArr[i].data = twoBucketsSliceIndexInfo[count++];
            getDstFileNameDstNode(&netMetaArr[i], dstHealFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
        }
    }

    for (int i = 0; i < EC_N + 1; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}

int clientRequestFullRecoverT(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N], int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM],
                              unsigned char **g_tblsArr, int expCount)
{

    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K]; // When e, selectNodesIndexArr is out of order
    getIncreasingNodesIndexArr(selectNodesIndexArr, increasingNodesIndexArr, stripeNum, 1);
    curOffset = packedDataFullRecoverTE(&netDataBuf, increasingNodesIndexArr, eachNodeInfo, stripeNum, HDFSInfo->fullRecoverFailBlockName, g_tblsArr[0]);

    /* Send to each nodes */
    for (int i = 0; i < EC_A; i++)
    {
        netMetaArrInit(i, expCount, 0);
        int tmpNode = i;
        if (tmpNode == FailNodeIndex)
        {
            netMetaArr[i].cmdType = 10;
            sprintf(netMetaArr[i].dstFileName, "%s", dstFailFileName); // filename on nodes
        }
        else
        {
            netMetaArr[i].cmdType = 11;
            sprintf(netMetaArr[i].dstFileName, "%s", dstHealFileName); // filename on nodes
        }
        pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
    }

    for (int i = 0; i < EC_A; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}

int clientRequestFullRecoverR(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char ***g_tbls2Arr, int expCount)
{
    /* get dstNodeIndex and srcNodeIndex arr*/
    int dstNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, srcNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    for (int i = 0; i < EC_A; i++)
        for (int j = 0; j < stripeNum; j++)
        {
            int tmpNode = i;
            int index = getArrIndex(selectNodesIndexArr[j], EC_K + 1, tmpNode);

            if (index == 0 || index == -1)
                dstNodeArr[i][j] = -1;
            else
                dstNodeArr[i][j] = selectNodesIndexArr[j][index - 1];
            if (index == EC_K || index == -1)
                srcNodeArr[i][j] = -1;
            else
                srcNodeArr[i][j] = selectNodesIndexArr[j][index + 1];
        }
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K]; // When e, selectNodesIndexArr is out of order
    getIncreasingNodesIndexArr(selectNodesIndexArr, increasingNodesIndexArr, stripeNum, 1);
    curOffset = packedDataFullRecoverR(&netDataBuf, increasingNodesIndexArr, eachNodeInfo, stripeNum, dstNodeArr, srcNodeArr, HDFSInfo->fullRecoverFailBlockName, g_tbls2Arr[0][0]);

    /* Send to each nodes */
    for (int i = 0; i < EC_A; i++)
    {
        netMetaArrInit(i, expCount, 0);
        int tmpNode = i;
        if (tmpNode == FailNodeIndex)
        {
            netMetaArr[i].cmdType = 12;
            sprintf(netMetaArr[i].dstFileName, "%s", dstFailFileName); // filename on nodes
        }
        else
        {
            netMetaArr[i].cmdType = 13;
            sprintf(netMetaArr[i].dstFileName, "%s", dstHealFileName); // filename on nodes
        }
        pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
    }

    for (int i = 0; i < EC_A; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}

int clientRequestFullRecoverE(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char **g_tblsArr, int expCount)
{
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K]; // When e, selectNodesIndexArr is out of order
    getIncreasingNodesIndexArr(selectNodesIndexArr, increasingNodesIndexArr, stripeNum, 1);
    curOffset = packedDataFullRecoverTE(&netDataBuf, increasingNodesIndexArr, eachNodeInfo, stripeNum, HDFSInfo->fullRecoverFailBlockName, g_tblsArr[0]);
    /* Send to each nodes */
    for (int i = 0; i < EC_A + 1; i++)
    {
        netMetaArrInit(i, expCount, 0);
        int tmpNode = i;
        if (tmpNode == EC_A)
            netMetaArr[i].cmdType = 14; // or 15
        else if (tmpNode == FailNodeIndex)
        {
            netMetaArr[i].cmdType = 14;
            sprintf(netMetaArr[i].dstFileName, "%s", dstFailFileName); // filename on nodes
        }
        else
        {
            netMetaArr[i].cmdType = 15;
            sprintf(netMetaArr[i].dstFileName, "%s", dstHealFileName); // filename on nodes
        }
        pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
    }

    for (int i = 0; i < EC_A + 1; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}

int clientRequestFullRecoverN(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N + MUL_FAIL_NUM - 1],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int maxDivisionNum,
                              int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionLines[][2 * EC_N], int divisionNum[],
                              unsigned char ***g_tbls2Arr, int expCount)
{
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N - 1]; // When e, selectNodesIndexArr is out of order
    getIncreasingNodesIndexArrN(selectNodesIndexArr, increasingNodesIndexArr, stripeNum, 1);
    curOffset = packedDataFullRecoverN(&netDataBuf, increasingNodesIndexArr, eachNodeInfo, stripeNum, maxDivisionNum,
                                       twoBucketsSliceIndexInfo, divisionLines, divisionNum, HDFSInfo->fullRecoverFailBlockName, g_tbls2Arr[0][0]);
    /* Send to each nodes */
    for (int i = 0; i < EC_A + 1; i++)
    {
        netMetaArrInit(i, expCount, 0);
        int tmpNode = i;
        if (tmpNode == EC_A)
            netMetaArr[i].cmdType = 16; // or 15
        else if (tmpNode == FailNodeIndex)
        {
            netMetaArr[i].cmdType = 16;
            sprintf(netMetaArr[i].dstFileName, "%s", dstFailFileName); // filename on nodes
        }
        else
        {
            netMetaArr[i].cmdType = 17;
            sprintf(netMetaArr[i].dstFileName, "%s", dstHealFileName); // filename on nodes
        }
        pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
    }
    for (int i = 0; i < EC_A + 1; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}

int clientRequestMulDegradedReadT(int selectNodesIndex[], unsigned char **g_tblsArr, int expCount)
{
    /* Send to each nodes */
    for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
    {
        netMetaArrInit(i, expCount, 1);
        int tmpNode = selectNodesIndex[i];
        if (tmpNode < MUL_FAIL_START || tmpNode > MUL_FAIL_START + MUL_FAIL_NUM - 1)
        {
            netMetaArr[i].cmdType = 3;
            getDstFileNameDstNode(&netMetaArr[i], dstHealFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMeta, (void *)&netMetaArr[i]);
        }
        else
        {
            netMetaArr[i].cmdType = 2;
            netMetaArr[i].size = EC_K * 32;
            netMetaArr[i].data = (char *)g_tblsArr[tmpNode - MUL_FAIL_START];
            getDstFileNameDstNode(&netMetaArr[i], dstFailFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
            continue;
        }
    }

    for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
        pthreadJoin(clientNetTid[i], NULL);
    return EC_OK;
}

int clientRequestMulDegradedReadR(int selectNodesIndex[], unsigned char ***g_tbls2Arr, int expCount)
{

    int selectNodesIndexTmp[EC_K + 1], increasingNodesIndexArr[EC_K], count = 0;
    for (int i = 0; i < EC_K + 1; i++)
        selectNodesIndexTmp[i] = selectNodesIndex[i + MUL_FAIL_NUM - 1]; // selectNodesIndex[ MUL_FAIL_NUM - 1] is FailNodeIndex/last node, selectNodesIndex[k] is first node
    for (int i = 0; i < EC_K; i++)
        increasingNodesIndexArr[i] = selectNodesIndex[i + MUL_FAIL_NUM];
    sortMinBubble(increasingNodesIndexArr, EC_K);

    unsigned char *g_tblsTmpBuf = NULL, **g_tblsTmpArr = NULL;
    create2PointerArrUC(&g_tblsTmpBuf, &g_tblsTmpArr, EC_K, MUL_FAIL_NUM * 2 * 1 * 32);
    /* Send to each nodes */
    for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
    {
        netMetaArrInit(i, expCount, 1);
        int tmpNode = selectNodesIndex[i];
        if (tmpNode < MUL_FAIL_START || tmpNode > MUL_FAIL_START + MUL_FAIL_NUM - 1)
        {
            netMetaArr[i].cmdType = 5;
            int index = getArrIndex(selectNodesIndexTmp, EC_K + 1, tmpNode);
            if (index == 0)
                netMetaArr[i].dstNodeIndex = -1;
            else
                netMetaArr[i].dstNodeIndex = selectNodesIndexTmp[index - 1];
            if (index == EC_K)
                netMetaArr[i].srcNodeIndex = -1;
            else
                netMetaArr[i].srcNodeIndex = selectNodesIndexTmp[index + 1];

            netMetaArr[i].size = 2 * 32;
            for (int j = 0; j < MUL_FAIL_NUM; j++)
                memcpy(g_tblsTmpArr[count], g_tbls2Arr[j][getArrIndex(increasingNodesIndexArr, EC_K, tmpNode)], sizeof(char) * 2 * 32);
            netMetaArr[i].data = (char *)g_tblsTmpArr[count++];
            netMetaArr[i].size = sizeof(char) * MUL_FAIL_NUM * 2 * 1 * 32;
            getDstFileNameDstNode(&netMetaArr[i], dstHealFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
        }
        else
        {
            netMetaArr[i].cmdType = 4;
            getDstFileNameDstNode(&netMetaArr[i], dstFailFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMeta, (void *)&netMetaArr[i]);
        }
    }
    for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
        pthreadJoin(clientNetTid[i], NULL);
    delete2ArrUC(g_tblsTmpBuf, g_tblsTmpArr);
    return EC_OK;
}

int clientRequestMulDegradedReadE(int selectNodesIndex[], unsigned char **g_tblsArr, int expCount)
{
    /* Send to each nodes */
    for (int i = 0; i < EC_K + MUL_FAIL_NUM + 1; i++)
    {
        netMetaArrInit(i, expCount, 1);
        if (i == EC_K + MUL_FAIL_NUM) // PND
        {
            netMetaArr[i].cmdType = 6; // or 7
            netMetaArr[i].size = MUL_FAIL_NUM * EC_K * 32;
            netMetaArr[i].data = (char *)g_tblsArr[0];
            pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
            continue;
        }
        int tmpNode = selectNodesIndex[i];
        if (tmpNode < MUL_FAIL_START || tmpNode > MUL_FAIL_START + MUL_FAIL_NUM - 1)
        {
            netMetaArr[i].cmdType = 7;
            getDstFileNameDstNode(&netMetaArr[i], dstHealFileName, tmpNode);
        }
        else
        {
            netMetaArr[i].cmdType = 6;
            getDstFileNameDstNode(&netMetaArr[i], dstFailFileName, tmpNode);
        }
        pthreadCreate(&clientNetTid[i], NULL, SendNetMeta, (void *)&netMetaArr[i]);
    }

    for (int i = 0; i < EC_K + MUL_FAIL_NUM + 1; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}

int clientRequestMulDegradedReadN(int selectNodesIndex[], int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum, unsigned char ***g_tbls2Arr, int expCount)
{
    int count = 0;
    curOffset = packedDataMulDegradedReadN(&netDataBuf, twoBucketsSliceIndexInfo, divisionLines, divisionNum, g_tbls2Arr[0][0]);

    /* Send to each nodes */
    for (int i = 0; i < EC_N + 1; i++)
    {
        netMetaArrInit(i, expCount, 1);
        int tmpNode;
        if (i != EC_N)
            tmpNode = selectNodesIndex[i];
        if (i == EC_N) // PND
        {
            netMetaArr[i].cmdType = 8; // or 9
            pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
        }
        else if (tmpNode < MUL_FAIL_START || tmpNode > MUL_FAIL_START + MUL_FAIL_NUM - 1)
        {
            netMetaArr[i].cmdType = 9;
            netMetaArr[i].size = 6 * sizeof(int);
            netMetaArr[i].data = twoBucketsSliceIndexInfo[count++];
            getDstFileNameDstNode(&netMetaArr[i], dstHealFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
        }
        else
        {
            netMetaArr[i].cmdType = 8;
            getDstFileNameDstNode(&netMetaArr[i], dstFailFileName, tmpNode);
            pthreadCreate(&clientNetTid[i], NULL, SendNetMeta, (void *)&netMetaArr[i]);
        }
    }

    for (int i = 0; i < EC_N + 1; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}

int clientRequestMulFullRecoverT(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char ***g_tbls2Arr, int expCount)
{
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K]; // When e, selectNodesIndexArr is out of order
    getIncreasingNodesIndexArr(selectNodesIndexArr, increasingNodesIndexArr, stripeNum, MUL_FAIL_NUM);
    curOffset = packedDataMulFullRecoverT(&netDataBuf, increasingNodesIndexArr, eachNodeInfo, stripeNum, eachStripeFailNodeNum, eachStripeFailNodeIndex, g_tbls2Arr[0][0]);
    /* Send to each nodes */
    for (int i = 0; i < EC_A; i++)
    {
        netMetaArrInit(i, expCount, 1);
        int tmpNode = i;
        if (tmpNode < MUL_FAIL_START || tmpNode > MUL_FAIL_START + MUL_FAIL_NUM - 1)
        {
            netMetaArr[i].cmdType = 11;
            sprintf(netMetaArr[i].dstFileName, "%s", dstHealFileName); // filename on nodes
        }
        else
        {
            netMetaArr[i].cmdType = 10;
            sprintf(netMetaArr[i].dstFileName, "%s", dstFailFileName); // filename on nodes
        }
        pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
    }

    for (int i = 0; i < EC_A; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}

int clientRequestMulFullRecoverR(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char ****g_tbls3Arr, int expCount)
{
    int selectNodesIndexTmp[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K + MUL_FAIL_NUM], nodeNum[FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    for (int i = 0; i < stripeNum; i++)
        for (int j = 0; j < EC_K + MUL_FAIL_NUM; j++)
            if (selectNodesIndexArr[i][j] != -1)
                selectNodesIndexTmp[i][nodeNum[i]++] = selectNodesIndexArr[i][j];

    /* get dstNodeIndex and srcNodeIndex arr*/
    int dstNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM] = {0}, srcNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM] = {0};
    for (int j = 0; j < stripeNum; j++)
        for (int i = 0; i < EC_A; i++)
        {
            int tmpNode = i;
            int index = getArrIndex(selectNodesIndexTmp[j], nodeNum[j], tmpNode);
            if (index >= 0 && index < nodeNum[j] - EC_K) // failNodeIndex
            {
                dstNodeArr[i][j] = -1;
                srcNodeArr[i][j] = selectNodesIndexTmp[j][nodeNum[j] - EC_K];
            }
            else if (index == nodeNum[j] - 1) // last heal_node
            {
                dstNodeArr[i][j] = selectNodesIndexTmp[j][index - 1];
                srcNodeArr[i][j] = -1;
            }
            else if (index == -1) // other_node： no exist for the stripe
            {
                dstNodeArr[i][j] = -1;
                srcNodeArr[i][j] = -1;
            }
            else
            {
                if (selectNodesIndexTmp[j][index - 1] < MUL_FAIL_START || selectNodesIndexTmp[j][index - 1] > MUL_FAIL_START + MUL_FAIL_NUM - 1) // first heal node
                    dstNodeArr[i][j] = selectNodesIndexTmp[j][index - 1];                                                                        // other heal_node
                else
                    dstNodeArr[i][j] = MUL_FAIL_START; // all failNodeIndex
                srcNodeArr[i][j] = selectNodesIndexTmp[j][index + 1];
            }
        }

    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K]; // When e, selectNodesIndexArr is out of order
    getIncreasingNodesIndexArr(selectNodesIndexArr, increasingNodesIndexArr, stripeNum, MUL_FAIL_NUM);
    curOffset = packedDataMulFullRecoverR(&netDataBuf, increasingNodesIndexArr, eachNodeInfo, stripeNum, dstNodeArr, srcNodeArr,
                                          eachStripeFailNodeNum, eachStripeFailNodeIndex, g_tbls3Arr[0][0][0]);

    /* Send to each nodes */
    for (int i = 0; i < EC_A; i++)
    {
        netMetaArrInit(i, expCount, 1);
        int tmpNode = i;
        if (tmpNode < MUL_FAIL_START || tmpNode > MUL_FAIL_START + MUL_FAIL_NUM - 1)
        {
            netMetaArr[i].cmdType = 13;
            sprintf(netMetaArr[i].dstFileName, "%s", dstHealFileName); // filename on nodes
        }
        else
        {
            netMetaArr[i].cmdType = 12;
            sprintf(netMetaArr[i].dstFileName, "%s", dstFailFileName); // filename on nodes
        }
        pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
    }

    for (int i = 0; i < EC_A; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}

int clientRequestMulFullRecoverE(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char ***g_tbls2Arr, int expCount)
{
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_K]; // When e, selectNodesIndexArr is out of order
    getIncreasingNodesIndexArr(selectNodesIndexArr, increasingNodesIndexArr, stripeNum, MUL_FAIL_NUM);
    curOffset = packedDataMulFullRecoverE(&netDataBuf, increasingNodesIndexArr, eachNodeInfo, stripeNum, eachStripeFailNodeNum, eachStripeFailNodeIndex, g_tbls2Arr[0][0]);

    /* Send to each nodes */
    for (int i = 0; i < EC_A + 1; i++)
    {
        netMetaArrInit(i, expCount, 1);
        int tmpNode = i;
        if (tmpNode == EC_A)
            netMetaArr[i].cmdType = 14; // or 15
        else if (tmpNode < MUL_FAIL_START || tmpNode > MUL_FAIL_START + MUL_FAIL_NUM - 1)
        {
            netMetaArr[i].cmdType = 15;
            sprintf(netMetaArr[i].dstFileName, "%s", dstHealFileName); // filename on nodes
        }
        else
        {
            netMetaArr[i].cmdType = 14;
            sprintf(netMetaArr[i].dstFileName, "%s", dstFailFileName); // filename on nodes
        }
        pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
    }

    for (int i = 0; i < EC_A + 1; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}

int clientRequestMulFullRecoverN(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N + MUL_FAIL_NUM - 1],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int maxDivisionNum,
                                 int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionLines[][2 * EC_N], int divisionNum[],
                                 unsigned char ****g_tbls3Arr, int expCount)
{
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N - 1]; // When e, selectNodesIndexArr is out of order
    getIncreasingNodesIndexArrN(selectNodesIndexArr, increasingNodesIndexArr, stripeNum, MUL_FAIL_NUM);
    curOffset = packedDataMulFullRecoverN(&netDataBuf, increasingNodesIndexArr, eachNodeInfo, stripeNum,
                                          maxDivisionNum, twoBucketsSliceIndexInfo, divisionLines, divisionNum, eachStripeFailNodeNum, eachStripeFailNodeIndex, g_tbls3Arr[0][0][0]);
    /* Send to each nodes */
    for (int i = 0; i < EC_A + 1; i++)
    {
        netMetaArrInit(i, expCount, 1);
        int tmpNode = i;
        if (tmpNode == EC_A)
            netMetaArr[i].cmdType = 16; // or 15
        else if (tmpNode < MUL_FAIL_START || tmpNode > MUL_FAIL_START + MUL_FAIL_NUM - 1)
        {
            netMetaArr[i].cmdType = 17;
            sprintf(netMetaArr[i].dstFileName, "%s", dstHealFileName); // filename on nodes
        }
        else
        {
            netMetaArr[i].cmdType = 16;
            sprintf(netMetaArr[i].dstFileName, "%s", dstFailFileName); // filename on nodes
        }
        pthreadCreate(&clientNetTid[i], NULL, SendNetMetaData, (void *)&netMetaArr[i]);
    }

    for (int i = 0; i < EC_A + 1; i++)
        pthreadJoin(clientNetTid[i], NULL);
    free(netDataBuf);
    return EC_OK;
}