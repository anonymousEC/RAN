#ifndef EXP_H
#define EXP_H

typedef struct exp_s // Experimental data
{
    int sockfd; // network sockfd fd
    int totalTime;
    int ioTime;  // disk read/write time
    int calTime; // calculatation time
    int netTime; // network transfer time
    int nodeIP;
} exp_t;

void printConfigure();
void printExp(exp_t *expArr, int nodeNum, int expCount);
void printAvgExp(exp_t **exp2Arr, int arr_size, int num);
void stripeAvgExp(exp_t *expArr, int nodeNum, int stripeNum);
int recvExp(exp_t *expArr, int *sockfdArr, int num);
void initExp(exp_t *exp, int clientFd, int nodeIP);

#endif // EXP_H