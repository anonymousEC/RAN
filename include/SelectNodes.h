#ifndef SELECTNODES_H
#define SELECTNODES_H

#include "ECConfig.h"

void selectNodesRand(int *selectNodesIndex);
void selectNodesDegradedReadTRENNetO(int *selectNodesIndex, int *selectChunksIndex);
void selectNodesDegradedReadTENetE(int *selectNodesIndex, int *selectChunksIndex);
void selectNodesDegradedReadRNetE(int *selectNodesIndex, int *selectChunksIndex);
void selectNodesFullRecoverTENetO(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                  int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM]);
void selectNodesFullRecoverRNetO(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM]);
void selectNodesFullRecoverNNetO(int selectNodesIndexArr[][EC_N + MUL_FAIL_NUM - 1], int selectChunksIndexArr[][EC_N + MUL_FAIL_NUM - 1],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM]);
void selectNodesFullRecoverTENetE(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                  int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM]);
void selectNodesFullRecoverRNetE(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                 int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM]);

void selectNodesMulDegradedReadTRENNetO(int *selectNodesIndex, int *selectChunksIndex);
void selectNodesMulDegradedReadTENetE(int *selectNodesIndex, int *selectChunksIndex);
void selectNodesMulDegradedReadRNetE(int *selectNodesIndex, int *selectChunksIndex);
void selectNodesMulFullRecoverTENetO(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                     int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM]);
void selectNodesMulFullRecoverRNetO(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                    int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM]);
void selectNodesMulFullRecoverNNetO(int selectNodesIndexArr[][EC_N + MUL_FAIL_NUM - 1], int selectChunksIndexArr[][EC_N + MUL_FAIL_NUM - 1],
                                    int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM]);
void selectNodesMulFullRecoverTENetE(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                     int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM]);
void selectNodesMulFullRecoverRNetE(int selectNodesIndexArr[][EC_N], int selectChunksIndexArr[][EC_N],
                                    int eachNodeInfo[][EC_A][FULL_RECOVERY_STRIPE_MAX_NUM]);

#endif