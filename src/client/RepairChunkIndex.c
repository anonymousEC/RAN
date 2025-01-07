#include "RepairChunkIndex.h"

/* from ClientMain.c */
extern int FailNodeIndex;

void repairChunkIndexDegradedReadTRE(int *selectNodesIndex, int *selectChunksIndex,
                                     int *healChunkIndex, int *healChunkNumP, int *failChunkIndex, int *failChunkNumP)
{
    (*healChunkNumP) = 0;
    (*failChunkNumP) = 0;
    for (int i = 0; i < EC_K + 1; i++)
        if (selectNodesIndex[i] == FailNodeIndex)
            failChunkIndex[(*failChunkNumP)++] = selectChunksIndex[i];
        else
            healChunkIndex[(*healChunkNumP)++] = selectChunksIndex[i];
    if ((*healChunkNumP) < EC_K)
        ERROR_RETURN_NULL("error num_nodes more than EC_M");
    sortMinBubble(healChunkIndex, (*healChunkNumP));
    sortMinBubble(failChunkIndex, (*failChunkNumP));
    return;
}

void repairChunkIndexDegradedReadN(int *selectNodesIndex, int *selectChunksIndex, int healChunkIndex[][EC_N],
                                   int healChunkNum[], int failChunkIndex[][EC_N], int failChunkNum[],
                                   int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum)
{
    for (int i = 0; i < divisionNum; i++)
    {
        healChunkNum[i] = 0;
        failChunkNum[i] = 0;
    }

    int nodeIndexTochunkIndex[EC_A] = {0}, increasingNodesIndex[EC_N - 1];
    for (int i = 0; i < EC_N; i++)
        nodeIndexTochunkIndex[selectNodesIndex[i]] = selectChunksIndex[i];
    for (int j = 0; j < EC_N - 1; j++)
        increasingNodesIndex[j] = selectNodesIndex[j + 1];
    sortMinBubble(increasingNodesIndex, EC_N - 1);

    int lowb, upb; // if division is < lowb and >ubp
    for (int k = 0; k < divisionNum; k++)
    {
        /* node contain this sub-chunk */
        if (k == 0)
            lowb = 0;
        else
            lowb = upb;
        upb = divisionLines[k];
        int count = 0;
        for (int i = 0; i < EC_N - 1; i++)
            for (int j = 0; j < 6 && count != EC_K; j += 3)
                if (twoBucketsSliceIndexInfo[i][j] != -1)
                    if (twoBucketsSliceIndexInfo[i][j] <= lowb && twoBucketsSliceIndexInfo[i][j] + twoBucketsSliceIndexInfo[i][j + 1] + 1 > upb)
                        healChunkIndex[k][count++] = nodeIndexTochunkIndex[increasingNodesIndex[i]];
        sortMinBubble(healChunkIndex[k], EC_K);
        healChunkNum[k] = EC_K; // but there only can access EC_K, for sub-stripe
    }

    if (selectNodesIndex[0] == FailNodeIndex) // 0 is FailNode for single fail
        for (int k = 0; k < divisionNum; k++)
            failChunkIndex[k][failChunkNum[k]++] = selectChunksIndex[0];
    return;
}

void repairChunkIndexMulDegradedReadTRE(int *selectNodesIndex, int *selectChunksIndex, int *healChunkIndex, int *healChunkNumP, int failChunkIndex2Arr[MUL_FAIL_NUM][EC_N], int *failChunkNumP)
{
    (*healChunkNumP) = 0;
    (*failChunkNumP) = 0;
    for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
        if (selectNodesIndex[i] == -1)
            continue;
        else if (selectNodesIndex[i] < MUL_FAIL_START || selectNodesIndex[i] > MUL_FAIL_START + MUL_FAIL_NUM - 1)
            healChunkIndex[(*healChunkNumP)++] = selectChunksIndex[i];
        else
            failChunkIndex2Arr[(*failChunkNumP)++][0] = selectChunksIndex[i];
    if ((*healChunkNumP) < EC_K)
        ERROR_RETURN_NULL("error num_nodes more than EC_M");
    (*healChunkNumP) = EC_K;
    sortMinBubble(healChunkIndex, (*healChunkNumP));
    (*failChunkNumP) = 1;
    return;
}

void repairChunkIndexMulDegradedReadN(int *selectNodesIndex, int *selectChunksIndex, int healChunkIndex[][EC_N],
                                      int healChunkNum[], int failChunk2ArrIndex[][2 * EC_N][EC_N], int failChunk2ArrNum[][2 * EC_N],
                                      int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum)
{
    for (int i = 0; i < divisionNum; i++)
        healChunkNum[i] = 0;
    for (int i = 0; i < MUL_FAIL_NUM; i++)
        for (int j = 0; j < 2 * EC_N; j++)
            failChunk2ArrNum[i][j] = 0;

    int nodeIndexTochunkIndex[EC_A] = {0}, increasingNodesIndex[EC_N - MUL_FAIL_NUM];

    for (int i = 0; i < EC_N; i++)
        nodeIndexTochunkIndex[selectNodesIndex[i]] = selectChunksIndex[i];
    for (int j = 0; j < EC_N - MUL_FAIL_NUM; j++)
        increasingNodesIndex[j] = selectNodesIndex[j + MUL_FAIL_NUM];
    sortMinBubble(increasingNodesIndex, EC_N - MUL_FAIL_NUM);

    int lowb, upb; // if division is < lowb and >ubp
    for (int k = 0; k < divisionNum; k++)
    {
        /* node contain this sub-chunk */
        if (k == 0)
            lowb = 0;
        else
            lowb = upb;
        upb = divisionLines[k];
        int count = 0;
        for (int i = 0; i < EC_N - MUL_FAIL_NUM; i++)
            for (int j = 0; j < 6 && count != EC_K; j += 3)
                if (twoBucketsSliceIndexInfo[i][j] != -1)
                    if (twoBucketsSliceIndexInfo[i][j] <= lowb && twoBucketsSliceIndexInfo[i][j] + twoBucketsSliceIndexInfo[i][j + 1] + 1 > upb)
                        healChunkIndex[k][count++] = nodeIndexTochunkIndex[increasingNodesIndex[i]];
        sortMinBubble(healChunkIndex[k], EC_K);
        healChunkNum[k] = EC_K; // but there only can access EC_K, for sub-stripe
    }

    int count = 0;
    for (int i = 0; i < MUL_FAIL_NUM; i++)
        if (selectNodesIndex[i] >= MUL_FAIL_START && selectNodesIndex[i] < MUL_FAIL_START + MUL_FAIL_NUM)
        {
            for (int k = 0; k < divisionNum; k++)
            {
                failChunk2ArrNum[count][k]++;
                failChunk2ArrIndex[count][k][0] = selectChunksIndex[i];
            }
            count++;
        }
    return;
}

void repairChunkIndexMulFullRecoverN(int *selectNodesIndex, int *selectChunksIndex, int healChunkIndex[][EC_N],
                                     int healChunkNum[], int failChunk2ArrIndex[][2 * EC_N][EC_N], int failChunk2ArrNum[][2 * EC_N],
                                     int twoBucketsSliceIndexInfo[][6], int divisionLines[], int divisionNum)
{
    for (int i = 0; i < divisionNum; i++)
        healChunkNum[i] = 0;
    for (int i = 0; i < MUL_FAIL_NUM; i++)
        for (int j = 0; j < 2 * EC_N; j++)
            failChunk2ArrNum[i][j] = 0;

    int nodeIndexTochunkIndex[EC_A] = {0}, increasingNodesIndex[EC_N - 1];

    for (int i = 0; i < EC_N + MUL_FAIL_NUM - 1; i++)
        if (selectNodesIndex[i] != -1)
            nodeIndexTochunkIndex[selectNodesIndex[i]] = selectChunksIndex[i];
    int healNum = 0;
    for (int j = 0; j < EC_N - 1; j++)
    {
        increasingNodesIndex[j] = selectNodesIndex[j + MUL_FAIL_NUM]; // for mulFullRecover,  increasing has -1
        if (selectNodesIndex[j + MUL_FAIL_NUM] != -1)
            healNum++;
    }
    sortMinBubble(increasingNodesIndex, healNum);

    int lowb, upb; // if division is < lowb and >ubp
    for (int k = 0; k < divisionNum; k++)
    {
        /* node contain this sub-chunk */
        if (k == 0)
            lowb = 0;
        else
            lowb = upb;
        upb = divisionLines[k];
        int count = 0;
        for (int i = 0; i < EC_N - 1; i++)
            for (int j = 0; j < 6 && count != EC_K; j += 3)
                if (twoBucketsSliceIndexInfo[i][j] != -1)
                    if (twoBucketsSliceIndexInfo[i][j] <= lowb && twoBucketsSliceIndexInfo[i][j] + twoBucketsSliceIndexInfo[i][j + 1] + 1 > upb)
                        healChunkIndex[k][count++] = nodeIndexTochunkIndex[increasingNodesIndex[i]];
        sortMinBubble(healChunkIndex[k], EC_K);
        healChunkNum[k] = EC_K; // but there only can access EC_K, for sub-stripe
    }

    int count = 0;
    for (int i = 0; i < MUL_FAIL_NUM; i++)
        if (selectNodesIndex[i] != -1 && selectNodesIndex[i] >= MUL_FAIL_START && selectNodesIndex[i] < MUL_FAIL_START + MUL_FAIL_NUM)
        {
            for (int k = 0; k < divisionNum; k++)
            {
                failChunk2ArrNum[count][k]++;
                failChunk2ArrIndex[count][k][0] = selectChunksIndex[i];
            }
            count++;
        }
    return;
}
