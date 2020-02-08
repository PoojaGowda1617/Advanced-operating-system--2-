#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <signal.h>
#include <netdb.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ifaddrs.h>

#define CLIENT_NODE_COUNT 5
#define MAX_NODE_BACKLOG 5
#define SERVER_COUNT (7 + 1) // 7 plus current server 0
#define PATH_TO_SERVER_CONFIG "../config/serverconfig.txt" //PATH to global file

typedef enum SERVER_MESSAGE_TYPE {
    SERVER_MESSAGE_TYPE_REQUEST,
    SERVER_MESSAGE_TYPE_RESPONSE
}SERVER_MESSAGE_TYPE;

typedef enum SERVER_REQUEST_RESPONSE {
    SERVER_REQUEST_ACCESS,
    SERVER_REQUEST_RELEASE,
    SERVER_REQUEST_COMPLETION,
    SERVER_REQUEST_TERMINATE,
    SERVER_RESPONSE_GRANTED,
    SERVER_RESPONSE_INVALID,
    SERVER_RESPONSE_SUCCESS,
    SERVER_RESPONSE_FAILED
}SERVER_REQUEST_RESPONSE;



typedef struct message_server {
    SERVER_MESSAGE_TYPE type;
    int id;
    time_t timestamp;
    union subtype{
        struct request{
            SERVER_REQUEST_RESPONSE type;
        }request;
        struct response {
            SERVER_REQUEST_RESPONSE type;
        }response;
    }subtype;
}message_server;

typedef struct servernodes {
    int serverid;
    char ip[25];
    unsigned int port;
    int socketfd;
}servernodes;

servernodes servers[SERVER_COUNT];
