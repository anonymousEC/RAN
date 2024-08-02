#ifndef RAN_H
#define RAN_H

#include "ECConfig.h"

void RANGetBaNetO(int *Ba, int nodeNum);
void RANGetBaNetE(int *Ba, int nodeNum);
void RANMulDegradedReadGetBaNetE(int *Ba, int nodeNum);

void RANDegradedReadCalBrTmax(int *Ba, int *Br, int *p_Tmax, int nodeNum);

int RANFullRecoverSolveLP(int selectNodesIndexArr[][EC_N + MUL_FAIL_NUM - 1], int Ba[EC_A - 1], int Dij[][EC_N - 1], int *p_Tmax);
int RANMulFullRecoverSolveLP(int selectNodesIndexArr[][EC_N + MUL_FAIL_NUM - 1], int Ba[EC_A - MUL_FAIL_NUM], int Dij[][EC_N - 1], int *p_Tmax);

void RANDegradedReadAllocateDataSize(int *Ds, int *Br, int Tmax, int nodeNum);

int RANDegradedReadFillDataBuckets(int twoBucketsSliceSizeInfo[][6], int Ds[], int size);
int RANFullRecoverFillDataBuckets(int twoBucketsSliceSizeInfo[][EC_N - 1][6], int Dij[][EC_N - 1]);
int RANMulFullRecoverFillDataBuckets(int twoBucketsSliceSizeInfo[][EC_N - 1][6], int Dij[][EC_N - 1]);

int RANDegradedReadSliceAlig(int twoBucketsSliceIndexInfo[][6], int twoBucketsSliceSizeInfo[][6], int size);
void RANFullRecoverSliceAlig(int twoBucketsSliceIndexInfo[][EC_N - 1][6], int twoBucketsSliceSizeInfo[][EC_N - 1][6]);
void RANMulFullRecoverSliceAlig(int twoBucketsSliceIndexInfo[][EC_N - 1][6], int twoBucketsSliceSizeInfo[][EC_N - 1][6]);

int RANDegradedReadDivisionLines(int divisionLines[], int twoBucketsSliceIndexInfo[][6], int size);
int RANFullRecoverDivisionLines(int divisionLines[][2 * EC_N], int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionNum[]);
int RANMulFullRecoverDivisionLines(int divisionLines[][2 * EC_N], int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionNum[]);

void RANDegradedReadSortPosIndex(int twoBucketsSliceIndexInfo[][6], int size);
void RANFullRecoverSortPosIndex(int twoBucketsSliceIndexInfo[][EC_N - 1][6]);
void RANMulFullRecoverSortPosIndex(int twoBucketsSliceIndexInfo[][EC_N - MUL_FAIL_NUM][6]);

#endif