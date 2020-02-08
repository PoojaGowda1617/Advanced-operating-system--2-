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

#define CLIENT_NODE_COUNT (5 + 1) // 5 clients + 1 server 0 as client
#define MAX_NODE_BACKLOG CLIENT_NODE_COUNT
#define SERVER_COUNT 7
#define PATH_TO_SERVER_CONFIG "../config/serverconfig.txt" //PATH to global file
#define MAX_MESSAGE_QUEUE_SIZE (CLIENT_NODE_COUNT+1)

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

typedef struct clientnodes {
    int clientid;
    int socketfd;
}clientnodes; //struct to hold client nodes info


//***********************************Queue handling**************************************

typedef enum QUEUE_CODE {
    QUEUE_CODE_FULL,
    QUEUE_CODE_EMPTH,
    QUEUE_CODE_OVERFLOW,
    QUEUE_CODE_UNDERFLOW,
    QUEUE_CODE_SUCCESS,
    QUEUE_CODE_INVALID
    
}QUEUE_CODE;

typedef struct queue {
    int front,rear,count;
    message_server server_msg_queue[MAX_MESSAGE_QUEUE_SIZE];
}QUEUE;
//***************************************************************************************
