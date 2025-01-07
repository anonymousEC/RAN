#include "RAN.h"
#include <glpk.h>

/* from ClientMain.c */
extern int FailNodeIndex;
extern int stripeNum;
extern int eachStripeFailNodeNum[FULL_RECOVERY_STRIPE_MAX_NUM];
extern hdfs_info_t *HDFSInfo;

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

void RANGetBaNetO(int *Ba, int nodeNum)
{
    for (int i = 0; i < nodeNum; i++)
        Ba[i] = 1000; // Any positive integer, not too large
    return;
}

void RANGetBaNetE(int *Ba, int nodeNum)
{
    /* Get uplink and downlink bandwidh*/
    int uplinkBandwidth[EC_A_MAX], downlinkBandwidth[EC_A_MAX];
    int nodeWeight[EC_A_MAX], weightMaxIndex[EC_A_MAX];
    if (readUpDownBandwidth(uplinkBandwidth, downlinkBandwidth, BAND_LOCATION, nodeNum) == EC_ERROR)
        ERROR_RETURN_VALUE("Fail readUpDownBandwidth");
    getDegradedReadWeightNodesTEN(uplinkBandwidth, nodeWeight, nodeNum, FailNodeIndex);

    int count = 0;
    for (int i = 0; i < nodeNum; i++)
        if (i == FailNodeIndex)
            continue;
        else
            Ba[count++] = nodeWeight[i];
    printArrINT(nodeNum, uplinkBandwidth, "Uplink bandwidth");
    printArrINT(nodeNum, downlinkBandwidth, "Downlink bandwidth");
    printArrINT(nodeNum, nodeWeight, "nodeWeight");
    return;
}

void RANMulDegradedReadGetBaNetE(int *Ba, int nodeNum)
{
    /* Get uplink and downlink bandwidh*/
    int uplinkBandwidth[EC_A_MAX], downlinkBandwidth[EC_A_MAX];
    int nodeWeight[EC_A_MAX], weightMaxIndex[EC_A_MAX];
    if (readUpDownBandwidth(uplinkBandwidth, downlinkBandwidth, BAND_LOCATION, nodeNum) == EC_ERROR)
        ERROR_RETURN_VALUE("Fail readUpDownBandwidth");
    getMulDegradedReadWeightNodesTEN(uplinkBandwidth, nodeWeight, nodeNum);
    int count = 0;
    for (int i = 0; i < nodeNum; i++)
        if (i < MUL_FAIL_START || i > MUL_FAIL_START + MUL_FAIL_NUM - 1)
            Ba[count++] = nodeWeight[i];
    printArrINT(nodeNum, uplinkBandwidth, "Uplink bandwidth");
    printArrINT(nodeNum, downlinkBandwidth, "Downlink bandwidth");
    printArrINT(nodeNum, nodeWeight, "nodeWeight");
    return;
}

void RANDegradedReadCalBrTmax(int *Ba, int *Br, int *p_Tmax, int nodeNum)
{
    int mathcal_N = 0, mathcal_N_pre = -1;
    double Tmax, Tmax_pre, sumBr;

    for (int node = 0; node < nodeNum; node++) // Initialize Br,Tmax
        Br[node] = Ba[node];
    Tmax = 0;
    for (int node = 0; node < nodeNum; node++)
        Tmax += Br[node];
    Tmax /= EC_K;
    // printf("func-Tmax=%0.2f\n", Tmax);
    while (mathcal_N != mathcal_N_pre) // while N changes
    {
        mathcal_N_pre = mathcal_N;
        for (int node = 0; node < nodeNum; node++) // Check if Br exceeds Tmax for each node
            if (Br[node] != 0 && Ba[node] > Tmax)
            {
                Br[node] = 0;
                mathcal_N++;
            }
        Tmax_pre = Tmax;
        sumBr = 0;
        for (int node = 0; node < nodeNum; node++) // Recalculate sumBr only considering Br < Tmax
            if (Ba[node] <= Tmax_pre)
                sumBr += Br[node];
        Tmax = sumBr / (EC_K - mathcal_N);
        // printf("func-Tmax=%0.2f\n", Tmax);
    }
    for (int node = 0; node < nodeNum; node++)
        if (Br[node] == 0)
            Br[node] = Tmax;
    (*p_Tmax) = Tmax;
    return;
}

int RANFullRecoverSolveLP(int selectNodesIndexArr[][EC_N + MUL_FAIL_NUM - 1], int Ba[EC_A - 1], int Dij[][EC_N - 1], int *p_Tmax)
{
    int i, j, k;
    int nodeNum = EC_A - 1;

    /* get limit */
    double DijDouble[stripeNum][EC_A - 1], Cij[stripeNum][EC_A - 1];
    for (int i = 0; i < stripeNum; i++)
        for (int j = 0; j < EC_A - 1; j++)
            Cij[i][j] = 0.0;
    for (int i = 0; i < stripeNum; i++)
        for (int j = 1; j < EC_N; j++)
        {
            int selectNode = selectNodesIndexArr[i][j];
            if (selectNode < FailNodeIndex)
                Cij[i][selectNode] = 1.0;
            else
                Cij[i][selectNode - 1] = 1.0;
        }

    glp_prob *lp;
    lp = glp_create_prob();
    glp_set_prob_name(lp, "RAN_throughput_FR");
    glp_set_obj_dir(lp, GLP_MIN); // Set to minimize the objective function

    /* Add right-hand side results for constraints */
    glp_add_rows(lp, stripeNum + nodeNum);
    for (i = 1; i <= stripeNum; i++)
        glp_set_row_bnds(lp, i, GLP_FX, EC_K, EC_K); // Set equality constraints
    for (i = stripeNum + 1; i <= stripeNum + nodeNum; i++)
        glp_set_row_bnds(lp, i, GLP_UP, 0.0, 0.0); // Set inequality constraints: GLP_UP is <, nodeTime <= lowestTime(BestTime)

    /* Add variables range and Objective function */
    glp_add_cols(lp, stripeNum * nodeNum + 1); // variables num: the last variable is 1/T, other is DijDouble
    for (i = 1; i <= stripeNum * nodeNum; i++)
    {
        double upper_bound = Cij[(i - 1) / nodeNum][(i - 1) % nodeNum];
        if (upper_bound == 0.0)
            upper_bound = 1e-8;
        glp_set_col_bnds(lp, i, GLP_DB, -1e-8, upper_bound);
    }
    glp_set_col_bnds(lp, stripeNum * nodeNum + 1, GLP_LO, 0.0, 0.0);
    glp_set_obj_coef(lp, stripeNum * nodeNum + 1, 1.0); // Objective function: min 1/T = max T

    /* Set constraint matrix */
    int *ia = (int *)Calloc(1 + stripeNum * nodeNum * 7, sizeof(int));
    int *ja = (int *)Calloc(1 + stripeNum * nodeNum * 7, sizeof(int));
    double ar[1 + stripeNum * nodeNum * 7];

    k = 1;
    for (i = 0; i < stripeNum; i++) // For equality constraints
        for (j = 0; j < nodeNum; j++)
        {
            ia[k] = i + 1;
            ja[k] = i * nodeNum + j + 1;
            ar[k] = 1.0;
            k++;
        }

    for (i = 0; i < nodeNum; i++) // For inequality constraints
    {
        for (j = 0; j < stripeNum; j++)
        {
            ia[k] = stripeNum + i + 1;
            ja[k] = j * nodeNum + i + 1;
            ar[k] = 1.0;
            k++;
        }
        ia[k] = stripeNum + i + 1;
        ja[k] = stripeNum * nodeNum + 1;
        ar[k] = -1.0 * stripeNum * Ba[i];
        k++;
    }

    glp_load_matrix(lp, k - 1, ia, ja, ar);

    /* Solve LP */
    // glp_simplex(lp, NULL);
    // glp_smcp param;
    // glp_init_smcp(&param);
    // param.tol_bnd = 1e-10;
    // param.tol_dj = 1e-10;
    // param.tol_piv = 1e-10;
    // param.meth = GLP_DUAL;
    // glp_simplex(lp, &param);

    glp_smcp param;
    glp_init_smcp(&param);
    param.msg_lev = GLP_MSG_ALL;
    param.presolve = GLP_ON;
    param.meth = GLP_DUAL;
    glp_simplex(lp, &param);

    /* Get the result */
    double z = glp_get_obj_val(lp);
    if (z == GLP_UNDEF)
    {
        glp_delete_prob(lp);
        return EC_ERROR;
    }
    else
    {
        for (i = 0; i < stripeNum; i++)
            for (j = 0; j < nodeNum; j++)
                DijDouble[i][j] = glp_get_col_prim(lp, i * nodeNum + j + 1);
        (*p_Tmax) = 1 / z;
    }
    glp_delete_prob(lp);

    /* get int Dij[EC_A-1] */
    int DijTmp[stripeNum][EC_A - 1];
    for (int i = 0; i < stripeNum; i++)
        for (int j = 0; j < EC_A - 1; j++)
            DijTmp[i][j] = DijDouble[i][j] * CHUNK_SIZE;

    /* get Dij[EC_N-1] */
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N - 1]; // When e, selectNodesIndexArr is out of order
    getIncreasingNodesIndexArrN(selectNodesIndexArr, increasingNodesIndexArr, stripeNum, 1);
    for (int i = 0; i < stripeNum; i++)
    {
        for (int j = 0; j < EC_N - 1; j++)
            Dij[i][j] = increasingNodesIndexArr[i][j] < FailNodeIndex ? DijTmp[i][increasingNodesIndexArr[i][j]] : DijTmp[i][increasingNodesIndexArr[i][j] - 1];
        for (int j = 0; j < EC_N - 1; j++)
            if (Dij[i][j] < SLICE_SIZE / 64) // Floating point error
                Dij[i][j] = 0;
            else if (Dij[i][j] > CHUNK_SIZE)
                Dij[i][j] = CHUNK_SIZE;
        int remainSize = EC_K * CHUNK_SIZE - sumArr(Dij[i], EC_N - 1); // Floating point error
        for (int j = 0; j < EC_N - 1 && remainSize != 0; j++)
            if (Dij[i][j] >= SLICE_SIZE / 64 && Dij[i][j] < CHUNK_SIZE)
                if (Dij[i][j] + remainSize > CHUNK_SIZE)
                {
                    remainSize = remainSize - (CHUNK_SIZE - Dij[i][j]);
                    Dij[i][j] = CHUNK_SIZE;
                }
                else
                {
                    Dij[i][j] += remainSize;
                    remainSize = 0;
                }
    }

    /* test */
    // double DijColSum[EC_A - 1] = {0.0}, CijColSum[EC_A - 1] = {0.0}, caltime[EC_A - 1], DijRowSum[FULL_RECOVERY_STRIPE_MAX_NUM] = {0.0};
    // for (i = 0; i < stripeNum; i++)
    //     for (j = 0; j < nodeNum; j++)
    //     {
    //         DijColSum[j] += DijDouble[i][j];
    //         CijColSum[j] += Cij[i][j];
    //         DijRowSum[i] += DijDouble[i][j];
    //     }
    // for (j = 0; j < nodeNum; j++)
    //     caltime[j] = DijColSum[j] / Ba[j];
    // print2ArrDOUBLE(stripeNum, nodeNum, Cij, "Cij");
    // printf("Optimal value: z = %f,Tmax = %f\n", z, 1 / z);
    // print2ArrDOUBLE(stripeNum, nodeNum, DijDouble, "DijDouble");
    // printArrDOUBLE(nodeNum, DijColSum, "DijColSum");
    // printArrDOUBLE(nodeNum, CijColSum, "CijColSum");
    // printArrDOUBLE(nodeNum, caltime, "caltime");
    // printArrDOUBLE(nodeNum, DijRowSum, "DijRowSum");
    // print2ArrINT(stripeNum, EC_A - 1, DijTmp, "DijTmp[EC_A-1]");
    // print2ArrINT(stripeNum, EC_N - 1, Dij, "Dij[EC_N-1]");
    return EC_OK;
}

int RANMulFullRecoverSolveLP(int selectNodesIndexArr[][EC_N + MUL_FAIL_NUM - 1], int Ba[EC_A - MUL_FAIL_NUM], int Dij[][EC_N - 1], int *p_Tmax)
{
    int i, j, k;
    int nodeNum = EC_A - MUL_FAIL_NUM;

    /* get limit */
    double DijDouble[stripeNum][EC_A - MUL_FAIL_NUM], Cij[stripeNum][EC_A - MUL_FAIL_NUM];
    for (int i = 0; i < stripeNum; i++)
        for (int j = 0; j < EC_A - MUL_FAIL_NUM; j++)
            Cij[i][j] = 0.0;
    for (int i = 0; i < stripeNum; i++)
        for (int j = MUL_FAIL_NUM; j < EC_N + MUL_FAIL_NUM - 1 && selectNodesIndexArr[i][j] != -1; j++)
        {
            if (selectNodesIndexArr[i][j] < MUL_FAIL_START)
                Cij[i][selectNodesIndexArr[i][j]] = 1.0;
            else
                Cij[i][selectNodesIndexArr[i][j] - MUL_FAIL_NUM] = 1.0;
        }

    glp_prob *lp;
    lp = glp_create_prob();
    glp_set_prob_name(lp, "RAN_throughput_FR");
    glp_set_obj_dir(lp, GLP_MIN); // Set to minimize the objective function

    /* Add right-hand side results for constraints */
    glp_add_rows(lp, stripeNum + nodeNum);
    for (i = 1; i <= stripeNum; i++)
        glp_set_row_bnds(lp, i, GLP_FX, EC_K, EC_K); // Set equality constraints
    for (i = stripeNum + 1; i <= stripeNum + nodeNum; i++)
        glp_set_row_bnds(lp, i, GLP_UP, 0.0, 0.0); // Set inequality constraints: GLP_UP is <, nodeTime <= lowestTime(BestTime)

    /* Add variables range and Objective function */
    glp_add_cols(lp, stripeNum * nodeNum + 1); // variables num: the last variable is 1/T, other is DijDouble
    for (i = 1; i <= stripeNum * nodeNum; i++)
    {
        double upper_bound = Cij[(i - 1) / nodeNum][(i - 1) % nodeNum];
        if (upper_bound == 0.0)
            upper_bound = 1e-16;
        glp_set_col_bnds(lp, i, GLP_DB, -1e-16, upper_bound);
        glp_set_obj_coef(lp, i, 0.0);
    }
    glp_set_col_bnds(lp, stripeNum * nodeNum + 1, GLP_LO, 0.0, 0.0);
    glp_set_obj_coef(lp, stripeNum * nodeNum + 1, 1.0); // Objective function: min 1/T = max T

    /* Set constraint matrix */
    int *ia = (int *)Calloc(1 + stripeNum * nodeNum * 7, sizeof(int));
    int *ja = (int *)Calloc(1 + stripeNum * nodeNum * 7, sizeof(int));
    double ar[1 + stripeNum * nodeNum * 7];

    k = 1;
    for (i = 0; i < stripeNum; i++) // For equality constraints
        for (j = 0; j < nodeNum; j++)
        {
            ia[k] = i + 1;
            ja[k] = i * nodeNum + j + 1;
            ar[k] = 1.0;
            k++;
        }

    for (i = 0; i < nodeNum; i++) // For inequality constraints
    {
        for (j = 0; j < stripeNum; j++)
        {
            ia[k] = stripeNum + i + 1;
            ja[k] = j * nodeNum + i + 1;
            ar[k] = 1.0;
            k++;
        }
        ia[k] = stripeNum + i + 1;
        ja[k] = stripeNum * nodeNum + 1;
        ar[k] = -1.0 * stripeNum * Ba[i];
        k++;
    }

    glp_load_matrix(lp, k - 1, ia, ja, ar);

    /* Solve LP */
    // glp_simplex(lp, NULL);
    glp_smcp param;
    glp_init_smcp(&param);
    param.msg_lev = GLP_MSG_ALL;
    param.presolve = GLP_ON;
    param.meth = GLP_DUAL;
    glp_simplex(lp, &param);

    /* Get the result */
    double z = glp_get_obj_val(lp);
    if (z == GLP_UNDEF)
    {
        glp_delete_prob(lp);
        return EC_ERROR;
    }
    else
    {
        for (i = 0; i < stripeNum; i++)
            for (j = 0; j < nodeNum; j++)
                DijDouble[i][j] = glp_get_col_prim(lp, i * nodeNum + j + 1);
        (*p_Tmax) = 1 / z;
    }

    glp_delete_prob(lp);

    /* get int Dij[EC_A-1] */
    int DijTmp[stripeNum][EC_A - MUL_FAIL_NUM];
    for (int i = 0; i < stripeNum; i++)
        for (int j = 0; j < EC_A - MUL_FAIL_NUM; j++)
            DijTmp[i][j] = DijDouble[i][j] * CHUNK_SIZE;

    // /* get Dij[EC_N-MUL_FAIL_NUM] */
    for (int i = 0; i < stripeNum; i++)
    {
        int cur_loc;
        for (int j = 0; j < EC_N - 1; j++)
        {
            if (selectNodesIndexArr[i][j + MUL_FAIL_NUM] == -1)
            {
                Dij[i][j] = -1;
                break;
            }
            else if (selectNodesIndexArr[i][j + MUL_FAIL_NUM] < MUL_FAIL_START)
                cur_loc = selectNodesIndexArr[i][j + MUL_FAIL_NUM];
            else
                cur_loc = selectNodesIndexArr[i][j + MUL_FAIL_NUM] - MUL_FAIL_NUM;
            Dij[i][j] = DijTmp[i][cur_loc];
        }
    }

    /* get Dij[EC_N-MUL_FAIL_NUM] */
    int increasingNodesIndexArr[FULL_RECOVERY_STRIPE_MAX_NUM][EC_N - 1]; // When e, selectNodesIndexArr is out of order
    getIncreasingNodesIndexArrN(selectNodesIndexArr, increasingNodesIndexArr, stripeNum, MUL_FAIL_NUM);
    for (int i = 0; i < stripeNum; i++)
    {
        for (int j = 0; j < EC_N - 1; j++)
            if (increasingNodesIndexArr[i][j] == -1)
                Dij[i][j] = 0;
            else
                Dij[i][j] = increasingNodesIndexArr[i][j] < FailNodeIndex ? DijTmp[i][increasingNodesIndexArr[i][j]] : DijTmp[i][increasingNodesIndexArr[i][j] - MUL_FAIL_NUM];
        for (int j = 0; j < EC_N - 1; j++)
            if (Dij[i][j] < SLICE_SIZE / 64) // Floating point error
                Dij[i][j] = 0;
            else if (Dij[i][j] > CHUNK_SIZE)
                Dij[i][j] = CHUNK_SIZE;
        int remainSize = EC_K * CHUNK_SIZE - sumArr(Dij[i], EC_N - 1); // Floating point error
        for (int j = 0; j < EC_N - 1 && remainSize != 0; j++)
            if (Dij[i][j] >= SLICE_SIZE / 64 && Dij[i][j] < CHUNK_SIZE)
                if (Dij[i][j] + remainSize > CHUNK_SIZE)
                {
                    remainSize = remainSize - (CHUNK_SIZE - Dij[i][j]);
                    Dij[i][j] = CHUNK_SIZE;
                }
                else
                {
                    Dij[i][j] += remainSize;
                    remainSize = 0;
                }
        // int remainSize = EC_K * CHUNK_SIZE - sumArr(Dij[i], EC_N - 1); // Floating point error
        // for (int j = 0; j < EC_N - 1 && remainSize != 0; j++)
        //     if (Dij[i][j] > 1024)
        //     {
        //         Dij[i][j] += remainSize;
        //         break;
        //     }
    }

    /* test */
    // double DijColSum[EC_A - MUL_FAIL_NUM] = {0.0}, CijColSum[EC_A - MUL_FAIL_NUM] = {0.0}, caltime[EC_A - MUL_FAIL_NUM], DijRowSum[FULL_RECOVERY_STRIPE_MAX_NUM] = {0.0};
    // for (i = 0; i < stripeNum; i++)
    //     for (j = 0; j < nodeNum; j++)
    //     {
    //         DijColSum[j] += DijDouble[i][j];
    //         CijColSum[j] += Cij[i][j];
    //         DijRowSum[i] += DijDouble[i][j];
    //     }
    // for (j = 0; j < nodeNum; j++)
    //     caltime[j] = DijColSum[j] / Ba[j];
    // print2ArrDOUBLE(stripeNum, nodeNum, Cij, "Cij");
    // printf("Optimal value: z = %f,Tmax = %f\n", z, 1 / z);
    // print2ArrDOUBLE(stripeNum, nodeNum, DijDouble, "DijDouble");
    // printArrDOUBLE(nodeNum, DijColSum, "DijColSum");
    // printArrDOUBLE(nodeNum, CijColSum, "CijColSum");
    // printArrDOUBLE(nodeNum, caltime, "caltime");
    // printArrDOUBLE(nodeNum, DijRowSum, "DijRowSum");
    // print2ArrINT(stripeNum, EC_A - MUL_FAIL_NUM, DijTmp, "DijTmp[EC_A-MUL_FAIL_NUM]");
    // print2ArrINT(stripeNum, EC_N - 1, Dij, "Dij[EC_N - 1]");
    return EC_OK;
}

void RANDegradedReadAllocateDataSize(int *Ds, int *Br, int Tmax, int nodeNum)
{
    for (int node = 0; node < nodeNum; node++)
        Ds[node] = (int)(1.0 * Br[node] / Tmax * CHUNK_SIZE);

    int remain = EC_K * CHUNK_SIZE - sumArr(Ds, nodeNum); // remain data
    for (int node = 0; node < nodeNum; node++)
        if (remain + Ds[node] <= CHUNK_SIZE)
        {
            Ds[node] += remain;
            break;
        }
    return;
}

int RANDegradedReadFillDataBuckets(int twoBucketsSliceSizeInfo[][6], int Ds[], int size)
{
    // twoBucketsSliceSizeInfo[]={position_start1,data_size1,databuket_index1,position_start2,data_size1,databuket_index2}, for each node, node data at most have 2 postion and 2 data

    /* check data bucket */
    if (sumArr(Ds, size) != EC_K * CHUNK_SIZE)
        ERROR_RETURN_VALUE("NEW: error allocation data size\n");

    for (int i = 0; i < size; i++)
        for (int j = 0; j < 6; j++)
            twoBucketsSliceSizeInfo[i][j] = -1;

    /* fill data bucket */
    int dataBuketCount = 0;
    twoBucketsSliceSizeInfo[0][0] = 0;
    twoBucketsSliceSizeInfo[0][1] = Ds[0];
    twoBucketsSliceSizeInfo[0][2] = dataBuketCount;
    for (int i = 1; i < size; i++)
    {
        int curPosition, remainSize;
        if (twoBucketsSliceSizeInfo[i - 1][3] == -1) // data bucket cur position
            curPosition = twoBucketsSliceSizeInfo[i - 1][0] + twoBucketsSliceSizeInfo[i - 1][1];
        else
            curPosition = twoBucketsSliceSizeInfo[i - 1][3] + twoBucketsSliceSizeInfo[i - 1][4];
        remainSize = CHUNK_SIZE - curPosition;
        if (remainSize >= Ds[i]) // data bucket can accommodate
        {
            twoBucketsSliceSizeInfo[i][0] = curPosition;
            twoBucketsSliceSizeInfo[i][1] = Ds[i];
            twoBucketsSliceSizeInfo[i][2] = dataBuketCount;
        }
        else if (remainSize == 0) // last data bucket just fill full
        {
            dataBuketCount++;
            twoBucketsSliceSizeInfo[i][0] = 0;
            twoBucketsSliceSizeInfo[i][1] = Ds[i];
            twoBucketsSliceSizeInfo[i][2] = dataBuketCount;
        }
        else // data bucket can not accommodate
        {
            twoBucketsSliceSizeInfo[i][0] = curPosition;
            twoBucketsSliceSizeInfo[i][1] = remainSize;
            twoBucketsSliceSizeInfo[i][2] = dataBuketCount;
            dataBuketCount++;
            twoBucketsSliceSizeInfo[i][3] = 0;
            twoBucketsSliceSizeInfo[i][4] = Ds[i] - remainSize;
            twoBucketsSliceSizeInfo[i][5] = dataBuketCount;
        }
    }
    return EC_OK;
}

int RANFullRecoverFillDataBuckets(int twoBucketsSliceSizeInfo[][EC_N - 1][6], int Dij[][EC_N - 1])
{
    for (int k = 0; k < stripeNum; k++)
        if (RANDegradedReadFillDataBuckets(twoBucketsSliceSizeInfo[k], Dij[k], EC_N - 1) == EC_ERROR) // twoBucketsSliceSizeInfo[]={position_start1,data_size1,databuket_index1,position_start2,data_size1,databuket_index2}, for each node, node data at most have 2 postion and 2 data
            ERROR_RETURN_NULL("NEW: error allocation data size\n");
    return EC_OK;
}

int RANMulFullRecoverFillDataBuckets(int twoBucketsSliceSizeInfo[][EC_N - 1][6], int Dij[][EC_N - 1])
{
    for (int k = 0; k < stripeNum; k++)
        for (int i = 0; i < EC_N - 1; i++)
            for (int j = 0; j < 6; j++)
                twoBucketsSliceSizeInfo[k][i][j] = -1;
    for (int k = 0; k < stripeNum; k++)
        if (RANDegradedReadFillDataBuckets(twoBucketsSliceSizeInfo[k], Dij[k], EC_N - eachStripeFailNodeNum[k]) == EC_ERROR) // twoBucketsSliceSizeInfo[]={position_start1,data_size1,databuket_index1,position_start2,data_size1,databuket_index2}, for each node, node data at most have 2 postion and 2 data
            ERROR_RETURN_NULL("NEW: error allocation data size\n");
    return EC_OK;
}

int RANDegradedReadSliceAlig(int twoBucketsSliceIndexInfo[][6], int twoBucketsSliceSizeInfo[][6], int size)
{
    // twoBucketsSliceIndexInfo[]={position_start_index1,slice_num1,databuket_index1,position_start_index2,slice_num2,databuket_index2}

    for (int i = 0; i < size; i++)
        for (int j = 0; j < 6; j++)
            twoBucketsSliceIndexInfo[i][j] = twoBucketsSliceSizeInfo[i][j];

    /* Get whether the current data needs to be aligned, add 1*/
    int addValue[size][6], last = 0, all = 0;
    for (int i = 0; i < size; i++)
        for (int j = 0; j < 6; j += 3)
            if (twoBucketsSliceIndexInfo[i][j] != -1)
            {
                int value = twoBucketsSliceIndexInfo[i][j + 1] % SLICE_SIZE + last;
                addValue[i][j + 1] = 0;
                last = value;
                if (value >= SLICE_SIZE)
                {
                    addValue[i][j + 1] = 1;
                    last = value - SLICE_SIZE;
                }
            }

    /* Align data */
    int lastIndex = 0;
    for (int i = 0; i < size; i++)
        for (int j = 0; j < 6; j += 3)
            if (twoBucketsSliceIndexInfo[i][j] != -1)
            {
                twoBucketsSliceIndexInfo[i][j] = lastIndex;
                twoBucketsSliceIndexInfo[i][j + 1] = twoBucketsSliceIndexInfo[i][j + 1] / SLICE_SIZE + addValue[i][j + 1];
                lastIndex += twoBucketsSliceIndexInfo[i][j + 1];
                if (lastIndex == CHUNK_SIZE / SLICE_SIZE)
                    lastIndex = 0;
            }

    /* handle exception value: if datasize is zero */
    for (int i = 0; i < size; i++)
        for (int j = 0; j < 6; j += 3)
            if (twoBucketsSliceIndexInfo[i][j + 1] == 0)
            {
                twoBucketsSliceIndexInfo[i][j] = -1;
                twoBucketsSliceIndexInfo[i][j + 1] = -1;
                twoBucketsSliceIndexInfo[i][j + 2] = -1;
            }
    return EC_OK;
}

void RANFullRecoverSliceAlig(int twoBucketsSliceIndexInfo[][EC_N - 1][6], int twoBucketsSliceSizeInfo[][EC_N - 1][6])
{
    for (int k = 0; k < stripeNum; k++)
        RANDegradedReadSliceAlig(twoBucketsSliceIndexInfo[k], twoBucketsSliceSizeInfo[k], EC_N - 1); // RANDegradedReadSliceAlig: // twoBucketsSliceIndexInfo[]={position_start_index1,slice_num1,databuket_index1,position_start_index2,slice_num2,databuket_index2}
    return;
}

void RANMulFullRecoverSliceAlig(int twoBucketsSliceIndexInfo[][EC_N - 1][6], int twoBucketsSliceSizeInfo[][EC_N - 1][6])
{
    for (int k = 0; k < stripeNum; k++)
        for (int i = 0; i < EC_N - 1; i++)
            for (int j = 0; j < 6; j++)
                twoBucketsSliceIndexInfo[k][i][j] = -1;
    for (int k = 0; k < stripeNum; k++)
        RANDegradedReadSliceAlig(twoBucketsSliceIndexInfo[k], twoBucketsSliceSizeInfo[k], EC_N - eachStripeFailNodeNum[k]); // RANDegradedReadSliceAlig: // twoBucketsSliceIndexInfo[]={position_start_index1,slice_num1,databuket_index1,position_start_index2,slice_num2,databuket_index2}
    return;
}

int RANDegradedReadDivisionLines(int divisionLines[], int twoBucketsSliceIndexInfo[][6], int size)
{
    for (int i = 0; i < 2 * size; i++)
        divisionLines[i] = 0;
    int divisionNum = 1;

    /* save different offset */
    for (int i = 0; i < size; i++)
    {
        int tmp0 = twoBucketsSliceIndexInfo[i][0];
        int tmp2 = twoBucketsSliceIndexInfo[i][3];
        int j;
        for (j = 0; j < divisionNum; j++)
            if (divisionLines[j] == tmp0)
                break;
        if (j == divisionNum)
            divisionLines[divisionNum++] = tmp0;

        for (j = 0; j < divisionNum; j++)
            if (divisionLines[j] == tmp2)
                break;
        if (j == divisionNum)
            divisionLines[divisionNum++] = tmp2;
    }

    /* delete special value: -1 0 and add special value: chunksize */
    for (int i = 0; i < divisionNum; i++)
        if (divisionLines[i] == -1)
            for (int j = i + 1; j < divisionNum; j++)
                divisionLines[j - 1] = divisionLines[j];
    divisionNum--;
    for (int i = 0; i < divisionNum; i++)
        if (divisionLines[i] == 0)
            for (int j = i + 1; j < divisionNum; j++)
                divisionLines[j - 1] = divisionLines[j];
    divisionLines[divisionNum - 1] = CHUNK_SIZE / SLICE_SIZE;
    sortMinBubble(divisionLines, divisionNum);
    return divisionNum;
}

int RANFullRecoverDivisionLines(int divisionLines[][2 * EC_N], int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionNum[])
{
    for (int k = 0; k < stripeNum; k++)
        divisionNum[k] = RANDegradedReadDivisionLines(divisionLines[k], twoBucketsSliceIndexInfo[k], EC_N - 1);
    return;
}

int RANMulFullRecoverDivisionLines(int divisionLines[][2 * EC_N], int twoBucketsSliceIndexInfo[][EC_N - 1][6], int divisionNum[])
{
    for (int k = 0; k < stripeNum; k++)
        for (int i = 0; i < 2 * EC_N; i++)
            divisionLines[k][i] = 0;
    for (int k = 0; k < stripeNum; k++)
        divisionNum[k] = RANDegradedReadDivisionLines(divisionLines[k], twoBucketsSliceIndexInfo[k], EC_N - eachStripeFailNodeNum[k]);
    return;
}

void RANDegradedReadSortPosIndex(int twoBucketsSliceIndexInfo[][6], int size)
{
    /* sort slice index: [i][0] is less than [i][3] if [i][3] has value */
    int tmp[3];
    for (int i = 0; i < size; i++)
        if (twoBucketsSliceIndexInfo[i][3] != -1 && twoBucketsSliceIndexInfo[i][0] > twoBucketsSliceIndexInfo[i][3])
            for (int j = 0; j < 3; j++)
            {
                tmp[j] = twoBucketsSliceIndexInfo[i][j];
                twoBucketsSliceIndexInfo[i][j] = twoBucketsSliceIndexInfo[i][j + 3];
                twoBucketsSliceIndexInfo[i][j + 3] = tmp[j];
            }
    return;
}

void RANFullRecoverSortPosIndex(int twoBucketsSliceIndexInfo[][EC_N - 1][6])
{
    for (int k = 0; k < stripeNum; k++)
        RANDegradedReadSortPosIndex(twoBucketsSliceIndexInfo[k], EC_N - 1);
    return;
}

void RANMulFullRecoverSortPosIndex(int twoBucketsSliceIndexInfo[][EC_N - MUL_FAIL_NUM][6])
{
    for (int k = 0; k < stripeNum; k++)
        RANDegradedReadSortPosIndex(twoBucketsSliceIndexInfo[k], EC_N - MUL_FAIL_NUM);
    return;
}
