#include "ECConfig.h"

int clientInitNetwork(int *sockfd_p, int port, int nodeIndex)
{
    struct sockaddr_in serverAddr;
    int sockfd;

    /* Create sockfd */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        ERROR_RETURN_VALUE("Fail create sockfd");

    /* Create sockaddr */
    char ipAddr[16];
    sprintf(ipAddr, "%s%d", IP_PREFIX, STORAGENODES_START_IP_ADDR + nodeIndex);
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ipAddr, &serverAddr.sin_addr) <= 0)
        ERROR_RETURN_VALUE("Invalid IP address");

    /* Connect data datanode of ipAddr */
    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        printf("Error connecting to %s\n", ipAddr);
        ERROR_RETURN_VALUE("Error connecting");
    }
    *sockfd_p = sockfd;
    return EC_OK;
}

int serverInitNetwork(int *server_fd_p, int port)
{
    int serverFd;
    struct sockaddr_in serverAddr;

    /* create sockfd */
    if ((serverFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        ERROR_RETURN_VALUE(" Fail create server sockfd");

    /* can reuse */
    int reuse = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    /* bind address and port */
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(serverFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
        ERROR_RETURN_VALUE("Fail bind sockfd");

    /* listen sockfd */
    if (listen(serverFd, EC_A_MAX) == -1)
        ERROR_RETURN_VALUE("Fail listen sockfd");

    *server_fd_p = serverFd;
    return EC_OK;
}

int Accept(int serverFd)
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    return accept(serverFd, (struct sockaddr *)&client_addr, &client_addr_len);
}

int getLocalIPLastNum(int *lastnum_p)
{
    struct ifaddrs *ifAddrStruct = NULL;
    struct ifaddrs *ifa = NULL;
    void *tmpAddrPtr = NULL;

    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr->sa_family == AF_INET)
        { // IPv4 address
            tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            if (strncmp(addressBuffer, IP_PREFIX, 10) == 0)
            {
                char *lastNumStr = strrchr(addressBuffer, '.') + 1;
                *lastnum_p = atoi(lastNumStr);
                return EC_OK;
            }
        }
    }

    if (ifAddrStruct != NULL)
        freeifaddrs(ifAddrStruct);
    return EC_ERROR;
}

int getIPLastNum(struct sockaddr_in *p_client_addr, int *lastnum_p)
{

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(p_client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    if (strncmp(client_ip, IP_PREFIX, 10) == 0)
    {
        char *lastNumStr = strrchr(client_ip, '.') + 1;
        *lastnum_p = atoi(lastNumStr);
        return EC_OK;
    }
    return EC_ERROR;
}

int AcceptAndGetNodeIndex(int serverFd, int *nodeIndexP)
{
    int clientFd, ipLastNum;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    clientFd = accept(serverFd, (struct sockaddr *)&client_addr, &client_addr_len);
    getIPLastNum(&client_addr, &ipLastNum);
    (*nodeIndexP) = ipLastNum - STORAGENODES_START_IP_ADDR;
    return clientFd;
}

void *Send(int sockfd, const void *buf, size_t len, const char *str)
{
    if (send(sockfd, buf, len, 0) < 0)
        ERROR_RETURN_NULL(str);
    return NULL;
}
void *Recv(int sockfd, void *buf, size_t len, char *str)
{
    if (recv(sockfd, buf, len, MSG_WAITALL) < 0)
        ERROR_RETURN_NULL(str);
    return NULL;
}

void *SendResponse(int sockfd)
{
    int error_response = 1;
    if (send(sockfd, &error_response, sizeof(error_response), 0) < 0)
        ERROR_RETURN_NULL("send response");
    return NULL;
}
void *RecvResponse(int sockfd)
{
    int error_response = 0;
    if (recv(sockfd, &error_response, sizeof(error_response), MSG_WAITALL) < 0)
        ERROR_RETURN_NULL("recv response case 1");
    if (error_response == 0)
        ERROR_RETURN_NULL("recv response case 2");
    return NULL;
}

void *SendExp(void *arg)
{
    exp_t *exp = (exp_t *)arg;
    Send(exp->sockfd, exp, sizeof(exp_t), "send exp data");
    RecvResponse(exp->sockfd);
    return NULL;
}

void *RecvExp(void *arg)
{
    exp_t *exp = (exp_t *)arg;
    int sockfd = exp->sockfd;
    Recv(sockfd, exp, sizeof(exp_t), "recv exp data");
    SendResponse(sockfd);
    exp->sockfd = sockfd;
    return NULL;
}

/* pthread func: Used to send to or receive from multiple nodes */
void *SendNetMetaChunk(void *arg)
{
    netMeta_t *netMeta = (netMeta_t *)arg;
    Send(netMeta->sockfd, netMeta, sizeof(netMeta_t), "send netMeta");
    Send(netMeta->sockfd, netMeta->data, CHUNK_SIZE, "send chunk");
    RecvResponse(netMeta->sockfd);
    return NULL;
}

void *RecvNetMetaChunk(void *arg)
{
    netMeta_t *netMeta = (netMeta_t *)arg;
    Send(netMeta->sockfd, netMeta, sizeof(netMeta_t), "send netMeta");
    Recv(netMeta->sockfd, netMeta->data, CHUNK_SIZE, "recv chunk");
    SendResponse(netMeta->sockfd);
    return NULL;
}

void *SendNetMetaData(void *arg)
{
    netMeta_t *netMeta = (netMeta_t *)arg;
    Send(netMeta->sockfd, netMeta, sizeof(netMeta_t), "send netMeta");
    Send(netMeta->sockfd, netMeta->data, netMeta->size, "send data");
    RecvResponse(netMeta->sockfd);
    return NULL;
}

void *SendNetMeta(void *arg)
{
    netMeta_t *netMeta = (netMeta_t *)arg;
    Send(netMeta->sockfd, netMeta, sizeof(netMeta_t), "send netMeta");
    RecvResponse(netMeta->sockfd);
    return NULL;
}

void *RecvDataSize(void *arg)
{
    netMeta_t *netMeta = (netMeta_t *)arg;
    Recv(netMeta->sockfd, netMeta->data, netMeta->size, "recv data");
    SendResponse(netMeta->sockfd);
    return NULL;
}

void *SendDataSize(void *arg)
{
    netMeta_t *netMeta = (netMeta_t *)arg;
    Send(netMeta->sockfd, netMeta->data, netMeta->size, "send data");
    RecvResponse(netMeta->sockfd);
    return NULL;
}
