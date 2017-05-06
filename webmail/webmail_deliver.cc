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
#include <mutex>
#include <set>
#include <vector>
#include <cstddef>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "client_header.h"

using namespace std;


struct Message{
	int messageID;
	int unmark_flag = 0;
	int begin_line;
	int end_line;
};

#define MAX_Message_Num 1000

struct Message MsgQueue[MAX_Message_Num];

void parse_record (unsigned char *buffer, size_t r,
		const char *section, ns_sect s,
		int idx, ns_msg *m) {

	ns_rr rr;
	int k = ns_parserr (m, s, idx, &rr);
	if (k == -1) {
		std::cerr << errno << " " << strerror (errno) << "\n";
		return;
	}

	std::cout << section << " " << ns_rr_name (rr) << " "
			<< ns_rr_type (rr) << " " << ns_rr_class (rr)
			<< ns_rr_ttl (rr) << " ";

	const size_t size = NS_MAXDNAME;
	unsigned char name[size];
	int t = ns_rr_type (rr);

	const u_char *data = ns_rr_rdata (rr);
	if (t == T_MX) {
		int pref = ns_get16 (data);
		ns_name_unpack (buffer, buffer + r, data + sizeof (u_int16_t),
				name, size);
		char name2[size];
		ns_name_ntop (name, name2, size);
		std::cout << pref << " " << name2;
	}
	else if (t == T_A) {
		unsigned int addr = ns_get32 (data);
		struct in_addr in;
		in.s_addr = ntohl (addr);
		char *a = inet_ntoa (in);
		std::cout << a;
	}
	else if (t == T_NS) {
		ns_name_unpack (buffer, buffer + r, data, name, size);
		char name2[size];
		ns_name_ntop (name, name2, size);
		std::cout << name2;
	}
	else {
		std::cout << "unhandled record";
	}

	std::cout << "\n";
}
int main(){

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
//			cout<<"MsgQueue[i].begin_line = "<<MsgQueue[i].begin_line<<endl;
//			cout<<"MsgQueue[i].end_line = "<<MsgQueue[i].end_line<<endl;
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
					for (int j = 0; j < rcvr_list.size(); j++){




					}
				}
				rcvr_list.clear();
			}
		}

		file.close();
	}

}
		/*
		// Connect to server
		struct connection conn1;
		initializeBuffers(&conn1, 5000);

		connectToPort(&conn1, atoi(argv[1]));
		expectToRead(&conn1, "220 localhost *");
		expectNoMoreData(&conn1);

		writeString(&conn1, "HELO tester\r\n");
		expectToRead(&conn1, "250 localhost");
		expectNoMoreData(&conn1);

		// Specify the sender and the receipient (with one incorrect recipient)

		writeString(&conn1, "MAIL FROM:<benjamin.franklin@localhost>\r\n");
		expectToRead(&conn1, "250 OK");
		expectNoMoreData(&conn1);

		writeString(&conn1, "RCPT TO:<linhphan@gmail.com>\r\n");
		expectToRead(&conn1, "250 OK");
		expectNoMoreData(&conn1);

		writeString(&conn1, "RCPT TO:<nonexistent.mailbox@localhost>\r\n");
		expectToRead(&conn1, "550 *");
		expectNoMoreData(&conn1);

		// Send the actual data

		writeString(&conn1, "DATA\r\n");
		expectToRead(&conn1, "354 *");
		expectNoMoreData(&conn1);

		writeString(&conn1, "From: Benjamin Franklin <benjamin.franklin@localhost>\r\n");
		writeString(&conn1, "To: Linh Thi Xuan Phan <linhphan@localhost>\r\n");
		writeString(&conn1, "Date: Fri, 21 Oct 2016 18:29:11 -0400\r\n");
		writeString(&conn1, "Subject: Testing my new email account\r\n");
		writeString(&conn1, "\r\n");
		writeString(&conn1, "Linh,\r\n");
		writeString(&conn1, "\r\n");
		writeString(&conn1, "I just wanted to see whether my new email account works.\r\n");
		writeString(&conn1, "\r\n");
		writeString(&conn1, "        - Ben\r\n");
		expectNoMoreData(&conn1);
		writeString(&conn1, ".\r\n");
		expectToRead(&conn1, "250 OK");
		expectNoMoreData(&conn1);

		// Close the connection

		writeString(&conn1, "QUIT\r\n");
		expectToRead(&conn1, "221 *");
		expectRemoteClose(&conn1);
		closeConnection(&conn1);

		freeBuffers(&conn1);

	}


	string domain_buffer;



	string temp = domain_buffer.substr(found+1); // extract server name
	size_t found2 = temp.find_last_of(">");
	string server_name = temp.substr(0,found2);
	const char* host = server_name.c_str();
	unsigned char host_buffer[BUFFER_SIZE];
	int r_size = res_query(host,C_IN,T_MX, host_buffer, BUFFER_SIZE);
	if(r_size == -1){
		char err_host[] = "ERR: Cannot find host server!";
		dowrite(comm_fd, err_host, sizeof(err_host)-1);
	}
	else if(r_size == static_cast<int> (BUFFER_SIZE)){
		char err[] = "ERR: Buffer too small, finding host server truncated!";
		dowrite(comm_fd, err, sizeof(err)-1);
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
	int question = ntohs (hdr->qdcount);
	int answers = ntohs (hdr->ancount);
	int nameservers = ntohs (hdr->nscount);
	int addrrecords = ntohs (hdr->arcount);

	std::cout << "Reply: question: " << question << ", answers: " << answers
			<< ", nameservers: " << nameservers
			<< ", address records: " << addrrecords << "\n";
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
		std::cout << "question " << ns_rr_name (rr) << " "
				<< ns_rr_type (rr) << " " << ns_rr_class (rr) << "\n";
	}
	for (int i = 0; i < answers; ++i) {
		parse_record (host_buffer, r_size, "answers", ns_s_an, i, &m);
	}

	for (int i = 0; i < nameservers; ++i) {
		parse_record (host_buffer, r_size, "nameservers", ns_s_ns, i, &m);
	}

	for (int i = 0; i < addrrecords; ++i) {
		parse_record (host_buffer, r_size, "addrrecords", ns_s_ar, i, &m);
	}
		 */






