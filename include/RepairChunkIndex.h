#ifndef REPAIRCHUNINDEX_H
#define REPAIRCHUNINDEX_H

#include "ECConfig.h"

void repairChunkIndexDegradedReadTRE(int *selectNodesIndex, int *selectChunksIndex, int *healChunkIndex, int *healChunkNumP, int *failChunkIndex, int *failChunkNumP);
void repairChunkIndexDegradedReadN(int *selectNodesIndexArr, int *selectChunksIndexArr, int healChunkIndex[][EC_N],
                                   int healChunkNum[], int failChunkIndex[][EC_N], int failChunkNum[],
                                   int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum);
void repairChunkIndexMulDegradedReadTRE(int *selectNodesIndex, int *selectChunksIndex, int *healChunkIndex, int *healChunkNumP,
                                        int failChunkIndex2Arr[MUL_FAIL_NUM][EC_N], int *failChunkNumP);
void repairChunkIndexMulDegradedReadN(int *selectNodesIndex, int *selectChunksIndex, int healChunkIndex[][EC_N],
                                      int healChunkNum[], int failChunk2ArrIndex[][2 * EC_N][EC_N], int failChunk2ArrNum[][2 * EC_N],
                                      int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum);
void repairChunkIndexMulFullRecoverN(int *selectNodesIndex, int *selectChunksIndex, int healChunkIndex[][EC_N],
                                     int healChunkNum[], int failChunk2ArrIndex[][2 * EC_N][EC_N], int failChunk2ArrNum[][2 * EC_N],
                                     int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum);
#endif