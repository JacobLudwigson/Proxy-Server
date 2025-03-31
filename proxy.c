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
#define ERROR -1
#define MAX_CLIENTS 100000 //Maybe?
#define MAX_DATA 2048 //Maybe?
#define LINETERMINATOR "\r\n"
#define HEADERSEPARATOR "\r\n\r\n"
atomic_int countActiveThreads;
typedef struct httpPacket{ 
    unsigned char* data;
    int status;
    int contentLength;
    char requestType[50];
    char pageRequest[50];
    char statusMessage[20];
    char httpVersion[50];
    char host[50];
    char connection[100];
    char contentType[150];
}httpPacket;
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
const char* get_content_type(const char* filename) {
    if (strstr(filename, ".html") != NULL || strstr(filename, ".htm") != NULL) {
        return "text/html";
    } else if (strstr(filename, ".css") != NULL) {
        return "text/css";
    } else if (strstr(filename, ".js") != NULL) {
        return "application/javascript";
    } else if (strstr(filename, ".jpg") != NULL ) {
        return "image/jpg";
    } else if (strstr(filename, ".png") != NULL) {
        return "image/png";
    } else if (strstr(filename, ".gif") != NULL) {
        return "image/gif";
    } else if (strstr(filename, ".txt") != NULL) {
        return "text/plain";
    } else if (strstr(filename, ".ico") != NULL) {
        return "image/x-icon";
    } else {
        return "application/octet-stream";
    }
}
int formulateHttpPacket(struct httpPacket* packet, char* buffer, size_t bufferSize){
    int len = snprintf(buffer, bufferSize,
        "%s %d %s\r\n"
        "Connection: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "\r\n",
        packet->httpVersion ? packet->httpVersion : "", 
        packet->status ? packet->status : 100, 
        packet->statusMessage ? packet->statusMessage : "",
        packet->connection ? packet->connection : "",
        packet->contentType ? packet->contentType : "",
        packet->contentLength ? packet->contentLength : 0
    );
    if (packet->contentLength + len >= bufferSize){
        printf("Insufficient Packet Length! Total Packet Size: %d, Buffer Size: %ld\n", packet->contentLength+len, bufferSize);
        return 0;
    }
    memcpy(buffer + len, packet->data, packet->contentLength);
    return len + packet->contentLength;
}
void printPacket(struct httpPacket* packet, char* buffer, size_t bufferSize){
    int len = snprintf(buffer, bufferSize,
        "%s %d %s\r\n"
        "Connection: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "\r\n",
        packet->httpVersion ? packet->httpVersion : "", 
        packet->status ? packet->status : 100, 
        packet->statusMessage ? packet->statusMessage : "",
        packet->connection ? packet->connection : "",
        packet->contentType ? packet->contentType : "",
        packet->contentLength ? packet->contentLength : 0
    );
    puts(buffer);
}
int decodeHttpPacket(struct httpPacket* packet, char* buffer, size_t bufferLength ){
    if (!buffer) {
        printf("Invalid Buffer!\n");
        return 0;
    }
    int count = 0;
    char* line = strtok(buffer, "\r\n");
    int status;
    char first[50];
    char second[1024];
    if (line && sscanf(line, "%49s %49s %49s",packet->requestType, packet->pageRequest, packet->httpVersion) != 3){
        printf("Invalid Scan 1!\n");
        return 0;
    }
    while ((line = strtok(NULL, "\r\n")) != NULL){
        if (strcmp(line,"") == 0 || line == NULL || strcmp(line,"\r\n")){
            break;
        }
        if (sscanf(line, "%s %s", first,second) != 2){
            printf("Invalid Scan 2!\n");
            return 0;
        } 
        first[strlen(first)-1] = '\0';
        if (strcmp(first, "Host") == 0){
            strcpy(packet->host, second);
        }
        else if (strcmp(first, "Connection") == 0){
            strcpy(packet->connection, second);
        }
        else if (strcmp(first, "Accept") == 0){
            strcpy(packet->contentType, second);
        }
    }
    return 1;
}
void errorPacket(int errorCode, struct httpPacket* responsePacket){
    strcpy(responsePacket->connection, "close");
    strcpy(responsePacket->contentType, "text/html");
    responsePacket->status = errorCode;
    return;
}
void buildResponsePacket(struct httpPacket* requestPacket, struct httpPacket* responsePacket, int decode){
    strcpy(responsePacket->httpVersion,requestPacket->httpVersion);
    if (decode != 1){
        errorPacket(400, responsePacket);
        return;
    }
    if (strcmp(requestPacket->requestType,"GET") != 0){
        errorPacket(405, responsePacket);
        return;
    }
    if (strcmp(requestPacket->httpVersion, "HTTP/1.1") != 0 && strcmp(requestPacket->httpVersion,"HTTP/1.0") != 0){
        strcpy(responsePacket->httpVersion,"HTTP/1.1");
        errorPacket(505, responsePacket);
    }
    if (strcmp(requestPacket->pageRequest,"/") == 0){
        strcpy(requestPacket->pageRequest,"/index.html");
    }
    //Might want to change this? Filenames probably shouldnt be over 100 characters but this could overflow buffer as is 
    char filename[100]= "www";
    strcat(filename,requestPacket->pageRequest);
    if (access(filename, F_OK) != 0){
        errorPacket(404, responsePacket);
        return;
    }
    FILE* fptr = fopen(filename, "rb");
    if (!fptr){
        errorPacket(403, responsePacket);
        return;
    }
    if (strcmp(requestPacket->connection, "keep-alive") == 0){
        strcpy(responsePacket->connection,"keep-alive");
    }
    else{
        if (!strcmp(responsePacket->connection, "") && !strcmp(responsePacket->httpVersion, "HTTP/1.1")){
            strcpy(responsePacket->connection,"keep-alive");
        } 
        else{
            strcpy(responsePacket->connection,"close");
        }
    }
    fseek(fptr,0, SEEK_END);
    long int fileSize = ftell(fptr);
    rewind(fptr);
    unsigned char* buffer = (unsigned char*) malloc(fileSize+1);
    long int bytesRead = fread(buffer,sizeof(char), fileSize,fptr);
    if (fileSize != bytesRead){
        printf("File read, supposed to read %ld bytes and actually read %ld\n",fileSize, bytesRead);
        errorPacket(403, responsePacket);
        return;
    }
    strcpy(responsePacket->contentType, get_content_type(requestPacket->pageRequest));
    responsePacket->contentLength = fileSize;
    buffer[bytesRead] = '\0';
    responsePacket->data = buffer;
    fclose(fptr);
    responsePacket->status = 200;
    return;
}
//ALL FIELDS OF PACKETS ARE THIS FUNCTIONS JOB TO FREE (I.E. A BUFFER WILL BE ALLOCATED FOR "data" SECTION OF HTTP PACKET THAT WILL NOT HAVE A COORESPONDING FREE)
void* serveClient(void* data){
    atomic_fetch_add(&countActiveThreads,1);
    intptr_t* intData = (intptr_t*) data; 
    int socket = (int)(intptr_t)intData[0];
    int pageTimeout = (int)(intptr_t)intData[1];

    char* responseBuffer;
    int data_len = 1;
    int persistant = 0;
    int decodeStatus;
    int length = 0;
    int bytesSent;
    char* buffer = calloc(1,MAX_DATA);
    struct httpPacket* requestPacket = (httpPacket*) calloc(1, sizeof(httpPacket));
    struct httpPacket* responsePacket= (httpPacket*) calloc(1, sizeof(httpPacket));
    do {
        data_len = recv(socket, buffer,MAX_DATA,0);
        if (data_len < 0) {
            printf("Client timeout, returning...\n");
            break;
        }
        decodeStatus = decodeHttpPacket(requestPacket, buffer, data_len);
        fwrite(buffer,1,data_len,stdout);
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
        printf("Incorrect Number of Args! Run with ./server [Port Number]\n");
        exit(-1);
    }
    struct sockaddr_in server;
    struct sockaddr_in client;
    int sock;
    int newClientSock;
    int socketaddr_len = sizeof(struct sockaddr_in);
    int data_len;
    char data[MAX_DATA];
    unsigned int pageTimeout = atoi(argv[2]);

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
    while (1){
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
            intptr_t arr[] = {(intptr_t)newClientSock, (intptr_t)pageTimeout};
            void* data = arr;
            pthread_t ptid;
            pthread_create(&ptid, NULL, &serveClient, data);
            pthread_detach(ptid);
        }
        printf("\n\nHandling Client Connected from port no %d and IP %s\n",ntohs(client.sin_port), ip_str);
    }
    close(sock);
}