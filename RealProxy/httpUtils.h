#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define MAX_FILENAME_SIZE 500
#define OK 200
#define WRONG_HTTP_VERSION 505
#define NOT_FOUND 404
#define FILEDIRECTORY "cache"
#define MAX_URL_LENGTH 200
#define RESPONSE_MAX 10000 //Maybe?
typedef struct httpPacket{ 
    unsigned char* data;
    int status;
    int contentLength;
    char requestType[50];
    char pageRequest[MAX_URL_LENGTH];
    char statusMessage[20];
    char httpVersion[50];
    char host[50];
    char connection[100];
    char contentType[150];
}httpPacket;

const char* get_content_type(const char* filename);
const char* get_file_extension_or_default(const char* url);
int formulateHttpPacket(struct httpPacket* packet, char* buffer, size_t bufferSize);
void printPacket(struct httpPacket* packet);
int decodeHttpPacket(struct httpPacket* packet, char* buffer);
int decodeRecvPacket(struct httpPacket* packet, char* buffer1);
void get_hostname_from_url(const char *url, char *hostname);
void stripHttp(const char *url, char *result);
void extractReqFile(const char *url, char *result);
void errorPacket(int errorCode, struct httpPacket* responsePacket);
void replace_url_with_path(char *http_request);
void print_buffer_with_newlines_and_nulls(const char *buffer, unsigned int bufferLength);
int forwardRequest(char* hostname_with_port, char* buffer, ssize_t bufferLength, char** response);
void buildResponsePacket(struct httpPacket* requestPacket, struct httpPacket* responsePacket, int decode);
#endif