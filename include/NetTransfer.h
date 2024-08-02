#ifndef NETTRANSFER_H
#define NETTRANSFER_H
#include "ECConfig.h"
#include <stdlib.h>

int clientInitNetwork(int *sockfd_p, int port, int nodeIndex);
int serverInitNetwork(int *server_fd_p, int port);
int Accept(int serverFd);
int getLocalIPLastNum(int *lastnum_p);
int getIPLastNum(struct sockaddr_in *p_client_addr, int *lastnum_p);
int AcceptAndGetNodeIndex(int serverFd, int *nodeIndexP);

void *Send(int sockfd, const void *buf, size_t len, const char *str);
void *Recv(int sockfd, void *buf, size_t len, char *str);
void *SendResponse(int sockfd);
void *RecvResponse(int sockfd);

void *SendExp(void *arg);
void *RecvExp(void *arg);
void *SendNetMetaChunk(void *arg);
void *RecvNetMetaChunk(void *arg);
void *SendNetMetaData(void *arg);
void *SendNetMeta(void *arg);

void *RecvDataSize(void *arg);
void *SendDataSize(void *arg);

#endif // NETWORK_TRANSFER_H
