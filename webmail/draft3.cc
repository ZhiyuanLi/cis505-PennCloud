/*
 * draft3.cc
 *
 *  Created on: May 7, 2017
 *      Author: cis505
 */
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

int parse_record (unsigned char *buffer, size_t r,
		const char *section, ns_sect s,
		int idx, ns_msg *m) {

	ns_rr rr;
	int k = ns_parserr (m, s, idx, &rr);
	if (k == -1) {
		std::cerr << errno << " " << strerror (errno) << "\n";
		return 0;
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

int main(int argc, char *argv[]){



	const size_t size = 1024;
	unsigned char buffer[size];

	const char *host = argv[1];

	int r = res_query (host, C_IN, T_MX, buffer, size);
	if (r == -1) {
//		std::cerr << h_errno << " " << hstrerror (h_errno) << "\n";
		return 1;
	}
	else {
		if (r == static_cast<int> (size)) {
			std::cerr << "Buffer too small reply truncated\n";
			return 1;
		}
	}
	HEADER *hdr = reinterpret_cast<HEADER*> (buffer);

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
		}
		return 1;
	}
	int question = ntohs (hdr->qdcount);
	int answers = ntohs (hdr->ancount);
	int nameservers = ntohs (hdr->nscount);
	int addrrecords = ntohs (hdr->arcount);

	std::cout << "Reply: question: " << question << ", answers: " << answers
			<< ", nameservers: " << nameservers
			<< ", address records: " << addrrecords << "\n";

	ns_msg m;
	int k = ns_initparse (buffer, r, &m);
	if (k == -1) {
		std::cerr << errno << " " << strerror (errno) << "\n";
		return 1;
	}

	for (int i = 0; i < question; ++i) {
		ns_rr rr;
		int k = ns_parserr (&m, ns_s_qd, i, &rr);
		if (k == -1) {
			std::cerr << errno << " " << strerror (errno) << "\n";
			return 1;
		}
		std::cout << "question " << ns_rr_name (rr) << " "
				<< ns_rr_type (rr) << " " << ns_rr_class (rr) << "\n";
	}
	for (int i = 0; i < answers; ++i) {
		parse_record (buffer, r, "answers", ns_s_an, i, &m);
	}

	for (int i = 0; i < nameservers; ++i) {
		parse_record (buffer, r, "nameservers", ns_s_ns, i, &m);
	}

	for (int i = 0; i < addrrecords; ++i) {
		parse_record (buffer, r, "addrrecords", ns_s_ar, i, &m);
	}

}



