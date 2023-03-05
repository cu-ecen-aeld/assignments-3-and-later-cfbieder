/*********************************************************************************
Author      : 
Description : Socket Server Implementation
Reference   : https://beej.us/guide/bgnet/html/
              https://man7.org/linux/man-pages/man3/daemon.3.html
            
            
*********************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/time.h>

#define BUFFER_SIZE (1024)
#define PATH "/var/tmp/aesdsocketdata"
#define PORT "9000"

int len = 0;
char *client_buff;
int socktfd, clientfd;
int state;
char buff_rx[BUFFER_SIZE];
int file_fd;

typedef struct 
{
	int clientfd;
	pthread_mutex_t * mutex;
	pthread_t thread_id;
	bool thread_complete;
}thread_parameter;

struct slist_data_s
{
	thread_parameter thread_params;
	SLIST_ENTRY(slist_data_s) entries;
};

typedef struct slist_data_s slist_data_t;
pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;
slist_data_t *data_node = NULL;
SLIST_HEAD(slisthead, slist_data_s) head;

static void sign_handler()
{
	while (SLIST_FIRST(&head) != NULL)
	{
		SLIST_FOREACH(data_node, &head, entries)
		{
			close(data_node->thread_params.clientfd);				
			pthread_join(data_node->thread_params.thread_id, NULL);
			SLIST_REMOVE(&head, data_node,
			slist_data_s, entries);
			free(data_node);
			break;
		}
	}
	syslog(LOG_INFO, "Exiting and Clearing Buffers\n");
	printf("Clearing Buffers..\n");
	printf("Exiting..\n");
	unlink(PATH);
	close(socktfd);
	close(clientfd);
	exit(EXIT_SUCCESS);	
}

void *thread_function_handler(void *thread_param)
{
	bool p_state = false;
	ssize_t data_rx = 0;
	ssize_t data_wr = 0;
	
	thread_parameter *t_params = (thread_parameter*) thread_param;
	
	client_buff = (char*) malloc((sizeof(char) *BUFFER_SIZE));
	if (client_buff == NULL)
	{
		syslog(LOG_ERR, "Failed to allocate memory\n");
		exit(EXIT_FAILURE);
	}
	
	memset(client_buff, 0, BUFFER_SIZE);

	while (!p_state)
	{
				
		data_rx = recv(t_params->clientfd, buff_rx, BUFFER_SIZE, 0);
		if (data_rx == 0)
		{
			syslog(LOG_INFO,"Reception success\n");
		}
		else if (data_rx < 0)
		{
			syslog(LOG_ERR,"Error !! Recieve failed:%d\n", state);
			exit(EXIT_FAILURE);
		}
			
		int i;
		
		for (i = 0; i < BUFFER_SIZE; i++)
		{
			if (buff_rx[i] == '\n')
			{
				p_state = true;
				i++;
				break;
			}
		}

		len += i;

		client_buff = (char*) realloc(client_buff, (len + 1));
		if (client_buff == NULL)
		{
			syslog(LOG_ERR,"Error !! Realloc failed\n");
			exit(EXIT_FAILURE);
		}
		strncat(client_buff, buff_rx, i);
		memset(buff_rx, 0, BUFFER_SIZE);
	}	
	
	state = pthread_mutex_lock(t_params->mutex);
	if (state)
	{
		syslog(LOG_ERR, "Mutex loxk failed %d\n", state);
		exit(EXIT_FAILURE);
	}
	
	file_fd = open(PATH, O_APPEND | O_WRONLY);
	if (file_fd == -1)
	{
		syslog(LOG_ERR,"Error !! Failed to open file\n");
		exit(EXIT_FAILURE);
	}
		
	data_wr = write(file_fd, client_buff, strlen(client_buff));
	if (data_wr == -1)
	{
		syslog(LOG_ERR,"Error !! Write failed \n");
		exit(EXIT_FAILURE);
	}
	else if (data_wr != strlen(client_buff))
	{
		syslog(LOG_ERR,"Partial Write\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		syslog(LOG_INFO,"File write success!\n");
	}

	close(file_fd);
	
	file_fd = open(PATH, O_RDONLY);
	if (file_fd == -1)
	{
		syslog(LOG_ERR,"Error !! Open Failed to read\n");
		exit(EXIT_FAILURE);
	}
		
	char char_rd = 0;
		
	for (int i = 0; i < len; i++)
	{
		state = read(file_fd, &char_rd, 1);
		if (state == -1)
		{
			syslog(LOG_ERR,"Error !! Read Failed\n");
			exit(EXIT_FAILURE);
		}
		
		state = send(t_params->clientfd, &char_rd, 1, 0);
		if (state == -1)
		{
			syslog(LOG_ERR, "Error !! Send failed");
			exit(EXIT_FAILURE);
		}
	}
	
	state = pthread_mutex_unlock(t_params->mutex);
	if (state)
	{
		syslog(LOG_ERR, "pthread_mutex_unlock() failed with error number %d\n", state);
		exit(EXIT_FAILURE);
	}
	
	t_params->thread_complete = true;
	
	close(file_fd);
	free(client_buff);
	close(t_params->clientfd);
	
	return t_params;
}

static void timestamp(int signal)
{
	int fd;
	struct tm * pointer;
	char time_string[100];
	int writedata;
	time_t timestamp;

	timestamp = time(NULL);
	pointer = localtime(&timestamp);
	
	if (pointer == NULL)
	{
		syslog(LOG_ERR, "Error !! Localtime failed\n");
		exit(EXIT_FAILURE);
	}
	
	int length_time = 0;
	length_time = strftime(time_string, sizeof(time_string), "timestamp: %m/%d/%Y - %k:%M:%S\n", pointer);
	printf("%s\n", time_string);
	
	if (length_time == 0)
	{
		syslog(LOG_ERR, "Error !! Strftime failed\n");
		exit(EXIT_FAILURE);
	}

	fd = open(PATH, O_APPEND | O_WRONLY);
	if (fd == -1)
	{
		syslog(LOG_ERR, "Error !! File dint open\n");
		exit(EXIT_FAILURE);
	}

	state = pthread_mutex_lock(&mutex_lock);
	if (state)
	{
		syslog(LOG_ERR,"Error !! Mutex lock\n");
		exit(EXIT_FAILURE);
	}

	writedata = write(fd, time_string, length_time);
	if (writedata == -1)
	{
		syslog(LOG_ERR, "Error !! Write failed\n");
		exit(EXIT_FAILURE);
	}
	else if (writedata != length_time)
	{
		syslog(LOG_ERR, "Error !! Write failed completely\n");
		exit(EXIT_FAILURE);
	}

	len += length_time;

	state = pthread_mutex_unlock(&mutex_lock);
	if (state)
	{
		syslog(LOG_ERR,"Error !! Mutex Unlock\n");
		exit(EXIT_FAILURE);
	}

	close(fd);
}


int main(int argc, char *argv[])
{
	
	struct sockaddr_in client_addr;
	socklen_t client_addr_len;
	
	openlog("AESD - Assignment5 - Socket", 0, LOG_USER);
	
	if( argc > 2)
	{
		printf("Error : Invalid number of arguments\n");
		return -1;
	}

	if ((argc > 1) && (!strcmp("-d", (char*) argv[1])))
	{
	 /* Reference : https://man7.org/linux/man-pages/man3/daemon.3.html */
		state = daemon(0, 0);
		if (state == -1)
		{
			syslog(LOG_DEBUG, "Entering daemon mode failed!");
		}

	}
	
	signal(SIGINT, sign_handler);
	signal(SIGTERM, sign_handler);
	signal(SIGALRM, timestamp);
	
	pthread_mutex_init(&mutex_lock, NULL);
	
	SLIST_INIT(&head);
	struct addrinfo hints;
	struct addrinfo *param;
	struct itimerval t_interval;
	
	memset(buff_rx, 0, BUFFER_SIZE);
	memset(&hints, 0, sizeof(hints));
	
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	state = getaddrinfo(NULL, PORT, &hints, &param);
	if (state != 0)
	{
		syslog(LOG_ERR, "Error !! getaddrinfo failed \n");
		exit(EXIT_FAILURE);
	}
	
	socktfd = socket(AF_INET6, SOCK_STREAM, 0);
	if (socktfd == -1)
	{
		syslog(LOG_ERR, "Error !! Socket connection failed \n");
		exit(EXIT_FAILURE);
	}
	
	state = setsockopt(socktfd, SOL_SOCKET, SO_REUSEADDR, &(int)
	{ 1 }, sizeof(int));
	if (state == -1)
	{
		syslog(LOG_ERR, "setsockopt() failed.\n");
		printf("setsockopt() failed\n");
		exit(EXIT_FAILURE);
	}

	syslog(LOG_INFO, "setsockopt() succeeded!\n");
	printf("setsockopt() succeeded\n");
	
	state = bind(socktfd, param->ai_addr, param->ai_addrlen);
	if (state == -1)
	{
		syslog(LOG_ERR, "Error !! Binding failed : %d\n", state);
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(param);
	
	file_fd = creat(PATH, 0666);
	if (file_fd == -1)
	{
		syslog(LOG_ERR, "Error !! File creation failed : %d\n", file_fd);
		exit(EXIT_FAILURE);
	}
	
	t_interval.it_interval.tv_sec  = 10;	// 10 secs interval
	t_interval.it_interval.tv_usec = 0;
	t_interval.it_value.tv_sec     = 10;	//10 secs expiry
	t_interval.it_value.tv_usec    = 0;

	state = setitimer(ITIMER_REAL, &t_interval, NULL);
	if (state == -1)
	{
		syslog(LOG_ERR, "Failed in Set timer function");
		printf("setitimer() failed\n");
		exit(EXIT_FAILURE);
	}
	
	while(1)
	{

		state = listen(socktfd, 10);
		if (state == -1)
		{
			syslog(LOG_ERR, "Error !! Listen failed:%d\n", state);
			exit(EXIT_FAILURE);
		}

		client_addr_len = sizeof(struct sockaddr);

		clientfd = accept(socktfd, (struct sockaddr *) &client_addr, &client_addr_len);
		if (clientfd == -1)
		{
			syslog(LOG_ERR, " Error !! Accepting failed\n");
			exit(EXIT_FAILURE);
		}
		

		data_node = (slist_data_t*) malloc(sizeof(slist_data_t));
		SLIST_INSERT_HEAD(&head, data_node, entries);
		data_node->thread_params.clientfd = clientfd;
		data_node->thread_params.thread_complete = false;
		data_node->thread_params.mutex = &mutex_lock;

		pthread_create(&(data_node->thread_params.thread_id),	
			NULL,	
			thread_function_handler,	
			&data_node->thread_params);	

		printf("Threads Created\n");

		SLIST_FOREACH(data_node, &head, entries)
		{
			pthread_join(data_node->thread_params.thread_id, NULL);
			SLIST_REMOVE(&head, data_node, slist_data_s, entries);
			free(data_node);
			break;
		}

	}	
	close(file_fd);
	close(socktfd);
	syslog(LOG_INFO, "Connection closed\n");
	closelog();
	return 0;
}
