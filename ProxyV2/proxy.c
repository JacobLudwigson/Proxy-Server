#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <error.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdatomic.h>
#include <openssl/md5.h>

#define ERROR -1
#define BUFFER_SIZE 4096
#define MAX_RESPONSE_HEADER_SIZE 1024
#define CACHE_DIR "./cache"
#define MAX_CLIENTS 100
#define OK 200
#define MAX_URL_LENGTH 1024
#define MAX_HOSTNAME_LENGTH 512 //maybe? Idk how long the longest hostname is but these should be enough
#define DEBUG 1
atomic_int countActiveThreads;
int pageTimeout;

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
const char* getContentType(const char* filename) {
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
const char* getFileExtension(const char* filePath) {
    const char *dot = strrchr(filePath, '.');
    if (!dot || dot == filePath) {
        if (filePath[strlen(filePath) - 1] == '/') {
            return ".html";
        } else {
            return ".bin";
        }
    }

    if (!strcmp(dot, ".htm") || !strcmp(dot, ".html")) {
        return ".html";
    } else if (!strcmp(dot, ".css")) {
        return ".css";
    } else if (!strcmp(dot, ".js")) {
        return ".js";
    } else if (!strcmp(dot, ".jpeg") || !strcmp(dot, ".jpg")) {
        return ".jpg";
    } else if (!strcmp(dot, ".png")) {
        return ".png";
    } else if (!strcmp(dot, ".gif")) {
        return ".gif";
    } else if (!strcmp(dot, ".txt")) {
        return ".txt";
    } else if (!strcmp(dot, ".ico")) {
        return ".ico";
    } else {
        return dot; 
    }
}

/*
    As named: Extracts the hostname and "filename"/path from a const char buffer url
*/
void getHostNameAndFileFromURL(char *url, char *hostname, char* filePath) {
    char* start = url + 7; //http:// is 7 characters

    const char *end = strchr(start, '/');
    if (end == NULL) {
        strcpy(hostname, start);
        strcpy(filePath, "/");
    }
    else{
        strcpy(filePath, end);
        hostname = strncpy(hostname, start, end-start);
        hostname[end-start] = '\0';
    }
    if (!strcmp(filePath, "/")) strcpy(filePath, "/index.html");
}
void getPortFromHostname(char* hostname, int* port){
    char* sep = strchr(hostname, ':');
    if (sep){
        *sep = '\0';
        *port = atoi(sep+1);
    }
    else{
        *port = 80;
    }
}
int isDynamicPage(const char* filename){
    return strchr(filename, '?') != NULL;
}
int isValidFile(const char *filepath, int timeout) {
    struct stat st;
    if (stat(filepath, &st) == -1)
        return 0;
    time_t now = time(NULL);
    if (difftime(now, st.st_mtime) >= timeout)
        remove(filepath);
        return 0;
    return 1;
}
/*
    I cannot take complete credit for this function. It was inspired by one I saw online
    while looking into how to handle errors in C web programming. 
*/
void sendErrorPacket(int socket, int code, const char* failure){
    char responseBuffer[BUFFER_SIZE];
    snprintf(responseBuffer, sizeof(responseBuffer),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n"
             "<html><body><h1>%d %s</h1></body></html>",
             code, failure, strlen(failure) + 50, code, failure);
    send(socket, responseBuffer, strlen(responseBuffer),0);
}
//Gracefully exit serve client with a socket close, a message and a return value.
//Will also update the count active threads
void* exitServeClient(int socket, const char* message, void* returnVal){
    atomic_fetch_sub(&countActiveThreads,1);
    close(socket);
    if (message) fwrite(message, 1, strlen(message), stdout);
    return returnVal;
}
//After immense trial and error I think the best way to do this is keep everything in one function. Forwarding data gets a lot simpler.
//If I need to add a thread to source data I will have them operate in a shared buffer
void* serveClient(void* data){
    //Update the counter for active threads
    atomic_fetch_add(&countActiveThreads,1);
    
    //Decode the socket passed on the void pointer data parameter into a socketID
    int clientSocket = *(int*)data;
    free(data);
    
    //Allocate a buffer and receive a request packet and terminate with null
    char receiveBuffer[BUFFER_SIZE];
    memset(receiveBuffer, 0, BUFFER_SIZE);
    int bytesReceived = recv(clientSocket, receiveBuffer, BUFFER_SIZE-1,0);
    if (bytesReceived <= 0){
        return exitServeClient(clientSocket, "Client timeout, returning...\n", NULL);
    }
    receiveBuffer[BUFFER_SIZE] = '\0';

    //Write the packet to stdout for debug
    if (DEBUG) {
        printf("\n==================================\n");
        printf("======Client to proxy packet======\n");
        printf("==================================\n\n");
        fwrite(receiveBuffer, 1, BUFFER_SIZE, stdout);
    }

    //Check for a valid packet request structure
    char requestType[20], url[MAX_URL_LENGTH], httpVersion[20];
    //These shouldnt be full of stuff but for some reason sometimes they are!
    memset(url, 0, MAX_URL_LENGTH);
    if (sscanf(receiveBuffer, "%19s %1023s %15s", requestType, url, httpVersion) != 3){
        if (DEBUG) printf("Sscanf fail! First request line interpretted as %s, %s, %s\n", requestType, url, httpVersion);
        sendErrorPacket(clientSocket, 400, "Malformed Request!\n");
        return exitServeClient(clientSocket, "Closing client on malformed request...\n", NULL);
    }

    //If its not a http or GET request you can GET OUTA HERE RAHHHH
    if (!strstr(url, "http://") || strcmp(requestType, "GET") != 0){
        //Im not sure what this error packet should be? Gonna go with 405 but its not listed
        sendErrorPacket(clientSocket, 405, "Unsupported Method\n");
        return exitServeClient(clientSocket, "Closing client on unsupported method", NULL);
    }
    char* connHeader = strstr(receiveBuffer, "Connection: ");
    if(!connHeader) connHeader = "close";
    if (DEBUG) printf("Connection Header : %s", connHeader + 12);


    char hostname[MAX_HOSTNAME_LENGTH];
    char filePath[MAX_HOSTNAME_LENGTH]; //If each of these are half of URL max length it should be good
    int port, dynamic;
    getHostNameAndFileFromURL(url, hostname, filePath);
    if (DEBUG) printf("Extracted hostname and filepath: %s, %s\n", hostname, filePath);
    getPortFromHostname(hostname,&port);
    if (DEBUG) printf("Extracted port number : %d\n", port);
    dynamic = isDynamicPage(url);
    if (DEBUG) printf("Dynamic page = %d\n", dynamic);

    if (!dynamic){
        char cachePath[MAX_HOSTNAME_LENGTH+10] = CACHE_DIR;
        strcat(cachePath, filePath);
        const char* fileExtension = getFileExtension(cachePath);
        const char* contentType = getContentType(fileExtension);
        if (DEBUG) printf("Cache Path: %s\n", cachePath);
        if (DEBUG) printf("File Extension: %s\n", fileExtension);
        if (DEBUG) printf("Content Type: %s\n", contentType);

        if (isValidFile(cachePath, pageTimeout)){
            FILE* fptr;
            if ((fptr = fopen(cachePath, "rb"))){
                fseek(fptr, 0, SEEK_END);
                long unsigned int length = ftell(fptr);
                rewind(fptr);
                unsigned char* fileReadBuffer = (unsigned char*) malloc(length * sizeof(char));
                fread(fileReadBuffer, 1, length, fptr);
                //Need to formulate this into a packet first
                char* fullPacketBuffer = (char*) malloc(length + MAX_RESPONSE_HEADER_SIZE * sizeof(char));
                int bytesWritten = snprintf(fullPacketBuffer, MAX_RESPONSE_HEADER_SIZE,
                    "%s %d %s\r\n"
                    "Connection: %s\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %d\r\n"
                    "\r\n",
                    httpVersion, OK, "OK",
                    connHeader,
                    contentType,
                    length);
                memmove(fullPacketBuffer + bytesWritten, fileReadBuffer, length);
                send(clientSocket, fullPacketBuffer, length + bytesWritten, 0);
                if (DEBUG) {
                    printf("\n==================================\n");
                    printf("=Proxy to client cache filed packet=\n");
                    printf("==================================\n\n");
                    fwrite(receiveBuffer, 1, BUFFER_SIZE, stdout);
                }
                fclose(fptr);
                free(fileReadBuffer);
                free(fullPacketBuffer);
                return exitServeClient(clientSocket, "Closing client on cache served file...\n", NULL);
            }
        }
    }

    struct hostent *server = gethostbyname(hostname);
    if (!server){
        sendErrorPacket(clientSocket, 404, "Host not found");
        return exitServeClient(clientSocket, "Exiting client on host not found...\n", NULL);
    }
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        sendErrorPacket(clientSocket, 404, "Couldnt open server socket");
        return exitServeClient(clientSocket, "Couldnt open server socket...\n", NULL);
    }

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
    pageTimeout = atoi(argv[2]);

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


    // pthread_t ptid2;
    // pthread_create(&ptid2, NULL, &refreshCache, (void*) refArgs);
    // pthread_detach(ptid2);
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
            int* newClientSocket = (int*) malloc(sizeof(int));
            *newClientSocket = newClientSock;
            void* data = newClientSocket;
            pthread_t ptid;
            pthread_create(&ptid, NULL, &serveClient, data);
            pthread_detach(ptid);
        }
        printf("\n\nHandling Client Connected from port no %d and IP %s\n",ntohs(client.sin_port), ip_str);
    }
    close(sock);
}
