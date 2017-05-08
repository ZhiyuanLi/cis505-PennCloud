/*
 * webmail_server.cc
 * copy from smtp-server, adapt for mail relaying case.
 * For localmail: save to storage
 * For mails from other server: MX record
 *
 *  Created on: Apr 22, 2017
 *      Author: cis505
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <regex>
#include <mutex>
#include <set>
#include <openssl/md5.h>
#include <time.h>
#include <cstddef>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "server_header.h"
using namespace std;

int aflag = 0;
int ShutdownFlag = 0;
int debugflag = 0;
int port = 2300;
std::set<int> fds; // file descriptor of sockets
std::set<pthread_t> threads;
mutex mut;

void INThandler(int);
#define LISTEN_QUEUE_LENGTH 100  //max number of listen() function
#define BUFFER_SIZE 1024  //max buffer size
#define MAX_THREAD_NUM 200  // max number of threads

// worker thread without debug mode
void *worker(void *arg)
{
	void aflaghandler();

	int comm_fd = *(int*)arg;
	if(aflag == 1){ // print all the server -a info to client
		aflaghandler();
	}
	else{

		char greeting[] = "220 localhost *\r\n";
		dowrite(comm_fd, greeting, sizeof(greeting)-1); // write greeting info to client
		unsigned short rlen = 0;
		char buf[BUFFER_SIZE];
		bzero(&buf, sizeof(buf));

		std::set<std::string> rcvr_list;
		char reverse_path[100];
		bzero(&reverse_path,sizeof(reverse_path));
		int helo_flag = 0;
		int mail_flag = 0;
		int rcpt_flag = 0;

		while( (rlen = read_command(comm_fd, buf)) > 0){ // execute only when see input from client

			buf[rlen] = 0;
			if (debugflag == 1){
				fprintf(stderr, "[%d] C: %s", comm_fd, buf); // debug info from server
			}
			char helo[] = "HELO "; // set the string to compare with
			char mail_from[] = "MAIL FROM";
			char rcpt_to[] = "RCPT TO";
			char quit[] = "QUIT\r\n";
			char rset[] = "RSET\r\n";
			char noop[] = "NOOP";
			char send[] = "SEND";
			char fowd[] = "FOWD";
			char rply[] = "RPLY";


			char cmp_helo[5];
			char cmp_mail_from[9];
			char cmp_rcpt_to[7];
			char cmp_quit[6];
			char cmp_rset[6];
			char cmp_noop[4];
			char cmp_send[4];
			char cmp_fowd[4];
			char cmp_rply[4];

			strncpy(cmp_helo, buf, 5);//extract first x characters to compare from buffer
			strncpy(cmp_mail_from, buf, 9);
			strncpy(cmp_rcpt_to, buf, 7);
			strncpy(cmp_quit, buf, 6);
			strncpy(cmp_rset, buf, 6);
			strncpy(cmp_noop, buf, 4);
			strncpy(cmp_send, buf, 4);
			strncpy(cmp_fowd, buf, 4);
			strncpy(cmp_rply, buf, 4);

			char data[] = "DATA";
			char cmp_data[4];
			strncpy(cmp_data, buf, 4);

			//step 1 helo command
			if (strcasecmp(helo, cmp_helo) == 0 && helo_flag ==0){
				helo_flag = handle_helo(comm_fd, rlen, helo_flag);
			}else

				// step 2 mail_from command
				if (strcasecmp(mail_from, cmp_mail_from) == 0 && helo_flag ==1 && mail_flag == 0){
					mail_flag = handle_mail(comm_fd, buf, rlen, mail_flag, reverse_path, rcvr_list);
				}else

					// step 3 rcpt_to command
					if (strcasecmp(rcpt_to, cmp_rcpt_to) == 0 && helo_flag == 1 && mail_flag == 1){
						string host_file = handle_rcpt(comm_fd, buf, rcpt_flag, rcvr_list);
						rcvr_list.insert(host_file);
					}else

						//step 4 receive data and save to file
						if(strcasecmp(data, cmp_data) == 0 && rcpt_flag == 1){

							handle_data(comm_fd, buf, rlen, rcvr_list, BUFFER_SIZE, helo_flag);

						}else

							//step 5 send
							if (strcasecmp(send, cmp_send) == 0){
								handle_send(comm_fd, buf, rlen, BUFFER_SIZE);
							}else

							//step 6 quit
							if (strcasecmp(quit, cmp_quit) == 0){
								char QUITresp[] = "221 Service closing transmission channel\r\n";
								dowrite(comm_fd, QUITresp, sizeof(QUITresp)-1);
								close(comm_fd);
								break;
							}else

								// step 7 reset
								if (strcasecmp(rset, cmp_rset) == 0){
									rcvr_list.clear();
									int mail_flag = 0;
									int rcpt_flag = 0;
									bzero(&reverse_path,sizeof(reverse_path));
									bzero(buf, sizeof(buf));
									char resp[] = "250 OK\r\n";
									dowrite(comm_fd, resp, sizeof(resp)-1);
								}else

									//step 8 noop
									if (strcasecmp(noop, cmp_noop) == 0){
										char resp[] = "250 OK\r\n";
										dowrite(comm_fd, resp, sizeof(resp)-1);
									}
									else{
										char err[] = "ERR: not a command!\r\n";
										dowrite(comm_fd, err, sizeof(err)-1);
									}
		}
	}
	mut.lock();
	fds.erase(comm_fd);
	pthread_t tid = pthread_self();
	threads.erase(tid);
	mut.unlock();

	pthread_exit(NULL);
}

// tackle with SIGINT
void INThandler(int sig)
{
	char errshut[] = "-ERR Server shutting down\r\n";
	ShutdownFlag = 0;
	set<int>::iterator it; // close file descriptor
	for(it = fds.begin(); it != fds.end();it++){
		dowrite(*it, errshut, sizeof(errshut)-1);
		close(*it);
	}
	set<pthread_t>::iterator thrdit; // close threads
	for(thrdit = threads.begin(); thrdit != threads.end();thrdit++){
		pthread_cancel(*thrdit);
		pthread_join(*thrdit, NULL);
	}
	exit(0);
}

void aflaghandler()
{
	char serverr[] = "Mengjin Dong / mengjin \r\n";
	set<int>::iterator it; // close file descriptor
	for(it = fds.begin(); it != fds.end();it++){
		dowrite(*it, serverr, sizeof(serverr)-1);
		close(*it);
	}
	set<pthread_t>::iterator thrdit; // close threads
	for(thrdit = threads.begin(); thrdit != threads.end();thrdit++){
		pthread_cancel(*thrdit);
		pthread_join(*thrdit, NULL);
	}
	exit(0);
}

int main(int argc, char *argv[])
{

	signal(SIGINT, INThandler);
	char c;
	const char* optstring = "p:av";

	//tackle with command input
	while ((c = getopt(argc, argv, optstring)) != -1){
		switch(c){
		case 'p':
			port = std::stol(optarg);
			break;
		case 'a':
			aflag = 1; // set -a flag if sees -v option
			break;
		case 'v':
			// set debugflag if sees -v option
			debugflag = 1;
			break;
		case '?':
			fprintf(stderr,"Unknown flag -%c\n", optopt);
			exit(1);
		default:
			fprintf(stderr,"Unknown error\n");
			break;
		}
	}

	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htons(INADDR_ANY);
	servaddr.sin_port = htons(port);
	bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	listen(listen_fd, 10);

	while (true) {
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof(clientaddr);
		int *fd = new int;
		*fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
		pthread_t thread;

		// call corresponding function depending on the flag
		if (debugflag == 1){
			fprintf(stderr, "[%d] New connection\n", *fd);
		}

		pthread_create(&thread, NULL, worker, fd);

		mut.lock();
		fds.insert(*fd);
		threads.insert(thread);
		mut.unlock();
	}
	return 0;
}






