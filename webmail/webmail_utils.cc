#include <string>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/md5.h>
#include <signal.h>
#include <regex>
#include <set>
#include <vector>
#include <cstddef>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>


#include "webmail_utils.h"
#include "../frontend/store.h"

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

char* parse_record (unsigned char *buffer, size_t r,
		const char *section, ns_sect s,
		int idx, ns_msg *m) {

	ns_rr rr;
	int k = ns_parserr (m, s, idx, &rr);
	if (k == -1) {
		std::cerr << errno << " " << strerror (errno) << "\n";
		return 0;
	}

	std::cout << section << " " << ns_rr_name (rr) << " "
			<< ns_rr_type (rr) << " " << ns_rr_class (rr)<< " "
			<< ns_rr_ttl (rr) << " ";

	const size_t size = NS_MAXDNAME;
	unsigned char name[size];
	int t = ns_rr_type (rr);

	const u_char *data = ns_rr_rdata (rr);
	if (t == T_MX) {
		cout<<"t == T_MX"<<endl;
		int pref = ns_get16 (data);
		ns_name_unpack (buffer, buffer + r, data + sizeof (u_int16_t),
				name, size);
		char name2[size];
		ns_name_ntop (name, name2, size);
		std::cout << pref << " " << name2;
		return NULL;
	}
	else if (t == T_A) {
		cout<<"t == T_A"<<endl;
		unsigned int addr = ns_get32 (data);
		struct in_addr in;
		in.s_addr = ntohl (addr);
		char *a = inet_ntoa (in);
		//		std::cout << a;
		return a;
	}
	else if (t == T_NS) {
		cout<<"t == T_NS"<<endl;
		ns_name_unpack (buffer, buffer + r, data, name, size);
		char name2[size];
		ns_name_ntop (name, name2, size);
		std::cout << name2;
		return NULL;
	}
	else {
		std::cout << "unhandled record";
	}

	std::cout << "\n";
}

int handle_helo(int comm_fd, int rlen, int helo_flag){
	if (rlen > 7){
		char resp[] = "250 localhost\r\n";
		dowrite(comm_fd, resp, sizeof(resp)-1);
		helo_flag = 1;
	}
	else{
		char respERR[] = "501 HELO command must be followed by a domain name.\r\n";
		dowrite(comm_fd, respERR, sizeof(respERR)-1);
	}
	return helo_flag;
}

int handle_mail(int comm_fd, char* buf, int rlen, int mail_flag, char* reverse_path, std::set<std::string> rcvr_list){


	string buffer = buf; // convert read message buf to string buffer
	string domain_buffer = buffer.substr(10); // copy from 9th char in buffer
	size_t found = domain_buffer.find_last_of("@"); //find the '@' char in mail_from command
	if(found == -1){
		char err[] = "incorrect reverse path";
		dowrite(comm_fd, err, sizeof(err)-1);
	}
	if (domain_buffer.compare(found, 14, "@localhost.com") != 0){
		cout<<"here"<<endl;
		char err[] = "ERR: incorrect host name";
		dowrite(comm_fd, err, sizeof(err)-1);
		cout<<"comm_fd"<<comm_fd<<endl;
		cout<<"here!"<<endl;
	}
	else{
		bzero(&reverse_path,sizeof(reverse_path));
		rcvr_list.clear();
		string str = domain_buffer.substr(10, found);
		//		strcpy(reverse_path, str.c_str());
		mail_flag = 1;
		char OKresp[] = "250 OK\r\n";
		dowrite(comm_fd, OKresp, sizeof(OKresp)-1);

	}
	return mail_flag;
}

string handle_rcpt(int comm_fd, char* buf, int &rcpt_flag, std::set<std::string> rcvr_list){

	string buffer = buf;
	string domain_buffer = buffer.substr(8);
	size_t found = domain_buffer.find_last_of("@");
	if(found == -1){
		char err[] = "ERR: incorrect forward path";
		dowrite(comm_fd, err, sizeof(err)-1);
	}
	//non-local hostname, forward to other servers
	if (domain_buffer.compare(found, 14, "@localhost.com") != 0){

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

			//								int n = rcvr_list.size();
		}
		else { // if not exist file for receiver
			char err[] = "550 No such user here \r\n";
			dowrite(comm_fd, err, sizeof(err)-1);
		}
	}
}

int handle_data(int comm_fd, char* buf, int rlen, std::set<std::string> rcvr_list, int BUFFER_SIZE, int helo_flag){

	buf[rlen] = 0;
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

	for(set<string>::iterator it = rcvr_list.begin(); it != rcvr_list.end();it++){
//		ofstream userfile;
//		userfile.open(*it,ios::app);
//		userfile<< "##"<<digestBuffer<<endl;
//		userfile<<lines.str();
//		userfile.close();
//		string message = "put " + *it + "," + string(digestBuffer) + "," + line_data + "\r\n";
//		send_to_backend(message, user_name);

	}

	lines.str("");
	lines.clear();
	bzero(digestBuffer, sizeof(digestBuffer));

	helo_flag = 0;
	char OKresp[] = "250 OK\r\n";
	dowrite(comm_fd, OKresp, sizeof(OKresp)-1);
	return helo_flag;
}

int handle_send(int comm_fd, char* buf, int rlen, int BUFFER_SIZE){

	int sent_flag = 0;
	buf[rlen] = 0;
	//	char DATAresp[] = "354 send the mail data, end with .\r\n";
	//	dowrite(comm_fd, DATAresp, sizeof(DATAresp)-1);

	int MAX_Message_Num = 20;
	int currlen = 0;
	unsigned short data_len = 0;
	char data_buf[BUFFER_SIZE];
	bzero(&data_buf, sizeof(data_buf));

	ostringstream lines;
	struct Message Msg;
	struct Message MsgQueue[MAX_Message_Num];
	int max_msg = 0;
	int i = 0;
	string sender;

	while(read_data(comm_fd, data_buf, currlen) == 0){

		data_buf[currlen] = 0;
		string data = data_buf;
		lines << data_buf;

		if (data.compare(0, 5, "From:") == 0) {

			std::size_t found1 = data.find_first_of("<");
			std::size_t found2 = data.find_first_of(">");
			sender = data.substr(found1 + 1, found2 - found1 - 1);
		}

		if (data.compare(0, 3, "To:") == 0) {

			std::size_t found1 = data.find_first_of("<");
			std::size_t found2 = data.find_first_of(">");
			while(found1!=std::string::npos){

				Msg.messageID ++;

				Msg.rcvr = data.substr(found1 + 1, found2 - found1 - 1);
				found1=data.find_first_of("<",found1+1);
				found2=data.find_first_of(">",found2+1);
				MsgQueue[i] = Msg;
				i++;
			}
		}
		max_msg = i;
	}
	data_buf[currlen] = 0;
	string data = data_buf;
	lines << data_buf;

	for (int j = 0; j < max_msg; j++){

		string domain_buffer = MsgQueue[j].rcvr; // mengjin@seas.upenn.edu
		std::size_t found = domain_buffer.find_first_of("@");
		string server_name = domain_buffer.substr(found+1); // extract server name
		string user_name = domain_buffer.substr(0, found);

		if (server_name.compare(0, 13, "localhost.com") != 0){

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

			//			std::cout << "Reply: question: " << question << ", answers: " << answers
			//					<< ", nameservers: " << nameservers
			//					<< ", address records: " << addrrecords << "\n";

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

			//			for (int i = 0; i < answers; ++i) {
			//				parse_record (host_buffer, r_size, "answers", ns_s_an, i, &m);
			//			}
			//
			//			for (int i = 0; i < nameservers; ++i) {
			//				parse_record (host_buffer, r_size, "nameservers", ns_s_ns, i, &m);
			//			}
			//
			//			for (int i = 0; i < addrrecords; ++i) {
			//				parse_record (host_buffer, r_size, "addrrecords", ns_s_ar, i, &m);
			//			}

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
			char resp[BUFFER_SIZE] = "MAIL FROM:<";
			strcat(resp, sender.c_str());
			strcat(resp, ">\r\n");

			writeString(&conn, resp);
			expectToRead(&conn, "250 OK");
			expectNoMoreData(&conn);

			char resp1[BUFFER_SIZE] = "RCPT TO:<";
			strcat(resp1, MsgQueue[j].rcvr.c_str());
			strcat(resp1, ">\r\n");
			writeString(&conn, resp1);
			expectToRead(&conn, "250 OK");
			expectNoMoreData(&conn);

			// Send the actual data

			writeString(&conn, "DATA\r\n");
			expectToRead(&conn, "354 *");
			expectNoMoreData(&conn);
			string content = lines.str();
			writeString(&conn, content.c_str());
			expectToRead(&conn, "250 OK");
			expectNoMoreData(&conn);

			// Close the connection

			writeString(&conn, "QUIT\r\n");
			expectToRead(&conn, "221 *");
			expectRemoteClose(&conn);
			closeConnection(&conn);

			freeBuffers(&conn);

		}
		else{

			string line_data;
			line_data = lines.str();
			unsigned char digestBuffer[16];
			char * my_str = strdup(line_data.c_str());
			computeDigest(my_str, sizeof(line_data) , digestBuffer);

//			string message = "put " + user_name + "," + string(digestBuffer) + "," + line_data + "\r\n";
//			send_to_backend(message, user_name);

			//	for(set<string>::iterator it = rcvr_list.begin(); it != rcvr_list.end();it++){
			//		ofstream userfile;
			//		userfile.open(*it,ios::app);
			//		userfile<< "##"<<digestBuffer<<endl;
			//		userfile<<lines.str();
			//		userfile.close();
			//	}

			lines.str("");
			lines.clear();
			bzero(digestBuffer, sizeof(digestBuffer));

			char OKresp[] = "250 OK\r\n";
			dowrite(comm_fd, OKresp, sizeof(OKresp)-1);

			return sent_flag;
		}
	}
}


void log(const char *prefix, const char *data, int len, const char *suffix)
{
	printf("%s", prefix);
	for (int i=0; i<len; i++) {
		if (data[i] == '\n')
			printf("<LF>");
		else if (data[i] == '\r')
			printf("<CR>");
		else if (isprint(data[i]))
			printf("%c", data[i]);
		else
			printf("<0x%02X>", (unsigned int)(unsigned char)data[i]);
	}
	printf("%s", suffix);
}

// This function writes a string to a connection. If a LF is required,
// it must be part of the 'data' argument. (The idea is that we might
// sometimes want to send 'partial' lines to see how the server handles these.)

void writeString(struct connection *conn, const char *data)
{
	int len = strlen(data);
	log("C: ", data, len, "\n");

	int wptr = 0;
	while (wptr < len) {
		int w = write(conn->fd, &data[wptr], len-wptr);
		if (w<0)
			panic("Cannot write to conncetion (%s)", strerror(errno));
		if (w==0)
			panic("Connection closed unexpectedly");
		wptr += w;
	}
}

// This function verifies that the server has sent us more data at this point.
// It does this by temporarily putting the socket into nonblocking mode and then
// attempting a read, which (if there is no data) should return EAGAIN.
// Note that some of the server's data might still be 'in flight', so it is best
// to call this only after a certain delay.

void expectNoMoreData(struct connection *conn)
{
	int flags = fcntl(conn->fd, F_GETFL, 0);
	fcntl(conn->fd, F_SETFL, flags | O_NONBLOCK);
	int r = read(conn->fd, &conn->buf[conn->bytesInBuffer], conn->bufferSizeBytes - conn->bytesInBuffer);

	if ((r<0) && (errno != EAGAIN))
		panic("Read from connection failed (%s)", strerror(errno));

	if (r>0)
		conn->bytesInBuffer += r;

	if (conn->bytesInBuffer > 0) {
		log("S: ", conn->buf, conn->bytesInBuffer, " [unexpected; server should not have sent anything!]\n");
		conn->bytesInBuffer = 0;
	}

	fcntl(conn->fd, F_SETFL, flags);
}

// Attempts to connect to a port on the local machine.

void connectToPort(struct connection *conn, int portno)
{
	conn->fd = socket(PF_INET, SOCK_STREAM, 0);
	if (conn->fd < 0)
		panic("Cannot open socket (%s)", strerror(errno));

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(portno);
	inet_pton(AF_INET, "127.0.0.1", &(servaddr.sin_addr));

	if (connect(conn->fd, (struct sockaddr*)&servaddr, sizeof(servaddr))<0)
		panic("Cannot connect to localhost:10000 (%s)", strerror(errno));

	conn->bytesInBuffer = 0;
}

// Reads a line of text from the server (until it sees a LF) and then compares
// the line to the argument. The argument should not end with a LF; the function
// strips off any LF or CRLF from the incoming data before doing the comparison.
// (This is to avoid assumptions about whether the server terminates its lines
// with a CRLF or with a LF.)

void DoRead(struct connection *conn){

	int lfpos = -1;
	while (true) {
		for (int i=0; i<conn->bytesInBuffer; i++) {
			if (conn->buf[i] == '\n') {
				lfpos = i;
				break;
			}
		}

		if (lfpos >= 0)
			break;

		if (conn->bytesInBuffer >= conn->bufferSizeBytes)
			panic("Read %d bytes, but no CRLF found", conn->bufferSizeBytes);

		int bytesRead = read(conn->fd, &conn->buf[conn->bytesInBuffer], conn->bufferSizeBytes - conn->bytesInBuffer);
		if (bytesRead < 0)
			panic("Read failed (%s)", strerror(errno));
		if (bytesRead == 0)
			panic("Connection closed unexpectedly");

		conn->bytesInBuffer += bytesRead;
	}

	log("S: ", conn->buf, lfpos+1, "");

	// Get rid of the LF (or, if it is preceded by a CR, of both the CR and the LF)

	bool crMissing = false;
	if ((lfpos==0) || (conn->buf[lfpos-1] != '\r')) {
		crMissing = true;
		conn->buf[lfpos] = 0;
	} else {
		conn->buf[lfpos-1] = 0;
	}

	// 'Eat' the line we just parsed. However, keep in mind that there might still be
	// more bytes in the buffer (e.g., another line, or a part of one), so we have to
	// copy the rest of the buffer up.

	for (int i=lfpos+1; i<conn->bytesInBuffer; i++)
		conn->buf[i-(lfpos+1)] = conn->buf[i];
	conn->bytesInBuffer -= (lfpos+1);

}

void expectToRead(struct connection *conn, const char *data)
{
	// Keep reading until we see a LF

	int lfpos = -1;
	while (true) {
		for (int i=0; i<conn->bytesInBuffer; i++) {
			if (conn->buf[i] == '\n') {
				lfpos = i;
				break;
			}
		}

		if (lfpos >= 0)
			break;

		if (conn->bytesInBuffer >= conn->bufferSizeBytes)
			panic("Read %d bytes, but no CRLF found", conn->bufferSizeBytes);

		int bytesRead = read(conn->fd, &conn->buf[conn->bytesInBuffer], conn->bufferSizeBytes - conn->bytesInBuffer);
		if (bytesRead < 0)
			panic("Read failed (%s)", strerror(errno));
		if (bytesRead == 0)
			panic("Connection closed unexpectedly");

		conn->bytesInBuffer += bytesRead;
	}

	log("S: ", conn->buf, lfpos+1, "");

	// Get rid of the LF (or, if it is preceded by a CR, of both the CR and the LF)

	bool crMissing = false;
	if ((lfpos==0) || (conn->buf[lfpos-1] != '\r')) {
		crMissing = true;
		conn->buf[lfpos] = 0;
	} else {
		conn->buf[lfpos-1] = 0;
	}

	// Check whether the server's actual response matches the expected response
	// Note: The expected response might end in a wildcard (*) in which case
	// the rest of the server's line is ignored.

	int argptr = 0, bufptr = 0;
	bool match = true;
	while (match && data[argptr]) {
		if (data[argptr] == '*')
			break;
		if (data[argptr++] != conn->buf[bufptr++])
			match = false;
	}

	if (!data[argptr] && conn->buf[bufptr])
		match = false;

	// Annotate the output to indicate whether the response matched the expectation.

	if (match) {
		if (crMissing)
			printf(" [Terminated by LF, not by CRLF]\n");
		else
			printf(" [OK]\n");
	} else {
		log(" [Expected: '", data, strlen(data), "']\n");
	}

	// 'Eat' the line we just parsed. However, keep in mind that there might still be
	// more bytes in the buffer (e.g., another line, or a part of one), so we have to
	// copy the rest of the buffer up.

	for (int i=lfpos+1; i<conn->bytesInBuffer; i++)
		conn->buf[i-(lfpos+1)] = conn->buf[i];
	conn->bytesInBuffer -= (lfpos+1);
}

// This function verifies that the remote end has closed the connection.

void expectRemoteClose(struct connection *conn)
{
	int r = read(conn->fd, &conn->buf[conn->bytesInBuffer], conn->bufferSizeBytes - conn->bytesInBuffer);
	if (r<0)
		panic("Read failed (%s)", strerror(errno));
	if (r>0) {
		log("S: ", conn->buf, r + conn->bytesInBuffer, " [unexpected; server should have closed the connection]\n");
		conn->bytesInBuffer = 0;
	}
}

// This function initializes the read buffer

void initializeBuffers(struct connection *conn, int bufferSizeBytes)
{
	conn->fd = -1;
	conn->bufferSizeBytes = bufferSizeBytes;
	conn->bytesInBuffer = 0;
	conn->buf = (char*)malloc(bufferSizeBytes);
	if (!conn->buf)
		panic("Cannot allocate %d bytes for buffer", bufferSizeBytes);
}

// This function closes our local end of a connection

void closeConnection(struct connection *conn)
{
	close(conn->fd);
}

// This function frees the allocated read buffer

void freeBuffers(struct connection *conn)
{
	free(conn->buf);
	conn->buf = NULL;
}
