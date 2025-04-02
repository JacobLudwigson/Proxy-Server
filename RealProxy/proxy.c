#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <error.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdatomic.h>
#include <openssl/md5.h>
#include "dynArr.h"
#include "cache.h"
#include "httpUtils.h"
#define ERROR -1
#define MAX_CLIENTS 100000 //Maybe?
#define MAX_DATA 2048 //Maybe?
#define LINETERMINATOR "\r\n"
#define HEADERSEPARATOR "\r\n\r\n"
atomic_int countActiveThreads;
atomic_int countActiveFiles;
DynamicArray cache;

void killHandler(int sig){
    signal(sig, SIG_IGN);
    printf("\nFinishing serving clients...\n");
    printf("There are currently %d clients with a persistant connection\n", countActiveThreads);
    int cycles = 0;
    while (countActiveThreads >= 1){
        sleep(1);
        if (cycles >= 10){
            exit(1);
        }
        cycles+=1;
    }
    exit(1);
}
//ALL FIELDS OF PACKETS ARE THIS FUNCTIONS JOB TO FREE (I.E. A BUFFER WILL BE ALLOCATED FOR "data" SECTION OF HTTP PACKET THAT WILL NOT HAVE A COORESPONDING FREE)
void* serveClient(void* data){
    atomic_fetch_add(&countActiveThreads,1);
    intptr_t* intData = (intptr_t*) data; 
    int socket = (int)(intptr_t)intData[0];
    // int pageTimeout = (int)(intptr_t)intData[1];
    free(data);
    unsigned char* responseBuffer;
    int data_len = 1;
    int persistant = 0;
    int decodeStatus;
    int cacheStatus;
    int length = 0;
    int bytesSent;
    char* buffer = calloc(1,MAX_DATA);
    struct httpPacket* requestPacket = (httpPacket*) calloc(1, sizeof(httpPacket));
    struct httpPacket* responsePacket= (httpPacket*) calloc(1, sizeof(httpPacket));
    do {
        // printf("BEGINNING SERVICE MA BOY!\n");
        data_len = recv(socket, buffer,MAX_DATA,0);
        if (data_len < 0) {
            printf("Client timeout, returning...\n");
            break;
        }
        decodeStatus = decodeHttpPacket(requestPacket, buffer);
        cacheStatus = insertIntoCache(requestPacket->pageRequest, &cache);
        fileEntry* file = cache.items[cacheStatus];
        char filename[MAX_FILENAME_SIZE] = FILEDIRECTORY;
        char hostname_with_port[MAX_URL_LENGTH];
        get_hostname_from_url(requestPacket->pageRequest, hostname_with_port);
        strcat(filename,"/");
        strcat(filename,hostname_with_port);
        FILE *filePtr = fopen(filename, "r");
        if (!filePtr) {
            filePtr = fopen(filename, "w");
            if (!filePtr) {
                perror("Failed to create file");
                exit(-1);
            }
            char* recvBuffer;
            int bytes = forwardRequest(hostname_with_port, buffer, data_len, &recvBuffer);
            // puts(recvBuffer);
            struct httpPacket* recvPacket = (httpPacket*) calloc(1, sizeof(httpPacket));
            decodeHttpPacket(recvPacket, recvBuffer);
            memset(recvBuffer, 0, RESPONSE_MAX);
            printPacket(recvPacket, recvBuffer, RESPONSE_MAX);
            // fwrite(recvBuffer->data, 1, bytes, filePtr);
            // puts(recvBuffer);
            free(recvBuffer);
            free(recvPacket);
        }
        fclose(filePtr);
        pthread_mutex_unlock(&file->fileLock);
        // printCacheFilenames();
        /*

            1. Check cache for file
                1. Decomp requested file from url using some templating or some shit - DO NOT USE HOST FIELD
                2. 
            2. If Exists - Retreive it using buildResponsePacket
            3. If does not exists - request it using reqRemHostData & cache it, return it using buildResponsePacket
            
        */
        // fwrite(buffer,1,data_len,stdout);
        buildResponsePacket(requestPacket, responsePacket, decodeStatus);
        length = MAX_DATA/2 + responsePacket->contentLength + 1;
        responseBuffer = (unsigned char*) calloc(1,length);
        length = formulateHttpPacket(responsePacket,responseBuffer, length);
        bytesSent = send(socket, responseBuffer, length,0);
        persistant = (strcmp(responsePacket->connection,"keep-alive") == 0);
        printf("Sending with status: %d\n", responsePacket->status);
        if (bytesSent < 0) {
            printf("Send timeout, returning...\n");
            free(responseBuffer);
            break;
        }
        if (bytesSent != length){
            printf("Error in thread with socket %d\n\n", socket);
            printf("Sent %d/%d bytes\n", bytesSent,length);
            perror("Error in send ");
            free(responseBuffer);
            break;
        }
        else{
            printf("Served: %s\n\n", requestPacket->pageRequest);
        }
        if (requestPacket->data){
            free(requestPacket->data);
            requestPacket->data = NULL;
        }
        if (responsePacket->data){
            free(responsePacket->data);
            responsePacket->data = NULL;
        }
        free(responseBuffer);
        responseBuffer = NULL;
        memset(buffer,0,MAX_DATA);
    }while(persistant);
    if (requestPacket->data)free(requestPacket->data);
    if (responsePacket->data)free(responsePacket->data);
    free(buffer);
    free(responsePacket);
    free(requestPacket);
    close(socket);
    atomic_fetch_sub(&countActiveThreads,1);
    return NULL;
}
int main(int argc, char **argv){
    if (argc < 3){
        printf("Incorrect Number of Args! Run with ./server [Port Number] [Cache timeout]\n");
        exit(-1);
    }
    struct sockaddr_in server;
    struct sockaddr_in client;
    int sock;
    int newClientSock;
    unsigned int socketaddr_len = sizeof(struct sockaddr_in);
    unsigned int pageTimeout = atoi(argv[2]);
    init_cache(&cache);
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR){
        perror("Error in socket : ");
        exit(-1);
    }
    int op = 1;
    if (setsockopt(sock, SOL_SOCKET,SO_REUSEADDR, &op, sizeof(op)) < 0){
        perror("Error setting sock op: ");
        exit(-1);
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[1]));
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&server.sin_zero,8);
    if ((bind(sock, (struct sockaddr* )&server, socketaddr_len)) == ERROR){
        perror("Error in bind : ");
        exit(-1);
    }

    if ((listen(sock, MAX_CLIENTS)) == -1){
        perror("Error in listen : ");
        exit(-1);
    }
    char ip_str[INET_ADDRSTRLEN];
    countActiveThreads = 0;
    signal(SIGINT, killHandler);
    RefreshArgs* refArgs = malloc(sizeof(RefreshArgs));
    refArgs->cache = &cache;
    refArgs->timeout = pageTimeout;
    time_t last_run = time(NULL);
    while (1){
        time_t now = time(NULL);
        printf("TIME DIFF: %f\n", difftime(now, last_run));
        if (difftime(now, last_run) >= pageTimeout) {
            pthread_t ptid2;
            pthread_create(&ptid2, NULL, &refreshCache, (void*) refArgs);
            pthread_detach(ptid2);
            last_run = now;
        }
        if ((newClientSock = accept(sock, (struct sockaddr *) &client, &socketaddr_len)) == ERROR){
            perror("Error in accept : ");
            exit(-1);
        }
        if (inet_ntop(AF_INET, &client.sin_addr, ip_str, sizeof(ip_str)) == NULL) {
            perror("inet_ntop error");
        }
        struct timeval timeout;      
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        if (setsockopt (newClientSock, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof timeout) < 0 || setsockopt (newClientSock, SOL_SOCKET, SO_SNDTIMEO, &timeout,sizeof timeout) < 0){
            close(newClientSock);
            perror("setsockopt failed ");
        }
        else{
            intptr_t *arr = malloc(2 * sizeof(intptr_t));
            arr[0] = (intptr_t)newClientSock;
            arr[1] = (intptr_t) pageTimeout;
            pthread_t ptid;
            pthread_create(&ptid, NULL, &serveClient, (void*) arr);
            pthread_detach(ptid);
        }
        printf("\n\nHandling Client Connected from port no %d and IP %s\n",ntohs(client.sin_port), ip_str);
    }
    free(refArgs);
    free_cache(&cache);
    close(sock);
}