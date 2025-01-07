#include "HDFS.h"

/* from ClientMain.c */
extern int FailNodeIndex;
extern hdfs_info_t *HDFSInfo;

int getHDFSDegradedReadInfo(char *blockFileName)
{
    /* check node */
    for (int i = 0; i < HDFSInfo->bpNum; i++)
        for (int j = 0; j < EC_N; j++)
            if (HDFSInfo->bNodeIndex[i][j] >= EC_N + STORAGENODES_START_IP_ADDR ||
                HDFSInfo->bNodeIndex[i][j] < STORAGENODES_START_IP_ADDR)
                ERROR_RETURN_VALUE("node ip must STORAGENODES_START_IP_ADDR~STORAGENODES_START_IP_ADDR+EC_N-1: not support at present");
    int flag = 1;
    for (int i = 0; i < HDFSInfo->bpNum; i++)
        for (int j = 0; j < EC_N; j++)
            if (strncmp(HDFSInfo->bName[i][j], blockFileName, sizeof(char) * 24) == 0)
            {
                HDFSInfo->failBp = i;
                HDFSInfo->failChunkIndex = j;
                HDFSInfo->failNodeIndex = HDFSInfo->bNodeIndex[i][j] - STORAGENODES_START_IP_ADDR; // band and ip
                flag = 0;
                break;
            }

    if (flag)
        ERROR_RETURN_VALUE("cannot find blockFileName in hdfs info");
    for (int i = 0; i < EC_N; i++)
        HDFSInfo->chunkIndexToNodeIndex[i] = HDFSInfo->bNodeIndex[HDFSInfo->failBp][i] - STORAGENODES_START_IP_ADDR;
    for (int i = 0; i < EC_N; i++)
        HDFSInfo->nodeIndexToChunkIndex[HDFSInfo->chunkIndexToNodeIndex[i]] = i;
    return EC_OK;
}

int getHDFSFullRecoverInfo(char *blockFileName)
{
    int flag = 1, HDFSFailNode = -1, failBpCount = 0;
    for (int i = 0; i < HDFSInfo->bpNum; i++)
        for (int j = 0; j < EC_N; j++)
            if (strncmp(HDFSInfo->bName[i][j], blockFileName, sizeof(char) * 24) == 0)
            {
                HDFSFailNode = HDFSInfo->bNodeIndex[i][j];
                HDFSInfo->failNodeIndex = HDFSInfo->bNodeIndex[i][j] - STORAGENODES_START_IP_ADDR; // band and ip
                flag = 0;
                break;
            }
    if (flag)
        ERROR_RETURN_VALUE("cannot find blockFileName in hdfs info");

    char tmp[MAX_PATH_LEN] = {0};
    for (int i = 0; i < HDFSInfo->bpNum; i++)
        for (int j = 0; j < EC_N; j++)
            if (HDFSInfo->bNodeIndex[i][j] == HDFSFailNode)
            {
                HDFSInfo->failFullRecoverBp[failBpCount] = i;
                HDFSInfo->failFullRecoverChunkIndex[failBpCount] = j;
                for (int k = 0; k < EC_N; k++)
                    HDFSInfo->fullRecoverChunkIndexToNodeIndex[failBpCount][k] = HDFSInfo->bNodeIndex[i][k] - STORAGENODES_START_IP_ADDR;
                for (int k = 0; k < EC_N; k++)
                    HDFSInfo->fullRecoverNodeIndexToChunkIndex[failBpCount][HDFSInfo->fullRecoverChunkIndexToNodeIndex[failBpCount][k]] = k;
                for (int k = 0; k < EC_N; k++)
                {
                    int curNodeIndex = HDFSInfo->fullRecoverChunkIndexToNodeIndex[failBpCount][k];
                    getHDFSFilename(HDFSInfo->bpName, HDFSInfo->bName[i][k], tmp);
                }
                failBpCount++;
            }
    HDFSInfo->failBpNum = failBpCount;

    return EC_OK;
}

int getHDFSInfo()
{

    char buffer[MAX_LINE_LENGTH];
    char lines[MAX_LINES][MAX_LINE_LENGTH];
    int lineCount = 0;
    FILE *pipe;

#if (!HDFS_DEBUG)
    char cmd[] = "hdfs fsck / -files -blocks -locations";
    if (!(pipe = popen(cmd, "r")))
        ERROR_RETURN_VALUE("popen failed!");
#else
    if ((pipe = Fopen(HDFS_DEBUG_PATH, "r")) == EC_ERROR)
        ERROR_RETURN_VALUE("popen failed!");
#endif
    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
        if (lineCount < MAX_LINES)
        {
            strncpy(lines[lineCount], buffer, MAX_LINE_LENGTH);
            lines[lineCount][MAX_LINE_LENGTH - 1] = '\0';
            printf("%s", lines[lineCount]);
            lineCount++;
        }
    fclose(pipe);

    /* check k and m */
    int k = 0, m = 0, cellSize, tmpFlag = 0;

    for (int i = 0; i < lineCount; i++)
        if (strstr(lines[i], "erasure-coded: policy=RS-") != NULL)
        {
            sscanf(lines[i], "%*[^=]=RS-%d-%d-%dk", &k, &m, &cellSize);
            tmpFlag = 1;
            if (k != EC_K || m != EC_M)
                ERROR_RETURN_VALUE("error policy: not support more policy at present");
        }
    if (tmpFlag == 0)
        ERROR_RETURN_VALUE("error policy: not support not RS at present");
    HDFSInfo->ecK = k;
    HDFSInfo->ecM = m;
    HDFSInfo->cellSize = cellSize;

    /* check bp */
    int bpCount = 0;
    tmpFlag = 0;
    for (int i = 0; i < lineCount; i++)
        if (strstr(lines[i], "BP-") != NULL && strstr(lines[i], "erasure-coded: policy=RS-") == NULL)
        {
            bpCount++;
            char *BPStart = strstr(lines[i], "BP-");
            char *BPEnd = strchr(BPStart, ':');
            int len = BPEnd - BPStart;
            char tmpFileName[MAX_PATH_LEN];
            strncpy(tmpFileName, BPStart, len);
            if (tmpFlag == 0)
            {
                tmpFlag = 1;
                strncpy(HDFSInfo->bpName, BPStart, len);
                HDFSInfo->bpName[len] = '\0';
            }
            else if (strncmp(tmpFileName, HDFSInfo->bpName, len) != 0)
            {
                tmpFileName[len] = '\0';
                printf("h-%s,t-%s\n", HDFSInfo->bpName, tmpFileName);
                ERROR_RETURN_VALUE("error bpName: not support more bp at present");
            }
        }
    HDFSInfo->bpNum = bpCount;

    /* get len, block name, node, */
    int bpIndex = -1;
    for (int i = 0; i < lineCount; i++)
    {
        if (strstr(lines[i], "BP-") != NULL && strstr(lines[i], "erasure-coded: policy=RS-") == NULL)
        {
            bpIndex++;
            char *start = strstr(lines[i], "len=") + 4;
            char *end = strstr(start, " Live");
            int len = end - start;
            char str[MAX_PATH_LEN];
            strncpy(str, start, len);
            str[len] = '\0';
            HDFSInfo->len[bpIndex] = atoi(str);                                // len
            printf("HDFSInfo.len[%d]: %d\n", bpIndex, HDFSInfo->len[bpIndex]); // ych-test

            int bIndex = 0;
            start = strstr(lines[i], "[blk_") + 1;
            while (start != NULL) // block name
            {
                end = strchr(start, ':');
                len = end - start;
                strncpy(HDFSInfo->bName[bpIndex][bIndex], start, len);
                HDFSInfo->bName[bpIndex][bIndex][len] = '\0';
                printf("HDFSInfo->bName[%d][%d]: %s\n", bpIndex, bIndex, HDFSInfo->bName[bpIndex][bIndex]); // ych-test
                bIndex++;
                start = strstr(end, "blk_");
            }

            bIndex = 0;
            start = strstr(lines[i], "[");
            while (start != NULL) // node index
            {
                start++;
                for (int dotCount = 0; dotCount < 3; dotCount++)
                    start = strchr(start, '.') + 1;
                end = strchr(start, ':');
                len = end - start + 1;
                strncpy(str, start, len);
                str[len] = '\0';
                HDFSInfo->bNodeIndex[bpIndex][bIndex] = atoi(str);
                printf("HDFSInfo->bNodeIndex[%d][%d]: %d\n", bpIndex, bIndex, HDFSInfo->bNodeIndex[bpIndex][bIndex]); // ych-test
                bIndex++;
                start = strstr(end, "[");
            }
        }
    }

    printf("hdfs info:\n");
    printf("RS policy: ecK = %d, ecM = %d, cellSize = %d\n", HDFSInfo->ecK, HDFSInfo->ecM, HDFSInfo->cellSize);
    printf("Block Pool name = %s, Num =%d\n", HDFSInfo->bpName, HDFSInfo->bpNum);
    for (int i = 0; i < HDFSInfo->bpNum; i++)
    {
        printf("BP-%d: len=%d\n", i, HDFSInfo->len[i]);
        for (int j = 0; j < EC_N; j++)
        {
            printf("\tblock_filename[%d][%d]: %s\n", i, j, HDFSInfo->bName[i][j]);
            printf("\tblock_node[%d][%d]: %d\n", i, j, HDFSInfo->bNodeIndex[i][j]);
        }
    }

    /* check len */
    int checkLen = EC_K * CHUNK_SIZE;
    for (int i = 0; i < HDFSInfo->bpNum; i++)
        if (HDFSInfo->len[i] != checkLen)
            ERROR_RETURN_VALUE("error len!=EC_K * CHUNK_SIZE: not support at present");
    /* check node */
    for (int i = 0; i < HDFSInfo->bpNum; i++)
        for (int j = 0; j < EC_N; j++)
            if (HDFSInfo->bNodeIndex[i][j] >= EC_A + STORAGENODES_START_IP_ADDR ||
                HDFSInfo->bNodeIndex[i][j] < STORAGENODES_START_IP_ADDR)
                ERROR_RETURN_VALUE("node ip must STORAGENODES_START_IP_ADDR~STORAGENODES_START_IP_ADDR+EC_A-1: not support at present");

    return EC_OK;
}
