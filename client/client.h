#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
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

#define CLIENT_NODE_COUNT 5 //Total clients running 5
#define SERVER_COUNT (7 + 1) //7 + server 0 total 8
#define PATH_TO_SERVER_CONFIG "../config/serverconfig.txt" //PATH to global file
#define PATH_TO_GLOBAL "../File/log_file.txt" //PATH to global file
#define MAX_MESSAGES 20


void *request_generator_function(void *param);

typedef struct servernodes {
    int serverid;
    char ip[25];
    unsigned int port;
    int socketfd;
}servernodes;

servernodes servers[SERVER_COUNT];

typedef struct clientnodes {
    int clientid;
    char ip[25];
    unsigned int port;
    int socketfd;
}clientnodes; //struct to hold client nodes info

typedef struct sync_data {
    pthread_mutex_t lock;
    pthread_cond_t wait;
}sync_data;

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

typedef struct quorum_st {
    int srv_count;
    int srv_id[10];
}quorum_st;
