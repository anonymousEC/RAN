#include "SelectNodes.h"

/* from ClientMain.c */
extern int FailNodeIndex;
extern int stripeNum;
extern int eachStripeNodeIndex[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N];
extern int failStripeCountToAllStripeCount[FULL_RECOVERY_STRIPE_MAX_NUM];
extern hdfs_info_t *HDFSInfo;

/* mul use */
static int uplinkBandwidth[EC_A_MAX], downlinkBandwidth[EC_A_MAX];
static int nodeWeight[EC_A_MAX], weightMaxIndex[EC_A_MAX];

static void printBand(int nodeNum)
{
    printArrINT(nodeNum, uplinkBandwidth, "Uplink bandwidth");
    printArrINT(nodeNum, downlinkBandwidth, "Downlink bandwidth");
    printArrINT(nodeNum, nodeWeight, "nodeWeight");
}

void selectNodesRand(int *selectNodesIndex)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long long nanoseconds = (long long)ts.tv_sec * 1000000000 + ts.tv_nsec;
    srand((unsigned int)nanoseconds);
    for (int i = 0; i < EC_A; i++)
        selectNodesIndex[i] = i;

    /* Randomly select nodes index */
    for (int i = 0; i < EC_N; i++)
    {
        int index = rand() % (EC_A - i);
        int temp = selectNodesIndex[EC_A - 1 - i];
        selectNodesIndex[EC_A - 1 - i] = selectNodesIndex[index];
        selectNodesIndex[index] = temp;
    }
    sortMinBubble(selectNodesIndex, EC_N);
    return;
}

void selectNodesDegradedReadTRENNetO(int *selectNodesIndex, int *selectChunksIndex)
{
    /* get k nodes and FailNodeIndex */
    int tmpIndex = 0;
    selectNodesIndex[tmpIndex++] = FailNodeIndex;
    for (int i = 0; i < EC_N; i++)
        if (i != FailNodeIndex)
            selectNodesIndex[tmpIndex++] = i; // selectNodesIndex[0] is FailNodeIndex
    for (int i = 0; i < EC_N; i++)
        if (!HDFS_FLAG)
            selectChunksIndex[i] = selectNodesIndex[i];
        else
            selectChunksIndex[i] = HDFSInfo->nodeIndexToChunkIndex[selectNodesIndex[i]];
    return;
}

void selectNodesDegradedReadTENetE(int *selectNodesIndex, int *selectChunksIndex)
{
    /* Get uplink and downlink bandwidh*/

    if (readUpDownBandwidth(uplinkBandwidth, downlinkBandwidth, BAND_LOCATION, EC_N) == EC_ERROR)
        ERROR_RETURN_VALUE("Fail readUpDownBandwidth");

    getDegradedReadWeightNodesTEN(uplinkBandwidth, nodeWeight, EC_N, FailNodeIndex);
    getSortedMaxIndices(nodeWeight, weightMaxIndex, EC_N);
    /* get k nodes of max weight and FailNodeIndex */
    for (int i = 0; i < EC_K + 1; i++)
        selectNodesIndex[i] = weightMaxIndex[i]; // selectNodesIndex[0] is FailNodeIndex/last node

    for (int i = 0; i < EC_N; i++)
        if (!HDFS_FLAG)
            selectChunksIndex[i] = selectNodesIndex[i];
        else
            selectChunksIndex[i] = HDFSInfo->nodeIndexToChunkIndex[selectNodesIndex[i]];
    printBand(EC_N);
    return;
}

void selectNodesDegradedReadRNetE(int *selectNodesIndex, int *selectChunksIndex)
{
    /* Get uplink and downlink bandwidh, weight and weightMaxIndex  */

    if (readUpDownBandwidth(uplinkBandwidth, downlinkBandwidth, BAND_LOCATION, EC_N) == EC_ERROR)
        ERROR_RETURN_VALUE("Fail readUpDownBandwidth");

    getDegradedReadWeightNodesR(uplinkBandwidth, downlinkBandwidth, nodeWeight, EC_N, FailNodeIndex);
    getSortedMaxIndices(nodeWeight, weightMaxIndex, EC_N);
    /* get k-1 nodes of max weight and FailNodeIndex */
    for (int i = 0; i < EC_K; i++)
        selectNodesIndex[i] = weightMaxIndex[i]; // selectNodesIndex[0] is FailNodeIndex/last node, selectNodesIndex[k] is first node
    /* get 1 nodes of max uplink for remain nodes */
    selectNodesIndex[EC_K] = weightMaxIndex[EC_K];
    for (int i = EC_K + 1; i < EC_N; i++)
        if (uplinkBandwidth[weightMaxIndex[i]] > uplinkBandwidth[selectNodesIndex[EC_K]])
            selectNodesIndex[EC_K] = weightMaxIndex[i];

    for (int i = 0; i < EC_N; i++)
        if (!HDFS_FLAG)
            selectChunksIndex[i] = selectNodesIndex[i];
        else
            selectChunksIndex[i] = HDFSInfo->nodeIndexToChunkIndex[selectNodesIndex[i]];
    printBand(EC_N);
    return;
}

/* Select the first k nodes among n nodes */
void selectNodesFullRecoverTENetO(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                  int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM])
{
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < EC_A; i++)
            for (int j = 0; j < FULL_RECOVERY_STRIPE_MAX_NUM; j++)
                eachNodeInfo[k][i][j] = -1;

    int nodeStripeCount[EC_A] = {0};
    for (int k = 0; k < stripeNum; k++)
    {
        /* storage_heal */
        int tmpMinCount = 1;
        for (int i = 0; i < EC_K + 1; i++)
        {
            int curIndex = i;
            int curNodeIndex = eachStripeNodeIndex[k][curIndex];
            if (curNodeIndex == FailNodeIndex)
                continue;
            selectChunksIndexArr[k][tmpMinCount] = curIndex;
            selectNodesIndexArr[k][tmpMinCount++] = curNodeIndex;                                              // node
            eachNodeInfo[0][curNodeIndex][nodeStripeCount[curNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
            eachNodeInfo[1][curNodeIndex][nodeStripeCount[curNodeIndex]++] = curIndex;
            if (tmpMinCount == EC_K + 1)
                break;
        }

        /* storage_fail */
        for (int i = 0; i < EC_N; i++)
            if (eachStripeNodeIndex[k][i] == FailNodeIndex)
            {
                selectChunksIndexArr[k][0] = i;
                selectNodesIndexArr[k][0] = FailNodeIndex;                                                           // selectNodesIndex[0] is FailNodeIndex
                eachNodeInfo[0][FailNodeIndex][nodeStripeCount[FailNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][FailNodeIndex][nodeStripeCount[FailNodeIndex]++] = i;
            }
    }
    return;
}
void selectNodesFullRecoverRNetO(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM])
{
    /* homogeneous network:  node least recently select  */
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < EC_A; i++)
            for (int j = 0; j < FULL_RECOVERY_STRIPE_MAX_NUM; j++)
                eachNodeInfo[k][i][j] = -1;
    int nodeStripeCount[EC_A] = {0};
    int select_num[EC_A] = {0};
    int tmp_select_num[EC_N], tmp_index_select_num[EC_N];
    int cur_least_num = 0;
    for (int k = 0; k < stripeNum; k++)
    {

        for (int i = 0; i < EC_N; i++)
            tmp_select_num[i] = select_num[eachStripeNodeIndex[k][i]];
        getSortedMinIndices(tmp_select_num, tmp_index_select_num, EC_N);

        /* storage_heal */
        int tmpMinCount = 1;
        for (int i = 0; i < EC_K + 1; i++)
        {
            int cur_min_select_index = tmp_index_select_num[i];
            int curNodeIndex = eachStripeNodeIndex[k][cur_min_select_index];
            if (curNodeIndex == FailNodeIndex)
                continue;
            selectChunksIndexArr[k][tmpMinCount] = cur_min_select_index;
            selectNodesIndexArr[k][tmpMinCount++] = curNodeIndex;                                              // node
            eachNodeInfo[0][curNodeIndex][nodeStripeCount[curNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
            eachNodeInfo[1][curNodeIndex][nodeStripeCount[curNodeIndex]++] = cur_min_select_index;
            select_num[curNodeIndex]++;
            if (tmpMinCount == EC_K + 1)
                break;
        }

        /* storage_fail */
        for (int i = 0; i < EC_N; i++)
            if (eachStripeNodeIndex[k][i] == FailNodeIndex)
            {
                selectChunksIndexArr[k][0] = i;
                selectNodesIndexArr[k][0] = FailNodeIndex;                                                           // selectNodesIndex[0] is FailNodeIndex
                eachNodeInfo[0][FailNodeIndex][nodeStripeCount[FailNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][FailNodeIndex][nodeStripeCount[FailNodeIndex]++] = i;
            }
    }
    return;
}

void selectNodesFullRecoverNNetO(int selectNodesIndexArr[][EC_N + MUL_FAIL_NUM - 1], int selectChunksIndexArr[][EC_N + MUL_FAIL_NUM - 1],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM])
{
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < EC_A; i++)
            for (int j = 0; j < FULL_RECOVERY_STRIPE_MAX_NUM; j++)
                eachNodeInfo[k][i][j] = -1;

    for (int i = 0; i < stripeNum; i++)
        for (int j = 0; j < EC_N + MUL_FAIL_NUM - 1; j++)
        {
            selectChunksIndexArr[i][j] = -1;
            selectNodesIndexArr[i][j] = -1;
        }

    int nodeStripeCount[EC_A] = {0};
    for (int k = 0; k < stripeNum; k++)
    {
        /* storage_heal */
        int tmpMinCount = 1;
        for (int i = 0; i < EC_N; i++)
        {
            int curIndex = i;
            int curNodeIndex = eachStripeNodeIndex[k][curIndex];
            if (curNodeIndex == FailNodeIndex)
                continue;
            selectChunksIndexArr[k][tmpMinCount] = curIndex;
            selectNodesIndexArr[k][tmpMinCount++] = curNodeIndex;                                              // node
            eachNodeInfo[0][curNodeIndex][nodeStripeCount[curNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
            eachNodeInfo[1][curNodeIndex][nodeStripeCount[curNodeIndex]++] = curIndex;
            if (tmpMinCount == EC_N)
                break;
        }

        /* storage_fail */
        for (int i = 0; i < EC_N; i++)
            if (eachStripeNodeIndex[k][i] == FailNodeIndex)
            {
                selectChunksIndexArr[k][0] = i;
                selectNodesIndexArr[k][0] = FailNodeIndex;                                                           // selectNodesIndex[0] is FailNodeIndex
                eachNodeInfo[0][FailNodeIndex][nodeStripeCount[FailNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][FailNodeIndex][nodeStripeCount[FailNodeIndex]++] = i;
            }
    }
    return;
}

/* Select the first k nodes with max weight among n nodes */
void selectNodesFullRecoverTENetE(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N], int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM])
{
    if (readUpDownBandwidth(uplinkBandwidth, downlinkBandwidth, BAND_LOCATION, EC_A) == EC_ERROR)
        ERROR_RETURN_VALUE("Fail readUpDownBandwidth");
    getDegradedReadWeightNodesTEN(uplinkBandwidth, nodeWeight, EC_A, FailNodeIndex);
    printBand(EC_A);

    for (int k = 0; k < 2; k++)
        for (int i = 0; i < EC_A; i++)
            for (int j = 0; j < FULL_RECOVERY_STRIPE_MAX_NUM; j++)
                eachNodeInfo[k][i][j] = -1;

    int nodeStripeCount[EC_A] = {0};
    int tmpWeightNum[EC_N], tmpIndexWeightNum[EC_N];
    for (int k = 0; k < stripeNum; k++)
    {
        /* storage_heal */
        for (int i = 0; i < EC_N; i++)
            tmpWeightNum[i] = nodeWeight[eachStripeNodeIndex[k][i]];
        getSortedMaxIndices(tmpWeightNum, tmpIndexWeightNum, EC_N);

        int tmpMinCount = 1;
        for (int i = 0; i < EC_K + 1; i++)
        {
            int curIndex = tmpIndexWeightNum[i]; // curIndex is ChunkIndex
            int curNodeIndex = eachStripeNodeIndex[k][curIndex];
            if (curNodeIndex == FailNodeIndex)
                continue;
            selectChunksIndexArr[k][tmpMinCount] = curIndex;
            selectNodesIndexArr[k][tmpMinCount++] = curNodeIndex;                                              // node
            eachNodeInfo[0][curNodeIndex][nodeStripeCount[curNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
            eachNodeInfo[1][curNodeIndex][nodeStripeCount[curNodeIndex]++] = curIndex;
            if (tmpMinCount == EC_K + 1)
                break;
        }

        /* storage_fail */
        for (int i = 0; i < EC_N; i++)
            if (eachStripeNodeIndex[k][i] == FailNodeIndex)
            {
                selectChunksIndexArr[k][0] = i;
                selectNodesIndexArr[k][0] = FailNodeIndex;                                                           // selectNodesIndex[0] is FailNodeIndex
                eachNodeInfo[0][FailNodeIndex][nodeStripeCount[FailNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][FailNodeIndex][nodeStripeCount[FailNodeIndex]++] = i;
            }
    }
    return;
}

void selectNodesFullRecoverRNetE(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM])
{
    if (readUpDownBandwidth(uplinkBandwidth, downlinkBandwidth, BAND_LOCATION, EC_A) == EC_ERROR)
        ERROR_RETURN_VALUE("Fail readUpDownBandwidth");
    getDegradedReadWeightNodesR(uplinkBandwidth, downlinkBandwidth, nodeWeight, EC_A, FailNodeIndex);

    printBand(EC_A);

    for (int k = 0; k < 2; k++)
        for (int i = 0; i < EC_A; i++)
            for (int j = 0; j < FULL_RECOVERY_STRIPE_MAX_NUM; j++)
                eachNodeInfo[k][i][j] = -1;

    int nodeStripeCount[EC_A] = {0};
    int tmpWeightNum[EC_N], tmpIndexWeightNum[EC_N];
    for (int k = 0; k < stripeNum; k++)
    {
        /* storage_heal */
        for (int i = 0; i < EC_N; i++)
            tmpWeightNum[i] = nodeWeight[eachStripeNodeIndex[k][i]];
        getSortedMaxIndices(tmpWeightNum, tmpIndexWeightNum, EC_N);

        int tmpMinCount = 1;
        for (int i = 0; i < EC_K + 1; i++)
        {
            int curIndex = tmpIndexWeightNum[i];
            int curNodeIndex = eachStripeNodeIndex[k][curIndex];
            if (curNodeIndex == FailNodeIndex)
                continue;
            selectChunksIndexArr[k][tmpMinCount] = curIndex;
            selectNodesIndexArr[k][tmpMinCount++] = curNodeIndex;                                              // node
            eachNodeInfo[0][curNodeIndex][nodeStripeCount[curNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
            eachNodeInfo[1][curNodeIndex][nodeStripeCount[curNodeIndex]++] = curIndex;
            if (tmpMinCount == EC_K + 1)
                break;
        }

        /* storage_fail */
        for (int i = 0; i < EC_N; i++)
            if (eachStripeNodeIndex[k][i] == FailNodeIndex)
            {
                selectChunksIndexArr[k][0] = i;
                selectNodesIndexArr[k][0] = FailNodeIndex;                                                           // selectNodesIndex[0] is FailNodeIndex
                eachNodeInfo[0][FailNodeIndex][nodeStripeCount[FailNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][FailNodeIndex][nodeStripeCount[FailNodeIndex]++] = i;
            }
    }
    return;
}

void selectNodesMulDegradedReadTRENNetO(int *selectNodesIndex, int *selectChunksIndex)
{
    /* get k nodes and FailNodeIndex */
    int tmpIndex = 0;
    for (int i = MUL_FAIL_START; i < MUL_FAIL_START + MUL_FAIL_NUM; i++)
        selectNodesIndex[tmpIndex++] = i; // selectNodesIndex[0-MUL_FAIL_NUM-1] is FailNodeIndex
    for (int i = 0; i < EC_N; i++)
        if (i < MUL_FAIL_START || i > MUL_FAIL_START + MUL_FAIL_NUM - 1)
            selectNodesIndex[tmpIndex++] = i;

    for (int i = 0; i < EC_N; i++)
        selectChunksIndex[i] = selectNodesIndex[i];
    return;
}

void selectNodesMulDegradedReadTENetE(int *selectNodesIndex, int *selectChunksIndex)
{
    /* Get uplink and downlink bandwidh*/
    if (readUpDownBandwidth(uplinkBandwidth, downlinkBandwidth, BAND_LOCATION, EC_N) == EC_ERROR)
        ERROR_RETURN_VALUE("Fail readUpDownBandwidth");
    getMulDegradedReadWeightNodesTEN(uplinkBandwidth, nodeWeight, EC_N);
    getSortedMaxIndices(nodeWeight, weightMaxIndex, EC_N);

    /* get k nodes of max weight and FailNodeIndex */
    for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
        selectNodesIndex[i] = weightMaxIndex[i]; // selectNodesIndex[0-MUL_FAIL_NUM] is FailNodeIndex/last node

    for (int i = 0; i < EC_N; i++)
        selectChunksIndex[i] = selectNodesIndex[i];
    printBand(EC_N);
    return;
}

void selectNodesMulDegradedReadRNetE(int *selectNodesIndex, int *selectChunksIndex)
{
    /* Get uplink and downlink bandwidh, weight and weightMaxIndex  */
    if (readUpDownBandwidth(uplinkBandwidth, downlinkBandwidth, BAND_LOCATION, EC_N) == EC_ERROR)
        ERROR_RETURN_VALUE("Fail readUpDownBandwidth");
    getMulDegradedReadWeightNodesR(uplinkBandwidth, downlinkBandwidth, nodeWeight, EC_N);
    getSortedMaxIndices(nodeWeight, weightMaxIndex, EC_N);

    /* get k-1 nodes of max weight and FailNodeIndex */
    for (int i = 0; i < EC_K + MUL_FAIL_NUM - 1; i++)
        selectNodesIndex[i] = weightMaxIndex[i]; // selectNodesIndex[0] is FailNodeIndex/last node, selectNodesIndex[k] is first node

    /* get 1 nodes of max uplink for remain nodes */
    selectNodesIndex[EC_K + MUL_FAIL_NUM - 1] = weightMaxIndex[EC_K + MUL_FAIL_NUM - 1];
    for (int i = EC_K + MUL_FAIL_NUM; i < EC_N; i++)
        if (uplinkBandwidth[weightMaxIndex[i]] > uplinkBandwidth[selectNodesIndex[EC_K + MUL_FAIL_NUM - 1]])
            selectNodesIndex[EC_K + MUL_FAIL_NUM - 1] = weightMaxIndex[i];

    for (int i = 0; i < EC_N; i++)
        selectChunksIndex[i] = selectNodesIndex[i];
    printBand(EC_N);
    return;
}

/* Select the first k nodes among n-MUL_FAIL_NUM nodes */
void selectNodesMulFullRecoverTENetO(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                     int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM])
{
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < EC_A; i++)
            for (int j = 0; j < FULL_RECOVERY_STRIPE_MAX_NUM; j++)
                eachNodeInfo[k][i][j] = -1;
    for (int k = 0; k < stripeNum; k++)
        for (int i = 0; i < EC_N; i++)
        {
            selectNodesIndexArr[k][i] = -1;
            selectChunksIndexArr[k][i] = -1;
        }

    int nodeStripeCount[EC_A] = {0};
    for (int k = 0; k < stripeNum; k++)
    {
        /* storage_heal */
        int tmpMinCount = MUL_FAIL_NUM;
        for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
        {
            int curIndex = i;
            int curNodeIndex = eachStripeNodeIndex[k][curIndex];
            if (curNodeIndex < MUL_FAIL_START || curNodeIndex > MUL_FAIL_START + MUL_FAIL_NUM - 1) // heal node
            {
                selectChunksIndexArr[k][tmpMinCount] = curIndex;
                selectNodesIndexArr[k][tmpMinCount++] = curNodeIndex;                                              // node
                eachNodeInfo[0][curNodeIndex][nodeStripeCount[curNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][curNodeIndex][nodeStripeCount[curNodeIndex]++] = curIndex;
                if (tmpMinCount == EC_K + MUL_FAIL_NUM)
                    break;
            }
        }

        /* storage_fail */
        tmpMinCount = 0;
        for (int i = 0; i < EC_N; i++)
            if (eachStripeNodeIndex[k][i] < MUL_FAIL_START || eachStripeNodeIndex[k][i] > MUL_FAIL_START + MUL_FAIL_NUM - 1)
                continue;
            else
            {
                selectChunksIndexArr[k][tmpMinCount] = i;
                selectNodesIndexArr[k][tmpMinCount++] = eachStripeNodeIndex[k][i];                                                           // selectNodesIndex[0] is FailNodeIndex
                eachNodeInfo[0][eachStripeNodeIndex[k][i]][nodeStripeCount[eachStripeNodeIndex[k][i]]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][eachStripeNodeIndex[k][i]][nodeStripeCount[eachStripeNodeIndex[k][i]]++] = i;
            }
    }
    return;
}

void selectNodesMulFullRecoverRNetO(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                    int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM])
{
    /* homogeneous network:  node least recently select  */
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < EC_A; i++)
            for (int j = 0; j < FULL_RECOVERY_STRIPE_MAX_NUM; j++)
                eachNodeInfo[k][i][j] = -1;

    for (int k = 0; k < stripeNum; k++)
        for (int i = 0; i < EC_N; i++)
        {
            selectNodesIndexArr[k][i] = -1;
            selectChunksIndexArr[k][i] = -1;
        }

    int nodeStripeCount[EC_A] = {0};
    int select_num[EC_A] = {0};
    int tmp_select_num[EC_N], tmp_index_select_num[EC_N];
    int cur_least_num = 0;
    for (int k = 0; k < stripeNum; k++)
    {

        for (int i = 0; i < EC_N; i++)
            tmp_select_num[i] = select_num[eachStripeNodeIndex[k][i]];
        getSortedMinIndices(tmp_select_num, tmp_index_select_num, EC_N);

        /* storage_heal */
        int tmpMinCount = MUL_FAIL_NUM;
        for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
        {
            int cur_min_select_index = tmp_index_select_num[i];
            int curNodeIndex = eachStripeNodeIndex[k][cur_min_select_index];
            if (curNodeIndex < MUL_FAIL_START || curNodeIndex > MUL_FAIL_START + MUL_FAIL_NUM - 1) // heal node
            {
                selectChunksIndexArr[k][tmpMinCount] = cur_min_select_index;
                selectNodesIndexArr[k][tmpMinCount++] = curNodeIndex;                                              // node
                eachNodeInfo[0][curNodeIndex][nodeStripeCount[curNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][curNodeIndex][nodeStripeCount[curNodeIndex]++] = cur_min_select_index;
                select_num[curNodeIndex]++;
                if (tmpMinCount == EC_K + MUL_FAIL_NUM)
                    break;
            }
        }

        /* storage_fail */
        tmpMinCount = 0;
        for (int i = 0; i < EC_N; i++)
            if (eachStripeNodeIndex[k][i] < MUL_FAIL_START || eachStripeNodeIndex[k][i] > MUL_FAIL_START + MUL_FAIL_NUM - 1)
                continue;
            else
            {
                selectChunksIndexArr[k][tmpMinCount] = i;
                selectNodesIndexArr[k][tmpMinCount++] = eachStripeNodeIndex[k][i];                                                           // selectNodesIndex[0] is FailNodeIndex
                eachNodeInfo[0][eachStripeNodeIndex[k][i]][nodeStripeCount[eachStripeNodeIndex[k][i]]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][eachStripeNodeIndex[k][i]][nodeStripeCount[eachStripeNodeIndex[k][i]]++] = i;
            }
    }
    return;
}

void selectNodesMulFullRecoverNNetO(int selectNodesIndexArr[][EC_N + MUL_FAIL_NUM - 1], int selectChunksIndexArr[][EC_N + MUL_FAIL_NUM - 1],
                                    int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM])
{
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < EC_A; i++)
            for (int j = 0; j < FULL_RECOVERY_STRIPE_MAX_NUM; j++)
                eachNodeInfo[k][i][j] = -1;

    for (int i = 0; i < stripeNum; i++)
        for (int j = 0; j < EC_N + MUL_FAIL_NUM - 1; j++)
        {
            selectChunksIndexArr[i][j] = -1;
            selectNodesIndexArr[i][j] = -1;
        }

    int nodeStripeCount[EC_A] = {0};
    for (int k = 0; k < stripeNum; k++)
    {
        /* storage_heal */
        int tmpMinCount = MUL_FAIL_NUM;
        for (int i = 0; i < EC_N; i++)
        {
            int curIndex = i;
            int curNodeIndex = eachStripeNodeIndex[k][curIndex];
            if (curNodeIndex < MUL_FAIL_START || curNodeIndex > MUL_FAIL_START + MUL_FAIL_NUM - 1) // heal node
            {
                selectChunksIndexArr[k][tmpMinCount] = curIndex;
                selectNodesIndexArr[k][tmpMinCount++] = curNodeIndex;                                              // node
                eachNodeInfo[0][curNodeIndex][nodeStripeCount[curNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][curNodeIndex][nodeStripeCount[curNodeIndex]++] = curIndex;
                if (tmpMinCount == EC_N + MUL_FAIL_NUM - 1)
                    break;
            }
        }

        /* storage_fail */
        tmpMinCount = 0;
        for (int i = 0; i < EC_N; i++)
            if (eachStripeNodeIndex[k][i] < MUL_FAIL_START || eachStripeNodeIndex[k][i] > MUL_FAIL_START + MUL_FAIL_NUM - 1)
                continue;
            else
            {
                selectChunksIndexArr[k][tmpMinCount] = i;
                selectNodesIndexArr[k][tmpMinCount++] = eachStripeNodeIndex[k][i];                                                           // selectNodesIndex[0] is FailNodeIndex
                eachNodeInfo[0][eachStripeNodeIndex[k][i]][nodeStripeCount[eachStripeNodeIndex[k][i]]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][eachStripeNodeIndex[k][i]][nodeStripeCount[eachStripeNodeIndex[k][i]]++] = i;
            }
    }
    return;
}

/* Select the first k nodes with max weight among n nodes */
void selectNodesMulFullRecoverTENetE(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N], int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM])
{
    if (readUpDownBandwidth(uplinkBandwidth, downlinkBandwidth, BAND_LOCATION, EC_A) == EC_ERROR)
        ERROR_RETURN_VALUE("Fail readUpDownBandwidth");
    getMulDegradedReadWeightNodesTEN(uplinkBandwidth, nodeWeight, EC_A);

    printBand(EC_A);

    for (int k = 0; k < 2; k++)
        for (int i = 0; i < EC_A; i++)
            for (int j = 0; j < FULL_RECOVERY_STRIPE_MAX_NUM; j++)
                eachNodeInfo[k][i][j] = -1;

    for (int k = 0; k < stripeNum; k++)
        for (int i = 0; i < EC_N; i++)
        {
            selectNodesIndexArr[k][i] = -1;
            selectChunksIndexArr[k][i] = -1;
        }

    int nodeStripeCount[EC_A] = {0};
    int tmpWeightNum[EC_N], tmpIndexWeightNum[EC_N];
    for (int k = 0; k < stripeNum; k++)
    {
        /* storage_heal */
        for (int i = 0; i < EC_N; i++)
            tmpWeightNum[i] = nodeWeight[eachStripeNodeIndex[k][i]];
        getSortedMaxIndices(tmpWeightNum, tmpIndexWeightNum, EC_N);

        int tmpMinCount = MUL_FAIL_NUM;
        for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
        {
            int curIndex = tmpIndexWeightNum[i];
            int curNodeIndex = eachStripeNodeIndex[k][curIndex];
            if (curNodeIndex < MUL_FAIL_START || curNodeIndex > MUL_FAIL_START + MUL_FAIL_NUM - 1) // heal node
            {
                selectChunksIndexArr[k][tmpMinCount] = curIndex;
                selectNodesIndexArr[k][tmpMinCount++] = curNodeIndex;                                              // node
                eachNodeInfo[0][curNodeIndex][nodeStripeCount[curNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][curNodeIndex][nodeStripeCount[curNodeIndex]++] = curIndex;
                if (tmpMinCount == EC_K + MUL_FAIL_NUM)
                    break;
            }
        }

        /* storage_fail */
        tmpMinCount = 0;
        for (int i = 0; i < EC_N; i++)
            if (eachStripeNodeIndex[k][i] < MUL_FAIL_START || eachStripeNodeIndex[k][i] > MUL_FAIL_START + MUL_FAIL_NUM - 1)
                continue;
            else
            {
                selectChunksIndexArr[k][tmpMinCount] = i;
                selectNodesIndexArr[k][tmpMinCount++] = eachStripeNodeIndex[k][i];                                                           // selectNodesIndex[0] is FailNodeIndex
                eachNodeInfo[0][eachStripeNodeIndex[k][i]][nodeStripeCount[eachStripeNodeIndex[k][i]]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][eachStripeNodeIndex[k][i]][nodeStripeCount[eachStripeNodeIndex[k][i]]++] = i;
            }
    }
    return;
}

void selectNodesMulFullRecoverRNetE(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                    int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM])
{
    if (readUpDownBandwidth(uplinkBandwidth, downlinkBandwidth, BAND_LOCATION, EC_A) == EC_ERROR)
        ERROR_RETURN_VALUE("Fail readUpDownBandwidth");
    getMulDegradedReadWeightNodesR(uplinkBandwidth, downlinkBandwidth, nodeWeight, EC_A);

    printBand(EC_A);

    for (int k = 0; k < 2; k++)
        for (int i = 0; i < EC_A; i++)
            for (int j = 0; j < FULL_RECOVERY_STRIPE_MAX_NUM; j++)
                eachNodeInfo[k][i][j] = -1;

    for (int k = 0; k < stripeNum; k++)
        for (int i = 0; i < EC_N; i++)
        {
            selectNodesIndexArr[k][i] = -1;
            selectChunksIndexArr[k][i] = -1;
        }

    int nodeStripeCount[EC_A] = {0};
    int tmpWeightNum[EC_N], tmpIndexWeightNum[EC_N];
    for (int k = 0; k < stripeNum; k++)
    {
        /* storage_heal */
        for (int i = 0; i < EC_N; i++)
            tmpWeightNum[i] = nodeWeight[eachStripeNodeIndex[k][i]];
        getSortedMaxIndices(tmpWeightNum, tmpIndexWeightNum, EC_N);

        int tmpMinCount = MUL_FAIL_NUM;
        for (int i = 0; i < EC_K + MUL_FAIL_NUM; i++)
        {
            int curIndex = tmpIndexWeightNum[i];
            int curNodeIndex = eachStripeNodeIndex[k][curIndex];
            if (curNodeIndex < MUL_FAIL_START || curNodeIndex > MUL_FAIL_START + MUL_FAIL_NUM - 1) // heal node
            {
                selectChunksIndexArr[k][tmpMinCount] = curIndex;
                selectNodesIndexArr[k][tmpMinCount++] = curNodeIndex;                                              // node
                eachNodeInfo[0][curNodeIndex][nodeStripeCount[curNodeIndex]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][curNodeIndex][nodeStripeCount[curNodeIndex]++] = curIndex;
                if (tmpMinCount == EC_K + MUL_FAIL_NUM)
                    break;
            }
        }

        /* storage_fail */
        tmpMinCount = 0;
        for (int i = 0; i < EC_N; i++)
            if (eachStripeNodeIndex[k][i] < MUL_FAIL_START || eachStripeNodeIndex[k][i] > MUL_FAIL_START + MUL_FAIL_NUM - 1)
                continue;
            else
            {
                selectChunksIndexArr[k][tmpMinCount] = i;
                selectNodesIndexArr[k][tmpMinCount++] = eachStripeNodeIndex[k][i];                                                           // selectNodesIndex[0] is FailNodeIndex
                eachNodeInfo[0][eachStripeNodeIndex[k][i]][nodeStripeCount[eachStripeNodeIndex[k][i]]] = failStripeCountToAllStripeCount[k]; // 0-recordstripe,1-loc
                eachNodeInfo[1][eachStripeNodeIndex[k][i]][nodeStripeCount[eachStripeNodeIndex[k][i]]++] = i;
            }
    }
    return;
}
