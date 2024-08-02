#include "ECConfig.h"

/* readUpDownBandwidth EC_N nodes */
int readUpDownBandwidth(int *uplinkBandwidth, int *downlinkBandwidth, int bandLocation, int nodeNum)
{
    if (bandLocation < 1 || bandLocation > NUM_BAND_MAX)
        ERROR_RETURN_VALUE("error bandLocation:bandLocation < 1 || bandLocation > NUM_BAND_MAX");

    /* File arguments */

    int fileSize; // size of file
    char band_filename[MAX_PATH_LEN] = {0};
    char curDir[MAX_PATH_LEN] = {0}, ecDir[MAX_PATH_LEN] = {0};
    getcwd(curDir, sizeof(curDir));
    strncpy(ecDir, curDir, strlen(curDir) - 6);                                    // -6 to sub script/
    sprintf(band_filename, "%s%s%s", ecDir, BANDWIDTH_PATH, "node_bandwidth.txt"); // get band_filename
    printf("band_filename = %s\n", band_filename);
    char line[MAX_LINE_LENGTH];
    FILE *band_fp = Fopen(band_filename, "r"); // src_file pointers

    /* Read and validate the identifier line */
    int nodeNumTmp, bandLocationTmp;
    while (fgets(line, MAX_LINE_LENGTH, band_fp) != NULL) // skip to nodeNum identifier
    {
        sscanf(line, "nodeNum %d", &nodeNumTmp);
        if (nodeNumTmp == EC_A_MAX)
            break;
    }
    while (fgets(line, MAX_LINE_LENGTH, band_fp) != NULL) // skip to identifier
    {
        sscanf(line, "bandLocation %d", &bandLocationTmp);
        if (bandLocationTmp == bandLocation)
            break;
    }
    /* Read uplink and downlink bandwidth data */
    if (fgets(line, MAX_LINE_LENGTH, band_fp) != NULL)
    {
        char *token = strtok(line, " ");
        for (int i = 0; i < nodeNumTmp; ++i)
        {
            if (token == NULL)
                ERROR_RETURN_VALUE("File format error: format uplink data.");
            sscanf(token, "%d", &uplinkBandwidth[i]);
            token = strtok(NULL, " ");
        }
    }
    else
        ERROR_RETURN_VALUE("File format error: no uplink data.");

    if (fgets(line, MAX_LINE_LENGTH, band_fp) != NULL)
    {
        char *token = strtok(line, " ");
        for (int i = 0; i < nodeNumTmp; ++i)
        {
            if (token == NULL)
                ERROR_RETURN_VALUE("File format error: format downlink data.");
            sscanf(token, "%d", &downlinkBandwidth[i]);
            token = strtok(NULL, " ");
        }
    }
    else
        ERROR_RETURN_VALUE("File format error: no downlink data.");

    fclose(band_fp); // Close the file
    return EC_OK;
}

/* getBandwidthBetweenNodes nodes */
void getBandwidthBetweenNodes(int *uplinkBandwidth, int *downlinkBandwidth, int **nodes_b_band)
{
    for (int i = 0; i < EC_N; i++)
        for (int j = 0; j < EC_N; j++)
            if (i == j)
                nodes_b_band[i][j] = INT_MAX;
            else
                nodes_b_band[i][j] = uplinkBandwidth[i] > downlinkBandwidth[j] ? downlinkBandwidth[j] : uplinkBandwidth[i];
    return;
}

/* get_nodes_weight rp: min(uplink_band, downlink_band) */
void getDegradedReadWeightNodesR(int *uplinkBandwidth, int *downlinkBandwidth, int *nodeWeight, int nodeNum, int FailNodeIndex)
{
    for (int i = 0; i < nodeNum; i++)
        nodeWeight[i] = uplinkBandwidth[i] > downlinkBandwidth[i] ? downlinkBandwidth[i] : uplinkBandwidth[i];
    nodeWeight[FailNodeIndex] = INT_MAX;
    return;
}

/* get_nodes_weight tra or netec or new: min(uplink_band, downlink_band) */
void getDegradedReadWeightNodesTEN(int *uplinkBandwidth, int *nodeWeight, int nodeNum, int FailNodeIndex)
{
    for (int i = 0; i < nodeNum; i++)
        nodeWeight[i] = uplinkBandwidth[i];
    nodeWeight[FailNodeIndex] = INT_MAX;
    return;
}

/* get_nodes_weight rp: min(uplink_band, downlink_band) */
void getMulDegradedReadWeightNodesR(int *uplinkBandwidth, int *downlinkBandwidth, int *nodeWeight, int nodeNum)
{
    for (int i = 0; i < nodeNum; i++)
        nodeWeight[i] = uplinkBandwidth[i] > downlinkBandwidth[i] ? downlinkBandwidth[i] : uplinkBandwidth[i];
    for (int i = MUL_FAIL_START; i < MUL_FAIL_START + MUL_FAIL_NUM; i++)
        nodeWeight[i] = INT_MAX;
    return;
}

/* get_nodes_weight tra or netec or new: min(uplink_band, downlink_band) */
void getMulDegradedReadWeightNodesTEN(int *uplinkBandwidth, int *nodeWeight, int nodeNum)
{
    for (int i = 0; i < nodeNum; i++)
        nodeWeight[i] = uplinkBandwidth[i];
    for (int i = MUL_FAIL_START; i < MUL_FAIL_START + MUL_FAIL_NUM; i++)
        nodeWeight[i] = INT_MAX;
    return;
}
