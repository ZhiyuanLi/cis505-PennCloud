/**********************************************************************/
/* This is a multi-threaded chunk server with LRU Cache + Virtual Memory.
 * Implements the idea of key-value store
 * Created April 2017
 * CIS 505 (Software Systems), Prof. Linh
 * University of Pennsylvania
 * @version: 05/05/2017 */
/**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
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
#include <bitset>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <set>

#include "chunkserver.h"
#include "parameters.h"
#include "tools.h"
#include "../master/config.h"

using namespace std;

/* Node identity: Primary or Secondary. */
bool isPrimary = false;

/* Node ID */
int node_id;

string primary_ip;
unsigned int primary_port;
int secondary_fd;

map<string, vector<int>> chunk_info;           // key: virtual memory / checkpointing (since we'll rewrite checkpointing periodically, thus separate these chunks)
                                               // value: #0: the # of current chunk (start from chunk 0),
                                               //        #1: current chunk size

map<string, vector<vector<int>>> virmem_meta;  // key: user (in virtual memory)
                                               // value: a sequence of (#chunk, start_idx, length)

map<string, vector<vector<int>>> cp_meta;      // key: user (in memory) when checkpointing
                                               // value: a sequence of (#chunk, start_idx, length)


/* Keep a record of the space left. */
long long space_left = NODE_CAPACITY;

/* Keep a record of the active threads. */
vector<int> active_threads;
int thread_counter = 0;

/* Command line arguments. */
int port_number = 4711;                        // default port number
bool opt_a = false;                            // output author
bool opt_v = false;                            // verbose mode
bool opt_n = false;                            // brand new storage node with no existent chunks

/* A new chunkserver instance. */
Chunkserver server(NODE_CAPACITY);

/* Parse the arguments given by command line. */
void parse_arguments (int argc, char* argv[]) {
	int index;
	int c;
	opterr = 0;
	while ((c = getopt (argc, argv, "avi:p:nt:")) != -1)
	    switch (c)
	      {
	      case 'a':
	    	opt_a = true;
	    	break;
	      case 'v':
	      	opt_v = true;
	      	break;
	      case 'i':
	        node_id = atoi(optarg);
	        break;
	      case 'p':
	    	port_number = atoi(optarg);
	        break;
	      case 'n':
	      	opt_n = true;
	      	break;
	      case 't':
	      	cout << optarg << endl;
	        if (strcmp(optarg, "P") == 0) isPrimary = true;
	        if (strcmp(optarg, "S") == 0) isPrimary = false;
	        break;
	      case '?':
	        if (optopt == 'p' || optopt == 'i')
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

void initialize_chunk_info(string key){
	chunk_info[key].push_back(0);
	chunk_info[key].push_back(0);
}

void initialization() {
	create_dir("virtual memory");
	create_dir("checkpointing");
	create_dir("metadata");

	if (opt_n) {                              // pay attention to relative path here
		clear_dir("virtual memory/");         // assume you are in the folder where these directories live
		clear_dir("checkpointing/virtual memory/");
		clear_dir("checkpointing/");
		clear_dir("metadata/");
		initialize_chunk_info("virtual memory");
		initialize_chunk_info("checkpointing");
	} else {
		// If this is not the very first time that this machine is activated as storage node,
		// have to do the following crash recovery:
		server.load_checkpointing();         // load checkpointing
		server.replay_log();                 // replay log
	}
}

void parse_ip_and_port(string s)
{
    string delimiter = ":";

    size_t pos = 0;
    string token;
    while ((pos = s.find(delimiter)) != string::npos) {
        primary_ip = s.substr(0, pos);
        s.erase(0, pos + delimiter.length());
    }
    primary_port = stoi(s);
}

void confirm_identity(char feedback []) {
	string s(feedback);
	if (s.size() == 1 && s.compare("P") == 0) {
		isPrimary = true;
	} else {

		// parse ip and port
		string ip_port = s.substr(2, s.size() - 1);
		parse_ip_and_port(ip_port);

		// if -v, print ip and port of primary
		if (opt_v) {
			print_time();
			cout << "primary ip: " << primary_ip << endl;
			print_time();
			cout << "primary port: " << primary_port << endl;
		}
		isPrimary = false;
	}
}

void contact_master() {

	// contact master with UDP
	int udp_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) {
        fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
        exit(1);
    }

    // associate with master IP address and port number
    struct sockaddr_in dest;
    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(MASTER_PORT);
    inet_pton(AF_INET, MASTER_IP, &(dest.sin_addr));

    struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(port_number);
	int ret = bind(udp_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));

    // ask master for identity P or S
    string ask_master = "!" + to_string(node_id);
    sendto(udp_fd, ask_master.c_str(), ask_master.size(), 0, (struct sockaddr*)&dest, sizeof(dest));

    struct sockaddr_in src;
    socklen_t srcSize = sizeof(src);
    char feedback[255];
    int rlen = recvfrom(udp_fd, feedback, sizeof(feedback) - 1, 0, (struct sockaddr*)&src, &srcSize);
    feedback[rlen] = 0;

    // if -v, print conversation with master
	if (opt_v) {
		print_time();
		cout << "To master: " << ask_master << endl;
		print_time();
		cout << "From master: " << feedback << endl;
	}

	confirm_identity(feedback);
    close(udp_fd);
}

// TODO: unfinished
void contact_primary() {
	// Creates a stream socket
	int sock_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
		exit(1);
	}

	// Associates a socket with a specific port or IP address
	struct sockaddr_in primaryaddr;
	bzero(&primaryaddr, sizeof(primaryaddr));
	primaryaddr.sin_family = AF_INET;
	primaryaddr.sin_port = htons(primary_port);
	inet_pton(AF_INET, primary_ip.c_str(), &(primaryaddr.sin_addr));
	connect(sock_fd, (struct sockaddr*)&primaryaddr, sizeof(primaryaddr));

	// Report to primary
	const char* greeting = "S\r\n";
	send(sock_fd, greeting, strlen(greeting),0);

	// if -v, print conversation with primary
	if (opt_v) {
		print_time();
		cout << "To primary: "  << greeting << endl;
	}
	close(sock_fd);
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

string parse_command(char* line)
{
	int i = 0;
	int idx = 0;
	while(line[i]) {
		string temp(1, line[i]);
		if (temp.compare(" ") == 0) {
			idx = i;
			break;
		}
		i++;
	}

	if (idx > 0) {
		char *command = new char[idx + 1];
		strncpy(command, line, idx);
		command[idx] = '\0';
		tolower(command);
		string c(command);
		delete [] command;
		return c;
	} else {
		char *command = new char[strlen(line) - 2 + 1];
		strncpy(command, line, strlen(line) - 2 );
		command[strlen(line) - 2 ] = '\0';
		tolower(command);
		string c(command);
		delete [] command;
		return c;
	}
}

/* This thread worker does checkpointing periodically. */
void *cp_worker (void *arg) {

	while (true) {
		sleep(CP_INTERVAL);
		server.checkpointing();

		// If -v
		if (opt_v) {
			print_time();
			cout << "Checkpointing ongoing" << endl;
		}
	}
}

/* Thread worker deals with 4 commands.*/
void *worker (void *arg) {
	pthread_detach(pthread_self());
	int comm_fd = *(int*) arg;

	// if -v
	if (opt_v) {
		print_time();
		fprintf(stderr, "[%d] New connection\n", comm_fd);
	}

	int bufsize = 10000000;
	int counter = 0;
	bool isQuit = false;
	int temp_buf_size = 0;
	char *temp_buffer = new char[1];
	temp_buffer[0] = '\0';

	// Executes until user quits.
	if (!isQuit) {

		// Reads msg into buffer
		char *line = new char [bufsize];
		int nread = recv(comm_fd, line, bufsize - 1, 0);
    line[nread]='\0';

		// if -v
		if (opt_v) {
			print_time();
			fprintf(stderr, "[%d] C: %s\n", comm_fd, line);
		}

		// Counts the number of chars in current buffer.
		counter += nread;

		if (nread > 0) {

			// Merges temporary buffer(with info from previous buffer) and new buffer
			// merge_buffer(temp_buf_size, buffer, temp_buffer);

			// Executes command as long as there are <CR><LF> in current buffer
			// while (strstr(buffer, "\r\n") > 0) {

				// Finds the index of the first <CR><LF>
				// int index = strstr(buffer, "\r\n") - buffer;
				//
				// // Puts the full line in one array
				// char *line = new char [index + 3];
				// strncpy(line, buffer, index + 2);
				// line[index + 2] = '\0';
				//
				// // Updates buffer
				// temp_buf_size = counter - index - 2;
				// update_buffer(temp_buf_size, index, buffer, temp_buffer);
				//
				// // Updates counter
				// counter = counter - index - 2;

				// Extracts command
				string command = parse_command(line);


				// Executes commands
				if(command.compare("put") == 0) {
					server.put(line, true, comm_fd);

				}

				else if(command.compare("get") == 0) {
					server.get(line, comm_fd);

				}

				else if(command.compare("cput") == 0) {
					server.cput(line, true, comm_fd);

				}

				else if(command.compare("dele") == 0) {
					server.dele(line, true, comm_fd);

				}

				else if(command.compare("getlist") == 0) {
					server.getlist(line, comm_fd);

				}

				else if(command.compare("quit") == 0) {
					isQuit = true;

				}else{
					server.error(comm_fd);
				}

			// } // end while (still have lines end with <CRLF>)

			// Stores the info in the temporary buffer if no <CR><LF> found
			// if(strstr(buffer, "\r\n") <= 0) {
			// 	store_incomplete_line(temp_buf_size, temp_buffer, buffer);
			// }
		} // end if (nread > 0)
		delete [] line;
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
	// parse command line arguments
	parse_arguments(argc, argv);

	// if -a
	if (opt_a) {
		fprintf(stderr, "Author: CIS 505 T20\r\n");
		exit(3);
	}

	// ask master for identity: P or S
	contact_master();

	initialization();

	if (isPrimary) {
		print_time();
		cout << "I'm primary with node ID: " << node_id << endl;
	} else {
		// contact_primary(); //TODO
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

	// if -v, print server ip and port
	if (opt_v) {
		print_time();
		cout << "Server ip: "  << inet_ntoa(servaddr.sin_addr) << endl;
		print_time();
		cout << "Server port: " << to_string(ntohs(servaddr.sin_port)) << endl;
	}

	if(ret < 0)
	{
		fprintf(stderr,"bind error!\n");
	    exit(-1);
	}

	// Puts a socket into the listening state
	listen(listen_fd, 10);

	// Creates a signal thread to deal with checkpointing
	pthread_t cp_thread;
	pthread_create(&cp_thread, NULL, cp_worker, NULL);

	while (true) {
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof(clientaddr);

		// if -v, print server ip and port
		if (opt_v) {
			print_time();
			cout << "Client ip: "  << inet_ntoa(clientaddr.sin_addr) << endl;
			print_time();
			cout << "Client port: " << to_string(ntohs(clientaddr.sin_port)) << endl;
		}

		// Accepts the next incoming connection
		int *fd = (int*) malloc(sizeof(int));
		*fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
		if (*fd < 0) {
			fprintf(stderr, "accept error \n");
			exit(-1);
		}

		// Sends greeting message
		// const char* greeting = "+OK chunkserver ready [localhost]\r\n";
		// send(*fd, greeting, strlen(greeting),0);

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
