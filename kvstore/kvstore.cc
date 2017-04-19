/**********************************************************************/
/* This is the minimal version of the multi-threaded chunk server. 
 * Implements the idea of key-value store
 * Created April 2017 
 * CIS 505 (Software Systems), Prof. Linh
 * University of Pennsylvania 
 * @version: 04/12/2017 */
/**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <bits/stdc++.h>
#include <ctype.h>
#include <csignal>
#include <signal.h>
#include <algorithm>
#include <vector>
#include <dirent.h>
#include <time.h>
#include <map>
#include <experimental/filesystem>
#include <bitset>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>

#include "chunkserver.h"

using namespace std;

/* Keep a record of the active threads. */
vector<int> active_threads;
int thread_counter = 0;

/* Whether the option -v is specified. */
int opt_v = 0;

/* Parse the arguments given by command line. */
void parse_arguments (int argc, char* argv[], int &port_number, int &opt_a, int &opt_v) {
	int index;
	int c;
	opterr = 0;
	while ((c = getopt (argc, argv, "avp:")) != -1)
	    switch (c)
	      {
	      case 'p':
	    	port_number = atoi(optarg);
	        break;
	      case 'a':
	    	opt_a = 1;
	    	break;
	      case 'v':
	      	opt_v = 1;
	      	break;
	      case '?':
	        if (optopt == 'p')
	          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
	        else if (isprint (optopt))
	          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
	        else
	          fprintf (stderr,
	                   "Unknown option character `\\x%x'.\n",
	                   optopt);
	        exit(1);
	      default:
	    	  fprintf(stderr, "Unknown error\n");
	    	  break;
	      }
}

/* Change commands into lower case. */
void tolower(char* &buffer) {
	int i=0;
	while (buffer[i])
	{
		buffer[i] = tolower(buffer[i]);
		i++;
	}
}

/* Simple error handling functions */
#define handle_error_en(en, msg) \
    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

/* Signal thread deals with signals.*/
static void *sig_thread(void *arg)
{
	sigset_t *set = (sigset_t*)arg;
    int s, sig;
    const char *exitmsg = "-ERR Server shutting down\r\n";

    /* Wait for the Ctrl+C signal and do process. */
    for (;;) {
        s = sigwait(set, &sig);
        if (s != 0)
             handle_error_en(s, "sigwait");
        int i = 0;
        while (active_threads[i]) {
        	send(active_threads[i], exitmsg, strlen(exitmsg), 0);
        	close (active_threads[i]);
        	i++;
        }
        exit(0);
    }
}

/* When a connection closed, remove the thread from active threads. */
void active_thread_manager(int comm_fd)
{
	int i = 0;
	while(active_threads[i]){
		if(active_threads[i] == comm_fd) {
			active_threads.erase(active_threads.begin()+i);
			break;
		}
		i++;
	}
}

void merge_buffer(int temp_buf_size, char* &buffer, char* &temp_buffer)
{
	if (temp_buf_size > 0) {
		char * new_buffer = new char [1024];
		strcpy(new_buffer, temp_buffer);
		int i = 0;
		while (buffer[i]) {
			new_buffer[temp_buf_size + i] = buffer[i];
			i++;
		}
		strcpy(buffer, new_buffer);
	}
}

void update_buffer(int temp_buf_size, int index, char* &buffer, char* &temp_buffer)
{
	temp_buffer = new char[1024];
	strncpy(temp_buffer, buffer + index + 2, temp_buf_size);
	temp_buffer[temp_buf_size] = '\0';
	buffer = temp_buffer;
	temp_buffer = new char[temp_buf_size + 1];
	strncpy(temp_buffer, buffer + index + 2, temp_buf_size);
	temp_buffer[temp_buf_size] = '\0';
}

void store_incomplete_line(int temp_buf_size, char* &buffer, char* &temp_buffer)
{
	temp_buf_size = 0;
	while (buffer[temp_buf_size]) {
		temp_buf_size ++;
	}
	temp_buffer = new char[temp_buf_size + 1];
	strncpy(temp_buffer, buffer, temp_buf_size);
	temp_buffer[temp_buf_size] = '\0';
}


/* Thread worker deals with 4 commands.*/
void *worker (void *arg)
{
	pthread_detach(pthread_self());
	int comm_fd = *(int*) arg;

	// A new chunkserver instance
	Chunkserver server(opt_v);

	/* if option 'v' is given. */
	if (opt_v) {
		fprintf(stderr, "[%d] New connection\n", comm_fd);
	}

	int bufsize = 1024;
	int counter = 0;
	bool isQuit = false;
	int temp_buf_size = 0;
	char *temp_buffer = new char[1];
	temp_buffer[0] = '\0';

	// Executes until user quits. 
	while (!isQuit) {

		// Reads msg into buffer
		char *buffer = new char [bufsize];
		int nread = recv(comm_fd, buffer, bufsize - 1, 0);
        buffer[nread]='\0';

		/* If option 'v' is given. */
		if (opt_v) {
			fprintf(stderr, "[%d] C: %s", comm_fd, buffer);
		}

		// Counts the number of chars in current buffer. 
		counter += nread;

		if (nread > 0) {

			// Merges temporary buffer(with info from previous buffer) and new buffer
			merge_buffer(temp_buf_size, buffer, temp_buffer);

			// Executes command as long as there are <CR><LF> in current buffer
			while (strstr(buffer, "\r\n") > 0) {

				// Finds the index of the first <CR><LF>
				int index = strstr(buffer, "\r\n") - buffer;

				// Puts the full line in one array
				char *line = new char [index + 3];
				strncpy(line, buffer, index + 2);
				line[index + 2] = '\0';

				// Updates buffer 
				temp_buf_size = counter - index - 2;
				update_buffer(temp_buf_size, index, buffer, temp_buffer);

				// Updates counter
				counter = counter - index - 2;

				// Extracts command
				char *command = new char [5];
				strncpy(command, line, 4);
				command[4] = '\0';
				tolower(command);

				int checkcmd = index - 4;

				// Executes commands 
				if(strcmp(command, "vput") == 0) {
					server.vput(line, comm_fd);
					continue;
				}

				if(strcmp(command, "vget") == 0) {
					server.vget(line, comm_fd);
					continue;
				}

				if(strcmp(command, "cput") == 0) {
					server.cput(line, comm_fd);
					continue;
				}

				if(strcmp(command, "dele") == 0) {
					server.dele(line, comm_fd);
					continue;
				}

				if (strcmp(command, "quit") == 0 && checkcmd == 0) {
					isQuit = true;
					continue;
				}

				server.error(comm_fd);

			} // end while (still have lines end with <CRLF>)

			// Stores the info in the temporary buffer if no <CR><LF> found 
			if(strstr(buffer, "\r\n") <= 0) {
				store_incomplete_line(temp_buf_size, temp_buffer, buffer);
			}
		} // end if (nread > 0)
		delete [] buffer;
	} // end while (!quit)

	// Closes the socket and terminate the thread
	active_thread_manager(comm_fd);
	close(comm_fd);
	thread_counter -= 1;
	pthread_exit(NULL);
}

/* Main function. */
int main(int argc, char *argv[])
{
	int port_number = 4711;
	int opt_a = 0;
	// parse command line arguments
	parse_arguments(argc, argv, port_number, opt_a, opt_v);

	/* If option 'a' is given. */
	if (opt_a) {
		fprintf(stderr, "Author: CIS 505 T20\r\n");
		exit(3);
	}

	// Creates a stream socket
	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
		exit(1);
	}

	// Associates a socket with a specific port or IP address
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(port_number);
	int ret = bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if(ret < 0)
	{
		fprintf(stderr,"bind error!\n");
	    exit(-1);
	}

	// Puts a socket into the listening state
	listen(listen_fd, 10);

	while(true) {
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof(clientaddr);

		// Accepts the next incoming connection
		int *fd = (int*) malloc(sizeof(int));
		*fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
		if (*fd < 0) {
			fprintf(stderr, "accept error \n");
			exit(-1);
		}

		// Sends greeting message
		const char* greeting = "+OK chunkserver ready [localhost]\r\n";
		send(*fd, greeting, strlen(greeting),0);

		// Creates a signal thread to deal with signals
		pthread_t signal_thread;
		sigset_t set;
		int s;

		// Block SIGINT; other threads created by main()
		// will inherit a copy of the signal mask. 
		sigemptyset(&set);
		sigaddset(&set, SIGINT);
		s = pthread_sigmask(SIG_BLOCK, &set, NULL);
		if (s != 0)
			handle_error_en(s, "pthread_sigmask");
		s = pthread_create(&signal_thread, NULL, &sig_thread, (void *) &set);
		if (s != 0)
		    handle_error_en(s, "pthread_create");

		// Dispatch tasks to multiple threads
		pthread_t thread;
		pthread_create(&thread, NULL, worker, fd);

		// Keep record of active threads
		active_threads.push_back(*fd);
		thread_counter += 1;
	}
	return 0;
}
