#include "ECEncode.h"

/* isa base without accelerate */

int gfMatrixMultiply(unsigned char *mat1, unsigned char *mat2, unsigned char *result, int row1, int col1, int row2, int col2)
{
    if (col1 != row2)
        ERROR_RETURN_VALUE("col1!=row2");
    for (int i = 0; i < row1; i++)
        for (int j = 0; j < col2; j++)
        {
            result[i * col2 + j] = 0;
            for (int k = 0; k < col1; k++)
                result[i * col2 + j] ^= gf_mul(mat1[i * col1 + k], mat2[k * col2 + j]);
        }
    return EC_OK;
}

void ec_encode_data_base_old(int len, int srcs, int dests, unsigned char *v,
                             unsigned char **src, unsigned char **dest)
{
    unsigned char s;

    for (int l = 0; l < dests; l++) // Loop of ecM: get all parity chunks
    {
        for (int i = 0; i < len; i++) // Chunksize loop: get all bytes of a parity chunk
        {
            s = 0;
            for (int j = 0; j < srcs; j++) // Loop of ecK: Multiplication and addition to get one byte of a parity chunk
                s ^= gf_mul(src[j][i], v[j * 32 + l * srcs * 32 + 1]);

            dest[l][i] = s;
        }
    }
}

/* isa base without accelerate */
void gf_vect_dot_prod_base_old(int len, int vlen, unsigned char *v,
                               unsigned char **src, unsigned char *dest)
{
    int i, j;
    unsigned char s;
    for (i = 0; i < len; i++)
    {
        s = 0;
        for (j = 0; j < vlen; j++)
            s ^= gf_mul(src[j][i], v[j * 32 + 1]);

        dest[i] = s;
    }
}

/* A modified version: without accelerate */
void gf_vect_dot_prod_base_new(int len, int vlen, unsigned char *v,
                               unsigned char **src, unsigned char *dest)
{

    memset(dest, 0, len);
    unsigned char s;
    for (int j = 0; j < vlen; j++)
        for (int i = 0; i < len; i++)
            dest[i] ^= gf_mul(src[j][i], v[j * 32 + 1]);
}

/* isa base without accelerate */
int gf_vect_mul_base_old(int len, unsigned char *a,
                         unsigned char *src, unsigned char *dest)
{
    // 2nd element of table arr is ref value used to fill it in
    unsigned char c = a[1];

    // Len must be aligned to 32B
    if ((len % 32) != 0)
    {
        return -1;
    }

    while (len-- > 0)
        *dest++ = gf_mul(c, *src++);
    return 0;
}

/* A modified version: but still slow */
/* must src and dest % 32Byte==0 */
void gf_vect_dot_prod_new(int len, int vlen, unsigned char *v,
                          unsigned char **src, unsigned char *dest)
{

    memset(dest, 0, len);
    unsigned char *tmp_mul_chunk = (unsigned char *)Malloc(sizeof(unsigned char) * len);
    unsigned char s;
    for (int j = 0; j < vlen; j++)
    {
        gf_vect_mul(len, &v[j * 32], src[j], tmp_mul_chunk);
        for (int i = 0; i < len; i++)
            dest[i] ^= tmp_mul_chunk[i];
    }
    free(tmp_mul_chunk);
}

/* isa dot prod func = ec_encode_data */
void ec_encode_data_using_dot_prod(int len, int srcs, int dests, unsigned char *v,
                                   unsigned char **src, unsigned char **dest)
{
    for (int i = 0; i < dests; i++)
        gf_vect_dot_prod(len, srcs, v + i * srcs * 32, src, dest[i]); // Chunksize loop: get all bytes of a parity chunk: the row vector of the encoding matrix is dot multiplied by all data chunks
}

/* new dot prod func = ec_encode_data */
/* must src and dest % 32Byte==0 */
void ec_encode_data_using_dot_prod_new(int len, int srcs, int dests, unsigned char *v,
                                       unsigned char **src, unsigned char **dest)
{
    for (int i = 0; i < dests; i++)
        gf_vect_dot_prod_new(len, srcs, v + i * srcs * 32, src, dest[i]); // Chunksize loop: get all bytes of a parity chunk: the row vector of the encoding matrix is dot multiplied by all data chunks
                                                                          // gf_vect_dot_prod(len, srcs, v + i * srcs * 32, src, dest[i]); // Chunksize loop: get all bytes of a parity chunk: the row vector of the encoding matrix is dot multiplied by all data chunks
}
/* isa gf_vect_mul func = ec_encode_data */
/* must src and dest % 32Byte==0 */
void ec_encode_data_using_gf_vect_mul(int len, int srcs, int dests, unsigned char *v,
                                      unsigned char **src, unsigned char **dest)
{

    unsigned char *tmp_mul_chunk = (unsigned char *)Malloc(sizeof(unsigned char) * len * dests);
    unsigned char **tmp_dest = (unsigned char *)Malloc(sizeof(unsigned char) * dests);

    for (int i = 0; i < dests; i++)
    {
        tmp_dest[i] = tmp_mul_chunk + i * len;
        memset(dest[i], 0, len);
        for (int j = 0; j < srcs; j++)
        {
            gf_vect_mul(len, &v[i * srcs * 32 + j * 32], src[j], tmp_dest[i]); // Get the intermediate parity chunk
            for (int k = 0; k < len; k++)
                dest[i][k] ^= tmp_dest[i][k]; // parity chunk after XOR
        }
    }

    free(tmp_dest);
    free(tmp_mul_chunk);
}

void *sglThreadEncode(void *arg)
{
    mulThreadEncode_t *mul_thread_encode = (mulThreadEncode_t *)arg;
    ec_encode_data(mul_thread_encode->chunksize, mul_thread_encode->k, mul_thread_encode->needRepairDataNum, mul_thread_encode->g_tbls, (unsigned char **)mul_thread_encode->data, (unsigned char **)mul_thread_encode->coding);
    return NULL;
}

int mulThreadEncode(int chunksize, int k, int needRepairDataNum, unsigned char *g_tbls, char **data, char **coding)
{
    if (NUM_ENCODING_CPU > NUM_ENCODING_CPU_MAX || NUM_ENCODING_CPU < 0)
        ERROR_RETURN_VALUE("error NUM_ENCODING_CPU");

    pthread_t tid[NUM_ENCODING_CPU_MAX];
    int avg_size = chunksize / NUM_ENCODING_CPU;
    mulThreadEncode_t mul_thread_encode[NUM_ENCODING_CPU];

    for (int i = 0; i < NUM_ENCODING_CPU; i++)
    {
        if (i == NUM_ENCODING_CPU - 1)
            mul_thread_encode[i].chunksize = avg_size + chunksize % NUM_ENCODING_CPU;
        else
            mul_thread_encode[i].chunksize = avg_size;
        mul_thread_encode[i].k = k;
        mul_thread_encode[i].needRepairDataNum = needRepairDataNum;
        mul_thread_encode[i].g_tbls = g_tbls;
        mul_thread_encode[i].data = (char **)Malloc(sizeof(char *) * k);                   // data chunk
        mul_thread_encode[i].coding = (char **)Malloc(sizeof(char *) * needRepairDataNum); // coding chunk
        for (int j = 0; j < k; j++)
            mul_thread_encode[i].data[j] = data[j] + avg_size * i;
        for (int j = 0; j < needRepairDataNum; j++)
            mul_thread_encode[i].coding[j] = coding[j] + avg_size * i;
        pthreadCreate(&tid[i], NULL, sglThreadEncode, (void *)&mul_thread_encode[i]);
    }
    for (int i = 0; i < NUM_ENCODING_CPU; i++)
        pthreadJoin(tid[i], NULL);
    for (int i = 0; i < NUM_ENCODING_CPU; i++)
    {
        free(mul_thread_encode[i].data);
        free(mul_thread_encode[i].coding);
    }
    return EC_OK;
}

int encodeDataChunks(char **dataChunk, char **codingChunk)
{

    unsigned char *encode_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *g_tbls = (unsigned char *)Malloc(sizeof(unsigned char) * EC_K * EC_M * 32);

    gf_gen_rs_matrix(encode_matrix, EC_N, EC_K);
    ec_init_tables(EC_K, EC_M, &encode_matrix[EC_K * EC_K], g_tbls);

    mulThreadEncode(CHUNK_SIZE, EC_K, EC_M, g_tbls, (unsigned char **)dataChunk, (unsigned char **)codingChunk);

    free(encode_matrix);
    free(g_tbls);
    return EC_OK;
}

int repairDataChunks(int *healChunkIndex, int healChunkNum, int *failChunkIndex, int failChunkNum, char **dataChunk, char **codingChunk, char **repairData)
{

    if (healChunkNum < EC_K)
        ERROR_RETURN_VALUE("The number of heal chunks is less than EC_K");

    /* Create decoding arguments for ISA-L */
    unsigned char *encode_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *decode_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *invert_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *temp_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *g_tbls = (unsigned char *)Malloc(sizeof(unsigned char) * EC_K * EC_M * 32);

    gf_gen_rs_matrix(encode_matrix, EC_N, EC_K);

    /* Construct temp_matrix by add ok rows */
    for (int i = 0; i < EC_K; i++)
        for (int j = 0; j < EC_K; j++)
            temp_matrix[EC_K * i + j] = encode_matrix[EC_K * healChunkIndex[i] + j];

    /* get invert matrix */
    if (gf_invert_matrix(temp_matrix, invert_matrix, EC_K) < 0)
        ERROR_RETURN_VALUE("Fail gf_invert_matrix");

    /* get temp matrix and decode matrix for cal parity chunk */
    for (int i = 0; i < failChunkNum; i++)
        for (int j = 0; j < EC_K; j++)
            temp_matrix[EC_K * i + j] = encode_matrix[EC_K * failChunkIndex[i] + j];
    gfMatrixMultiply(temp_matrix, invert_matrix, decode_matrix, failChunkNum, EC_K, EC_K, EC_K);

    // Recover dataChunk
    ec_init_tables(EC_K, failChunkNum, decode_matrix, g_tbls);
    mulThreadEncode(CHUNK_SIZE, EC_K, failChunkNum, g_tbls, (unsigned char **)dataChunk, (unsigned char **)codingChunk);

    for (int i = 0; i < failChunkNum; i++)
        repairData[failChunkIndex[i]] = codingChunk[i];
    for (int i = 0; i < healChunkNum; i++)
        repairData[healChunkIndex[i]] = dataChunk[i];

    free(encode_matrix);
    free(decode_matrix);
    free(invert_matrix);
    free(temp_matrix);
    free(g_tbls);
    return EC_OK;
}

/* The chunks that need to be repaired must be all data chunks or all parity chunks. Call 2 times if mixed: : Here failChunkNum supports 1, other values are not tested */
int templateRepairDataChunksSlice(int *healChunkIndex, int healChunkNum, int *failChunkIndex, int failChunkNum, char **data, char **coding, char **repairData)
{

    if (healChunkNum < EC_K)
        ERROR_RETURN_VALUE("The number of heal chunks is less than EC_K");

    /* Create decoding arguments for ISA-L */
    unsigned char *encode_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *decode_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *invert_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *temp_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *g_tbls = (unsigned char *)Malloc(sizeof(unsigned char) * EC_K * EC_M * 32);

    gf_gen_rs_matrix(encode_matrix, EC_N, EC_K);

    /* Construct temp_matrix by add ok rows */
    for (int i = 0; i < EC_K; i++)
        for (int j = 0; j < EC_K; j++)
            temp_matrix[EC_K * i + j] = encode_matrix[EC_K * healChunkIndex[i] + j];

    /* get invert matrix */
    if (gf_invert_matrix(temp_matrix, invert_matrix, EC_K) < 0)
        ERROR_RETURN_VALUE("Fail gf_invert_matrix");

    /* get temp matrix and decode matrix for cal parity chunk */
    for (int i = 0; i < failChunkNum; i++)
        for (int j = 0; j < EC_K; j++)
            temp_matrix[EC_K * i + j] = encode_matrix[EC_K * failChunkIndex[i] + j];
    gfMatrixMultiply(temp_matrix, invert_matrix, decode_matrix, failChunkNum, EC_K, EC_K, EC_K);

    /* Construct an arr of decoding matrices for each node */
    unsigned char *decode_matrix_buffer = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K * 2);
    memset(decode_matrix_buffer, 0, EC_N * EC_K * 2);
    unsigned char **decode_matrix_arr = (unsigned char **)Malloc(sizeof(unsigned char *) * EC_K);
    for (int i = 0; i < EC_K; i++)
        decode_matrix_arr[i] = decode_matrix_buffer + i * EC_N * 2;

    for (int k = 0; k < EC_K; k++)
        for (int i = 0; i < failChunkNum; i++)
        {
            decode_matrix_arr[k][2 * i + 0] = 1;                           // for xor
            decode_matrix_arr[k][2 * i + 1] = decode_matrix[EC_K * i + k]; // for mul
        }

    /* Construct an arr of g_tbls for each node */
    unsigned char *g_tblsBuf = (unsigned char *)Calloc(2 * EC_M * 32 * EC_K, sizeof(unsigned char));
    unsigned char **g_tblsArr = (unsigned char **)Malloc(sizeof(unsigned char *) * EC_K);
    for (int i = 0; i < EC_K; i++)
        g_tblsArr[i] = g_tblsBuf + i * 2 * EC_M * 32;
    for (int i = 0; i < EC_K; i++)
        ec_init_tables(2, failChunkNum, decode_matrix_arr[i], g_tblsArr[i]);

    /* decoding: Here failChunkNum supports 1, other values are not tested */
    char **data_tmp = (char **)Malloc(sizeof(char *) * 2); // tmp data pointer
    char *chunk_tmp = (char *)Calloc(CHUNK_SIZE, sizeof(char));
    data_tmp[0] = chunk_tmp;
    for (int k = 0; k < failChunkNum; k++)
        for (int i = 0; i < EC_K; i++)
        {
            if (i == 0)
                data_tmp[0] = chunk_tmp;
            else
                memcpy(chunk_tmp, coding[k], CHUNK_SIZE);
            data_tmp[1] = data[i];
            data_tmp[0] = chunk_tmp;
            mulThreadEncode(CHUNK_SIZE, 2, 1, g_tblsArr[i], (unsigned char **)data_tmp, (unsigned char **)&coding[k]);
        }

    for (int i = 0; i < failChunkNum; i++)
        repairData[failChunkIndex[i]] = coding[i];
    for (int i = 0; i < healChunkNum; i++)
        repairData[healChunkIndex[i]] = data[i];

    free(chunk_tmp);
    free(data_tmp);
    delete2ArrUC(g_tblsBuf, g_tblsArr);
    free(decode_matrix_buffer);
    free(decode_matrix_arr);

    free(encode_matrix);
    free(decode_matrix);
    free(invert_matrix);
    free(temp_matrix);
    free(g_tbls);
    return EC_OK;
}

/* The chunks that need to be repaired must be all data chunks or all parity chunks. Call 2 times if mixed: : Here failChunkNum supports 1, other values are not tested */
int repairMatrixR(int *healChunkIndex, int healChunkNum, int *failChunkIndex, int failChunkNum, unsigned char **g_tblsArr)
{

    if (healChunkNum < EC_K)
        ERROR_RETURN_VALUE("The number of heal chunks is less than EC_K");

    /* Create decoding arguments for ISA-L */
    unsigned char *encode_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *decode_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *invert_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *temp_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);

    gf_gen_rs_matrix(encode_matrix, EC_N, EC_K);

    /* Construct temp_matrix by add ok rows */
    for (int i = 0; i < EC_K; i++)
        for (int j = 0; j < EC_K; j++)
            temp_matrix[EC_K * i + j] = encode_matrix[EC_K * healChunkIndex[i] + j];

    /* get invert matrix */
    if (gf_invert_matrix(temp_matrix, invert_matrix, EC_K) < 0)
        ERROR_RETURN_VALUE("Fail gf_invert_matrix");

    /* get temp matrix and decode matrix for cal parity chunk */
    for (int i = 0; i < failChunkNum; i++)
        for (int j = 0; j < EC_K; j++)
            temp_matrix[EC_K * i + j] = encode_matrix[EC_K * failChunkIndex[i] + j];
    gfMatrixMultiply(temp_matrix, invert_matrix, decode_matrix, failChunkNum, EC_K, EC_K, EC_K);

    /* Construct an arr of decoding matrices for each node */
    unsigned char *decode_matrix_buffer = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K * 2);
    memset(decode_matrix_buffer, 0, EC_N * EC_K * 2);
    unsigned char **decode_matrix_arr = (unsigned char **)Malloc(sizeof(unsigned char *) * EC_K);
    for (int i = 0; i < EC_K; i++)
        decode_matrix_arr[i] = decode_matrix_buffer + i * EC_N * 2;

    for (int k = 0; k < EC_K; k++)
        for (int i = 0; i < failChunkNum; i++)
        {
            decode_matrix_arr[k][2 * i + 0] = 1;                           // for xor
            decode_matrix_arr[k][2 * i + 1] = decode_matrix[EC_K * i + k]; // for mul
        }

    /* Construct an arr of g_tbls for each node */
    for (int i = 0; i < EC_K; i++)
        ec_init_tables(2, failChunkNum, decode_matrix_arr[i], g_tblsArr[i]);

    free(decode_matrix_buffer);
    free(decode_matrix_arr);

    free(encode_matrix);
    free(decode_matrix);
    free(invert_matrix);
    free(temp_matrix);

    return EC_OK;
}

int repairMatrixTE(int *healChunkIndex, int healChunkNum, int *failChunkIndex, int failChunkNum, unsigned char *g_tbls)
{

    if (healChunkNum < EC_K)
        ERROR_RETURN_VALUE("The number of heal chunks is less than EC_K");

    /* Create decoding arguments for ISA-L */
    unsigned char *encode_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *decode_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *invert_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *temp_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);

    gf_gen_rs_matrix(encode_matrix, EC_N, EC_K);

    /* Construct temp_matrix by add ok rows */
    for (int i = 0; i < EC_K; i++)
        for (int j = 0; j < EC_K; j++)
            temp_matrix[EC_K * i + j] = encode_matrix[EC_K * healChunkIndex[i] + j];

    /* get invert matrix */
    if (gf_invert_matrix(temp_matrix, invert_matrix, EC_K) < 0)
        ERROR_RETURN_VALUE("Fail gf_invert_matrix");

    /* get temp matrix and decode matrix for cal parity chunk */
    for (int i = 0; i < failChunkNum; i++)
        for (int j = 0; j < EC_K; j++)
            temp_matrix[EC_K * i + j] = encode_matrix[EC_K * failChunkIndex[i] + j];
    gfMatrixMultiply(temp_matrix, invert_matrix, decode_matrix, failChunkNum, EC_K, EC_K, EC_K);

    // Recover data
    ec_init_tables(EC_K, failChunkNum, decode_matrix, g_tbls);

    free(encode_matrix);
    free(decode_matrix);
    free(invert_matrix);
    free(temp_matrix);
    return EC_OK;
}

int repairMatrixN(int healChunkIndex[][EC_N], int healChunkNum[], int failChunkIndex[][EC_N], int failChunkNum[],
                  int divisionNum, unsigned char **g_tblsArr)
{

    /* Create decoding arguments for ISA-L */
    unsigned char *encode_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *decode_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *invert_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);
    unsigned char *temp_matrix = (unsigned char *)Malloc(sizeof(unsigned char) * EC_N * EC_K);

    int healChunkIndexTmp[EC_K], tmp_nok = EC_K;
    int lowb, upb; // Reachable lower bounds and unreachable upper bounds
    for (int k = 0; k < divisionNum; k++)
    {
        if (healChunkNum[k] < EC_K)
            ERROR_RETURN_VALUE("The number of heal chunks is less than EC_K");

        gf_gen_rs_matrix(encode_matrix, EC_N, EC_K);

        /* Construct temp_matrix by add ok rows */
        for (int i = 0; i < EC_K; i++)
            for (int j = 0; j < EC_K; j++)
                temp_matrix[EC_K * i + j] = encode_matrix[EC_K * healChunkIndex[k][i] + j];

        /* get invert matrix */
        if (gf_invert_matrix(temp_matrix, invert_matrix, EC_K) < 0)
            ERROR_RETURN_VALUE("Fail gf_invert_matrix");

        /* get temp matrix and decode matrix for cal parity chunk */
        for (int i = 0; i < failChunkNum[k]; i++)
            for (int j = 0; j < EC_K; j++)
                temp_matrix[EC_K * i + j] = encode_matrix[EC_K * failChunkIndex[k][i] + j];
        gfMatrixMultiply(temp_matrix, invert_matrix, decode_matrix, failChunkNum[k], EC_K, EC_K, EC_K);

        // Recover data
        ec_init_tables(EC_K, failChunkNum[k], decode_matrix, g_tblsArr[k]);
    }

    free(encode_matrix);
    free(decode_matrix);
    free(invert_matrix);
    free(temp_matrix);
    return EC_OK;
}
