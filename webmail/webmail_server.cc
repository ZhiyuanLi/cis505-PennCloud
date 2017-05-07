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
using namespace std;

int aflag = 0;
int ShutdownFlag = 0;
int debugflag = 0;
int port = 2300;
std::set<int> fds; // file descriptor of sockets
std::set<pthread_t> threads;
mutex mut, usermut;

void INThandler(int);
#define LISTEN_QUEUE_LENGTH 100  //max number of listen() function
#define BUFFER_SIZE 1024  //max buffer size
#define MAX_THREAD_NUM 200  // max number of threads

// wrap read() function, read char by char
int read_command(int fd, char *buf) {
	// read from client, a char at each time
	// if success, return number of chars, else return 0
	int rcvd = 0;
	int rd = 0;
	while(buf[rcvd-1] != '\n' || buf[rcvd-2] != '\r'){
		rd = read(fd, &buf[rcvd], sizeof(char));
		if (rd <0) return(-1);
		rcvd++;
	}
	return rcvd;
}

int read_data(int fd, char *buf, int &rcvd) {
	// read data from mail, a char at each time
	// if success, return number of chars, else return 0
	int lastlen = rcvd;
	rcvd = 0;
	int endflag = 0;
	int rd = 0;
	while(buf[rcvd-1] != '\n' || buf[rcvd-2] != '\r'){
		rd = read(fd, &buf[rcvd], sizeof(char));
		if (rd <0) return(-1);
		rcvd++;
	}
	if (lastlen > 0 && strncmp (buf, ".\r\n",3) == 0){
		endflag = 1;
	}
	return endflag;
}

//wrap write() function

bool dowrite(int fd, char *buf, int len) {
	int sent = 0;
	while (sent < len) {
		int n = write(fd, &buf[sent],len-sent);
		if (n<0)
			return false;
		sent += n;
	}
	return true;
}

void computeDigest(char *data, int dataLengthBytes, unsigned char *digestBuffer)
{
	/* The digest will be written to digestBuffer, which must be at least MD5_DIGEST_LENGTH bytes long */

	MD5_CTX c;
	MD5_Init(&c);
	MD5_Update(&c, data, dataLengthBytes);
	MD5_Final(digestBuffer, &c);
}

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

			char cmp_helo[5];
			char cmp_mail_from[9];
			char cmp_rcpt_to[7];
			char cmp_quit[6];
			char cmp_rset[6];
			char cmp_noop[4];

			strncpy(cmp_helo, buf, 5);//extract first x characters to compare from buffer
			strncpy(cmp_mail_from, buf, 9);
			strncpy(cmp_rcpt_to, buf, 7);
			strncpy(cmp_quit, buf, 6);
			strncpy(cmp_rset, buf, 6);
			strncpy(cmp_noop, buf, 4);

			char data[] = "DATA";
			char cmp_data[4];
			strncpy(cmp_data, buf, 4);

			//step 1 helo command
			if (strcasecmp(helo, cmp_helo) == 0 && helo_flag ==0){
				if (rlen > 7){
					char resp[] = "250 localhost\r\n";
					dowrite(comm_fd, resp, sizeof(resp)-1);
					helo_flag = 1;
				}
				else{
					char respERR[] = "501 HELO command must be followed by a domain name.\r\n";
					dowrite(comm_fd, respERR, sizeof(respERR)-1);
				}
			}else

				// step 2 mail_from command
				if (strcasecmp(mail_from, cmp_mail_from) == 0 && helo_flag ==1 && mail_flag == 0){

					string buffer = buf; // convert read message buf to string buffer
					string domain_buffer = buffer.substr(10); // copy from 9th char in buffer
					size_t found = domain_buffer.find_last_of("@"); //find the '@' char in mail_from command
					if(found == -1){
						char err[] = "incorrect reverse path";
						dowrite(comm_fd, err, sizeof(err)-1);
					}
					if (domain_buffer.compare(found, 10, "@localhost") != 0){
						char err[] = "ERR: incorrect host name";
						dowrite(comm_fd, err, sizeof(err)-1);
					}
					else{
						bzero(&reverse_path,sizeof(reverse_path));
						rcvr_list.clear();
						string str = domain_buffer.substr(10, found);
						strcpy(reverse_path, str.c_str());
						mail_flag = 1;
						char OKresp[] = "250 OK\r\n";
						dowrite(comm_fd, OKresp, sizeof(OKresp)-1);
					}
				}else

					// step 3 rcpt_to command
					if (strcasecmp(rcpt_to, cmp_rcpt_to) == 0 && helo_flag == 1 && mail_flag == 1){
						string buffer = buf;
						string domain_buffer = buffer.substr(8);
						size_t found = domain_buffer.find_last_of("@");
						if(found == -1){
							char err[] = "ERR: incorrect forward path";
							dowrite(comm_fd, err, sizeof(err)-1);
						}
						//non-local hostname, forward to other servers
						if (domain_buffer.compare(found, 10, "@localhost") != 0){

							string host_file = "mqueue";

							if(std::ifstream(host_file)){
								rcpt_flag = 1;
								char OKresp[] = "250 OK\r\n";
								dowrite(comm_fd, OKresp, sizeof(OKresp)-1);
								rcvr_list.insert(host_file);
								//								int n = rcvr_list.size();
							}
							else { // if not exist file for receiver
								ofstream out(host_file.c_str());
								rcpt_flag = 1;
								char OKresp[] = "250 OK\r\n";
								dowrite(comm_fd, OKresp, sizeof(OKresp)-1);
								rcvr_list.insert(host_file);
							}

						}else{// local server name, save to local file

							string host_name = domain_buffer.substr(1,found-1); // extract host name
							string host_file = host_name.append(".mbox"); // host_name is also changed
							// if receiver file exists
							// ready to receive data command
							if(std::ifstream(host_file)){
								rcpt_flag = 1;
								char OKresp[] = "250 OK\r\n";
								dowrite(comm_fd, OKresp, sizeof(OKresp)-1);
								rcvr_list.insert(host_file);
								//								int n = rcvr_list.size();
							}
							else { // if not exist file for receiver
								char err[] = "550 No such user here \r\n";
								dowrite(comm_fd, err, sizeof(err)-1);
							}
						}
					}else

						//step 4 receive data and save to file
						if(strcasecmp(data, cmp_data) == 0 && rcpt_flag == 1){

							//Sample email format:
							//##-\Ȋܲ�G����WTڽ220 localhost *
							//
							//2017-5-1-9-35-15
							//From: Benjamin Franklin <benjamin.franklin@localhost>
							//To: Linh Thi Xuan Phan <linhphan@localhost>
							//Date: Fri, 21 Oct 2016 18:29:11 -0400 (not necessary, appended by user)
							//Subject: Testing my new email account
							//
							//Linh,
							//
							//I just wanted to see whether my new email account works.
							//
							//        - Ben


							buf[rlen] = 0;
							usermut.lock();
							char DATAresp[] = "354 send the mail data, end with .\r\n";
							dowrite(comm_fd, DATAresp, sizeof(DATAresp)-1);

							int currlen = 0;
							unsigned short data_len = 0;
							char data_buf[BUFFER_SIZE];
							bzero(&data_buf, sizeof(data_buf));
							int n = rcvr_list.size();

							ostringstream lines;

							while(read_data(comm_fd, data_buf, currlen) == 0){
								data_buf[currlen] = 0;
								lines << data_buf;
							}

							string line_data;
							line_data = lines.str();
							unsigned char digestBuffer[16];
							char * my_str = strdup(line_data.c_str());
							computeDigest(my_str, sizeof(line_data) , digestBuffer);

							time_t t = time(0);   // get time now
							struct tm * now = localtime( & t );

							for(set<string>::iterator it = rcvr_list.begin(); it != rcvr_list.end();it++){
								ofstream userfile;
								userfile.open(*it,ios::app);
								userfile<< "##"<<digestBuffer<<endl;
//								userfile<< (now->tm_year + 1900) << '-'
//										<< (now->tm_mon + 1) << '-'
//										<<  now->tm_mday << '-'
//										<<  now->tm_hour << '-'
//										<<  now->tm_min << '-'
//										<<  now->tm_sec
//										<< endl;
								userfile<<lines.str();
								userfile.close();
							}
							lines.str("");
							lines.clear();
							bzero(digestBuffer, sizeof(digestBuffer));

							helo_flag = 0;
							char OKresp[] = "250 OK\r\n";
							dowrite(comm_fd, OKresp, sizeof(OKresp)-1);
							usermut.unlock();
						}else

							//step 5 quit
							if (strcasecmp(quit, cmp_quit) == 0){

								char QUITresp[] = "221 Service closing transmission channel\r\n";
								dowrite(comm_fd, QUITresp, sizeof(QUITresp)-1);
								close(comm_fd);
								break;
							}else

								// step 6 reset
								if (strcasecmp(rset, cmp_rset) == 0){

									rcvr_list.clear();
									int mail_flag = 0;
									int rcpt_flag = 0;
									bzero(&reverse_path,sizeof(reverse_path));
									bzero(buf, sizeof(buf));
									char resp[] = "250 OK\r\n";
									dowrite(comm_fd, resp, sizeof(resp)-1);
								}else

									//step 7 noop
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






