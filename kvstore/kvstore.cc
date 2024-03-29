/**********************************************************************/
/* This is a multi-threaded chunk server with LRU Cache + Virtual Memory. 
 * Implements the idea of key-value store
 * Created April 2017 
 * CIS 505 (Software Systems), Prof. Linh
 * University of Pennsylvania 
 * @version: 05/08/2017 */
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

/* For interaction between Primary and Secondary. */
string primary_ip;
unsigned int primary_port;
int secondary_fd;

/* For data migration. */
string migration_ip;
unsigned int migration_port;
int migration_fd;
string migration_user_list;

struct MsgPair {
	int sq;
	string msg;
};

map<string, vector<int>> chunk_info;             // key: virtual memory / checkpointing (since we'll rewrite checkpointing periodically, thus separate these chunks)
                                                 // value: #0: the # of current chunk (start from chunk 0), 
                                                 //        #1: current chunk size

map<string, vector<vector<int>>> virmem_meta;    // key: user (in virtual memory)
                                                 // value: a sequence of (#chunk, start_idx, length) 

map<string, vector<vector<int>>> cp_meta;        // key: user (in memory) when checkpointing
                                                 // value: a sequence of (#chunk, start_idx, length) 

map<string, int> seq_num_records;                // for FIFO
											     // key: user 
                                                 // value: the highest sequence number proposed

map<string, vector<MsgPair>> primary_holdback;   // for Crash Recovery
											     // key: user 
                                                 // value: a sequence of held back msg pairs

map<string, vector<MsgPair>> secondary_holdback; // for FIFO
											     // key: user 
                                                 // value: a sequence of held back msg pairs

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
	while ((c = getopt (argc, argv, "avi:p:n")) != -1)
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

void initialize_chunk_info(string key) {
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

void parse_ip_and_port(string s, string &ip, unsigned int &port) {
    string delimiter = ":";

    size_t pos = 0;
    string token;
    while ((pos = s.find(delimiter)) != string::npos) {
        ip = s.substr(0, pos);
        s.erase(0, pos + delimiter.length());
    }
    port = stoi(s);
}

/* This thread worker handles data migration. */
void *migrate_worker (void *arg) { 

	pthread_detach(pthread_self());

	// Creates a stream socket
	int sock_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
		exit(1);
	}

	// Associates a socket with a specific port or IP address
	struct sockaddr_in serveraddr;
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(migration_port);
	inet_pton(AF_INET, migration_ip.c_str(), &(serveraddr.sin_addr));
	connect(sock_fd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	
	string request_data = "REQUEST " + migration_user_list;
	send(sock_fd, request_data.c_str(), request_data.size(), 0);

	// if -v, print conversation with primary
	if (opt_v) {
		print_time();
		cout << request_data << endl;
	}

	int bufsize = 10000000;
	bool isDone = false;

	while (!isDone) {

		// Reads msg into buffer
		char *line = new char [bufsize];
		int nread = recv(sock_fd, line, bufsize - 1, 0);
        line[nread] = '\0';

        if (nread > 0) {

        	// if -v
			if (opt_v) {
				print_time();
				fprintf(stderr, "[%d] C: %s", sock_fd, line);
			}

			if (strcmp(line, "MIGRATION DONE\r\n") == 0) {
				isDone = true;
			}

			// Extracts command
			string command = parse_command(line);

			// Executes commands 
			if (command.compare("put") == 0) {
				server.put(line, false, sock_fd, 0);
			}

        }		
		delete [] line;
	} // end while

	// Closes the socket and terminate the thread
	close(sock_fd);
	pthread_exit(NULL);	
}

void confirm_identity(char feedback []) {

	string s(feedback);

	if (s.size() == 1 && s.compare("P") == 0) {
		isPrimary = true;
	}

	else if (s.size() > 1 && s.substr(1, 1).compare(" ") == 0) {
		isPrimary = true;
		string ip_port; 
		string delimiter = ","; 

	    size_t pos = 0;
	    while ((pos = s.find(delimiter)) != string::npos) {
	        ip_port = s.substr(2, pos);
	        s.erase(0, pos + delimiter.length());
	        break;
	    }
	    migration_user_list = s;
	    parse_ip_and_port(ip_port, migration_ip, migration_port);
	    // Creates a thread for connection with master
		pthread_t migrate_thread;
		pthread_create(&migrate_thread, NULL, migrate_worker, NULL);

		if (opt_v) {
			print_time();
			cout << "Request data migration from: " << migration_ip << ":" << migration_port << endl;
		}				

	} else {

		// parse ip and port 
		string ip_port = s.substr(2, s.size() - 1);
		parse_ip_and_port(ip_port, primary_ip, primary_port);

		// if -v, print ip and port of primary
		if (opt_v) {
			print_time();
			cout << "primary ip: " << primary_ip << " primary port: " << primary_port << endl;
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

    // bind to a specific port
    struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(port_number);
	int ret = bind(udp_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	// associate with master IP address and port number
    struct sockaddr_in dest;
    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(MASTER_PORT);
    inet_pton(AF_INET, MASTER_IP, &(dest.sin_addr));


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

void handle_secondary_report(char* line) {
	string report(line);
	string s = report.substr(5, report.size() - 5);
	string user;
	int seq_num;
	string delimiter = ",";

    size_t pos = 0;
    while ((pos = s.find(delimiter)) != string::npos) {
        user = s.substr(0, pos);
        s.erase(0, pos + delimiter.length());
    }
    seq_num = stoi(s);
    primary_holdback[user].erase (
        remove_if(primary_holdback[user].begin(), primary_holdback[user].end(), [&](MsgPair const & msgpair) {
            return msgpair.sq == seq_num;
        }),
        primary_holdback[user].end());
    if (primary_holdback[user].size() == 0) {
    	primary_holdback.erase(user);
    }
}

void check_primary_holdback() {
	if (primary_holdback.size() > 0) {
		for (auto it: primary_holdback) {
			vector<MsgPair> temp = primary_holdback[it.first];
			for (int i = 0; i < temp.size(); i++) {
				string msg_to_s = to_string(temp.at(i).sq) + "," + temp.at(i).msg;
				send(secondary_fd, msg_to_s.c_str(), msg_to_s.size(), 0);
			}
    	}
	}
}

void data_migration(char* line, int comm_fd) {    // user list end with ","
	string request(line);
	string s = request.substr(8, request.size() - 8);
	vector<string> users;
	string delimiter = ","; 

    size_t pos = 0;
    while ((pos = s.find(delimiter)) != string::npos) {
        users.push_back(s.substr(0, pos));
        s.erase(0, pos + delimiter.length());
    }
    for (int i = 0; i < users.size(); i++) {
    	server.migrate_data(users.at(i), comm_fd);
    }
    string successmsg = "MIGRATION DONE\r\n";
    send(comm_fd, successmsg.c_str(), successmsg.size(), 0);
}

/* This thread worker maintains a connection between P and S. */
void *ps_worker (void *arg) { 

	pthread_detach(pthread_self());

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
	send(sock_fd, greeting, strlen(greeting), 0);

	// if -v, print conversation with primary
	if (opt_v) {
		print_time();
		cout << "To primary: "  << greeting << endl;
	}

	int bufsize = 10000000;

	while (true) {

		// Reads msg into buffer
		char *buffer = new char [bufsize];
		int nread = recv(sock_fd, buffer, bufsize - 1, 0);
        buffer[nread] = '\0';

        if (nread > 0) {

        	// if -v
			if (opt_v) {
				print_time();
				fprintf(stderr, "[%d] C: %s", sock_fd, buffer);
			}

			// Finds the index of the first comma
			int comma_idx = strstr(buffer, ",") - buffer;

			// Gets the sequence number
			char *sq = new char [comma_idx + 1];
			strncpy(sq, buffer, comma_idx);
			sq[comma_idx] = '\0';

			int seq_num = atoi(sq);

			// Gets the operation
			char *line = new char [strlen(buffer) - comma_idx];
			strncpy(line, buffer + comma_idx + 1, strlen(buffer) - comma_idx - 1);
			line[strlen(buffer) - comma_idx - 1] = '\0';

			// Extracts command
			string command = parse_command(line);

			// Executes command
			if (command.compare("put") == 0) {
				server.put(line, true, sock_fd, seq_num);
			}

			else if (command.compare("cput") == 0) {
				server.cput(line, true, sock_fd, seq_num);
			}

			else if (command.compare("dele") == 0) {
				server.dele(line, true, false, sock_fd, seq_num);
			}

			else if (command.compare("rename") == 0) {
				server.rename(line, true, sock_fd, seq_num);
			} 

			delete [] sq;
			delete [] line;
        }
		
		delete [] buffer;
	} // end while

	// Closes the socket and terminate the thread
	close(sock_fd);
	pthread_exit(NULL);	
}

/* This thread worker does checkpointing periodically. */
void *cp_worker (void *arg) {

	pthread_detach(pthread_self());

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

	// Executes until user quits.
	while (!isQuit) {

		// Reads msg into buffer
		char *line = new char [bufsize];
		int nread = recv(comm_fd, line, bufsize - 1, 0);
    	line [nread] = '\0';

		// Counts the number of chars in current buffer. 
		counter += nread;

		if (nread > 0) {

			// if -v
			if (opt_v) {
				print_time();
				fprintf(stderr, "[%d] C: %s", comm_fd, line);
			}

			// Records secondary's fd
			string s(line);
			if (s.compare("S\r\n") == 0) {
				secondary_fd = comm_fd;
				check_primary_holdback();
				delete [] line;
				continue;
			}

			if (s.compare("shutdown\r\n") == 0) {
				delete [] line;
				exit(5);
			}		

			if (comm_fd != secondary_fd) {//&& (comm_fd != migration_fd))  {
				isQuit = true;
			}	

			// Extracts command
			string command = parse_command(line);

			string done = "!!!DONE!!!\r\n";

			// Executes commands
			if (command.compare("put") == 0) {
				server.put(line, true, comm_fd, 0);
            	send(comm_fd, done.c_str(), done.size(), 0);
			}

			else if (command.compare("get") == 0) {
				server.get(line, comm_fd);
            	send(comm_fd, done.c_str(), done.size(), 0);
			}

			else if (command.compare("cput") == 0) {
				server.cput(line, true, comm_fd, 0);
            	send(comm_fd, done.c_str(), done.size(), 0);
			}

			else if (command.compare("dele") == 0) {
				server.dele(line, true, false, comm_fd, 0);
            	send(comm_fd, done.c_str(), done.size(), 0);
			}

			else if (command.compare("getlist") == 0) {
				server.getlist(line, comm_fd);
            	send(comm_fd, done.c_str(), done.size(), 0);
			}

			else if (command.compare("getfile") == 0) {
				server.getfile(line, comm_fd);
            	send(comm_fd, done.c_str(), done.size(), 0);
			}

			else if (command.compare("rename") == 0) {
				server.rename(line, true, comm_fd, 0);
            	send(comm_fd, done.c_str(), done.size(), 0);
			}

			else if (command.compare("done") == 0) {
				handle_secondary_report(line);
			}

			else if (command.compare("request") == 0) {
				migration_fd = comm_fd;
				data_migration(line, comm_fd);
			}

			else if (command.compare("quit") == 0) {
				isQuit = true;
			} 

			else {
				server.error(comm_fd);
			}
		}
		
		delete [] line;
	} 

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
		// Creates a thread for connection with master
		pthread_t ps_thread;
		pthread_create(&ps_thread, NULL, ps_worker, NULL);
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

	if (ret < 0) {
		fprintf(stderr,"bind error!\n");
	    exit(-1);
	}

	// if -v, print server ip and port
	if (opt_v) {
		print_time();
		cout << "Server ip: "  << inet_ntoa(servaddr.sin_addr) << endl;
		print_time();
		cout << "Server port: " << to_string(ntohs(servaddr.sin_port)) << endl;
	}

	// Puts a socket into the listening state
	listen(listen_fd, 10);

	// Creates a signal thread to deal with checkpointing
	pthread_t cp_thread;
	pthread_create(&cp_thread, NULL, cp_worker, NULL);

	while (true) {

		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof(clientaddr);

		// bool is_admin = (inet_ntoa(clientaddr.sin_addr) == "127.0.0.1" && 
		// 				 to_string(ntohs(clientaddr.sin_port)) == "5050");

		// if -v, print server ip and port
		if (opt_v) {
			print_time();
			cout << "Client ip: "  << inet_ntoa(clientaddr.sin_addr) << endl;
			print_time();
			cout << "Client port: " << to_string(ntohs(clientaddr.sin_port)) << endl;
		}

		if (thread_counter != 0 && !isPrimary) { //&& !is_admin
			isPrimary = true;
		}

		// Accepts the next incoming connection
		int *fd = (int*) malloc(sizeof(int));
		*fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
		if (*fd < 0) {
			fprintf(stderr, "accept error \n");
			exit(-1);
		}

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