#ifndef HDFS_H
#define HDFS_H

#include "ECConfig.h"

int getHDFSDegradedReadInfo(char *blockFileName);
int getHDFSFullRecoverInfo(char *blockFileName);
int getHDFSInfo();

#endif