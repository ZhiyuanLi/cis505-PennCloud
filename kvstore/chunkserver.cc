#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <vector>
#include <map>
#include <bitset>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <ctype.h>
#include <csignal>
#include <signal.h>
#include <algorithm>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

#include "chunkserver.h"
#include "lrucache.h"
#include "tools.h"

using namespace std;

extern map<string, vector<int>> chunk_info;
extern map<string, vector<vector<int>>> virmem_meta;

Chunkserver::Chunkserver(int v, int capacity) {  //TODO: 1. need a timer for each chunkserver to do checkpointing periodically
	this->opt_v = v;							 //		 2. locking for multi-threaded writing
	this->capacity = capacity;  // for now: only calculate the size of value
	this->lru = new LRUCache(v, capacity);
}

/* Sends the ERROR message to the client. */
void Chunkserver::error(int comm_fd) {
	const char *errormsg = "-ERR Not supported\r\n";
	send(comm_fd, errormsg, strlen(errormsg), 0);

	// If -v
	if (opt_v) {
		fprintf(stderr, "[%d] S: %s", comm_fd, errormsg);
	}
}

void Chunkserver::parse_line(string s, vector<string> &arguments, int num_of_args) {
    string delimiter = ",";  //TODO: what if there are "," in v1 when CPUT r,c,v1,v2 ? maybe use binary for v1
    int counter = 0;

    size_t pos = 0;
    string token;
    while ((pos = s.find(delimiter)) != string::npos && counter != num_of_args - 1) {
        arguments.push_back(s.substr(0, pos));
        s.erase(0, pos + delimiter.length());
        counter ++;
    }
    arguments.push_back(s);
}

void Chunkserver::write_log(char* request) { //TODO: force write
	string dest = "log";
	ofstream ofs;
	ofs.open(dest, ofstream::out | ofstream::app); // append
	ofs << request;
	ofs.close();
} 

void Chunkserver::replay_log() {

}

void Chunkserver::checkpointing() { 
	
}

void Chunkserver::load_checkpointing() {

}

/* PUT r,c,v */
void Chunkserver::put(char* &line, int comm_fd) {
	char *arguments = new char[strlen(line) - 4 + 1];
	strncpy(arguments, line + 4, strlen(line) - 4);
	arguments[strlen(line) - 4] = '\0';

	string to_be_parsed(arguments);

	vector<string> rcv;
	parse_line(to_be_parsed, rcv, 3);
	if (rcv.size() == 3) {
		string user = rcv.at(0);
		string filename = rcv.at(1);
		string value = rcv.at(2);
		lru->put(user, filename, value, comm_fd);
		write_log(line);
	} else {
		error(comm_fd);
	}
	delete [] arguments;
}

/* GET r,c */
void Chunkserver::get(char *line, int comm_fd) {
	char *arguments = new char[strlen(line) - 4 + 1];
	strncpy(arguments, line + 4, strlen(line) - 4);
	arguments[strlen(line) - 4] = '\0';

	string to_be_parsed(arguments);

	vector<string> rc;
	parse_line(to_be_parsed, rc, 2);
	if (rc.size() == 2) {
		string user = rc.at(0);
		string filename;
		filename.assign(rc.at(1), 0, rc.at(1).size() - 2);
		// Get the value in the corresponding row and colummn
		lru->get(user, filename, comm_fd);
	} else {
		error(comm_fd);
	}
	delete [] arguments;
}

/* CPUT r,c,v1,v2 */
void Chunkserver::cput(char* &line, int comm_fd) {
	char *arguments = new char[strlen(line) - 5 + 1];
	strncpy(arguments, line + 5, strlen(line) - 5);
	arguments[strlen(line) - 5] = '\0';

	string to_be_parsed(arguments);

	vector<string> rcv2;
	parse_line(to_be_parsed, rcv2, 4);
	if (rcv2.size() == 4) {
		string user = rcv2.at(0);
		string filename = rcv2.at(1);
		string old_value = rcv2.at(2) + "\r\n";
		string new_value = rcv2.at(3);
		// Don't support "," in old_value now, old_value may be better transfered as binary
		lru->cput(user, filename, old_value, new_value, comm_fd);
		write_log(line);
	} else {
		error(comm_fd);
	}
	delete [] arguments;
}

/* DELE r,c */
void Chunkserver::dele(char* &line, int comm_fd) {
	char *arguments = new char[strlen(line) - 5 + 1];
	strncpy(arguments, line + 5, strlen(line) - 5);
	arguments[strlen(line) - 5] = '\0';

	string to_be_parsed(arguments);

	vector<string> rc;
	parse_line(to_be_parsed, rc, 2);
	if (rc.size() == 2) {
		string user = rc.at(0);
		string filename;
		filename.assign(rc.at(1), 0, rc.at(1).size() - 2);
		//Get the value in the corresponding row and colummn
		lru->dele(user, filename, comm_fd);
		write_log(line);
	} else {
		error(comm_fd);
	}
	delete [] arguments;
}