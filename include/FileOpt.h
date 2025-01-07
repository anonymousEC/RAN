#ifndef FILEOPT_H
#define FILEOPT_H

typedef struct file_rw_s // Experimental data
{
    const char *filename;
    const char *mode;
    char *data;
    int size;
    int num;
} file_rw_t;
FILE *Fopen(const char *filename, const char *mode);
int readFileToBuffer(const char *filename, char *data, int size);

int openWriteFile(const char *filename, const char *mode, char *data, int size);
int openReadFile(const char *filename, const char *mode, char *data, int size);
int openReadOffsetFile(const char *filename, const char *mode, int offset, char *data, int size);
int openWriteMulFile(const char *filename, const char *mode, char **data, int size, int num);

int clearFileContent(const char *filename);
int getFileSize(const char *filename);
void *writeFile(void *arg);

void getECDir(char *ecDir);
void getHDFSFilename(char *bpName, char *bName, char *file_name);
void getECFileName(char *ecDir, char *path, char *name, char *ecFileName);
void getEachStripeNodeIndexFileName(char **argv, char *ecDir, int stripeIndex, char *eachStripeNodeIndexFileName);
void getDegradedReadExpFilename(char **argv, char *ecDir, char *dstFailFileName, char *dstHealFileName);
void getFullRecoverExpFilename(char **argv, char *ecDir, char *dstFailFileName, char *dstHealFileName);
#endif // FILE_OPT_H