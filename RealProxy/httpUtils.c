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


void printPacket(struct httpPacket* packet, char* buffer, size_t bufferSize){
    snprintf(buffer, bufferSize,
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
    char filename[MAX_FILENAME_SIZE]= FILEDIRECTORY;
    strcat(filename,requestPacket->pageRequest);
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
void replace_url_with_path(char *http_request) {
    // Find the position of the space after the method (GET)
    char *url_start = strchr(http_request, ' '); // Find space after GET
    if (!url_start) return; // If no space found, return (invalid format)
    url_start++;  // Move past the space to start of the URL

    // Find the position of the space after the URL (before HTTP version)
    char *url_end = strchr(url_start, ' ');  // Find space after URL
    if (!url_end) return; // If no space found, return (invalid format)

    // Extract the URL from the request
    size_t url_length = url_end - url_start;
    char url[url_length + 1];
    strncpy(url, url_start, url_length);
    url[url_length] = '\0';  // Null-terminate the extracted URL

    // Find the position of the first '/' after the host in the URL
    char *path_start = strchr(url, '/');
    if (!path_start) return; // If there's no path (just domain), return (invalid URL)

    // Construct the new URL with just the path (i.e., starting from the first '/')
    char new_path[1024];  // Buffer to store the new path

    // If the path starts with '/', we use it directly
    if (path_start) {
        strcpy(new_path, path_start);
    } else {
        strcpy(new_path, "/");  // If no path, set it to "/"
    }

    // Now replace the original URL with the new path in the HTTP request
    size_t prefix_len = url_end - http_request + 1;  // Include the space before HTTP version
    size_t http_request_len = strlen(http_request);

    // Shift the rest of the request to accommodate the new path
    memmove(url_start + strlen(new_path), url_end, http_request_len - (url_end - http_request));

    // Copy the new path into the HTTP request
    memcpy(url_start, new_path, strlen(new_path));

    // Null-terminate the modified request
    http_request[http_request_len] = '\0';
}
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
int forwardRequest(char* hostname_with_port, char* buffer, ssize_t bufferLength, char** response_out){
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
        exit(-1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(-1);
    }



    bzero(&serveraddr, sizeof(serveraddr));

    //RESOLVE HOSTNAME AND EXTRACT PORT HERE
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    memcpy(&serveraddr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sockfd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) != 0){
        perror("Couldnt connect with server!\n");
        exit(-1);
    }
    // print_buffer_with_newlines_and_nulls(buffer, bufferLength);
    // fwrite(buffer, 1, bufferLength, stdout);
    if (send(sockfd, buffer, bufferLength, 0) != bufferLength) {
        perror("send failed");
        exit(-1);
    }

    char *response = malloc(RESPONSE_MAX);
    if (!response) {
        perror("malloc failed");
        close(sockfd);
        return -1;
    }

    size_t total_received = 0;
    ssize_t bytes_received;
    while ((bytes_received = recv(sockfd, response + total_received, RESPONSE_MAX - total_received - 1, 0)) > 0) {
        total_received += bytes_received;
        if (total_received >= RESPONSE_MAX - 1) break; // prevent overflow
    }

    if (bytes_received < 0) {
        perror("recv failed");
        free(response);
        close(sockfd);
        return -1;
    }

    response[total_received] = '\0'; // Null-terminate
    // fwrite(response, 1, total_received, stdout);
    *response_out = response;

    close(sockfd);
    return (int)total_received;
}

