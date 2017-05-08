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
using namespace std;
#ifndef __webmail_utils_h__
#define __webmail_utils_h__

#define panic(a...) do { fprintf(stderr, a); fprintf(stderr, "\n"); exit(1); } while (0)

// For each connection we keep a) its file descriptor, and b) a buffer that contains
// any data we have read from the connection but not yet processed. This is necessary
// because sometimes the server might send more bytes than we immediately expect.

struct connection {
	int fd;
	char *buf;
	int bytesInBuffer;
	int bufferSizeBytes;
};

struct Message{
	int messageID;
	string rcvr;
};

void log(const char *prefix, const char *data, int len, const char *suffix);
void writeString(struct connection *conn, const char *data);
void expectNoMoreData(struct connection *conn);
void connectToPort(struct connection *conn, int portno);
void expectToRead(struct connection *conn, const char *data);
void expectRemoteClose(struct connection *conn);
void initializeBuffers(struct connection *conn, int bufferSizeBytes);
void closeConnection(struct connection *conn);
void freeBuffers(struct connection *conn);
void DoRead(struct connection *conn);

int read_command(int fd, char *buf);
int read_data(int fd, char *buf, int &rcvd);
bool dowrite(int fd, char *buf, int len);
void computeDigest(char *data, int dataLengthBytes, unsigned char *digestBuffer);
char* parse_record (unsigned char *buffer, size_t r,
		const char *section, ns_sect s,
		int idx, ns_msg *m);

int handle_helo(int comm_fd, int rlen, int helo_flag);
int handle_mail(int comm_fd, char* buf, int rlen, int mail_flag, char* reverse_path, std::set<std::string> rcvr_list);
string handle_rcpt(int comm_fd, char* buf, int &rcpt_flag, std::set<std::string> rcvr_list);
int handle_data(int comm_fd, char* buf, int rlen, std::set<std::string> rcvr_list, int BUFFER_SIZE, int helo_flag);
int handle_send(int comm_fd, char* buf, int rlen, int BUFFER_SIZE);

#endif /* defined(__client_header_h__) */
