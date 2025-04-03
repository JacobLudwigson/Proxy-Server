#ifndef CACHE_H
#define CACHE_H
#include <stdatomic.h>
#include <pthread.h>
#include <openssl/md5.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include "dynArr.h"
#include "httpUtils.h"
typedef struct fileEntry{
    unsigned char* hash;
    char* filename;
    time_t timestamp;
    int toDelete;
    pthread_mutex_t fileLock;
}fileEntry;
typedef struct RefreshArgs{
    DynamicArray* cache;
    int timeout;
}RefreshArgs;
void clear_cache_directory(const char *dirPath);
void init_cache(DynamicArray* cache);
void free_cache(DynamicArray* cache);
void compute_md5(const char* filename, unsigned char* hashBuffer);
int checkCache(char* filename, DynamicArray* cache);
void printCacheFilenames(DynamicArray* cache);
int insertIntoCache(char* filename, DynamicArray* cache);
void* refreshCache(void* args);
#endif