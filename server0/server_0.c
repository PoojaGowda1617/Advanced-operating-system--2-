#include "server_0.h"

int myid;

pthread_t clientnode_threads[CLIENT_NODE_COUNT],proccess_thread; //threads to interact with client nodes and process thread

static int completion_count = 0,connected_clients = 0;
pthread_mutex_t lock_mutex;
void endentiresystem(void);

int parseConfigFiles() {
    
    FILE *file;
    int i = 0;
    
    file = fopen(PATH_TO_SERVER_CONFIG, "r");
    
    if(file==NULL) {
        printf("Error: can't open server config file\n");
        return -1;
    }
    
    for(i=0;i<SERVER_COUNT;i++)
    {
        fscanf(file,"%d",&servers[i].serverid);//Reading server info from config file
        fscanf(file,"%s",servers[i].ip);
        fscanf(file,"%d",&servers[i].port);
        servers[i].socketfd = -1;
    }
    
    return 0;
}


void sendResponse(int fd, message_server response){
    send(fd,&response,sizeof(message_server),0);
}

void * handleclient(void *val) {
    int fd = (uintptr_t)val;
    message_server rcv_msg = { 0 };
    
        if( recv(fd , &rcv_msg, sizeof(message_server), MSG_WAITALL) <= 0){
            printf("recv error so exit\n");
            perror("recv() receiving request from server");
            close(fd);
            exit(1);
        }

        switch(rcv_msg.type) {
             
            case SERVER_MESSAGE_TYPE_REQUEST: {
                switch(rcv_msg.subtype.request.type) {
                    case SERVER_REQUEST_COMPLETION: {
                        pthread_mutex_lock(&lock_mutex);
                        completion_count++;
                        pthread_mutex_unlock(&lock_mutex);
                        printf("received completion message from client %d\n",rcv_msg.id);
                        if(completion_count == connected_clients) {
                            printf("============================================\n\n");
                            printf("All completion messages received Terminating commections\n");
                            endentiresystem(); printf("============================================\n\n");

                        }
                        close(fd);
                        return NULL;
                    }
                    break;
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
    close(fd);
    return NULL;
    
}

void endentiresystem(void) {
    
    int i = 0;
    parseConfigFiles();
    for (i = 1; i<SERVER_COUNT ; i++) {
        servernodes *serverinfo = &servers[i];
        struct sockaddr_in server_addr;
        struct hostent *host;
        message_server server_msg = {0};
        
        if ((serverinfo->socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("FAULT: Server socket create failed");
            exit(1);
        }
        if ((host=gethostbyname(serverinfo->ip)) == NULL)
        {
            perror("gethostbyname");
            exit(1);
        }
        
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(serverinfo->port);
        server_addr.sin_addr = *((struct in_addr *)host->h_addr);
        memset(&(server_addr.sin_zero), '\0', 8);
        
        if (connect(serverinfo->socketfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
        {
            perror("FAULT: Server socket connect failed");
            exit(1);
        }
        
        server_msg.id = -1; // to differentiate server 0
        server_msg.type = SERVER_MESSAGE_TYPE_REQUEST;
        server_msg.subtype.request.type = SERVER_REQUEST_TERMINATE;
        server_msg.timestamp = time(NULL);
        
        send(serverinfo->socketfd,&server_msg,sizeof(message_server),0);
    }
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
    if (myid < 0 || myid >= SERVER_COUNT) {
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
    
    pthread_mutex_init(&lock_mutex,NULL);
    
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
    
    while (currentconnections < CLIENT_NODE_COUNT) {
        
        if(( newfd =  accept(listensockfd, (struct sockaddr *)&cliaddr, &len)) == -1) {
            perror("accept failed");
        }
        connected_clients ++;        pthread_create( &clientnode_threads[currentconnections++], NULL, &handleclient, (void *)(uintptr_t)newfd);

    }
    
    for( i=0;i<currentconnections;i++)
    {
        pthread_join( clientnode_threads[i], NULL);
    }
}
