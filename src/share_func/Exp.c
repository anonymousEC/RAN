#include "ECConfig.h"

int find_max_exp(exp_t *exp_max, exp_t *expArr, int size)
{
    exp_max->totalTime = expArr[0].totalTime;
    exp_max->ioTime = expArr[0].ioTime;
    exp_max->calTime = expArr[0].calTime;
    exp_max->netTime = expArr[0].netTime;

    for (int i = 1; i < size; i++)
    {
        if (expArr[i].totalTime > exp_max->totalTime)
            exp_max->totalTime = expArr[i].totalTime;
        if (expArr[i].ioTime > exp_max->ioTime)
            exp_max->ioTime = expArr[i].ioTime;
        if (expArr[i].calTime > exp_max->calTime)
            exp_max->calTime = expArr[i].calTime;
        if (expArr[i].netTime > exp_max->netTime)
            exp_max->netTime = expArr[i].netTime;
    }
    return EC_OK;
}

void printConfigure()
{
    printf("Experiment Configure: EXP_TIMES = %d\n", EXP_TIMES);
    printf("\tEC:\tEC_K = %d, EC_M = %d, EC_N = %d, EC_A = %d,EC_W = %d\n\t\tCHUNK_SIZE = %dB[%dKB,%dMB], SLICE_SIZE=%dB[%dKB,%dMB]\n\t\tNUM_ENCODING_CPU = %d, FLAG_WRITE_REPAIR = %d\n",
           EC_K, EC_M, EC_N, EC_A, EC_W,
           CHUNK_SIZE, CHUNK_SIZE / 1024, CHUNK_SIZE / 1024 / 1024,
           SLICE_SIZE, SLICE_SIZE / 1024, SLICE_SIZE / 1024 / 1024,
           NUM_ENCODING_CPU, FLAG_WRITE_REPAIR);
    printf("\tFILE:\tWRITE = %s, READ = %s, REPAIR = %s, METADATA = %s\n",
           WRITE_PATH, READ_PATH, REPAIR_PATH, FILE_SIZE_PATH);
    printf("\tNODE:\tIP_PREFIX = %s, PNETDEVICE_IP_ADDR= %d, CLIENT_COORDINATOR_NAMENODE_IP_ADDR = %d\n\t\tSTORAGENODES_START_IP_ADDR = %d, FAIL_NODE_INDEX = %d\n",
           IP_PREFIX, PNETDEVICE_IP_ADDR, CLIENT_COORDINATOR_NAMENODE_IP_ADDR, STORAGENODES_START_IP_ADDR, FAIL_NODE_INDEX + STORAGENODES_START_IP_ADDR);
    printf("\tNET:\tMUL_PORT_NUM = %d, EC_CLIENT_STORAGENODES_PORT = %d\n\t\tEC_STORAGENODES_PORT = %d, EC_PND_STORAGENODES_PORT = %d, EC_PND_CLIENT_PORT = %d\n",
           MUL_PORT_NUM, EC_CLIENT_STORAGENODES_PORT, EC_STORAGENODES_PORT, EC_PND_STORAGENODES_PORT, EC_PND_CLIENT_PORT);
    PRINT_FLUSH;
    return;
}

void printExp(exp_t *expArr, int nodeNum, int expCount)
{
    exp_t *exp_max = (exp_t *)Malloc(sizeof(exp_t)); // only the func free
    find_max_exp(exp_max, expArr, nodeNum);

    printf("Experiment time_exp_index[%d]\n", expCount);
    for (int i = 0; i < nodeNum; i++)
        printf("[node-%d]: totalTime = %d ms, ioTime = %d ms, calTime = %d ms, netTime = %d ms\n",
               expArr[i].nodeIP, expArr[i].totalTime, expArr[i].ioTime, expArr[i].calTime, expArr[i].netTime);
    printf("[MAX]: totalTime = %d ms, ioTime = %d ms, calTime = %d ms, netTime = %d ms\n", exp_max->totalTime, exp_max->ioTime, exp_max->calTime, exp_max->netTime);

    printf("Experiment throughput_exp_index[%d]\n", expCount);
    int mb_chunksize = CHUNK_SIZE / 1024 / 1024;
    int tmp = mb_chunksize * 1000;

    for (int i = 0; i < nodeNum; i++)
        printf("[node-%d]: totalThroughout = %d MB/s, ioThroughout = %d MB/s, calThroughout = %d MB/s, netThroughout = %d MB/s\n",
               expArr[i].nodeIP,
               expArr[i].totalTime > 0 ? tmp / expArr[i].totalTime : 0,
               expArr[i].ioTime > 0 ? tmp / expArr[i].ioTime : 0,
               expArr[i].calTime > 0 ? tmp / expArr[i].calTime : 0,
               expArr[i].netTime > 0 ? tmp / expArr[i].netTime : 0);
    printf("[MAX]: totalThroughout = %d MB/s, ioThroughout = %d MB/s, calThroughout = %d MB/s, netThroughout = %d MB/s\n",
           exp_max->totalTime > 0 ? tmp / exp_max->totalTime : 0,
           exp_max->ioTime > 0 ? tmp / exp_max->ioTime : 0,
           exp_max->calTime > 0 ? tmp / exp_max->calTime : 0,
           exp_max->netTime > 0 ? tmp / exp_max->netTime : 0);
    PRINT_FLUSH;

    free(exp_max);
    return;
}

void printAvgExp(exp_t **exp2Arr, int arr_size, int nodeNum)
{
    exp_t *exp_avg = (exp_t *)Malloc(sizeof(exp_t) * nodeNum); // only the func free

    for (int i = 0; i < nodeNum; i++)
    {
        exp_avg[i].totalTime = 0;
        exp_avg[i].ioTime = 0;
        exp_avg[i].calTime = 0;
        exp_avg[i].netTime = 0;
    }

    for (int i = 0; i < nodeNum; i++)
    {
        for (int j = 0; j < arr_size; j++)
        {
            exp_avg[i].totalTime += exp2Arr[j][i].totalTime;
            exp_avg[i].ioTime += exp2Arr[j][i].ioTime;
            exp_avg[i].calTime += exp2Arr[j][i].calTime;
            exp_avg[i].netTime += exp2Arr[j][i].netTime;
        }
        exp_avg[i].totalTime /= EXP_TIMES;
        exp_avg[i].ioTime /= EXP_TIMES;
        exp_avg[i].calTime /= EXP_TIMES;
        exp_avg[i].netTime /= EXP_TIMES;
        exp_avg[i].nodeIP = exp2Arr[0][i].nodeIP;
    }

    printf("\n#################EXP AVG RESULT[times=%d]#################\n", EXP_TIMES);
    printExp(exp_avg, nodeNum, EXP_TIMES);
    free(exp_avg);
    return;
}

void stripeAvgExp(exp_t *expArr, int nodeNum, int stripeNum)
{
    printf("[AVG STRIPE RESULT]\n");
    for (int i = 0; i < nodeNum; i++)
    {
        expArr[i].totalTime /= stripeNum;
        expArr[i].ioTime /= stripeNum;
        expArr[i].calTime /= stripeNum;
        expArr[i].netTime /= stripeNum;
    }
    return;
}

int recvExp(exp_t *expArr, int *sockfdArr, int nodeNum)
{

    pthread_t tid[nodeNum];
    for (int i = 0; i < nodeNum; i++)
    {
        expArr[i].sockfd = sockfdArr[i];
        pthreadCreate(&tid[i], NULL, RecvExp, (void *)&expArr[i]);
    }
    for (int i = 0; i < nodeNum; i++)
        pthreadJoin(tid[i], NULL);
    return EC_OK;
}

void initExp(exp_t *exp, int clientFd, int nodeIP)
{
    exp->totalTime = 0;
    exp->ioTime = 0;
    exp->calTime = 0;
    exp->netTime = 0;
    exp->sockfd = clientFd;
    exp->nodeIP = nodeIP;
    return;
}