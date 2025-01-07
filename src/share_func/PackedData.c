#include "PackedData.h"

int packedNetDataBuf(char **netDataBufP, requestData_t rqData)
{
    /* bufSize */
    int countCell = 0, bufSize = 0, curOffset = 0;
    for (; countCell < rqData.rqNum.intNum; countCell++)
        bufSize += ((rqCell_t *)&rqData + countCell)->size * sizeof(int);
    for (; countCell < rqData.rqNum.intNum + rqData.rqNum.charNum; countCell++)
        bufSize += ((rqCell_t *)&rqData + countCell)->size * sizeof(char);
    (*netDataBufP) = (char *)Malloc(bufSize);

    /* copy data */
    countCell = 0;
    for (; countCell < rqData.rqNum.intNum; countCell++)
        memcpyMove((*netDataBufP) + curOffset, ((rqCell_t *)&rqData + countCell)->data, ((rqCell_t *)&rqData + countCell)->size * sizeof(int), &curOffset);
    for (; countCell < rqData.rqNum.intNum + rqData.rqNum.charNum; countCell++)
        memcpyMove((*netDataBufP) + curOffset, ((rqCell_t *)&rqData + countCell)->data, ((rqCell_t *)&rqData + countCell)->size * sizeof(char), &curOffset);

    return curOffset;
}

int unpackedNetDataBuf(char *netDataBuf, requestData_t rqData)
{
    /* copy data */
    int countCell = 0, curOffset = 0;
    for (; countCell < rqData.rqNum.intNum; countCell++)
        memcpyMove(((rqCell_t *)&rqData + countCell)->data, netDataBuf + curOffset, ((rqCell_t *)&rqData + countCell)->size * sizeof(int), &curOffset);
    for (; countCell < rqData.rqNum.intNum + rqData.rqNum.charNum; countCell++)
        memcpyMove(((rqCell_t *)&rqData + countCell)->data, netDataBuf + curOffset, ((rqCell_t *)&rqData + countCell)->size * sizeof(char), &curOffset);
    return curOffset;
}

int packedDataDegradedReadN(char **netDataBufP, int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum, unsigned char *g_tbls)
{
    requestData_t rqData;

    rqData.data1.data = twoBucketsSliceIndexInfo;
    rqData.data1.size = (EC_N - 1) * 6;
    rqData.data2.data = divisionLines;
    rqData.data2.size = 2 * EC_N;
    rqData.data3.data = &divisionNum;
    rqData.data3.size = 1;
    rqData.data4.data = g_tbls;
    rqData.data4.size = divisionNum * EC_K * 32;
    rqData.rqNum.intNum = 3;
    rqData.rqNum.charNum = 1;

    return packedNetDataBuf(netDataBufP, rqData);
}

int unpackedDataDegradedReadN1(char *netDataBuf, int twoBucketsSliceIndexInfo[EC_N - 1][6], int divisionLines[2 * EC_N], int *divisionNumP)
{
    requestData_t rqData;
    rqData.data1.data = twoBucketsSliceIndexInfo;
    rqData.data1.size = (EC_N - 1) * 6;
    rqData.data2.data = divisionLines;
    rqData.data2.size = 2 * EC_N;
    rqData.data3.data = divisionNumP;
    rqData.data3.size = 1;
    rqData.rqNum.intNum = 3;
    rqData.rqNum.charNum = 0;

    return unpackedNetDataBuf(netDataBuf, rqData);
}

int packedDataFullRecoverTE(char **netDataBufP, int increasingNodesIndexArr[][EC_K],
                            int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum,
                            char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN], unsigned char *g_tbls)
{
    requestData_t rqData;
    rqData.data1.data = increasingNodesIndexArr;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_K;
    rqData.data2.data = eachNodeInfo;
    rqData.data2.size = 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data3.data = &stripeNum;
    rqData.data3.size = 1;
    rqData.data4.data = fullRecoverFailBlockName;
    rqData.data4.size = EC_A * HDFS_MAX_BP_COUNT * MAX_PATH_LEN;
    rqData.data5.data = g_tbls;
    rqData.data5.size = stripeNum * EC_K * 32;
    rqData.rqNum.intNum = 3;
    rqData.rqNum.charNum = 2;
    if (!HDFS_FLAG)
    {
        rqData.data4.data = g_tbls;
        rqData.data4.size = stripeNum * EC_K * 32;
        rqData.rqNum.charNum = 1;
    }
    return packedNetDataBuf(netDataBufP, rqData);
}

int unpackedDataFullRecoverTE1(char *netDataBuf, int increasingNodesIndexArr[][EC_K],
                               int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int *stripeNumP,
                               char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN])
{
    requestData_t rqData;
    rqData.data1.data = increasingNodesIndexArr;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_K;
    rqData.data2.data = eachNodeInfo;
    rqData.data2.size = 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data3.data = stripeNumP;
    rqData.data3.size = 1;
    rqData.data4.data = fullRecoverFailBlockName;
    rqData.data4.size = EC_A * HDFS_MAX_BP_COUNT * MAX_PATH_LEN;
    rqData.rqNum.intNum = 3;
    rqData.rqNum.charNum = 1;
    if (!HDFS_FLAG)
        rqData.rqNum.charNum = 0;
    return unpackedNetDataBuf(netDataBuf, rqData);
}

int packedDataFullRecoverR(char **netDataBufP, int increasingNodesIndexArr[][EC_K], int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum,
                           int dstNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int srcNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM],
                           char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN], unsigned char *g_tbls)
{
    requestData_t rqData;
    rqData.data1.data = increasingNodesIndexArr;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_K;
    rqData.data2.data = eachNodeInfo;
    rqData.data2.size = 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data3.data = &stripeNum;
    rqData.data3.size = 1;

    rqData.data4.data = dstNodeArr;
    rqData.data4.size = EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data5.data = srcNodeArr;
    rqData.data5.size = EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;

    rqData.data6.data = fullRecoverFailBlockName;
    rqData.data6.size = EC_A * HDFS_MAX_BP_COUNT * MAX_PATH_LEN;
    rqData.data7.data = g_tbls;
    rqData.data7.size = stripeNum * EC_K * 2 * 32;
    rqData.rqNum.intNum = 5;
    rqData.rqNum.charNum = 2;
    if (!HDFS_FLAG)
    {
        rqData.data6.data = g_tbls;
        rqData.data6.size = stripeNum * EC_K * 2 * 32;
        rqData.rqNum.charNum = 1;
    }
    return packedNetDataBuf(netDataBufP, rqData);
}

int unpackedDataFullRecoverR1(char *netDataBuf, int increasingNodesIndexArr[][EC_K], int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM],
                              int *stripeNumP, int dstNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int srcNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM],
                              char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN])
{

    requestData_t rqData;
    rqData.data1.data = increasingNodesIndexArr;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_K;
    rqData.data2.data = eachNodeInfo;
    rqData.data2.size = 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data3.data = stripeNumP;
    rqData.data3.size = 1;

    rqData.data4.data = dstNodeArr;
    rqData.data4.size = EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data5.data = srcNodeArr;
    rqData.data5.size = EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;

    rqData.data6.data = fullRecoverFailBlockName;
    rqData.data6.size = EC_A * HDFS_MAX_BP_COUNT * MAX_PATH_LEN;
    rqData.rqNum.intNum = 5;
    rqData.rqNum.charNum = 1;
    if (!HDFS_FLAG)
        rqData.rqNum.charNum = 0;
    return unpackedNetDataBuf(netDataBuf, rqData);
}

int packedDataFullRecoverN(char **netDataBufP, int increasingNodesIndexArr[][EC_N - 1],
                           int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum, int maxDivisionNum,
                           int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionLines[][2 * EC_N], int divisionNum[],
                           char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN], unsigned char *g_tbls)
{

    requestData_t rqData;
    rqData.data1.data = increasingNodesIndexArr;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM * (EC_N - 1);
    rqData.data2.data = eachNodeInfo;
    rqData.data2.size = 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data3.data = &stripeNum;
    rqData.data3.size = 1;

    rqData.data4.data = &maxDivisionNum;
    rqData.data4.size = 1;
    rqData.data5.data = twoBucketsSliceIndexInfo;
    rqData.data5.size = stripeNum * (EC_N - 1) * 6;

    rqData.data6.data = divisionLines;
    rqData.data6.size = stripeNum * 2 * EC_N;

    rqData.data7.data = divisionNum;
    rqData.data7.size = stripeNum;

    rqData.data8.data = fullRecoverFailBlockName;
    rqData.data8.size = EC_A * HDFS_MAX_BP_COUNT * MAX_PATH_LEN;
    rqData.data9.data = g_tbls;
    rqData.data9.size = stripeNum * maxDivisionNum * 1 * EC_K * 32;

    rqData.rqNum.intNum = 7;
    rqData.rqNum.charNum = 2;
    if (!HDFS_FLAG)
    {
        rqData.data8.data = g_tbls;
        rqData.data8.size = stripeNum * maxDivisionNum * 1 * EC_K * 32;
        rqData.rqNum.charNum = 1;
    }
    return packedNetDataBuf(netDataBufP, rqData);
}

int unpackedDataFullRecoverN1(char *netDataBuf, int increasingNodesIndexArr[][EC_N - 1],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int *stripeNumP, int *maxDivisionNumP)
{
    requestData_t rqData;
    rqData.data1.data = increasingNodesIndexArr;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM * (EC_N - 1);
    rqData.data2.data = eachNodeInfo;
    rqData.data2.size = 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data3.data = stripeNumP;
    rqData.data3.size = 1;

    rqData.data4.data = maxDivisionNumP;
    rqData.data4.size = 1;
    rqData.rqNum.intNum = 4;
    rqData.rqNum.charNum = 0;

    return unpackedNetDataBuf(netDataBuf, rqData);
}

int unpackedDataFullRecoverN2(char *netDataBuf, int stripeNum, int maxDivisionNum, int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionLines[][2 * EC_N],
                              int divisionNum[], char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN], unsigned char *g_tbls)
{
    requestData_t rqData;
    rqData.data1.data = twoBucketsSliceIndexInfo;
    rqData.data1.size = stripeNum * (EC_N - 1) * 6;
    rqData.data2.data = divisionLines;
    rqData.data2.size = stripeNum * 2 * EC_N;
    rqData.data3.data = divisionNum;
    rqData.data3.size = stripeNum;

    rqData.data4.data = fullRecoverFailBlockName;
    rqData.data4.size = EC_A * HDFS_MAX_BP_COUNT * MAX_PATH_LEN;
    rqData.data5.data = g_tbls;
    rqData.data5.size = stripeNum * maxDivisionNum * 1 * EC_K * 32;
    rqData.rqNum.intNum = 3;
    rqData.rqNum.charNum = 2;
    if (!HDFS_FLAG)
    {
        rqData.data4.data = g_tbls;
        rqData.data4.size = stripeNum * maxDivisionNum * 1 * EC_K * 32;
        rqData.rqNum.charNum = 1;
    }
    return unpackedNetDataBuf(netDataBuf, rqData);
}

int packedDataMulDegradedReadN(char **netDataBufP, int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum, unsigned char *g_tbls)
{

    requestData_t rqData;
    rqData.data1.data = twoBucketsSliceIndexInfo;
    rqData.data1.size = (EC_N - MUL_FAIL_NUM) * 6;
    rqData.data2.data = divisionLines;
    rqData.data2.size = 2 * EC_N;
    rqData.data3.data = &divisionNum;
    rqData.data3.size = 1;
    rqData.data4.data = g_tbls;
    rqData.data4.size = MUL_FAIL_NUM * divisionNum * EC_K * 32;
    rqData.rqNum.intNum = 3;
    rqData.rqNum.charNum = 1;
    return packedNetDataBuf(netDataBufP, rqData);
}

int unpackedDataMulDegradedReadN1(char *netDataBuf, int twoBucketsSliceIndexInfo[][6], int divisionLines[], int *divisionNumP)
{
    requestData_t rqData;
    rqData.data1.data = twoBucketsSliceIndexInfo;
    rqData.data1.size = (EC_N - MUL_FAIL_NUM) * 6;
    rqData.data2.data = divisionLines;
    rqData.data2.size = 2 * EC_N;
    rqData.data3.data = divisionNumP;
    rqData.data3.size = 1;
    rqData.rqNum.intNum = 3;
    rqData.rqNum.charNum = 0;
    return unpackedNetDataBuf(netDataBuf, rqData);
}

int packedDataMulFullRecoverT(char **netDataBufP, int increasingNodesIndexArr[][EC_K],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum,
                              int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                              unsigned char *g_tbls)
{
    requestData_t rqData;
    rqData.data1.data = increasingNodesIndexArr;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_K;
    rqData.data2.data = eachNodeInfo;
    rqData.data2.size = 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data3.data = &stripeNum;
    rqData.data3.size = 1;
    rqData.data4.data = eachStripeFailNodeNum;
    rqData.data4.size = FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data5.data = eachStripeFailNodeIndex;
    rqData.data5.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_M;
    rqData.data6.data = g_tbls;
    rqData.data6.size = stripeNum * MUL_FAIL_NUM * EC_K * 1 * 32;
    rqData.rqNum.intNum = 5;
    rqData.rqNum.charNum = 1;
    return packedNetDataBuf(netDataBufP, rqData);
}

int unpackedDataMulFullRecoverTE1(char *netDataBuf, int increasingNodesIndexArr[][EC_K],
                                  int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int *stripeNumP)
{
    requestData_t rqData;
    rqData.data1.data = increasingNodesIndexArr;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_K;
    rqData.data2.data = eachNodeInfo;
    rqData.data2.size = 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data3.data = stripeNumP;
    rqData.data3.size = 1;
    rqData.rqNum.intNum = 3;
    rqData.rqNum.charNum = 0;
    return unpackedNetDataBuf(netDataBuf, rqData);
}

int unpackedDataMulFullRecoverTE2(char *netDataBuf, int stripeNum,
                                  int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                                  unsigned char *g_tbls)
{

    requestData_t rqData;
    rqData.data1.data = eachStripeFailNodeNum;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data2.data = eachStripeFailNodeIndex;
    rqData.data2.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_M;
    rqData.data3.data = g_tbls;
    rqData.data3.size = stripeNum * MUL_FAIL_NUM * EC_K * 32;
    rqData.rqNum.intNum = 2;
    rqData.rqNum.charNum = 1;
    return unpackedNetDataBuf(netDataBuf, rqData);
}

int packedDataMulFullRecoverR(char **netDataBufP, int increasingNodesIndexArr[][EC_K],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum,
                              int dstNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int srcNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM],
                              int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                              unsigned char *g_tbls)
{
    requestData_t rqData;
    rqData.data1.data = increasingNodesIndexArr;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_K;
    rqData.data2.data = eachNodeInfo;
    rqData.data2.size = 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data3.data = &stripeNum;
    rqData.data3.size = 1;

    rqData.data4.data = dstNodeArr;
    rqData.data4.size = EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data5.data = srcNodeArr;
    rqData.data5.size = EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;

    rqData.data6.data = eachStripeFailNodeNum;
    rqData.data6.size = FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data7.data = eachStripeFailNodeIndex;
    rqData.data7.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_M;

    rqData.data8.data = g_tbls;
    rqData.data8.size = stripeNum * MUL_FAIL_NUM * EC_K * 2 * 32;
    rqData.rqNum.intNum = 7;
    rqData.rqNum.charNum = 1;

    return packedNetDataBuf(netDataBufP, rqData);
}

int unpackedDataMulFullRecoverR1(char *netDataBuf, int increasingNodesIndexArr[][EC_K], int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM],
                                 int *stripeNumP, int dstNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int srcNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM])
{

    requestData_t rqData;
    rqData.data1.data = increasingNodesIndexArr;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_K;
    rqData.data2.data = eachNodeInfo;
    rqData.data2.size = 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data3.data = stripeNumP;
    rqData.data3.size = 1;

    rqData.data4.data = dstNodeArr;
    rqData.data4.size = EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data5.data = srcNodeArr;
    rqData.data5.size = EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.rqNum.intNum = 5;
    rqData.rqNum.charNum = 0;
    return unpackedNetDataBuf(netDataBuf, rqData);
}

int unpackedDataMulFullRecoverR2(char *netDataBuf, int stripeNum,
                                 int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                                 unsigned char *g_tbls)
{
    requestData_t rqData;
    rqData.data1.data = eachStripeFailNodeNum;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data2.data = eachStripeFailNodeIndex;
    rqData.data2.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_M;
    rqData.data3.data = g_tbls;
    rqData.data3.size = stripeNum * MUL_FAIL_NUM * EC_K * 2 * 32;
    rqData.rqNum.intNum = 2;
    rqData.rqNum.charNum = 1;
    return unpackedNetDataBuf(netDataBuf, rqData);
}

int packedDataMulFullRecoverE(char **netDataBufP, int increasingNodesIndexArr[][EC_K],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum,
                              int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                              unsigned char *g_tbls)
{
    requestData_t rqData;
    rqData.data1.data = increasingNodesIndexArr;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_K;
    rqData.data2.data = eachNodeInfo;
    rqData.data2.size = 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data3.data = &stripeNum;
    rqData.data3.size = 1;

    rqData.data4.data = eachStripeFailNodeNum;
    rqData.data4.size = FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data5.data = eachStripeFailNodeIndex;
    rqData.data5.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_M;

    rqData.data6.data = g_tbls;
    rqData.data6.size = stripeNum * MUL_FAIL_NUM * EC_K * 32;
    rqData.rqNum.intNum = 5;
    rqData.rqNum.charNum = 1;
    return packedNetDataBuf(netDataBufP, rqData);
}

int packedDataMulFullRecoverN(char **netDataBufP, int increasingNodesIndexArr[][EC_N - 1],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum, int maxDivisionNum,
                              int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionLines[][2 * EC_N], int divisionNum[],
                              int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                              unsigned char *g_tbls)
{

    requestData_t rqData;
    rqData.data1.data = increasingNodesIndexArr;
    rqData.data1.size = FULL_RECOVERY_STRIPE_MAX_NUM * (EC_N - 1);
    rqData.data2.data = eachNodeInfo;
    rqData.data2.size = 2 * EC_A * FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data3.data = &stripeNum;
    rqData.data3.size = 1;

    rqData.data4.data = &maxDivisionNum;
    rqData.data4.size = 1;
    rqData.data5.data = twoBucketsSliceIndexInfo;
    rqData.data5.size = stripeNum * (EC_N - 1) * 6;

    rqData.data6.data = divisionLines;
    rqData.data6.size = stripeNum * 2 * EC_N;

    rqData.data7.data = divisionNum;
    rqData.data7.size = stripeNum;

    rqData.data8.data = eachStripeFailNodeNum;
    rqData.data8.size = FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data9.data = eachStripeFailNodeIndex;
    rqData.data9.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_M;

    rqData.data10.data = g_tbls;
    rqData.data10.size = stripeNum * MUL_FAIL_NUM * maxDivisionNum * EC_K * 32;
    rqData.rqNum.intNum = 9;
    rqData.rqNum.charNum = 1;
    return packedNetDataBuf(netDataBufP, rqData);
}

int unpackedDataMulFullRecoverN2(char *netDataBuf, int stripeNum, int maxDivisionNum, int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionLines[][2 * EC_N],
                                 int divisionNum[], int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                                 unsigned char *g_tbls)
{
    requestData_t rqData;
    rqData.data1.data = twoBucketsSliceIndexInfo;
    rqData.data1.size = stripeNum * (EC_N - 1) * 6;
    rqData.data2.data = divisionLines;
    rqData.data2.size = stripeNum * 2 * EC_N;
    rqData.data3.data = divisionNum;
    rqData.data3.size = stripeNum;
    rqData.data4.data = eachStripeFailNodeNum;
    rqData.data4.size = FULL_RECOVERY_STRIPE_MAX_NUM;
    rqData.data5.data = eachStripeFailNodeIndex;
    rqData.data5.size = FULL_RECOVERY_STRIPE_MAX_NUM * EC_M;

    rqData.data6.data = g_tbls;
    rqData.data6.size = stripeNum * MUL_FAIL_NUM * maxDivisionNum * EC_K * 32;
    rqData.rqNum.intNum = 5;
    rqData.rqNum.charNum = 1;
    return unpackedNetDataBuf(netDataBuf, rqData);
}
