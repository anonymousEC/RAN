#ifndef REQUESTDATA_H
#define REQUESTDATA_H

#include "ECConfig.h"

typedef struct rqCel_s
{
    char *data;
    int size;
} rqCell_t;

typedef struct rqNum_s
{
    int intNum;
    int charNum;
} rqNum_t;

typedef struct requestData_s
{
    rqCell_t data1;
    rqCell_t data2;
    rqCell_t data3;
    rqCell_t data4;
    rqCell_t data5;
    rqCell_t data6;
    rqCell_t data7;
    rqCell_t data8;
    rqCell_t data9;
    rqCell_t data10;
    rqCell_t data11;
    rqCell_t data12;
    rqCell_t data13;
    rqCell_t data14;
    rqCell_t data15;
    rqCell_t data16;
    rqCell_t data17;
    rqCell_t data18;
    rqNum_t rqNum;
} requestData_t;

/* DegradedRead */
int packedDataDegradedReadN(char **netDataBufP, int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum, unsigned char *g_tbls);
int unpackedDataDegradedReadN1(char *netDataBuf, int twoBucketsSliceIndexInfo[EC_N - 1][6], int divisionLines[2 * EC_N], int *divisionNumP);

/* FullRecover */
int packedDataFullRecoverTE(char **netDataBufP, int increasingNodesIndexArr[][EC_K],
                            int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum,
                            char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN], unsigned char *g_tbls);
int unpackedDataFullRecoverTE1(char *netDataBuf, int increasingNodesIndexArr[][EC_K],
                               int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int *stripeNumP,
                               char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN]);
int packedDataFullRecoverR(char **netDataBufP, int increasingNodesIndexArr[][EC_K], int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum,
                           int dstNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int srcNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM],
                           char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN], unsigned char *g_tbls);
int unpackedDataFullRecoverR1(char *netDataBuf, int increasingNodesIndexArr[][EC_K], int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM],
                              int *stripeNumP, int dstNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int srcNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM],
                              char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN]);
int packedDataFullRecoverE(char **netDataBufP, int increasingNodesIndexArr[][EC_K], int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM],
                           int stripeNum, char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN],
                           unsigned char *g_tbls);
int packedDataFullRecoverN(char **netDataBufP, int increasingNodesIndexArr[][EC_N - 1],
                           int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum, int maxDivisionNum,
                           int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionLines[][2 * EC_N], int divisionNum[],
                           char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN], unsigned char *g_tbls);
int unpackedDataFullRecoverN1(char *netDataBuf, int increasingNodesIndexArr[][EC_N - 1],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int *stripeNumP, int *maxDivisionNumP);
int unpackedDataFullRecoverN2(char *netDataBuf, int stripeNum, int maxDivisionNum, int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionLines[][2 * EC_N],
                              int divisionNum[], char fullRecoverFailBlockName[EC_A][HDFS_MAX_BP_COUNT][MAX_PATH_LEN], unsigned char *g_tbls);

/* Mul DegradedRead */
int packedDataMulDegradedReadN(char **netDataBufP, int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum, unsigned char *g_tbls);
int unpackedDataMulDegradedReadN1(char *netDataBuf, int twoBucketsSliceIndexInfo[][6], int divisionLines[], int *divisionNumP);

/* Mul FullRecover */
int packedDataMulFullRecoverT(char **netDataBufP, int increasingNodesIndexArr[][EC_K],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum,
                              int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                              unsigned char *g_tbls);
int unpackedDataMulFullRecoverTE1(char *netDataBuf, int increasingNodesIndexArr[][EC_K],
                                  int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int *stripeNumP);
int unpackedDataMulFullRecoverTE2(char *netDataBuf, int stripeNum,
                                  int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                                  unsigned char *g_tbls);
int packedDataMulFullRecoverR(char **netDataBufP, int increasingNodesIndexArr[][EC_K],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum,
                              int dstNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int srcNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM],
                              int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                              unsigned char *g_tbls);
int unpackedDataMulFullRecoverR1(char *netDataBuf, int increasingNodesIndexArr[][EC_K], int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM],
                                 int *stripeNumP, int dstNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int srcNodeArr[EC_A][FULL_RECOVERY_STRIPE_MAX_NUM]);
int unpackedDataMulFullRecoverR2(char *netDataBuf, int stripeNum,
                                 int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                                 unsigned char *g_tbls);
int packedDataMulFullRecoverE(char **netDataBufP, int increasingNodesIndexArr[][EC_K],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum,
                              int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                              unsigned char *g_tbls);
int packedDataMulFullRecoverN(char **netDataBufP, int increasingNodesIndexArr[][EC_N - 1],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int stripeNum, int maxDivisionNum,
                              int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionLines[][2 * EC_N], int divisionNum[],
                              int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                              unsigned char *g_tbls);
int unpackedDataMulFullRecoverN2(char *netDataBuf, int stripeNum, int maxDivisionNum, int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionLines[][2 * EC_N],
                                 int divisionNum[], int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM], int eachStripeFailNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_M],
                                 unsigned char *g_tbls);
#endif