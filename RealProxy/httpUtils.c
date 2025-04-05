#include "httpUtils.h"

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
int isDynamicPage(const char* filename){
    return strstr(filename, "?") != NULL;
}
const char* get_file_extension_or_default(const char* url) {
    const char *lastSlash = strrchr(url, '/');
    if (!lastSlash) {
        lastSlash = url;  // No slash found, consider the whole URL
    } else {
        lastSlash++;  // Move past the slash
    }

    const char *dot = strrchr(lastSlash, '.');
    if (!dot || dot == url) {
        if (url[strlen(url) - 1] == '/') {
            return ".html";
        } else {
            return "";
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

const char* get_file_type(const char* contentType) {
    if (!strcmp(contentType, "text/html")){
        return ".html";
    } else if (!strcmp(contentType, "text/css")) {
        return ".css";
    } else if (!strcmp(contentType, "application/javascript")) {
        return ".js";
    } else if (!strcmp(contentType, "image/jpg")) {
        return ".jpg";
    } else if (!strcmp(contentType, "image/png")) {
        return ".png";
    } else if (!strcmp(contentType, "image/gif")) {
        return ".gif";
    } else if (!strcmp(contentType, "text/plain")) {
        return ".txt";
    } else if (!strcmp(contentType, "image/x-icon")) {
        return ".ico";
    } else {
        return ".bin";
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
    if ( ((size_t) packet->contentLength + len) >= bufferSize){
        printf("Insufficient Packet Length! Total Packet Size: %d, Buffer Size: %ld\n", packet->contentLength+len, bufferSize);
        return 0;
    }
    if (packet->data){
        memcpy(buffer + len, packet->data, packet->contentLength);
        return len + packet->contentLength;
    }
    return len;
}


void printPacket(struct httpPacket* packet){
    printf("%s %d %s\r\n"
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
}
void remove_header(char *request, const char *header_to_remove) {
    char *header_start = strstr(request, header_to_remove);
    if (header_start) {
        char *end_of_line = strstr(header_start, "\r\n");
        if (end_of_line) {
            // Move the rest of the text over the header
            end_of_line += 2; // Skip the "\r\n"
            memmove(header_start, end_of_line, strlen(end_of_line) + 1);
        }
    }
}
int decodeHttpPacket(struct httpPacket* packet, char* buffer1){
    if (!buffer1) {
        printf("Invalid Buffer!\n");
        return 0;
    }
    char buffer[strlen(buffer1)+1];
    strcpy(buffer, buffer1);
    char* line = strtok(buffer, "\r\n");
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
int decodeRecvPacket(struct httpPacket* packet, char* buffer1, ssize_t bytes){
    if (!buffer1) {
        printf("Invalid Buffer!\n");
        return 0;
    }
    size_t len = bytes;
    char buffer[len+1];
    strncpy(buffer, buffer1, bytes);
    char* line = strtok(buffer, "\r\n");
    char first[50];
    char temp[50];
    char second[1024];
    if (line && sscanf(line, "%49s %49s %49s",packet->httpVersion, temp, packet->statusMessage) != 3){
        printf("Invalid Scan 1!\n");
        return 0;
    }
    packet->status = atoi(temp);
    while ((line = strtok(NULL, "\r\n")) != NULL){
        if (strcmp(line,"") == 0 || (*(line-2) == '\r')){
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
        else if (strcmp(first, "Content-Length") == 0){
            packet->contentLength = atoi(second);
        }
    }
    if (line == NULL) return -1;
    else return line - buffer;
}
void stripHttp(const char *url, char *result){
    const char *start = url;
    if (strncmp(url, "http://", 7) == 0) {
        start = url + 7; 
    } else if (strncmp(url, "https://", 8) == 0) { 
        start = url + 8; 
    }
    strncpy(result, start, (url+strlen(url)) - start);
}
void extractReqFile(const char *url, char *result){
    const char *end = strchr(url, '/');
    if (end == NULL) {
        strcpy(result, "/\0");
        return;
    }
    strncpy(result, end, (url + strlen(url)) - end);
}
void get_hostname_from_url(const char *url, char *hostname) {
    const char *start = url;
    if (strncmp(url, "http://", 7) == 0) {
        start = url + 7; // Skip "http://"
    } else if (strncmp(url, "https://", 8) == 0) { //Not sure if we should skip this or not? 
        start = url + 8; 
    }

    const char *end = strchr(start, '/');
    if (end == NULL) {
        end = start + strlen(start); // If no '/', take the rest of the string
    }

    // Copy the domain to the hostname
    strncpy(hostname, start, end - start);
    hostname[end - start] = '\0';
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
        errorPacket(WRONG_HTTP_VERSION, responsePacket);
    }
    if (strcmp(requestPacket->pageRequest,"/") == 0){
        strcpy(requestPacket->pageRequest,"/index.html");
    }
    char filename[MAX_FILENAME_SIZE];
    strcpy(filename,requestPacket->pageRequest);
    if (access(filename, F_OK) != 0){
        errorPacket(NOT_FOUND, responsePacket);
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
    responsePacket->status = OK;
    return;
}
// void buildResponsePacket(struct httpPacket* responsePacket, int decode){
//     strcpy(responsePacket->httpVersion,requestPacket->httpVersion);
//     if (decode != 1){
//         errorPacket(400, responsePacket);
//         return;
//     }
//     if (strcmp(requestPacket->requestType,"GET") != 0){
//         errorPacket(405, responsePacket);
//         return;
//     }
//     if (strcmp(requestPacket->httpVersion, "HTTP/1.1") != 0 && strcmp(requestPacket->httpVersion,"HTTP/1.0") != 0){
//         strcpy(responsePacket->httpVersion,"HTTP/1.1");
//         errorPacket(WRONG_HTTP_VERSION, responsePacket);
//     }
//     if (strcmp(requestPacket->pageRequest,"/") == 0){
//         strcpy(requestPacket->pageRequest,"/index.html");
//     }
//     char filename[MAX_FILENAME_SIZE];
//     strcpy(filename,requestPacket->pageRequest);
//     if (access(filename, F_OK) != 0){
//         printf("ATTEMPTING TO OPEN: %s\n",filename);
//         errorPacket(NOT_FOUND, responsePacket);
//         return;
//     }
//     FILE* fptr = fopen(filename, "rb");
//     fseek(fptr,0, SEEK_END);
//     long int fileSize = ftell(fptr);
//     rewind(fptr);
//     unsigned char* buffer = (unsigned char*) malloc(fileSize+1);
//     long int bytesRead = fread(buffer,sizeof(char), fileSize,fptr);
//     if (fileSize != bytesRead){
//         printf("File read, supposed to read %ld bytes and actually read %ld\n",fileSize, bytesRead);
//         errorPacket(403, responsePacket);
//         return;
//     }
//     strcpy(responsePacket->contentType, get_content_type(requestPacket->pageRequest));
//     responsePacket->contentLength = fileSize;
//     buffer[bytesRead] = '\0';
//     responsePacket->data = buffer;
//     fclose(fptr);
//     responsePacket->status = OK;
//     return;
// }
void print_buffer_with_newlines_and_nulls(const char *buffer, unsigned int bufferLength) {
    // Iterate over each character in the buffer, including the null terminator
    for (size_t i = 0; i <= bufferLength; i++) {
        if (buffer[i] == '\r') {
            // Print \r explicitly for carriage return
            printf("\\r");
        } else if (buffer[i] == '\n') {
            // Print \n explicitly for newline
            printf("\\n");
        } else if (buffer[i] == '\0') {
            // Print \0 explicitly for null terminator
            printf("\\0");
        } else {
            // Print the character as is
            putchar(buffer[i]);
        }
    }
    printf("\n");  // Print a final newline to finish output
}

/*
    Still need some work here. This will send the packet, not receive it.
*/
int forwardRequest(char* hostname_with_port, char* buffer, ssize_t bufferLength, char* filename){
    int sockfd;
    struct sockaddr_in serveraddr;
    struct hostent *server;

    char hostname[256];
    int port = 80;  // default
    char *colon = strchr(hostname_with_port, ':');
    if (colon) {
        size_t len = colon - hostname_with_port;
        strncpy(hostname, hostname_with_port, len);
        hostname[len] = '\0';
        port = atoi(colon + 1);
    } else {
        strncpy(hostname, hostname_with_port, sizeof(hostname));
    }

    server = gethostbyname(hostname);
    if (!server) {
        fprintf(stderr, "ERROR: no such host: %s\n", hostname);
        return -1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return -1;
    }



    bzero(&serveraddr, sizeof(serveraddr));

    //RESOLVE HOSTNAME AND EXTRACT PORT HERE
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    memcpy(&serveraddr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sockfd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) != 0){
        perror("Couldnt connect with server!\n");
        return -1;
    }
    // print_buffer_with_newlines_and_nulls(buffer, bufferLength);
    printf("PRE MODIFICATION\n");

    fwrite(buffer, 1, bufferLength, stdout);
    remove_header(buffer, "If-Modified-Since: ");
    printf("POST MODIFICATION\n");
    fwrite(buffer, 1, bufferLength, stdout);

    if (send(sockfd, buffer, bufferLength, 0) != bufferLength) {
        perror("send failed");
        return -1;
    }

    char *response = malloc(RESPONSE_BUFFER+1);
    if (!response) {
        perror("malloc failed");
        close(sockfd);
        return -1;
    }

    size_t total_received = 0;
    ssize_t bytes_received;
    FILE *fptr = NULL;
    char *bodyStart = NULL;
    int headersDecoded = 0;
    struct httpPacket* recvPacket = (httpPacket*) calloc(1, sizeof(httpPacket));

    while ((bytes_received = recv(sockfd, response + total_received, RESPONSE_BUFFER, 0)) > 0) {
        if (!bodyStart) {
            if (!headersDecoded){
                response[RESPONSE_BUFFER] = '\0';
                decodeRecvPacket(recvPacket, response, bytes_received);
                printPacket(recvPacket);
            }
            bodyStart = strstr(response, "\r\n\r\n");
            if (bodyStart && (recvPacket->status == 200)) {
                bodyStart += 4; // Move past the headers
                fptr = fopen(filename, "w");
                if (!fptr) {
                    perror("Failed to create file");
                    free(response);
                    close(sockfd);
                    return -1;
                }
                fwrite(bodyStart, 1, response + bytes_received - bodyStart, fptr);
            }
        } else if (fptr) {
            fwrite(response, 1, bytes_received, fptr);
        }
        memset(response, 0, RESPONSE_BUFFER);
    }


    if (bytes_received < 0) {
        perror("recv failed");
        free(response);
        close(sockfd);
        return -1;
    }
    free(response);
    if (fptr){
        fclose(fptr);
    }
    close(sockfd);
    return (int)total_received;
}

