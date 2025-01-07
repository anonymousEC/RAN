#include "ECConfig.h"

FILE *Fopen(const char *filename, const char *mode)
{
    FILE *fp = fopen(filename, mode);
    if (fp == NULL)
        ERROR_RETURN_NULL("Fail open file");
    return fp;
}

int readFileToBuffer(const char *filename, char *data, int size)
{
    FILE *fp = Fopen(filename, "rb");

    /* Read file to data */
    int size_fread;
    size_fread = fread(data, sizeof(char), size, fp);

    /* Padding data */
    if (size_fread < size)
    {
        for (int j = size_fread; j < size; j++)
            data[j] = '0';
    }
    fclose(fp);
    return EC_OK;
}

int openWriteFile(const char *filename, const char *mode, char *data, int size)
{
    FILE *fp = Fopen(filename, mode);
    if (fwrite(data, sizeof(char), (size_t)size, fp) != (size_t)size)
        ERROR_RETURN_VALUE("Fail write file");
    fclose(fp);
    return EC_OK;
}

int openReadFile(const char *filename, const char *mode, char *data, int size)
{
    FILE *fp = Fopen(filename, mode);
    if (fread(data, sizeof(char), (size_t)size, fp) != (size_t)size)
        ERROR_RETURN_VALUE("Fail read file");
    fclose(fp);
    return EC_OK;
}

int openReadOffsetFile(const char *filename, const char *mode, int offset, char *data, int size)
{
    FILE *fp = Fopen(filename, mode);
    if (fseek(fp, offset, SEEK_SET) != 0)
        ERROR_RETURN_VALUE("Error seeking in file");
    if (fread(data, sizeof(char), (size_t)size, fp) != (size_t)size)
        ERROR_RETURN_VALUE("Fail read file");
    fclose(fp);
    return EC_OK;
}

int openWriteMulFile(const char *filename, const char *mode, char **data, int size, int num)
{

    FILE *fp = Fopen(filename, mode);
    for (int i = 0; i < num; i++)
        if (fwrite(data[i], sizeof(char), (size_t)size, fp) != (size_t)size)
            ERROR_RETURN_VALUE("Fail write file");
    fclose(fp);
    return EC_OK;
}

int clearFileContent(const char *filename)
{
    FILE *fp = Fopen(filename, "wb");
    fclose(fp);
    return EC_OK;
}

int getFileSize(const char *filename)
{
    struct stat file_status;
    stat(filename, &file_status);
    return file_status.st_size;
}

void *writeFile(void *arg)
{
    file_rw_t *file_rw = (file_rw_t *)arg; // must free in this func
    if (openWriteMulFile(file_rw->filename, file_rw->mode, file_rw->data, file_rw->size, file_rw->num) == EC_ERROR)
        ERROR_RETURN_NULL("openWriteMulFile");
    free(file_rw);
    return NULL;
}

void getECDir(char *ecDir)
{
    char curDir[MAX_PATH_LEN] = {0};
    getcwd(curDir, sizeof(curDir));
    strncpy(ecDir, curDir, strlen(curDir) - strlen(SCRIPT_PATH)); // -6 to sub script/
    return;
}

void getECFileName(char *ecDir, char *path, char *name, char *ecFileName)
{
    sprintf(ecFileName, "%s%s%s", ecDir, path, name);
}

void getEachStripeNodeIndexFileName(char **argv, char *ecDir, int stripeIndex, char *eachStripeNodeIndexFileName)
{
    char tmpArgv3[MAX_PATH_LEN] = {0};
    sprintf(tmpArgv3, "%s%s%d", argv[3], "fr", stripeIndex);
    getECFileName(ecDir, FILE_STRIPE_BITMAP, tmpArgv3, eachStripeNodeIndexFileName);
}

void getHDFSFilename(char *bpName, char *bName, char *file_name)
{
    sprintf(file_name, "%s%s%s%s", HDFS_PATH1, bpName, HDFS_PATH2, bName);
}

void getDegradedReadExpFilename(char **argv, char *ecDir, char *dstFailFileName, char *dstHealFileName)
{
    getECFileName(ecDir, REPAIR_PATH, argv[2], dstFailFileName); // get dstFailFileName
    getECFileName(ecDir, WRITE_PATH, argv[3], dstHealFileName);  // get dstHealFileName
}

void getFullRecoverExpFilename(char **argv, char *ecDir, char *dstFailFileName, char *dstHealFileName)
{
    getECFileName(ecDir, REPAIR_RAND, argv[2], dstFailFileName); // get dstFailFileName
    getECFileName(ecDir, WRITE_RAND, argv[3], dstHealFileName);  // get dstHealFileName
}
