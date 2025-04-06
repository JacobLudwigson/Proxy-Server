#include "extraCredit.h"
void freeLinks(char **links) {
    if (links) {
        for (int i = 0; links[i] != NULL; i++) {
            free(links[i]);
        }
        free(links);
    }
}
int isBlocked(const char *host, const char *ip) {
    FILE *fp = fopen(BLOCKLIST_FILE, "r");
    if (!fp) return 0;

    char pattern[MAX_PATTERN_SIZE];
    int block = 0;

    while (fgets(pattern, sizeof(pattern), fp) != NULL) {
        char *newline = strchr(pattern, '\n');
        if (newline) *newline = '\0';
        if (pattern[0] == '\0') continue;

        regex_t regex;
        if (regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB) != 0) continue;

        // If the IP/hostname is blocked then return true.
        if (regexec(&regex, host, 0, NULL, 0) == 0 || regexec(&regex, ip, 0, NULL, 0) == 0) {
            block = 1;
            regfree(&regex);
            break;
        }
        regfree(&regex);
    }
    fclose(fp);
    return block;
}
void clearCache(const char *dirPath) {
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
void compute_md5(const char* filename, unsigned char* hashBuffer){
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, filename, strlen(filename));
    MD5_Final(hashBuffer, &ctx);
}
void safeFilename(char *hex_str) {
    for (int i = 0; i < 16; i++) {
        if (hex_str[i] == '/') {
            hex_str[i] = '_';
        }
    }
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
    if(DEBUG) printf("CHECKING IF FILE IN CACHE\n");

    struct stat st;
    if (stat(filepath, &st) == -1){
        if(DEBUG) printf("Invalid file on does not exist\n");
        return 0;
    }
    time_t now = time(NULL);
    if (difftime(now, st.st_mtime) >= timeout) {
        if(DEBUG) printf("Invalid file on timeout\n");
        return 0;
    }
    if(DEBUG) printf("Valid file\n");
    return 1;
}
void htmlLinkParser(char* htmlDoc, char*** links1) {
    size_t capacity = 8;
    size_t count = 0;
    *links1 = malloc(capacity * sizeof(char*));
    if (!*links1) {
        perror("malloc failed");
        return;
    }
    char **links = *links1;

    char *p = htmlDoc;
    while (*p != '\0') {
        // Find the next occurrence of either "href=" or "src=".
        char *hrefPos = strstr(p, "href=");
        char *srcPos = strstr(p, "src=");
        char *next = NULL;
        int attrLen = 0;
        
        if (hrefPos && srcPos) {
            if (hrefPos < srcPos) {
                next = hrefPos;
                attrLen = 5;  // length of "href="
            } else {
                next = srcPos;
                attrLen = 4;  // length of "src="
            }
        } else if (hrefPos) {
            next = hrefPos;
            attrLen = 5;
        } else if (srcPos) {
            next = srcPos;
            attrLen = 4;
        } else {
            break;  // No more occurrences found
        }

        // Move pointer past the attribute name.
        next += attrLen;

        // Expecting a quote character as the delimiter (either " or ').
        if (*next != '"' && *next != '\'') {
            // If no quote is found, skip this occurrence.
            p = next;
            continue;
        }
        char delim = *next;
        next++;  // Skip the opening delimiter.

        // Find the closing delimiter.
        char *end = strchr(next, delim);
        if (!end) {
            // Malformed attribute; break out of the loop.
            break;
        }

        // Calculate the length of the URL.
        size_t len = end - next;

        // Allocate space for the URL string.
        char *link = malloc(len + 1);
        if (!link) {
            perror("malloc failed");
            for (size_t i = 0; i < count; i++) {
                free(links[i]);
            }
            free(links);
            return;
        }

        // Copy the URL into the allocated string.
        strncpy(link, next, len);
        link[len] = '\0';

        // Expand the links array if needed.
        if (count >= capacity) {
            capacity *= 2;
            char **tmp = realloc(links, capacity * sizeof(char*));
            if (!tmp) {
                perror("realloc failed");
                free(link);
                for (size_t i = 0; i < count; i++) {
                    free(links[i]);
                }
                free(links);
                return;
            }
            links = tmp;
            *links1 = links; // update the caller's pointer as well
        }
        links[count++] = link;

        // Advance pointer p past the closing delimiter to continue parsing.
        p = end + 1;
    }

    // NULL-terminate the array.
    char **finalLinks = realloc(links, (count + 1) * sizeof(char*));
    if (finalLinks) {
        links = finalLinks;
        *links1 = links; // update the caller's pointer as well
    }
    links[count] = NULL;
}
int isUrl(const char *s) {
    return (strstr(s, "://") != NULL);
}
void* preFetchManagerThread(void* args){
    preFetchArgs* preFetchArguments = (preFetchArgs*) *args;
    char** links = preFetchArguments->links;
    for (int i = 0; links[i] != NULL; i++) {
        if(isUrl(links[i])){
            preFetchURL(links[i]);
        }
        else{
            preFetchPath(links[i])
        }
    }
    freeLinks(links);
    free(preFetchArguments)
}
void preFetchURL(char* url){
    printf("ATTEMPTING TO PREFETCH %s\n", url);
    char hostname[MAX_HOSTNAME_LENGTH];
    char filePath[MAX_HOSTNAME_LENGTH]; //If each of these are half of URL max length it should be good
    char temp[MAX_HOSTNAME_LENGTH*2] = "";
    char hash[16];
    int port, dynamic;
    getHostNameAndFileFromURL(url, hostname, filePath);
    if (EXTRA_CREDIT_DEBUG) printf("Extracted hostname and filepath: %s, %s\n", hostname, filePath);
    getPortFromHostname(hostname,&port);
    if (EXTRA_CREDIT_DEBUG) printf("Extracted port number : %d\n", port);
    dynamic = isDynamicPage(url);
    if (dynamic) return;
    if (EXTRA_CREDIT_DEBUG) printf("Dynamic page = %d\n", dynamic);
    struct hostent *server = gethostbyname(hostname);
    char *ip_str = inet_ntoa(*(struct in_addr *)server->h_addr_list[0]);
    if (isBlocked(hostname, ip_str)){
        return;
    }

    char cachePath[MAX_HOSTNAME_LENGTH+10] = "";
    memset(cachePath, 0, MAX_HOSTNAME_LENGTH+10);
    strcat(cachePath,CACHE_DIR);
    strcat(temp, hostname);
    strcat(temp, filePath);
    compute_md5(temp, hash);
    strncat(cachePath,hash,16);

    const char* fileExtension = getFileExtension(filePath);
    const char* contentType = getContentType(fileExtension);
    if (EXTRA_CREDIT_DEBUG) printf("Cache Path: %s\n", cachePath);
    if (EXTRA_CREDIT_DEBUG) printf("File Extension: %s\n", fileExtension);
    if (EXTRA_CREDIT_DEBUG) printf("Content Type: %s\n", contentType);
    //     if (isValidFile(cachePath, pageTimeout)){
    //         FILE* fptr;
    //         if ((fptr = fopen(cachePath, "rb"))){
    //             fseek(fptr, 0, SEEK_END);
    //             long unsigned int length = ftell(fptr);
    //             rewind(fptr);
    //             unsigned char* fileReadBuffer = (unsigned char*) malloc(length * sizeof(char));
    //             int fd = open(cachePath, O_RDWR, 0644);
    //             flock(fd, LOCK_EX);
    //             fread(fileReadBuffer, 1, length, fptr);
    //             flock(fd, LOCK_UN);
    //             close(fd);
    //             //Need to formulate this into a packet first
    //             char* fullPacketBuffer = (char*) malloc(length + MAX_RESPONSE_HEADER_SIZE * sizeof(char));
    //             int bytesWritten = snprintf(fullPacketBuffer, MAX_RESPONSE_HEADER_SIZE,
    //                 "%s %d %s\r\n"
    //                 "Connection: %s\r\n"
    //                 "Content-Type: %s\r\n"
    //                 "Content-Length: %ld\r\n"
    //                 "\r\n",
    //                 httpVersion, OK, "OK",
    //                 connHeader,
    //                 contentType,
    //                 length);
    //             memmove(fullPacketBuffer + bytesWritten, fileReadBuffer, length);
    //             send(clientSocket, fullPacketBuffer, length + bytesWritten, 0);
    //             // if (DEBUG) {
    //             //     printf("\n==================================\n");
    //             //     printf("=Proxy to client cache filed packet=\n");
    //             //     printf("==================================\n\n");
    //             //     fwrite(fullPacketBuffer, 1, length+bytesWritten, stdout);
    //             // }
    //             fclose(fptr);
    //             free(fileReadBuffer);
    //             free(fullPacketBuffer);
    //             continue;
    //         }
    //     }
    // }
    // char requestPacketBuffer[MAX_RESPONSE_HEADER_SIZE];
    // memset(requestPacketBuffer, 0, MAX_RESPONSE_HEADER_SIZE);
    // struct sockaddr_in serveraddr;

    // if (!server){
    //     sendErrorPacket(clientSocket, 404, "Host not found");
    //     return exitServeClient(clientSocket, "Exiting client on host not found...\n", NULL);
    // }
    // serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    // int op = 1;
    // int optval = 1; 
    // if (serverSocket < 0 || setsockopt(serverSocket, SOL_SOCKET,SO_REUSEADDR, &op, sizeof(op)) < 0) {
    //     sendErrorPacket(clientSocket, 500, "Couldnt open server socket");
    //     return exitServeClient(clientSocket, "Couldnt open server socket...\n", NULL);
    // }
    // bzero(&serveraddr, sizeof(serveraddr));
    // serveraddr.sin_family = AF_INET;
    // serveraddr.sin_port = htons(port);
    // memcpy(&serveraddr.sin_addr.s_addr, server->h_addr, server->h_length);

    // if (connect(serverSocket, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) != 0){
    //     close(serverSocket);
    //     sendErrorPacket(clientSocket, 500, "Couldnt connect with server");
    //     return exitServeClient(clientSocket, "Couldnt connect with server...\n", NULL);
    // }

    // int bytesWritten = snprintf(requestPacketBuffer, MAX_RESPONSE_HEADER_SIZE,
    //     "GET %s %s\r\n"
    //     "Connection: %s\r\n"
    //     "Host: %s\r\n"
    //     "\r\n",
    //     filePath, httpVersion,
    //     connHeader,
    //     hostname);
    // if (DEBUG) {
    //     printf("\n===================================\n");
    //     printf("===Proxy to server request packet===\n");
    //     printf("====================================\n\n");
    //     fwrite(requestPacketBuffer, 1, MAX_RESPONSE_HEADER_SIZE, stdout);
    // }
    // if (send(serverSocket, requestPacketBuffer, bytesWritten, 0) <= 0){
    //     sendErrorPacket(clientSocket, 500, "Error sending to server");
    //     return exitServeClient(clientSocket, "Couldnt send to server...\n", NULL);
    // }
    // int inData = 0;
    // FILE* fptr;
    // if (!dynamic){
    //     fptr = fopen(cachePath, "wb");
    // }
    // char* bodyStart;
    // int totalBodyReceived = 0;
    // char *headerEnd = NULL;
    // struct timeval timeout;
    // timeout.tv_sec = 10; 
    // timeout.tv_usec = 0;
    // setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    // int totalSize = 0;                    // Total bytes accumulated
    // int allocatedSize = INITIAL_BUFFER_SIZE;
    // char *responseBuffer = malloc(allocatedSize);
    // if (!responseBuffer) {
    //     perror("malloc failed");
    //     return exitServeClient(clientSocket, "Memory allocation error\n", NULL);
    // }
    
    // int headerParsed = 0;
    // int contentLength = -1;
    // int bytesReceived = 0;
    
    // while ((bytesReceived = recv(serverSocket, receiveBuffer, BUFFER_SIZE, 0)) > 0) {
    //     if (totalSize + bytesReceived > allocatedSize) {
    //         allocatedSize = (totalSize + bytesReceived) * 2;
    //         char *temp = realloc(responseBuffer, allocatedSize);
    //         if (!temp) {
    //             perror("realloc failed");
    //             free(responseBuffer);
    //             return exitServeClient(clientSocket, "Memory allocation error\n", NULL);
    //         }
    //         responseBuffer = temp;
    //     }
    
    //     memcpy(responseBuffer + totalSize, receiveBuffer, bytesReceived);
    //     totalSize += bytesReceived;
    
    //     if (!headerParsed) {
    //         char *headerEnd = strstr(responseBuffer, "\r\n\r\n");
    //         if (headerEnd != NULL) {
    //             headerParsed = 1;
    //             char *clHeader = strstr(responseBuffer, "Content-Length:");
    //             if (clHeader) {
    //                 sscanf(clHeader, "Content-Length: %d", &contentLength);
    //             }
    //         }
    //     }
        
    //     if (headerParsed && contentLength != -1) {
    //         char *headerEnd = strstr(responseBuffer, "\r\n\r\n");
    //         if (headerEnd != NULL) {
    //             int headerLength = (headerEnd - responseBuffer) + 4;
    //             if (totalSize - headerLength >= contentLength) {
    //                 break;
    //             }
    //         }
    //     }
    // }
    // if (fptr) {
    //     char *headerEnd = strstr(responseBuffer, "\r\n\r\n");
    //     char *contentTypeHeader = strstr(responseBuffer, "Content-Type:");
    //     char contentType1[50];
    //     int fetch = 0;
    //     if (contentTypeHeader) {
    //         sscanf(contentTypeHeader, "Content-Type: %s", contentType1);

    //         if (strstr(contentType1, "text/html")){
    //             fetch = 1;
    //         } 
    //     }
    //     if (headerEnd != NULL) {
    //         int headerLength = (headerEnd - responseBuffer) + 4;
    //         int fd = open(cachePath, O_RDWR, 0644);
    //         flock(fd, LOCK_EX);
    //         fwrite(responseBuffer + headerLength, 1, totalSize - headerLength, fptr);
    //         flock(fd, LOCK_UN);
    //         close(fd);
    //     }
    //     fclose(fptr);
    // }
    // free(responseBuffer);
}
void preFetchPath(int socket, char* filename){
    return;
}