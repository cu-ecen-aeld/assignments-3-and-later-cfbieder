/*****************************************************************
* Assignment 5 - Socket
* Chris Biedermann
******************************************************************/

//updated March 2023

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
#include <sys/wait.h>

char *port = "9000";
char *output_file = "/var/tmp/aesdsocketdata";
#define BACKLOG 20     // how many pending connections queue will
#define BUFF_SIZE	100  //size of buffer for data received
char *op_buffer = NULL;
int sockfd = 0;
int serv_sock = 0;


void sigchld_handler(int s) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void sigint_sigterm_handler(int s) {
    remove(output_file);
    syslog(LOG_DEBUG, "Cought signal, exiting");
    closelog();
    exit(0);
}

void bind_signal(struct sigaction *sa, void (*handler)(int), int signal) {
    sa->sa_handler = handler;
    sigemptyset(&sa->sa_mask);
    sa->sa_flags = SA_RESTART;
    int ret_sa = sigaction(signal, sa, NULL);
    if ( ret_sa == -1) {
        const char msg[] = "sigaction() failed";
        perror(msg);
        syslog(LOG_ERR, msg);
        closelog();
        exit(-1);
    }
}



int main(int argc, char *argv[])  {

  //open the log file
  printf("Info: Starting Program with logging\n");
  openlog("aesdsocket",LOG_PID,LOG_USER);
  syslog(LOG_DEBUG,"Starting Program");



  // Get server address info`
  // make sure the struct is empty and set values
  int status;
  struct addrinfo hints;
  struct addrinfo *servinfo;

  memset(&hints, 0, sizeof hints);
  hints.ai_flags = AI_PASSIVE; // fill in my IP for me
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(NULL, port, &hints, &servinfo);
  if(status != 0){
        printf("Error: getaddrinfo failed\n");
        syslog(LOG_ERR,"Error: getaddrinfo failed");
        exit(1);
  }
  else {
    printf("Info: getaddrinfo completed\n");
    struct sockaddr_in *addr;
    addr = (struct sockaddr_in *)servinfo->ai_addr;
    printf("Info: IP address = %s\n",inet_ntoa((struct in_addr)addr->sin_addr));
    syslog(LOG_DEBUG,"getaddrinfo completed %s",inet_ntoa((struct in_addr)addr->sin_addr));

  }


  //Open the socket
  printf("Info: Opening the socket\n");
  syslog(LOG_DEBUG,"Opening the socket");
  serv_sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if(serv_sock == -1){
    printf("Error: socket() failed\n");
    syslog(LOG_ERR,"Error: socket() failed");
    freeaddrinfo(servinfo);
    exit(1);
  }

  //Bind to socket
  printf("Info: Binding the socket descriptor:%d.\n",serv_sock);
  syslog(LOG_DEBUG,"Binding the socket descriptor:%d.\n",serv_sock);
  int bind_sock = bind(serv_sock, servinfo->ai_addr, servinfo->ai_addrlen);
  if(bind_sock == -1){
    printf("Error: Bind failed\n");
    syslog(LOG_ERR,"Error: Bind failed");
    printf("Info: bind error: %s\n",strerror(errno));
    freeaddrinfo(servinfo);
    exit(1);
  }

  //Listen on socket

  struct sockaddr their_addr;
  socklen_t addr_size;
  addr_size = sizeof(struct sockaddr);
  struct sockaddr_in *addr;
  char ipv_4[INET_ADDRSTRLEN];



  printf("Info: Listening on socket.\n");
  syslog(LOG_DEBUG,"Listening on socket");
  int list_ret = listen(serv_sock, BACKLOG);
  if(list_ret == -1){
    printf("Error: Error on listen!\n");
    syslog(LOG_ERR,"Error: Error on listen!");
    exit(1);
  }

  //Create file
  printf("Info: Creating output file\n");
  syslog(LOG_DEBUG," Creating output file");
  int fd = creat(output_file, 0644);
  if(fd == -1){
    printf("Error: File could not be created!\n");
    syslog(LOG_ERR,"Error: File could not be created!");
    exit(1);
  }
  close(fd);

  // Daemonize
  if (argc > 1) {
      if (strcmp(argv[1], "-d") == 0) {
          if (fork()) {
              // ## Parent finish here ##
              exit(0);
          }
      }
  }

  struct sigaction sa_chld;
  bind_signal(&sa_chld, sigchld_handler, SIGCHLD);
  struct sigaction sa_int;
  bind_signal(&sa_int, sigint_sigterm_handler, SIGINT);
  struct sigaction sa_term;
  bind_signal(&sa_term, sigint_sigterm_handler, SIGTERM);

  while (1) {

      //make the buffer required for client input storage
      op_buffer = (char *) malloc(sizeof(char)*BUFF_SIZE);
      if(op_buffer == NULL){
        printf("Malloc failed!\n");
        exit(1);
      }
      memset(op_buffer,0,BUFF_SIZE);


      // accept an incoming connection:
      printf("Info: Ready to accept connection\n");
      syslog(LOG_DEBUG,"Ready to accept connection");
      sockfd = accept(serv_sock, (struct sockaddr *)&their_addr, &addr_size);
      if(sockfd == -1){
        printf("Error: Listen failed\n");
        syslog(LOG_ERR,"Error: Listen failed");
        freeaddrinfo(servinfo);
        exit(1);
      }

      addr = (struct sockaddr_in *)&their_addr;
      inet_ntop(AF_INET, &(addr->sin_addr),ipv_4,INET_ADDRSTRLEN);
      printf("Info: Accepted connection from %s\n",ipv_4);
      syslog(LOG_DEBUG,"Accepted connection from %s",ipv_4);

      int pid = fork();
      if (pid == -1) {
        printf( "ERROR pid\n" );
      }
      else if (pid == 0) { //// check if the child process
          int fd = open(output_file, O_RDWR | O_CREAT | O_APPEND, 0666);

          // Receive the data
          int recv_len;
          char recv_buf[200];

          do {
            recv_len = recv(sockfd, recv_buf, sizeof recv_buf, 0);
            if (recv_len < 0) {
              printf("Error: Receive failed: %s\n",strerror(errno));
              syslog(LOG_ERR,"Error: Receive failed");
              exit(1);
            }
            printf("Recv: %s", recv_buf);
            printf("Len: %d\n", recv_len);
            write(fd, recv_buf, recv_len);
          } while (recv_len == sizeof recv_buf);

          // Prepare Sending back
          int data_len = lseek(fd, 0, SEEK_END);  // Goto the last for checking size
          char *data_buf = malloc(data_len);

          lseek(fd, 0, SEEK_SET);  // Goto the beginning
          read(fd, data_buf, data_len);

          // Sending
          int send_len = send(sockfd,data_buf,data_len,0);
          if (send_len <0) {
            printf("Error: Sending failed: %s\n",strerror(errno));
            syslog(LOG_ERR,"Error: Send failed");
            exit(1);
          }

          free(data_buf);
          printf("Info: sent message:\n");
      }
      free(op_buffer);
      syslog(LOG_DEBUG,"Closed connection from %s",ipv_4);
      printf("Closed connection from %s\n",ipv_4);



  }

  close(sockfd);
  close(serv_sock);
  freeaddrinfo(servinfo);
  closelog();

}
