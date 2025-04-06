#include <sys/file.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <error.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <openssl/md5.h>
#include <dirent.h>
#include <regex.h>
#define ERROR -1
#define BUFFER_SIZE 4096
#define MAX_RESPONSE_HEADER_SIZE 1024
#define CACHE_DIR "./cache/"
#define MAX_CLIENTS 100
#define OK 200
#define MAX_URL_LENGTH 512
#define MAX_HOSTNAME_LENGTH 256 //maybe? Idk how long the longest hostname is but these should be enough
#define DEBUG 0
#define EXTRA_CREDIT_DEBUG 1
#define INITIAL_BUFFER_SIZE 2048
#define BLOCKLIST_FILE "./blocklist"
#define MAX_PATTERN_SIZE 1024
atomic_int countActiveThreads;
int pageTimeout;