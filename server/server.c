#include "server.h"

int myid,process_th_exit = 0;

clientnodes clinodes[CLIENT_NODE_COUNT];

pthread_t clientnode_threads[CLIENT_NODE_COUNT],proccess_thread; //threads to interact with client nodes and process thread

pthread_cond_t process_thread_wait;

static int is_Locked = 0;
pthread_mutex_t lock_mutex,requestqueue_lock,process_thread_lock;

int sockfd[CLIENT_NODE_COUNT];


//***********************************Queue handling**************************************

QUEUE request_queue;

void initqueue(QUEUE *queue);
QUEUE_CODE insert(QUEUE *queue,void *data);
QUEUE_CODE delete(QUEUE *source_queue,void *dest_buffer);

//***************************************************************************************

void sendResponse(int fd, message_server response){
    send(fd,&response,sizeof(message_server),0);
}

void closeallconnections(void) {
    
    int i = 0;
    for (i = 0 ; i < CLIENT_NODE_COUNT ; i++) {
        if(sockfd[i] != -1) {
            close(sockfd[i]);
            sockfd[i] = -1;
        }
    }
    process_th_exit = 1;
    pthread_cond_signal(&process_thread_wait);
}


void *processthread(void *param) {
    
    while(1) {
        pthread_mutex_lock(&process_thread_lock);
        pthread_cond_wait(&process_thread_wait,&process_thread_lock);
        pthread_mutex_unlock(&process_thread_lock);
        pthread_mutex_lock(&requestqueue_lock);
        if(process_th_exit == 1) {
	printf("Process thread exiting\n");
            return NULL;
        }
        message_server msg;
        QUEUE_CODE ret = delete(&request_queue,(void *)&msg);
        if(ret != QUEUE_CODE_SUCCESS) {
            pthread_mutex_unlock(&requestqueue_lock);
            continue;
        }
        pthread_mutex_lock(&lock_mutex);
        is_Locked = 1;
        pthread_mutex_unlock(&lock_mutex);
        
        message_server snd_msg = { 0 };
        
        snd_msg.type = SERVER_MESSAGE_TYPE_RESPONSE;
        snd_msg.id = myid;
        snd_msg.subtype.response.type = SERVER_RESPONSE_GRANTED;
        sendResponse(sockfd[msg.id],snd_msg);
	printf("Sending grant to clientg %d\n",msg.id);
        pthread_mutex_unlock(&requestqueue_lock);
    }
    
}

void * handleclient(void *val) {
    int fd = (uintptr_t)val;
    message_server rcv_msg = { 0 };
    
    while (1) {
        if( recv(fd , &rcv_msg, sizeof(message_server), MSG_WAITALL) <= 0){
            close(fd);
            return NULL;
        }
        
        printf("=================================\n\n\n");
        if(sockfd[rcv_msg.id] == -1 && rcv_msg.id != -1) {
            sockfd[rcv_msg.id] = fd;
        }
        
        switch(rcv_msg.type) {
             
            case SERVER_MESSAGE_TYPE_REQUEST: {
                switch(rcv_msg.subtype.request.type) {
                    
                    case SERVER_REQUEST_ACCESS: {
                        printf("Received access req from client %d\n",rcv_msg.id);
                        pthread_mutex_lock(&lock_mutex);
                        if(is_Locked) {
                            pthread_mutex_lock(&requestqueue_lock);
                            insert(&request_queue,(void *)&rcv_msg);
                            pthread_mutex_unlock(&requestqueue_lock);
                        } else {
                            is_Locked = 1;
                            message_server snd_msg = { 0 };
                            
                            snd_msg.type = SERVER_MESSAGE_TYPE_RESPONSE;
                            snd_msg.id = myid;
                            snd_msg.subtype.response.type = SERVER_RESPONSE_GRANTED;

			    printf("Sending Immediate Grant %d\n",rcv_msg.id);
                            sendResponse(fd,snd_msg);
                        }
                        pthread_mutex_unlock(&lock_mutex);
                        memset(&rcv_msg,0,sizeof(message_server));
                    }

                    break;
                    case SERVER_REQUEST_RELEASE: {
                        printf("Received RELEASE req from client %d\n",rcv_msg.id);
                        pthread_mutex_lock(&lock_mutex);
			printf("Lock released\n");
                        is_Locked = 0;
                        pthread_cond_signal(&process_thread_wait);
                        message_server snd_msg = { 0 };
                        
                        snd_msg.type = SERVER_MESSAGE_TYPE_RESPONSE;
                        snd_msg.id = myid;
                        snd_msg.subtype.response.type = SERVER_RESPONSE_SUCCESS;
                        sendResponse(fd,snd_msg);
                        pthread_mutex_unlock(&lock_mutex);
                    }
                    break;
                    case SERVER_REQUEST_TERMINATE: {
                        printf("Received Termination. Closing the connections\n");
                        closeallconnections();
                        return NULL;
                    }
                    default:
                        printf("Invalid OP %d\n",rcv_msg.id);
                        break;
                }
            }
                break;
            default :
                printf("Server received response message, send invalid reply");
                break;
        }
        
        printf("=================================\n\n\n");

    }

    return NULL;
    
}

int main(int argc, char **argv) {
    
    struct ifaddrs *ifaddr, *ifa;
    char ip[NI_MAXHOST];
    FILE *configfile;
    int listensockfd = -1,newfd = -1 , set = 1,port,currentconnections=0,i = 0;
    struct sockaddr_in addr,cliaddr;
    struct hostent *host;
    socklen_t len = sizeof (struct sockaddr_in);


    if(argc != 3) {
        printf("please provide arguments (serverID , PORT) \n");
        return 1;
    }
    myid = atoi(argv[1]);
    port = atoi(argv[2]);
    if (myid < 0 || myid > SERVER_COUNT) {
        printf("server id not supported\n");
        return 1;
    }
    
    //listen for connections
    if (getifaddrs(&ifaddr) == -1) {
        perror("ERROR: getifaddrs\n");
        exit(1);
    }
    
    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if((ifa->ifa_addr->sa_family == AF_INET) && strncmp(ifa->ifa_name, "lo", 2)) {
           int s = getnameinfo(ifa->ifa_addr,  sizeof(struct sockaddr_in) , ip, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            printf("IP address of this system is :%s\n", ip);
            break;
        }
    }
    freeifaddrs(ifaddr);
    if ((host=gethostbyname(ip)) == NULL)
    {
        perror("gethostbyname failed");
        exit(1);
    }
    configfile = fopen ( PATH_TO_SERVER_CONFIG, "a+" );
    if (!configfile) {
        perror( PATH_TO_SERVER_CONFIG);
        exit(1);
    }
    fprintf(configfile, "%d ", myid);
    fprintf(configfile, "%s ", ip);
    fprintf(configfile, "%d\n", port);

    fclose(configfile);
    
    initqueue(&request_queue);
    
    pthread_mutex_init(&lock_mutex,NULL);
    pthread_mutex_init(&requestqueue_lock,NULL);
    pthread_mutex_init(&process_thread_lock,NULL);
    pthread_cond_init(&process_thread_wait,NULL);
    
    for (i = 0 ; i < CLIENT_NODE_COUNT ; i++) {
        sockfd[i] = -1;
    }
    
    if((listensockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("FAULT: socket create failed");
        exit(1);
    }
    
    if (setsockopt(listensockfd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(int)) == -1) {
        perror("setsockopt() failed");
        exit(1);
    }
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr =  *((struct in_addr *)host->h_addr);
    memset(addr.sin_zero, '\0', sizeof (addr.sin_zero));
    
    if( bind(listensockfd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind() failed");
        close(listensockfd);
        exit(1);
    }
    
    if (listen(listensockfd,MAX_NODE_BACKLOG) == -1) {
        perror("listen failed");
        close(listensockfd);
        exit(1);
    }

    pthread_create( &proccess_thread, NULL, &processthread, NULL);     //process thread
    
    while (currentconnections < CLIENT_NODE_COUNT) {
        
        if(( newfd =  accept(listensockfd, (struct sockaddr *)&cliaddr, &len)) == -1) {
            perror("accept failed");
        }
    
    printf("A client connected\n");
    pthread_create( &clientnode_threads[currentconnections++], NULL, &handleclient, (void *)(uintptr_t)newfd);     //Creating server threas

    }
    
    for( i=0;i<currentconnections;i++)
    {
        pthread_join( clientnode_threads[i], NULL);               //Join all  threads
    }
    
    pthread_join(proccess_thread, NULL);
   
}



//************************************ QUEUE HANDLING *********************************

void initqueue(QUEUE *queue)
{
    memset(queue,0,sizeof(QUEUE));
    queue->front = -1;
    queue->rear = 0;
    queue->count = 0;
}

QUEUE_CODE insert(QUEUE *queue,void *data)
{
    int i = 0,j = 0;
    if(((queue->rear + 1)%MAX_MESSAGE_QUEUE_SIZE)  == queue->front) {
        return QUEUE_CODE_OVERFLOW;
    }

        message_server actualdata = *(message_server *)data;
    printf("Inserting req to the queue client id %d and time %jd \n",actualdata.id,actualdata.timestamp);

        if(queue->front != -1) {
            for( i = queue->front; i != queue->rear;) {
                message_server msg = (message_server)(queue->server_msg_queue[i]);
                if((actualdata.timestamp < msg.timestamp) || (actualdata.timestamp == msg.timestamp && (actualdata.id < msg.id))) { //New msg have highest priority so place it before
                    printf("This req has high priority so lets shift\n");
                    queue->server_msg_queue[i] = actualdata;
                    actualdata = msg;
                    for (j = (i+1)%MAX_MESSAGE_QUEUE_SIZE; j != queue->rear;) {
                        msg = (message_server)(queue->server_msg_queue[j]);
                        queue->server_msg_queue[j] = actualdata;
                        actualdata = msg;
                        j = (j+1)%MAX_MESSAGE_QUEUE_SIZE;
                        
                    }
                    break;
                }
                i = (i+1)%MAX_MESSAGE_QUEUE_SIZE;
            }
        }
        queue->server_msg_queue[queue->rear] = actualdata;
        queue->rear = (queue->rear + 1)%MAX_MESSAGE_QUEUE_SIZE;
    
    if(queue->front  == -1) {
        queue->front  = 0;
    }
    
    if(queue->front  == (queue->rear + 1) % MAX_MESSAGE_QUEUE_SIZE) {
        return QUEUE_CODE_FULL;
    }
    return QUEUE_CODE_SUCCESS;
    
}

QUEUE_CODE delete(QUEUE *source_queue,void *dest_buffer)
{
    
    if(source_queue->front == -1 || source_queue->front == source_queue->rear) {
        return QUEUE_CODE_UNDERFLOW;
    }
    
    if(source_queue->front == source_queue->rear) {
        source_queue->front = -1;
        source_queue->rear = 0;
        return QUEUE_CODE_EMPTH;
    }
    if(dest_buffer == NULL) {
        source_queue->front = (source_queue->front + 1)%MAX_MESSAGE_QUEUE_SIZE;
    } else {

            memcpy(dest_buffer,&source_queue->server_msg_queue[source_queue->front],sizeof(message_server));
            source_queue->front = (source_queue->front + 1)%MAX_MESSAGE_QUEUE_SIZE;
    }

    return QUEUE_CODE_SUCCESS;
}
//*************************************************************************************
