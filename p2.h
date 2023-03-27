#pragma once
#include <stdio.h>
#include <stdlib.h>
#include "getword.h"
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "CHK.h"
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>
#include <linux/limits.h>

#define MAX_PATH  3841 // 4096-255 total max size is 4096, with file name being 255 per https://www.ibm.com/docs/en/spectrum-protect/8.1.9?topic=parameters-file-specification-syntax
#define MAX_ITEMS 100 // // maximum number of words per line
#define MAX_CHARS 25500 // size 100 * 255 = 25500 b/c 100 max possible words of max length 255
#define MAX_PIPES 10 // max number of pipes is 10

void pipeHelper();
void closeFileDescriptors(int* filedes, int numFDs);
void signalHandler(int signum);
int isMetaChar(char* buffPtr);
int parse();
int flagPresent();
