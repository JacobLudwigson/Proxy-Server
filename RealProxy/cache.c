#include "cache.h"
pthread_mutex_t mutex;
void clear_cache_directory(const char *dirPath) {
    DIR *dir = opendir(dirPath);
    if (!dir) {
        perror("opendir failed");
        return;
    }

    struct dirent *entry;
    char filePath[512];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full file path
        snprintf(filePath, sizeof(filePath), "%s/%s", dirPath, entry->d_name);

        if (remove(filePath) != 0) {
            perror("remove failed");
        }
    }

    closedir(dir);
}
void init_cache(DynamicArray* cache){
    clear_cache_directory("cache");
    da_init(cache);
    int result = pthread_mutex_init(&mutex, NULL);
    if (result != 0){
        perror("Error in mutex init!\n");
        exit(-1);
    }
}
void free_cache(DynamicArray* cache){
    for (long unsigned int i = 0; i < cache->size; i++){
        free(cache->items[i]);
    }
    da_free(cache);
}

void safe_filename(char *hex_str) {
    for (int i = 0; i < 16; i++) {
        if (hex_str[i] == '/') {
            hex_str[i] = '_';
        }
    }
}
void compute_md5(const char* filename, unsigned char* hashBuffer){
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, filename, strlen(filename));
    MD5_Final(hashBuffer, &ctx);
}
// Note to self: Keep an eye out for better ways to do this w/o needing a mutex.
// THEORECTICALLY since this is only reading I shouldnt need this, but its a sanity check for now.
int checkCache(DynamicArray* cache, unsigned char* hash){
    int index = -1;
    for (long unsigned int i = 0; i < cache->size; i++){
        fileEntry* currFile = cache->items[i];
        //Should probably do a sanity check on elements being within the pageTimeout timestamp here as well
        if (!memcmp(hash,currFile->hash,16)){
            if((currFile->toDelete==1)){
                da_remove(cache, i);
                return -1;
            }
            index = i;
            break;
        }
    }
    return index;
}
void printCacheFilenames(DynamicArray* cache) {
    for (unsigned int i = 0; i < cache->size; i++) {
        fileEntry* currFile = cache->items[i];
        if (currFile->filename != NULL) {
            printf("File %d: %s\n", i, currFile->filename);
        } else {
            printf("File %d: (null)\n", i);
        }
    }
}
/*
    Will use cache mutex lock to check if a given filename exists in the cache. If not, it creates one and inserts it.
    REMEMBER: The return value of this function is index of file. Negative if new file was created and Positive if file already existed.
    The return value is also offset by one (I.E. if indexing cache based off of the return value use cache[abs(retVal)-1])
*/
int insertIntoCache(char* url, DynamicArray* cache){
    unsigned char hash[16];
    char filename[MAX_FILENAME_SIZE] = FILEDIRECTORY;
    char httpStrippedUrl[MAX_URL_LENGTH];
    char requestedFile[MAX_URL_LENGTH];

    stripHttp(url, httpStrippedUrl);
    compute_md5(httpStrippedUrl, hash);
    extractReqFile(httpStrippedUrl, requestedFile);
    const char* extension = get_file_extension_or_default(requestedFile);
    strcat(filename,"/");
    safe_filename(hash);
    strncat(filename,(char*)hash,16);
    strcat(filename, extension);
    pthread_mutex_lock(&mutex);//=========================/
        int retVal = checkCache(cache, hash);
        if (retVal == -1){
            fileEntry* newFile = (fileEntry*) malloc(sizeof(fileEntry));
            newFile->hash = (unsigned char*) malloc(16 * sizeof(char));
            newFile->filename = (char*) malloc(MAX_FILENAME_SIZE * sizeof(char));
            strcpy(newFile->filename,filename);
            strncpy((char*) newFile->hash, (char*) hash, 16);
            newFile->timestamp = time(NULL);
            int result = pthread_mutex_init(&newFile->fileLock, NULL);
            if (result != 0){
                perror("Error in mutex init!\n");
                exit(-1);
            }
            da_push(cache,newFile);
            retVal = -cache->size;
            pthread_mutex_lock(&newFile->fileLock);
        }
        else{
            fileEntry* file = cache->items[retVal];
            retVal += 1;
            pthread_mutex_lock(&file->fileLock);//********************/
        }
    pthread_mutex_unlock(&mutex);//=========================/
    return retVal;
}
//Note to self to free fileEntry structure elements, they are allocated in insertIntoCache
//da_remove is expensive as fuuuuuuuuuuuuuuuuuuuuuuuck. Look here for perf improvements
void* refreshCache(void* args){
    RefreshArgs* refArgs = (RefreshArgs*) args;
    DynamicArray* cache = refArgs->cache;
    int timeout = refArgs->timeout;
    time_t now = time(NULL);
    while(1){
        // printf("refreshing cache!\n");
        pthread_mutex_lock(&mutex);
        for (unsigned int i = 0; i < cache->size;){
            fileEntry* currFile = cache->items[i];
            pthread_mutex_lock(&currFile->fileLock);
            if (fabs(difftime(now,currFile->timestamp)) >= timeout || (currFile->toDelete == 1)){
                da_remove(cache,i);
                remove(currFile->filename);
                free(currFile->filename);
                free(currFile->hash);
                pthread_mutex_unlock(&currFile->fileLock);
                free(currFile);
                continue;
            }
            pthread_mutex_unlock(&currFile->fileLock);
            i++;
        }
        pthread_mutex_unlock(&mutex);
        sleep((int)timeout/2);
    }

    return NULL;
}