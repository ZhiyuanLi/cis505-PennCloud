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
#include "node.h"
#include "parameters.h"
#include "tools.h"

using namespace std;

struct MsgPair {
	int sq;
	string msg;
};

extern map<string, vector<int>> chunk_info;
extern map<string, vector<vector<int>>> virmem_meta;
extern map<string, vector<vector<int>>> cp_meta;
extern map<string, int> seq_num_records;
extern map<string, vector<MsgPair>> primary_holdback;
extern bool isPrimary;
extern int secondary_fd;
extern bool opt_v;


Chunkserver::Chunkserver(int capacity) {                     //TODO: locking for multi-threaded read and write								 
	this->capacity = capacity;                         
	this->lru = new LRUCache(MEMORY_CAPACITY);               
}

/* Sends the ERROR message to the client. */
void Chunkserver::error(int comm_fd) {
	const char *errormsg = "-ERR Not supported\r\n";
	send(comm_fd, errormsg, strlen(errormsg), 0);

	// If -v
	if (opt_v) {
		print_time();
		fprintf(stderr, "[%d] S: %s", comm_fd, errormsg);
	}
}

void Chunkserver::parse_line(string s, vector<string> &arguments, int num_of_args) {
    string delimiter = ",";  //TODO: what if there are "," in v1 when CPUT r,c,v1,v2 ? maybe use binary for v1
    int counter = 0;

    size_t pos = 0;
    while ((pos = s.find(delimiter)) != string::npos && counter != num_of_args - 1) {
        arguments.push_back(s.substr(0, pos));
        s.erase(0, pos + delimiter.length());
        counter ++;
    }
    arguments.push_back(s);
}

void Chunkserver::write_cp_meta() {
	string meta = "";
	for (auto it: cp_meta) {
		vector<vector<int>> temp = cp_meta[it.first];
		string user = "@" + it.first + ";";
		meta += user;
		for (int i = 0; i < temp.size(); i++) {
			string s = to_string(temp.at(i).at(0)) + "," 
			         + to_string(temp.at(i).at(1)) + "," 
			         + to_string(temp.at(i).at(2)) + ";";
			meta += s;
		}
    }
    write_file("metadata/cp_meta", meta);
}

void Chunkserver::write_virmem_meta() {
	string meta = "";
	for (auto it: virmem_meta) {
		vector<vector<int>> temp = virmem_meta[it.first];
		string user = "@" + it.first + ";";
		meta += user;
		for (int i = 0; i < temp.size(); i++) {
			string s = to_string(temp.at(i).at(0)) + "," 
			         + to_string(temp.at(i).at(1)) + "," 
			         + to_string(temp.at(i).at(2)) + ";";
			meta += s;
		}
    }
    write_file("metadata/virmem_meta", meta);
}

void Chunkserver::write_chunk_info() {
	string meta = to_string(chunk_info["checkpointing"].at(0)) + "," 
	            + to_string(chunk_info["checkpointing"].at(1)) + ","
				+ to_string(chunk_info["virtual memory"].at(0)) + "," 
				+ to_string(chunk_info["virtual memory"].at(1)) + ","
				+ to_string(lru->memory_used);
	write_file("metadata/chunk_info", meta);
}

void Chunkserver::write_user_size() {
	string meta = "";
	for (auto it: lru->user_size) {
		string s = it.first + "," + to_string(lru->user_size[it.first]) + ",";
		meta += s;	
    }
    write_file("metadata/user_size", meta);
}

vector<int> Chunkserver::rebuild_vector(string s) {
	vector<int> v;
	string delimiter = ",";  

    size_t pos = 0;
    string user;
    while ((pos = s.find(delimiter)) != string::npos) {
    	v.push_back(stoi(s.substr(0, pos)));
    	s.erase(0, pos + delimiter.length());
    }
    v.push_back(stoi(s));
    return v;
}

void Chunkserver::parse_meta(string s, string type) {
	string delimiter = ";";  
    int counter = 0;

    size_t pos = 0;
    string user;
    while ((pos = s.find(delimiter)) != string::npos) {
    	if (counter == 0) {
    		user = s.substr(0, pos);
    	} else {
    		vector<int> record = rebuild_vector(s.substr(0, pos));

    		if (type.compare("virtual memory") == 0) {
		        virmem_meta[user].push_back(record);
		    }

		    if (type.compare("checkpointing") == 0) {
		        cp_meta[user].push_back(record);
		    } 
    	}
        s.erase(0, pos + delimiter.length());
        counter ++;
    }
}

void Chunkserver::load_cp_vm_meta(string path, string type) {
	string s = read_file(path);
	string delimiter = "@";  
    int counter = 0;

    size_t pos = 0;
    while ((pos = s.find(delimiter)) != string::npos) {
    	if (counter != 0) {
    		parse_meta(s.substr(0, pos), type); 
    	}
        s.erase(0, pos + delimiter.length());
        counter ++;
    }
    parse_meta(s, type); 
}

void Chunkserver::load_chunk_info() {
	string s = read_file("metadata/chunk_info");
	string delimiter = ","; 
    int counter = 0;

    size_t pos = 0;
    while ((pos = s.find(delimiter)) != string::npos) {
    	if (counter == 0 || counter == 1) {
    		chunk_info["checkpointing"].push_back(stoi(s.substr(0, pos)));
    	}
        
        if (counter == 2 || counter == 3) {
    		chunk_info["virtual memory"].push_back(stoi(s.substr(0, pos)));
    	}
    	
        s.erase(0, pos + delimiter.length());
        counter ++;
    }
}

void Chunkserver::load_user_size() {
	string s = read_file("metadata/user_size");
	string delimiter = ","; 
    int counter = 0;

    size_t pos = 0;
    string user;
    while ((pos = s.find(delimiter)) != string::npos) {
    	counter += 1;
    	if (counter % 2 != 0) {
    		user = s.substr(0, pos);
    	} else {
    		lru->user_size[user] = stoi(s.substr(0, pos));
    	}
        s.erase(0, pos + delimiter.length());
    }
}

/* Write checkpointing from memory to disk. */
void Chunkserver::checkpointing() {

	// empty previous checkpointing and log
	clear_dir("checkpointing/virtual memory/");	
	clear_dir("checkpointing/");
	clear_dir("metadata/");
	string log("log");
	remove(log.c_str());

	// clear metadata for the last checkpointing
	map<string, vector<vector<int>>> temp;
	cp_meta = temp;

	// since checkpointing chunks are rewritten, the chunk info back to zero
	chunk_info["checkpointing"].at(0) = 0;
	chunk_info["checkpointing"].at(1) = 0;

	// write in-memory user into disk
	for (auto it: lru->tablets) {
		Node *user = lru->tablets[it.first];
		string row = "";
	    lru->format_node(user, row);
	    lru->write_to_chunk(user->key, row, "checkpointing");
    }

	// write metadata into disk
	write_cp_meta();
	write_virmem_meta();
	write_chunk_info();
	write_user_size();

	// snapshot virtual memory to avoid inconsistency between virtual memory and its cp metadata
	create_dir("checkpointing/virtual memory");
	copy_dir("virtual memory/", "checkpointing/virtual memory/");
}

/* Load checkpointing from disk to memory. */
void Chunkserver::load_checkpointing() {

	// load metadata
	load_cp_vm_meta("metadata/cp_meta", "checkpointing");
	load_cp_vm_meta("metadata/virmem_meta", "virtual memory");
	load_chunk_info();
	load_user_size();

	// load user files
	for (auto it: cp_meta) {
		string user = it.first;
	    lru->load_user_from_disk("checkpointing", user, -1);  // comm_fd = -1 here, don't want to send cp info
    }

    // maintain consistency between virtual memory and its cp metadata
    clear_dir("virtual memory/");
    copy_dir("checkpointing/virtual memory/", "virtual memory/");  
}

void Chunkserver::write_log(char* request) { //TODO: force write
	string dest = "log";
	ofstream ofs;
	ofs.open(dest, ofstream::out | ofstream::app); // append
	ofs << request;
	ofs.close();
} 

void Chunkserver::replay_log() {
	string line;
	ifstream myfile ("log");
	if (myfile.is_open()) {
		while ( getline (myfile, line) ) {

			char *record = new char[line.size() + 2];
			strcpy(record, line.c_str());
			strcat(record, "\n");
			record[line.size() + 1] = '\0';

			// parse command
			string command; 
			string delimiter = " "; 
		    size_t pos = 0;
		    while ((pos = line.find(delimiter)) != string::npos) {
		        command = line.substr(0, pos);
		        break;
		    }

			// execute command
			if(command.compare("put") == 0) {
				put(record, false, -1, 0);                         // comm_fd = -1 here and below			
			} else if(command.compare("cput") == 0) {
				cput(record, false, -1, 0); 
			} else if(command.compare("dele") == 0) {
				dele(record, false, -1, 0);                 				
			} else {
				if (opt_v) {
					print_time();
					fprintf(stderr, "S: %s replay failed", record);
				}
			}
			delete [] record;
		}
		myfile.close();
	} 
}

void Chunkserver::send_to_secondary(char* line, string user) {

	int sequence = seq_num_records[user] + 1;
	seq_num_records[user] = sequence;
	MsgPair *msg_held = new MsgPair();
	msg_held->sq = sequence;
	string operation(line);
	msg_held->msg = operation;
	primary_holdback[user].push_back(*msg_held);
	string temp(line);
	string msg_to_s = to_string(sequence) + "," + temp;
	send(secondary_fd, msg_to_s.c_str(), msg_to_s.size(), 0);
	
}

/* PUT r,c,v */
void Chunkserver::put(char* &line, bool external, int comm_fd, int seq_num) {

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
		lru->put(user, filename, value, comm_fd, true, seq_num);
	
		if (external) {
			write_log(line);
			if (isPrimary) {
				send_to_secondary(line, user);
			}
		}	
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
void Chunkserver::cput(char* &line, bool external, int comm_fd, int seq_num) {
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
		lru->cput(user, filename, old_value, new_value, comm_fd, seq_num);
		if (external) {
			write_log(line);
			if (isPrimary) send_to_secondary(line, user);
		}
	} else {
		error(comm_fd);
	}
	delete [] arguments;
}

/* DELE r,c */
void Chunkserver::dele(char* line, bool external, bool is_migration, int comm_fd, int seq_num) {
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
		lru->dele(user, filename, comm_fd, seq_num, is_migration);
		if (external) {
			write_log(line);
			if (isPrimary) send_to_secondary(line, user);
		}
	} else {
		error(comm_fd);
	}
	delete [] arguments;
}

/* GETLIST user,lst_type */
void Chunkserver::getlist(char *line, int comm_fd) {
	char *arguments = new char[strlen(line) - 8 + 1];
	strncpy(arguments, line + 8, strlen(line) - 8);
	arguments[strlen(line) - 8] = '\0';

	string to_be_parsed(arguments);

	vector<string> rc;
	parse_line(to_be_parsed, rc, 2);
	if (rc.size() == 2) {
		string user = rc.at(0);
		string type;
		type.assign(rc.at(1), 0, rc.at(1).size() - 2);
		// Get the value in the corresponding row and colummn
		lru->getlist(user, type, comm_fd);
	} else {
		error(comm_fd);
	}
	delete [] arguments;
}

/* GETFILE user */
void Chunkserver::getfile(char *line, int comm_fd) {

	if (strlen(line) > 10) {
		string to_be_parsed(line);
		string user = to_be_parsed.substr(8, to_be_parsed.size() - 10);
		// Get the filenames of the user
		lru->getfile(user, comm_fd);
	} else {
		error(comm_fd);
	}	
}

/* RENAME r,c1,c2 */
void Chunkserver::rename(char* &line, bool external, int comm_fd, int seq_num) {

	char *arguments = new char[strlen(line) - 7 + 1];
	strncpy(arguments, line + 7, strlen(line) - 7);
	arguments[strlen(line) - 7] = '\0';

	string to_be_parsed(arguments);

	vector<string> rc2;
	parse_line(to_be_parsed, rc2, 3);

	if (rc2.size() == 3) {
		string user = rc2.at(0);
		string old_filename = rc2.at(1);
		string new_filename;
		new_filename.assign(rc2.at(2), 0, rc2.at(2).size() - 2);
		lru->rename(user, old_filename, new_filename, comm_fd, seq_num);

		if (external) {
			write_log(line);
			if (isPrimary) {
				send_to_secondary(line, user);
			}
		}	
	} else {
		error(comm_fd);
	}
	delete [] arguments;
}

void Chunkserver::migrate_data(string user, int comm_fd) {    // assume user exists

	// send data to data migration requester	
	for (auto it: lru->tablets[user]->value) {
        string filename = it.first;     
        string value_read;
        if (lru->get_helper(user, filename, value_read, comm_fd)) { // get will update the user's position in the linked list
            string feedback = "put " + user + "," + filename + "," + value_read;
            send(comm_fd, feedback.c_str(), feedback.size(), 0);

            // If -v
            if (opt_v) {
                print_time();
                fprintf(stderr, "[%d] S: Migrating %s\n", comm_fd, feedback.c_str());
            }
        }
    }

    // delete the file on primary and inform secondary to do so too
    for (auto it: lru->tablets[user]->value) {  
        string filename = it.first;     
        string value_read;
        if (lru->get_helper(user, filename, value_read, comm_fd)) { // get will update the user's position in the linked list
            string temp = "dele " + user + "," + filename + "\r\n";
        	char *sync_secondary = new char[temp.size() + 1];
        	strcpy(sync_secondary, temp.c_str());
        	sync_secondary[temp.size()] = '\0';
        	dele(sync_secondary, true, true, comm_fd, 0); 

            // If -v
            if (opt_v) {
                print_time();
                fprintf(stderr, "[%d] S: Migrating %s\n", comm_fd, sync_secondary);
            }
            delete [] sync_secondary;
        }
    }
}