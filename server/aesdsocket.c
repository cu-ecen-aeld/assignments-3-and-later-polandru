#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <syslog.h>
#include <arpa/inet.h>
#include "queue.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#define BUFFSIZE 25000
#define _XOPEN_SOURCE 700

typedef struct slist_data_s slist_data_t;
struct slist_data_s
{
	pthread_t thread;
	bool running;
	int connfd;
	SLIST_ENTRY(slist_data_s)
	entries;
};

int sockfd;
pthread_mutex_t write_mutex = PTHREAD_MUTEX_INITIALIZER;
struct sigaction sa;

void handle_signals(int sgno)
{

	close(sockfd);
	syslog(LOG_ALERT, "Caught signal exiting");
	exit(0);
}

int send_data(int connfd, FILE *fp)
{

	
	char *data = calloc(sizeof(char), BUFFSIZE);
	char *tmp = calloc(sizeof(char), BUFFSIZE);
	for(int i =0; i< 10; i++){
		int c = fread(tmp, sizeof(char),BUFFSIZE, fp);
		if(c > 0){
			strcat(data,tmp);
			for(int t =0; t<strlen(data); t++){
				if(data[t] == '\n')
					i++;
			}
			
		}
	}
	write(connfd, data, strlen(data));
	free(data);
	return 0;
}
void *handle_connection(void *d)
{
	slist_data_t *data = (slist_data_t *)d;
	int connfd = data->connfd;
	data->running = true;

	char *buff;
	char tmp_buff[BUFFSIZE];

	for (;;)
	{

		buff = calloc(BUFFSIZE + 1, sizeof(char));
		int msg_len;
		msg_len = read(connfd, tmp_buff, BUFFSIZE);
		if (msg_len == 0)
		{
			break;
		}

		tmp_buff[msg_len] = '\0';
		strcat(buff, tmp_buff);

		FILE *output_file = fopen("/dev/aesdchar", "a+");
		fprintf(output_file, "%s", buff);
		fflush(output_file);
		send_data(connfd, output_file);

		free(buff);
		fclose(output_file);
	}

	close(connfd);
	free(buff);
	return NULL;
}


int main(int c, char **argv)
{
	sa.sa_handler = handle_signals;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);


	int len;
	struct sockaddr_in servaddr, cli;

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		perror("Can't create socket");
		exit(-1);
	}
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(9000);

	// Binding newly created socket to given IP and verification
	if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
	{
		perror("BIND Failed");
		exit(-1);
	}

	if (c == 2 && strcmp(argv[1], "-d") == 0)
	{
		pid_t pid;
		/* create new process */
		pid = fork();
		if (pid == -1)
			return -1;
		else if (pid != 0)
			exit(EXIT_SUCCESS);

		if (setsid() == -1)
			return -1;
		if (chdir("/tmp") == -1)
			return -1;

		/* redirect fd's 0,1,2 to /dev/null */
		open("/dev/null", O_RDWR);
		/* stdin */
		dup(0);
		/* stdout */
		dup(0);
		/* stderror */
	}

	if ((listen(sockfd, 5)) != 0)
	{
		perror("listen failed");
		exit(-1);
	}
	else
		len = sizeof(cli);

	while (1)
	{
		char ip[INET_ADDRSTRLEN];

		int connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t *)&len);
		if (connfd < 0)
		{
			perror("Accept failed");
			exit(-1);
		}
		else
		{

			inet_ntop(AF_INET, &(cli.sin_addr), ip, INET_ADDRSTRLEN);
			syslog(LOG_INFO, "Accepted connection from %s", ip);
		}

		STAILQ_HEAD(stailqhead, stailq_data_s)
		head;
		STAILQ_INIT(&head);

		slist_data_t *data = malloc(sizeof(slist_data_t));
		data->connfd = connfd;

		pthread_create(&data->thread, NULL, handle_connection, (void *)data);

	}
}
