#ifndef CLIENTREQUEST_H
#define CLIENTREQUEST_H

#include "ECConfig.h"

int clientRequestReadKChunks(char **dataChunk, char *dstFileName, int healChunkIndex[]);
int clientRequestWriteNChunks(char **dataChunk, char **codingChunk, char *dstFileName, int *sockfdArr);
int clientRequestDegradedReadT(int selectNodesIndex[], unsigned char *g_tbls, int expCount);
int clientRequestDegradedReadR(int selectNodesIndex[], unsigned char **g_tblsArr, int expCount);
int clientRequestDegradedReadE(int selectNodesIndex[], unsigned char *g_tbls, int expCount);
int clientRequestDegradedReadN(int selectNodesIndex[], int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum, unsigned char **g_tblsArr, int expCount);
int clientRequestFullRecoverT(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char **g_tblsArr, int expCount);
int clientRequestFullRecoverR(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char ***g_tbls2Arr, int expCount);
int clientRequestFullRecoverE(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char **g_tblsArr, int expCount);
int clientRequestFullRecoverN(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N + MUL_FAIL_NUM - 1],
                              int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int maxDivisionNum,
                              int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionLines[][2 * EC_N], int divisionNum[], unsigned char ***g_tbls2Arr, int expCount);
int clientRequestMulDegradedReadT(int selectNodesIndex[], unsigned char **g_tblsArr, int expCount);
int clientRequestMulDegradedReadR(int selectNodesIndex[], unsigned char ***g_tbls2Arr, int expCount);
int clientRequestMulDegradedReadE(int selectNodesIndex[], unsigned char **g_tblsArr, int expCount);
int clientRequestMulDegradedReadN(int selectNodesIndex[], int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum, unsigned char ***g_tbls2Arr, int expCount);
int clientRequestMulFullRecoverT(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char ***g_tbls2Arr, int expCount);
int clientRequestMulFullRecoverR(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char ****g_tbls3Arr, int expCount);
int clientRequestMulFullRecoverE(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char ***g_tbls2Arr, int expCount);
int clientRequestMulFullRecoverN(int selectNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N + MUL_FAIL_NUM - 1],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], int maxDivisionNum,
                                 int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionLines[][2 * EC_N], int divisionNum[],
                                 unsigned char ****g_tbls3Arr, int expCount);
#endif