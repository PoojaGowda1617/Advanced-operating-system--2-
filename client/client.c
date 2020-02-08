#include "client.h"

clientnodes currentnodeinfo; //self node information

int close_all_serv_connections = 0;

sync_data server_sync[SERVER_COUNT];
pthread_cond_t server_intn_done;

pthread_t server_threads[SERVER_COUNT]; //threads to interact with server
pthread_t request_generator;

struct timeval req_origination, req_processed_time;

static int serverresponsecount = 0,access_req_msgs = 0, release_req_msgs = 0,current_req_count=0;
static int serverrespawaitingcount = 0,received_msgs=0;

pthread_mutex_t serverresponsemutex,logger_lock;

message_server serverindividualmessages[SERVER_COUNT];

quorum_st quorums[] = {
    {3,{1,2,4}},
    {3,{1,2,5}},
    {3,{1,4,5}},
    {3,{1,3,6}},
    {3,{1,3,7}},
    {3,{1,6,7}},
    {4,{2,4,3,6}},
    {4,{2,4,3,7}},
    {4,{2,4,6,7}},
    {4,{2,5,3,6}},
    {4,{2,5,3,7}},
    {4,{2,5,6,7}},
    {4,{4,5,3,6}},
    {4,{4,5,3,7}},
    {4,{4,5,6,7}},
};

int parseConfigFiles(int myid) {
    
    FILE *file;
    int i = 0;
    
    file = fopen(PATH_TO_SERVER_CONFIG, "r");
    
    if(file==NULL) {
        printf("Error: can't open server config file\n");
        return -1;
    } else {
        printf("server config file opened successfully\n");
    }
    
    for(i=0;i<SERVER_COUNT;i++)
    {
        fscanf(file,"%d",&servers[i].serverid);//Reading server info from config file
        fscanf(file,"%s",servers[i].ip);
        fscanf(file,"%d",&servers[i].port);
        servers[i].socketfd = -1;
    }
    
    printf("====================================SERVERS================================================\n\n");
    
    for(i = 0 ; i < SERVER_COUNT ; i++)
        printf("id %d ip %s and port %d\n",servers[i].serverid,servers[i].ip,servers[i].port);
    
    printf("====================================================================================\n\n");
    return 0;
}

void *server_thread(void *serverid) {
    
    int id = (uintptr_t)serverid,set = 1;
    servernodes *serverinfo = &servers[id];
    struct sockaddr_in server_addr;
    struct hostent *host;
    message_server server_msg = {0};
    message_server server_resp = {0};
    
    printf("Establishing connection to server id %d, ip %s and port %d\n",serverinfo->serverid,serverinfo->ip,serverinfo->port);
    
    if ((serverinfo->socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("FAULT: Server socket create failed");
        exit(1);
    }
    
    if (setsockopt(serverinfo->socketfd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(int)) == -1) {
        perror("setsockopt() failed");
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
   printf("Server %d connection successfull \n",id); 
    while(1) {
        pthread_mutex_lock(&server_sync[id].lock);
        pthread_cond_wait(&server_sync[id].wait,&server_sync[id].lock);
        pthread_mutex_unlock(&server_sync[id].lock);
        
        if(close_all_serv_connections == 1) {
            return NULL;
        }
        
        server_msg = serverindividualmessages[id]; //Read the request from the server specific slot and send req
        send(serverinfo->socketfd,&server_msg,sizeof(message_server),0); //sending message
        int ret = recv(serverinfo->socketfd,(void *)&server_resp,sizeof(message_server),MSG_WAITALL);
        if(ret <= 0) {
            printf("server request failed for op %d for serv id %d\n",server_msg.subtype.request.type,id);
        } else {
            if(server_resp.subtype.response.type == SERVER_RESPONSE_FAILED) {
                printf("Request to server %d Operation failed\n",id);
            }
        }
        pthread_mutex_lock(&serverresponsemutex);
        serverresponsecount++; //Keep track of how many servers have responded.
        if(server_resp.subtype.response.type == SERVER_RESPONSE_GRANTED) {
        printf("Recived Grant message from server %d\n",id);
        }

        if(serverresponsecount == serverrespawaitingcount) {
            
            //Once we receive expected number of responces, siganal to the waiting party.
           // printf("server interaction finished\n");
            pthread_cond_signal(&server_intn_done);
            
            if(server_resp.subtype.response.type == SERVER_RESPONSE_GRANTED) {
                gettimeofday(&req_processed_time, NULL);

                printf("Delay between request and the Grant is %d\n",req_processed_time.tv_usec-req_origination.tv_usec);
                printf("Recived All Grants\n");

                FILE *file;
                char buf[100] = {0};
                file = fopen(PATH_TO_GLOBAL, "a+");
                snprintf(buf, 100, "entering %d %jd", currentnodeinfo.clientid,time(NULL));
                
                if(file != NULL) {
                    fseek(file, 0, SEEK_END);
                    fprintf(file,"%s\n", buf);
                    sleep(3);
                    fclose(file);
                }
            }
        }
        pthread_mutex_unlock(&serverresponsemutex);
        
    }
    exit(0);
}

void *request_generator_function(void *param) {
    
    int i = 0;
    while(current_req_count <=  (MAX_MESSAGES - 1)) {
        unsigned int sleeptime = ((rand() %5000000) + 5000000);
        usleep(sleeptime); //Random //TODO: CHANGE THE LOGIC
        current_req_count ++;
        quorum_st quorum_single = quorums[rand()%(sizeof(quorums)/sizeof(quorum_st))];
        
        printf("==============================================\n\n\n");
        printf("Request no: %d generated for quorum server count %d\n",current_req_count,quorum_single.srv_count);
        
        pthread_mutex_lock(&serverresponsemutex);
        serverresponsecount = 0;
        serverrespawaitingcount = 0;
        
        gettimeofday(&req_origination, NULL);

        for ( i = 0;i < quorum_single.srv_count ; i++) {
            printf("Sending access message to server %d\n",quorum_single.srv_id[i]);
            access_req_msgs++;
            message_server *msg = &serverindividualmessages[quorum_single.srv_id[i]];
            msg->id = currentnodeinfo.clientid;
            msg->type = SERVER_MESSAGE_TYPE_REQUEST;
            msg->subtype.request.type = SERVER_REQUEST_ACCESS;
            msg->timestamp = time(NULL);
            pthread_mutex_lock(&server_sync[quorum_single.srv_id[i]].lock);
            pthread_cond_signal(&server_sync[quorum_single.srv_id[i]].wait);
            pthread_mutex_unlock(&server_sync[quorum_single.srv_id[i]].lock);
                ++serverrespawaitingcount; //Number of servers we are requesting. Response should match this count
        }
        
        received_msgs = access_req_msgs;
        pthread_cond_wait(&server_intn_done,&serverresponsemutex);
        
        serverresponsecount = 0;
        serverrespawaitingcount = 0;
        for ( i = 0;i < quorum_single.srv_count ; i++) {
            printf("Sending release message to server %d\n",quorum_single.srv_id[i]);
            release_req_msgs++;
            message_server *msg = &serverindividualmessages[quorum_single.srv_id[i]];
            msg->id = currentnodeinfo.clientid;
            msg->type = SERVER_MESSAGE_TYPE_REQUEST;
            msg->subtype.request.type = SERVER_REQUEST_RELEASE;
            msg->timestamp = time(NULL);
            pthread_mutex_lock(&server_sync[quorum_single.srv_id[i]].lock);
            pthread_cond_signal(&server_sync[quorum_single.srv_id[i]].wait);
            pthread_mutex_unlock(&server_sync[quorum_single.srv_id[i]].lock);
                ++serverrespawaitingcount; //Number of servers we are requesting. Response should match this count
        }
        pthread_cond_wait(&server_intn_done,&serverresponsemutex);
        pthread_mutex_unlock(&serverresponsemutex);
        printf("==============================================\n\n\n");

    }
    
    printf("***************************************************\n\n\n");
    printf("%d requests generated send completion notification to the server 0 for termination\n\n",current_req_count);
    
    message_server *msg = &serverindividualmessages[0];
    msg->id = currentnodeinfo.clientid;
    msg->type = SERVER_MESSAGE_TYPE_REQUEST;
    msg->subtype.request.type = SERVER_REQUEST_COMPLETION;
    msg->timestamp = time(NULL);
    
    servernodes *serverinfo = &servers[0];

    send(serverinfo->socketfd,msg,sizeof(message_server),0);
    
    printf("Total Access Requests sent: %d  Total Release messages sent %d \n\n",access_req_msgs,release_req_msgs);
    
    printf("Total Grants Received %d \n\n",received_msgs);
    
    printf("Total Messages received and Transmitted %d \n\n",access_req_msgs + release_req_msgs + received_msgs);


    printf("***************************************************\n\n\n");

    close_all_serv_connections = 1;
    
    for ( i = 0;i < SERVER_COUNT ; i++) {
        pthread_cond_signal(&server_sync[i].wait);
    }
    
    return NULL;
}

int main(int argc, char **argv) {
    
    struct ifaddrs *ifaddr, *ifa;
    char ip[NI_MAXHOST];
    int myid=-1,port=-1,i = 0;
    struct hostent *host;
    
    if(argc != 3) {
        printf("please provide arguments (clientID, PORT)\n");
        return 1;
    }
    pthread_cond_init(&server_intn_done,NULL);
    pthread_mutex_init(&serverresponsemutex,NULL);

    myid = atoi(argv[1]);
    port = atoi(argv[2]);
    
    if (myid < 0 || myid >= CLIENT_NODE_COUNT) {
        printf("client id not supported\n");
        return 1;
    }
    
      srand(time(0) + getpid()); //seed current time + pid
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("ERROR: getifaddrs\n");
        exit(1);
    }
    
    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if((ifa->ifa_addr->sa_family == AF_INET) && strncmp(ifa->ifa_name, "lo", 2)) {
            int s = getnameinfo(ifa->ifa_addr,  sizeof(struct sockaddr_in) , ip, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            printf("IP address is :%s\n", ip);
            break;
        }
    }
    freeifaddrs(ifaddr);
    if ((host=gethostbyname(ip)) == NULL)
    {
        perror("gethostbyname failed\n");
        exit(1);
    }
    
    strncpy(currentnodeinfo.ip,ip,25);
    currentnodeinfo.port = port;
    currentnodeinfo.clientid = myid;
    
    if(parseConfigFiles(myid) < 0) {
        printf("Error in config parse.. abort\n");
        exit(1);
    }

    // Establish connections with all servers.
    for(i = 0 ; i < SERVER_COUNT ; i++) {
        printf("Server %d thread started\n",i);
        pthread_mutex_init(&(server_sync[i].lock),NULL);
        pthread_cond_init(&(server_sync[i].wait),NULL);
        pthread_create( &server_threads[i], NULL, &server_thread, (void *)(uintptr_t)i);     //Creating server threads
   	sleep(2);
   }
    
    pthread_create( &request_generator, NULL, &request_generator_function, NULL);     //request generating thread
    
    for(i=0;i<SERVER_COUNT;i++) {
        pthread_join( server_threads[i], NULL);               //Join all server threads
    }
    
    pthread_join(request_generator,NULL);  //Join request gen thread

}
