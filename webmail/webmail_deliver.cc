/*
 * webmail_deliver.cc
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
#include <set>
#include <vector>
#include <cstddef>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "server_header.h"
using namespace std;

#define BUFFER_SIZE 1024

#define MAX_Message_Num 1000

struct Message MsgQueue[MAX_Message_Num];

int main(){

	//Sample email format:
	//##-\Ȋܲ�G����WTڽ220 localhost *
	//
	//2017-5-1-9-35-15
	//From: Benjamin Franklin <benjamin.franklin@localhost>
	//To: Linh Thi Xuan Phan <linhphan@localhost>
	//Date: Fri, 21 Oct 2016 18:29:11 -0400
	//Subject: Testing my new email account
	//
	//Linh,
	//
	//I just wanted to see whether my new email account works.
	//
	//        - Ben

	// statistical information of mqueue


	struct Message Msg;
	Msg.messageID = 0;
	Msg.unmark_flag = 0; // delete message flag
	Msg.begin_line = 1;
	Msg.end_line = 0;
	int max_msg = 0;

	ifstream file("mqueue", ifstream::in | ifstream::binary);
	std::string line;
	int i = 0;
	while(getline(file, line)){

		if (line.compare(0, 5, "From:") == 0) {
			if(i != 0) MsgQueue[i-1] = Msg;
			Msg.messageID ++;
			Msg.begin_line = Msg.end_line + 1;
			i++;
		}
		Msg.end_line ++;
	}
	MsgQueue[i-1] = Msg;
	max_msg = Msg.messageID;
	file.close();

	cout<<"max_msg = "<<max_msg<<endl;

	//	connect to server and deliver message
	for (int i = 0; i < max_msg; i++){
		// Find receivers from message
		ifstream file("mqueue", ifstream::in | ifstream::binary);
		std::string line;
		int lineNo = 0;
		std::set<std::string> rcvr_list;

		while(getline(file, line)){

			lineNo++;
			if (lineNo > MsgQueue[i].begin_line && lineNo < MsgQueue[i].end_line){
				if (line.compare(0,3,"To:") == 0){

					// extract receiver information
					std::size_t found1 = line.find_first_of("<");
					std::size_t found2 = line.find_first_of(">");
					while(found1!=std::string::npos){
						string rcvr = line.substr(found1 + 1, found2 - found1 - 1);
						cout<<"rcvr = "<<rcvr<<endl;
						rcvr_list.insert(rcvr);
						found1=line.find_first_of("<",found1+1);
						found2=line.find_first_of(">",found2+1);
					}

					//connect to receiver and send mail
					for (int j = 0; j < rcvr_list.size(); j++){
						for(set<string>::iterator it = rcvr_list.begin(); it != rcvr_list.end();it++){
							string domain_buffer = *it;

							std::size_t found = domain_buffer.find_first_of("@");

							string server_name  = domain_buffer.substr(found+1); // extract server name
							const char* host = server_name.c_str();
							unsigned char host_buffer[BUFFER_SIZE];
							int r_size = res_query(host,C_IN,T_MX, host_buffer, BUFFER_SIZE);

							if(r_size == -1){
								char err_host[] = "ERR: Cannot find host server!";
								cout<<err_host<<endl;
							}
							else if(r_size == static_cast<int> (BUFFER_SIZE)){
								char err[] = "ERR: Buffer too small, finding host server truncated!";
								cout<<err<<endl;
							}
							HEADER *hdr = reinterpret_cast<HEADER*> (host_buffer);
							if (hdr->rcode != NOERROR) {

								std::cerr << "Error: ";
								switch (hdr->rcode) {
								case FORMERR:
									std::cerr << "Format error";
									break;
								case SERVFAIL:
									std::cerr << "Server failure";
									break;
								case NXDOMAIN:
									std::cerr << "Name error";
									break;
								case NOTIMP:
									std::cerr << "Not implemented";
									break;
								case REFUSED:
									std::cerr << "Refused";
									break;
								default:
									std::cerr << "Unknown error";
									break;
								}
							}

							//print information of the answers
							int question = ntohs (hdr->qdcount);
							int answers = ntohs (hdr->ancount);
							int nameservers = ntohs (hdr->nscount);
							int addrrecords = ntohs (hdr->arcount);

							ns_msg m;
							int k = ns_initparse (host_buffer, r_size, &m);
							if (k == -1) {
								std::cerr << errno << " " << strerror (errno) << "\n";
							}

							for (int i = 0; i < question; ++i) {
								ns_rr rr;
								int k = ns_parserr (&m, ns_s_qd, i, &rr);
								if (k == -1) {
									std::cerr << errno << " " << strerror (errno) << "\n";
								}
							}

							char* IP_address = parse_record (host_buffer, r_size, "addrrecords", ns_s_ar, 2, &m);

							cout<<"IP_address = "<<IP_address<<endl;

							// connect to mail server

							struct connection conn;

							conn.fd = -1;
							conn.bufferSizeBytes = BUFFER_SIZE;
							conn.bytesInBuffer = 0;
							conn.buf = (char*)malloc(BUFFER_SIZE);

							conn.fd = socket(PF_INET, SOCK_STREAM, 0);
							if (conn.fd < 0)
								panic("Cannot open socket (%s)", strerror(errno));

							struct sockaddr_in servaddr;
							bzero(&servaddr, sizeof(servaddr));
							servaddr.sin_family=AF_INET;
							servaddr.sin_port=htons(25);
							inet_pton(AF_INET, IP_address, &(servaddr.sin_addr));

							if (connect(conn.fd, (struct sockaddr*)&servaddr, sizeof(servaddr))<0)
								panic("Cannot connect to localhost:10000 (%s)", strerror(errno));

							conn.bytesInBuffer = 0;

							expectToRead(&conn, "220 localhost *");
							expectNoMoreData(&conn);

							writeString(&conn, "HELO tester\r\n");
							expectToRead(&conn, "250 localhost");
							expectNoMoreData(&conn);

							// Specify the sender and the receipient (with one incorrect recipient)

							writeString(&conn, "MAIL FROM:<atrueworld@localhost.com>\r\n");
							expectToRead(&conn, "250 OK");
							expectNoMoreData(&conn);

							writeString(&conn, "RCPT TO:<mengjin@seas.upenn.edu>\r\n");
							expectToRead(&conn, "250 OK");
							expectNoMoreData(&conn);

							// Send the actual data

							writeString(&conn, "DATA\r\n");
							expectToRead(&conn, "354 *");
							expectNoMoreData(&conn);

							writeString(&conn, "From: Benjamin Franklin <atrueworld@localhost.com>\r\n");
							writeString(&conn, "To: Mengjin <mengjin@seas.upenn.edu>\r\n");
							writeString(&conn, "Date: Wed Dec 28 06:47:03 2016 EST\r\n");
							writeString(&conn, "Subject: Testing my new email account\r\n");
							writeString(&conn, "\r\n");
							writeString(&conn, "Linh,\r\n");
							writeString(&conn, "\r\n");
							writeString(&conn, "I just wanted to see whether my new email account works.\r\n");
							writeString(&conn, "\r\n");
							writeString(&conn, "        - Ben\r\n");
							expectNoMoreData(&conn);
							writeString(&conn, ".\r\n");
							expectToRead(&conn, "250 OK");
							expectNoMoreData(&conn);

							// Close the connection

							writeString(&conn, "QUIT\r\n");
							expectToRead(&conn, "221 *");
							expectRemoteClose(&conn);
							closeConnection(&conn);

							freeBuffers(&conn);


						}
					}
				}
				rcvr_list.clear();
			}
		}

		file.close();
	}

}







