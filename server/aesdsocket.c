/*****************************************************************
* Assignment 5 - Socket
* Chris Biedermann
******************************************************************/

//updated March 2023/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h> 
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/time.h>

#define LISTEN_BACKLOG  10
#define BUFF_SIZE   100
#define TOTAL_THREADS 10

char *port = "9000";
char *file_path = "/var/tmp/aesdsocketdata";

//defining socket file descriptor
int sfd = 0;
int accept_fd = 0;
int fd = 0;

//Function prototypes
void socket_open(void);
void* thread_handler(void* thread_param);
void temp_function(void);

//Packet parse variables
int packet_size = 0;
char c = 0;

//Thread parameter structure
typedef struct{
    bool thread_comp_flag;
    pthread_t thread_id;
    int cl_accept_fd;
    pthread_mutex_t *mutex;
}thread_params;

//Linked list node
struct slist_data_s{
    thread_params   thread_param;
    SLIST_ENTRY(slist_data_s) entries;
};

typedef struct slist_data_s slist_data_t;
pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;
slist_data_t *datap = NULL;
SLIST_HEAD(slisthead, slist_data_s) head;

//Function prototypes
static void graceful_exit(void);


void sighandler(int sig_no){

    //handle the particular signal
    switch(sig_no)
    {
        case SIGINT:
        case SIGTERM:
        case SIGKILL:
            printf("SIGINT/SIGTEM/SIGKILL detected!\n");
            syslog(LOG_DEBUG,"Caught signal, exiting");
            break;

    }

    graceful_exit();

    exit(EXIT_SUCCESS);
}


static void graceful_exit(void){
    
    /*A6 thread part graceful exit - free all the 
    thread parameters*/
    while(SLIST_FIRST(&head) != NULL){
        SLIST_FOREACH(datap,&head,entries){
            close(datap->thread_param.cl_accept_fd);
            pthread_join(datap->thread_param.thread_id,NULL);
            SLIST_REMOVE(&head, datap, slist_data_s, entries);
            free(datap);
            break;
        }
    }

    //Free mutex
    pthread_mutex_destroy(&mutex_lock);
    
    unlink(file_path); //delete the file
    //free(op_buffer);
    close(accept_fd);
    close(sfd);

}


static void timer_handler(int sig_no){

    /*first store the local time in a buffer*/
    char time_string[200];
    time_t ti;
    struct tm *tm_ptr;
    int timer_len = 0;

    ti = time(NULL);
    tm_ptr = localtime(&ti);
    if(tm_ptr == NULL){
        perror("Local timer error!");
        exit(EXIT_FAILURE);
    }

    timer_len = strftime(time_string,sizeof(time_string),"timestamp:%d.%b.%y - %k:%M:%S\n",tm_ptr);
    if(timer_len == 0){
        perror("strftimer returned 0!");
        exit(EXIT_FAILURE);
    }

    printf("time value:%s\n",time_string);

    /*Now write this time to the file*/
    //int fd = open(file_path,O_APPEND | O_WRONLY);
    fd = open(file_path,O_APPEND | O_WRONLY);
    if(fd == -1){
        printf("File open error for appending\n");
        exit(EXIT_FAILURE);
    }

    int m_ret = pthread_mutex_lock(&mutex_lock);
    if(m_ret){
        printf("Mutex lock error before write\n");
        exit(EXIT_FAILURE);
    }

    int nr = write(fd,time_string,timer_len);
    if(nr == -1){
        printf("Error: File could not be written!\n");
        syslog(LOG_ERR,"Error: File could not be written!");
        exit(EXIT_FAILURE);
    }else if(nr != timer_len){
        printf("Error: File partially written!\n");
        syslog(LOG_ERR,"Error: File partially written!");
        exit(EXIT_FAILURE);
    }
    /*update the global packet size variable, as this is used for reading and sending data
    to client*/
    packet_size += timer_len;

    m_ret = pthread_mutex_unlock(&mutex_lock);
    if(m_ret){
        printf("Mutex unlock error after write\n");
        exit(EXIT_FAILURE);
    }

    close(fd);

}

int main(int argc, char *argv[])
{

    //open the log file
    openlog("aesdsocket-a5",LOG_PID,LOG_USER);

    //Write a test string to log
    syslog(LOG_DEBUG,"Syslog opened");

    /*register for signal*/
    signal(SIGINT,sighandler);
    signal(SIGTERM,sighandler);
    signal(SIGKILL,sighandler);

    pthread_mutex_init(&mutex_lock, NULL);
    
    //Timer configutaion for A6-P1
    //registering signal handler for timer
    signal(SIGALRM,timer_handler);

    //Check the actual value of argv here:
    if((argc > 1) && (!strcmp("-d",(char*)argv[1]))){
    
    //if(argc > 1){
        printf("Entering daemon mode!\n");
        syslog(LOG_DEBUG,"aesdsocket entering daemon mode");
        
        //Enter daemon mode
        int d_ret = daemon(0,0);
        if(d_ret == -1){
            printf("Entering daemon mode failed!\n");
            syslog(LOG_DEBUG,"Entering daemon mode failed!");
            printf("daemon error: %s\n",strerror(errno));
        }
    }

    socket_open();

    //closing syslog
    closelog();

    return 0;
}


void socket_open(void)
{
    //defining all socket related resources
    struct addrinfo hints;
    struct addrinfo *results;
    struct sockaddr client_addr;
    socklen_t client_addr_size;
    char ipv_4[INET_ADDRSTRLEN];
    
    //new variables for A6-P1
    SLIST_INIT(&head);

    //1. Set the sockaddr using getaddrinfo
    
    //clear the hints first
    memset(&hints,0,sizeof(hints));
    
    //set all the hint parameters then
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    //store the result 
    int getaddr_ret = getaddrinfo(NULL,port,&hints,&results);
    if(getaddr_ret != 0){
        printf("Error: getaddrinfo failed\n");
        syslog(LOG_ERR,"Error: getaddrinfo failed");
        exit(1);
    }

    //2. First open the socket
    printf("Opening socket.\n");
    sfd = socket(PF_INET6, SOCK_STREAM, 0);//IPv4,TCP,Any protocol
    if(sfd == -1){
        printf("Error: socket() failed\n");
        syslog(LOG_ERR,"Error: socket() failed");
        freeaddrinfo(results);
        exit(1);
    }

    int sopt_ret = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if(sopt_ret < 0){
        printf("Error: setsockopt failed\n");
        syslog(LOG_ERR,"Error: setsockopt failed");
        printf("setsockopt error: %s\n",strerror(errno));
        freeaddrinfo(results);
        exit(1);
    }

    //3. Bind / assign name to a socket
    printf("Binding the socket descriptor:%d.\n",sfd);
    int bind_ret = bind(sfd, results->ai_addr, results->ai_addrlen);
    if(bind_ret == -1){
        printf("Error: Bind failed\n");
        syslog(LOG_ERR,"Error: Bind failed");
        printf("bind error: %s\n",strerror(errno));
        freeaddrinfo(results);
        exit(1);
    }

    //Create file
    fd = creat(file_path, 0644);
    if(fd == -1){
        printf("Error: File could not be created!\n");
        syslog(LOG_ERR,"Error: File could not be created!");
        exit(1);
    }

    //close fd after creating
    close(fd);

    //free after use
    freeaddrinfo(results);

    /*Timer handler part*/
    struct itimerval inter_timer;

    inter_timer.it_interval.tv_sec = 10; //timer interval of 10 secs
    inter_timer.it_interval.tv_usec = 0;
    inter_timer.it_value.tv_sec = 10; //time expiration of 10 secs
    inter_timer.it_value.tv_usec = 0;

    //arming the timer, choosing wall clock, not storing in old_value
    int tm_ret = setitimer(ITIMER_REAL, &inter_timer, NULL);
    if(tm_ret == -1){
        printf("timer error:%s\n",strerror(errno));
        syslog(LOG_DEBUG,"timer error:%s",strerror(errno));
    }

    while(1){

        //4. Listen on the socket
        printf("Listening on socket.\n");
        int list_ret = listen(sfd, LISTEN_BACKLOG);
        if(list_ret == -1){
            printf("Error: Listen failed\n");
            syslog(LOG_ERR,"Error: Listen failed");
            freeaddrinfo(results);
            exit(1);
        }

        //5. accept the socket
        client_addr_size = sizeof(struct sockaddr);
    
        printf("Accepting connection.\n");
        accept_fd = accept(sfd,(struct sockaddr *)&client_addr, &client_addr_size);
        if(accept_fd == -1){
            printf("Error: accept failed\n");
            printf("accept error: %s\n",strerror(errno));
            syslog(LOG_ERR,"Error: accept failed");
            freeaddrinfo(results);
            exit(1);
        }

        //Below logic required to get ip address in readable format.
        struct sockaddr_in *addr = (struct sockaddr_in *)&client_addr;

        inet_ntop(AF_INET, &(addr->sin_addr),ipv_4,INET_ADDRSTRLEN);

        //Logging the client connection and address
        syslog(LOG_DEBUG,"Accepting connection from %s",ipv_4);
        printf("Accepting connection from %s\n",ipv_4);

        /*Adding below part for A6-P1*/

        //allocating new node for the data
        datap = (slist_data_t *) malloc(sizeof(slist_data_t));
        SLIST_INSERT_HEAD(&head,datap,entries);

        //Inserting thread parameters now
        datap->thread_param.cl_accept_fd = accept_fd;
        datap->thread_param.thread_comp_flag = false;
        datap->thread_param.mutex = &mutex_lock;

        pthread_create(&(datap->thread_param.thread_id),            //the thread id to be created
                            NULL,           //the thread attribute to be passed
                            thread_handler,             //the thread handler to be executed
                            &datap->thread_param//the thread parameter to be passed
                            );

        printf("All thread created now waiting to exit\n");

        SLIST_FOREACH(datap,&head,entries){
            //pthread_join(datap->thread_param.thread_id,NULL);
            if(datap->thread_param.thread_comp_flag == true){
                pthread_join(datap->thread_param.thread_id,NULL);
                SLIST_REMOVE(&head, datap, slist_data_s, entries);
                free(datap);
                break;
            }
        }
        /*The above code follow the below logic:
            while(head!=NULL){
                data = head;
                head = head->next;
                free(data);
            }
        */
        
        printf("All thread exited!\n");

        syslog(LOG_DEBUG,"Closed connection from %s",ipv_4);
        printf("Closed connection from %s\n",ipv_4);

    }

    //9. Close sfd, accept_fd
    close(accept_fd);
    close(sfd);

}


void* thread_handler(void* thread_param){

    //Package storage related variables
    bool packet_comp = false;
    int i;
    int recv_ret = 0;
    int m_ret = 0;
    char buff[BUFF_SIZE] = {0};
    char *op_buffer = NULL;

    //get the parameter of the thread
    thread_params * params = (thread_params *) thread_param;

    //For test
    op_buffer = (char *) malloc(sizeof(char)*BUFF_SIZE);
    if(op_buffer == NULL){
        printf("Malloc failed!\n");
        exit(1);
    }
    memset(op_buffer,0,BUFF_SIZE);
    //For test

    /*Packet reception, detection and storage logic*/
    while(packet_comp == false){

        //printf("Receiving data from descriptor:%d.\n",sfd);

        recv_ret = recv(params->cl_accept_fd, buff, BUFF_SIZE ,0); //**!check the flag
        if(recv_ret < 0){
            printf("Error: Receive failed\n");
            printf("recv error: %s\n",strerror(errno));
            syslog(LOG_ERR,"Error: Receive failed");
            exit(1);
        }else if(recv_ret == 0){
            break;
        }

        /*Detect '\n' or ASCII value 
            10 in the packet.
        */
        for(i = 0;i < BUFF_SIZE;i++){

            if(buff[i] == 10){
                packet_comp = true;
                i++;
                break;
            }

        }
        packet_size += i;

        /*reallocate to a larger buffer now as static buffer can
            only accomodate upto fixed size*/
        op_buffer = (char *) realloc(op_buffer,(packet_size+1));
        if(op_buffer == NULL){
            printf("Realloc failed\n");
            exit(1);
        }

        strncat(op_buffer,buff,i);

        memset(buff,0,BUFF_SIZE);
    }

    //Test
    m_ret = pthread_mutex_lock(params->mutex);
    if(m_ret){
        printf("Mutex lock error before write\n");
        exit(1);
    }
    //Test

    /*Write the data received from client to the 
    file first & open in append mode*/
    int fd = open(file_path,O_APPEND | O_WRONLY);
    if(fd == -1){
        printf("File open error for appending\n");
        exit(1);
    }

    int nr = write(fd,op_buffer,strlen(op_buffer));
    if(nr == -1){
        printf("Error: File could not be written!\n");
        syslog(LOG_ERR,"Error: File could not be written!");
        exit(1);
    }else if(nr != strlen(op_buffer)){
        printf("Error: File partially written!\n");
        syslog(LOG_ERR,"Error: File partially written!");
        exit(1);
    }

    close(fd);

    /*Below is the logic for reading the data from the
        input file byte by byte and sending to client.
    */
    memset(buff,0,BUFF_SIZE);

    fd = open(file_path,O_RDONLY);
    if(fd == -1){
        printf("File open error for reading\n");
        exit(1);
    }

    //sending data byte-by-byte
    for(int i=0;i<packet_size;i++){

        //read the file data in a buffer
        int rd = read(fd, &c, 1);
        //printf("---Read File descriptor:%d\n---",fd);
        if(rd == -1){
            printf("Reading from file failed!\n");
            printf("file read error: %s\n",strerror(errno));
            exit(1);
        }

        int send_ret = send(params->cl_accept_fd,&c,1,0);
        //printf("---Write Client descriptor:%d\n---",params->cl_accept_fd);
        if(send_ret == -1){
            printf("Error: Data could not be sent:%s\n",strerror(errno));
            syslog(LOG_ERR,"Error: Data could not be sent");
            exit(1);
        }
    }
    
    close(fd);

    m_ret = pthread_mutex_unlock(params->mutex);
    if(m_ret){
        printf("Mutex unlock error after read/send\n");
        exit(1);
    }

    params->thread_comp_flag = true;

    close(params->cl_accept_fd);

    //Free the allocated buffer
    free(op_buffer);


    return params;
}