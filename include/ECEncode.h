#ifndef ECENCODE_H
#define ECENCODE_H
#include "ECConfig.h"
/* encoding method */
void ec_encode_data_base_old(int len, int srcs, int dests, unsigned char *v,
                             unsigned char **src, unsigned char **dest);
void gf_vect_dot_prod_base_old(int len, int vlen, unsigned char *v,
                               unsigned char **src, unsigned char *dest);
void gf_vect_dot_prod_base_new(int len, int vlen, unsigned char *v,
                               unsigned char **src, unsigned char *dest);
int gf_vect_mul_base_old(int len, unsigned char *a,
                         unsigned char *src, unsigned char *dest);
void gf_vect_dot_prod_new(int len, int vlen, unsigned char *v,
                          unsigned char **src, unsigned char *dest);
void ec_encode_data_using_dot_prod(int len, int srcs, int dests, unsigned char *v,
                                   unsigned char **src, unsigned char **dest);
void ec_encode_data_using_dot_prod_new(int len, int srcs, int dests, unsigned char *v,
                                       unsigned char **src, unsigned char **dest);
void ec_encode_data_using_gf_vect_mul(int len, int srcs, int dests, unsigned char *v,
                                      unsigned char **src, unsigned char **dest);

/* common */
void *sglThreadEncode(void *arg);

int mulThreadEncode(int chunksize, int k, int needRepairDataNum, unsigned char *g_tbls, char **data, char **coding);

int encodeDataChunks(char **data, char **coding);

int repairDataChunks(int *healChunkIndex, int healChunkNum, int *failChunkIndex, int failChunkNum, char **data, char **coding, char **repairData);

int templateRepairDataChunksSlice(int *healChunkIndex, int healChunkNum, int *failChunkIndex, int failChunkNum, char **data, char **coding, char **repairData);

int repairMatrixR(int *healChunkIndex, int healChunkNum, int *failChunkIndex, int failChunkNum, unsigned char **g_tblsArr);

int repairMatrixTE(int *healChunkIndex, int healChunkNum, int *failChunkIndex, int failChunkNum, unsigned char *g_tbls);

int repairMatrixN(int healChunkIndex[][EC_N], int healChunkNum[], int failChunkIndex[][EC_N], int failChunkNum[],
                  int divisionNum, unsigned char **g_tblsArr);
#endif