#ifndef MULPORTTRANSFER_H
#define MULPORTTRANSFER_H

#include "ECConfig.h"

void *sglPortNetTransfer(void *arg);
int mulPortNetTransfer(char **data, int **mulPortSockfdArr, int data_size, int nodeNum, int mode);
void mulPortNetMetaInitBasic(mulPortNetMeta_t *mulPortNetMetaP, int nodeNum, int mode, int totalSliceNum, int flagEncode,
                             int **mulPortSockfdArr, char **dataRecv, char **dataSend, int *sliceIndexP, unsigned char *g_tbls);
void mulPortNetMetaInit3Arr(mulPortNetMeta_t *mulPortNetMetaP, int nodeNum, int mode, int totalSliceNum, int flagEncode,
                            int **mulPortSockfdArr, char ***recvDataBucket, queue_t *sliceIndexRecvQueue, int **sliceBitmap, unsigned char *g_tbls);
void mulPortNetMetaInitSend2(mulPortNetMeta_t *mulPortNetMetaP, int startFailNodeIndex, int failNodeNum, int *failNodeIndex);
void degradedReadNetRecordNInit(int (*twoBucketsSliceIndexInfo)[6], int divisionNum, int *divisionLines, unsigned char ***g_tbls2Arr);
void fullRecoverNetRecordNInit(int stripeNum, int increasingNodesIndexArr[][EC_N - 1], int (*twoBucketsSliceIndexInfo)[EC_N - 1][6], int(*divisionNum), int (*divisionLines)[2 * EC_N],
                               int eachNodeInfo[2][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM], unsigned char ***g_tbls2Arr, unsigned char ****g_tbls3Arr);

void *netSlice1Node1Fail(void *arg);
void *netSliceKNode1FailRecv(void *arg);
void *netSliceKNode(void *arg);
void *netSliceNNode1FailRecv(void *arg);
void *netSliceNNode(void *arg);
void *netSliceANode1FailRecv(void *arg);
void *netSliceANode1Fail(void *arg);
void *netSliceANodeMulFailRecv(void *arg);
void *netSliceANodeMulFail(void *arg);

#endif