#ifndef BANDWIDTHWEIGHT_H
#define BANDWIDTHWEIGHT_H

int readUpDownBandwidth(int *uplinkBandwidth, int *downlinkBandwidth, int bandLocation, int nodeNum);
void getBandwidthBetweenNodes(int *uplinkBandwidth, int *downlinkBandwidth, int **nodes_b_band);
void getDegradedReadWeightNodesR(int *uplinkBandwidth, int *downlinkBandwidth, int *nodeWeight, int nodeNum, int FailNodeIndex);
void getDegradedReadWeightNodesTEN(int *uplinkBandwidth, int *nodeWeight, int nodeNum, int FailNodeIndex);
void getMulDegradedReadWeightNodesR(int *uplinkBandwidth, int *downlinkBandwidth, int *nodeWeight, int nodeNum);
void getMulDegradedReadWeightNodesTEN(int *uplinkBandwidth, int *nodeWeight, int nodeNum);
#endif