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
#include <math.h>

using namespace std;

#include "lrucache.h"
#include "node.h"
#include "parameters.h"
#include "tools.h"

extern map<string, vector<int>> chunk_info;
extern map<string, vector<vector<int>>> virmem_meta;
extern map<string, vector<vector<int>>> cp_meta;
extern bool isPrimary;
extern int secondary_fd;
extern bool opt_v;


LRUCache::LRUCache(int capacity) {
    this->capacity = capacity;
    this->memory_used = 0;
    tail->prev = head;
    head->next = tail;
}

/* Sends the server message to the client. */
void LRUCache::servermsg(const char* msg, int comm_fd) {
    send(comm_fd, msg, strlen(msg), 0);

    // if -v
    if (opt_v) {
        print_time();
        fprintf(stderr, "[%d] S: %s", comm_fd, msg);
    }
}

/* If -v, show memory status. */
void LRUCache::print_memory_status(int value_size, int comm_fd) {

    string memory_status = "Memory used: " + to_string(memory_used) + " Value size: " + to_string(value_size) + " Capacity: " + to_string(capacity);
    
    if (opt_v) {
        print_time();
        fprintf(stderr, "[%d] S: %s\n", comm_fd, memory_status.c_str());
    }
}

void LRUCache::move_to_tail(Node *current) {
    current->prev = tail->prev;
    tail->prev = current;
    current->prev->next = current;
    current->next = tail;
}

void LRUCache::clear_metadata(string type, string user) {

    if (type.compare("virtual memory") == 0) {
        virmem_meta.erase(user);
    }

    if (type.compare("checkpointing") == 0) {
        cp_meta.erase(user);
    } 
}

void LRUCache::update_metadata(string type, string user, string str, int size) {
    string dest = type + "/chunk" + to_string(chunk_info[type].at(0));
    write_file(dest, str);
    vector<int> record;
    record.push_back(chunk_info[type].at(0)); // #chunk
    record.push_back(chunk_info[type].at(1)); // start index
    record.push_back(size); // length

    if (type.compare("virtual memory") == 0) {
        virmem_meta[user].push_back(record);
    }

    if (type.compare("checkpointing") == 0) {
        cp_meta[user].push_back(record);
    } 

    chunk_info[type].at(1) += size;
}

int LRUCache::format_node(Node *user_node, string &row) {   
    int value_size = 0;
    for (map<string, string>::iterator it = user_node->value.begin(); it != user_node->value.end(); ++it) {
        string line = it->first + "," + it->second + ","; // value should be binary here, easy to parse with comma when read back to memory
        row += line;
        value_size += it->second.size();
    }
    return value_size;
}

void LRUCache::write_to_chunk(string user, string row, string type) { //options for type: virtual memory / checkpointing
    
    // clear the existent meta first
    clear_metadata(type, user);

    // update meta
    if (CHUNK_SIZE - chunk_info[type].at(1) >= row.size()) {
        update_metadata(type, user, row, row.size());
    } else {
        // how many chunks needed
        int current_chunk_left = CHUNK_SIZE - chunk_info[type].at(1);
        int chunk_needed = ceil(float(row.size() - current_chunk_left) / float(CHUNK_SIZE)) + 1;
        int start = 0;
        for (int i = 0; i < chunk_needed; i++) {
            if (i == 0) {
                string temp = row.substr(start, current_chunk_left);
                update_metadata(type, user, temp, current_chunk_left);
                start = current_chunk_left;
            } else if (i == chunk_needed - 1) {
                string temp = row.substr(start, row.size() - start);
                chunk_info[type].at(0) += 1;
                update_metadata(type, user, temp, row.size() - start);
            } else {
                string temp = row.substr(start, CHUNK_SIZE);
                chunk_info[type].at(0) += 1;
                update_metadata(type, user, temp, CHUNK_SIZE);
                start += CHUNK_SIZE;
            }
        }
    }
}

void LRUCache::use_virtual_memory() { 
    Node *lru_user = head->next;
    string row = "";
    int size = format_node(lru_user, row);
    write_to_chunk(lru_user->key, row, "virtual memory");
    tablets.erase(head->next->key);
    head->next = head->next->next;
    head->next->prev = head;
    memory_used -= size;
}

void LRUCache::restore_user(string user, string s, int comm_fd) {
    string delimiter = ",";  // fn_value string is ended with ","
    int counter = 0;

    size_t pos = 0;
    string token;
    string filename;
    string value;
    while ((pos = s.find(delimiter)) != string::npos) {
        counter += 1;
        if (counter % 2 != 0) {
            filename = s.substr(0, pos);
        } else {
            value = s.substr(0, pos);
            put(user, filename, value, comm_fd, false, 0);
        }
        s.erase(0, pos + delimiter.length());
    }
}

void LRUCache::load_user_from_disk(string type, string user, int comm_fd) {

    // extract info from metadata, then delete this user from meta so that he can be inserted as a new user
    vector<vector<int>> user_meta;

    if (type.compare("virtual memory") == 0) {
        user_meta = virmem_meta[user];
        map<string,vector<vector<int>>>::iterator it = virmem_meta.find(user);
        virmem_meta.erase(it);
    }

    if (type.compare("checkpointing") == 0) {
        user_meta = cp_meta[user];
    }
     
    string fn_value;
    for (int i = 0; i < user_meta.size(); i++) {
        vector<int> temp = user_meta.at(i);
        int chunk_id = temp.at(0);
        int start = temp.at(1);
        int length = temp.at(2);
        string path;
        if (type.compare("virtual memory") == 0) {
            path = "virtual memory/chunk" + to_string(chunk_id);
        }
        if (type.compare("checkpointing") == 0) {
            path = "checkpointing/chunk" + to_string(chunk_id);
        }
        
        ifstream file(path);
        string s;
        if(file.is_open()) {
            file.seekg(start);
            s.resize(length);
            file.read(&s[0], length);
        }
        fn_value += s;
    }
    restore_user(user, fn_value, comm_fd);
}

bool LRUCache::put_helper(string user, string filename, string value, int comm_fd, bool external, bool isCPUT, int seq_num) {

    print_memory_status(value.size(), comm_fd);

	//check single user size
	if (external || isCPUT) {
		if (user_size[user] + value.size() >= capacity) {
	        const char* feedback = "-ERR Your storage exceeds node capacity\r\n";
	        servermsg(feedback, comm_fd);
	        return true;
		}
	}

    if (memory_used + value.size() <= capacity) {

        bool isNotInMemory = (tablets.find(user) == tablets.end());
        bool isNotInVirMem = (virmem_meta.find(user) == virmem_meta.end());

        // user not exists
        if (isNotInMemory && isNotInVirMem) {
            Node *insert = new Node(user);
            insert->value[filename] = value;
            tablets[user] = insert;
            move_to_tail(insert);
        }

        // user in memory
        if (!isNotInMemory) {
            if (isCPUT) {
                tablets[user]->value[filename] = value;  // rewrite
            } else {
                tablets[user]->value[filename] += value; // append
            }
            move_to_tail(tablets[user]);
        }

        // uesr in virtual memory
        if (!isNotInVirMem) {
            load_user_from_disk("virtual memory", user, comm_fd); // but not delete this info in the disk cuz file tranlation, better to have another process handling disk space management issue.
            tablets[user]->value[filename] += value;
            move_to_tail(tablets[user]);
        }

        // update used memory
        memory_used += value.size();

        if (external || isCPUT) {
        	user_size[user] += value.size();
        }

        if (external) {
            if (isPrimary) {
                if (isCPUT) {
                    const char* feedback = "+OK New value stored\r\n";
                    servermsg(feedback, comm_fd);
                } else {
                    const char* feedback = "+OK Value stored\r\n";
                    servermsg(feedback, comm_fd);
                } 
            } else {
                string report = "DONE " + user + "," + to_string(seq_num) + "\r\n";
                send(comm_fd, report.c_str(), report.size(), 0); 
            }                      
        }
        return true;
    }
    return false;
}

bool LRUCache::get_helper(string user, string filename, string &value, int comm_fd) {

    bool isInMemory = (tablets.find(user) != tablets.end());
    bool isInVirMem = (virmem_meta.find(user) != virmem_meta.end());

    if (isInMemory || isInVirMem) {

        if (isInVirMem) {
            load_user_from_disk("virtual memory", user, comm_fd);
        }

        // remove current
        Node *current = tablets[user];
        current->prev->next = current->next;
        current->next->prev = current->prev;

        // move current to tail
        move_to_tail(current);

        if (tablets[user]->value.find(filename) != tablets[user]->value.end()) {
            value = tablets[user]->value[filename];
            return true;

        } else {
            const char* nosuchfile = "-ERR No such file\r\n";
            servermsg(nosuchfile, comm_fd);
            return false;
        }

    } else {
        const char* feedback = "-ERR User not exist\r\n";
        servermsg(feedback, comm_fd);
        return false;
    }
}

void LRUCache::put(string user, string filename, string value, int comm_fd, bool external, int seq_num) {
    if (value.size() > capacity) {
        const char* feedback = "-ERR File size exceeds node capacity\r\n";
        servermsg(feedback, comm_fd);
        return;
    }
    while (!put_helper(user, filename, value, comm_fd, external, false, seq_num)) {
        use_virtual_memory();
    }
}

void LRUCache::get(string user, string filename, int comm_fd) {
    string value_read;
    if (get_helper(user, filename, value_read, comm_fd)) {
        const char* feedback = "+OK value as follows: \r\n";
        servermsg(feedback, comm_fd);
        send(comm_fd, value_read.c_str(), value_read.size(), 0);

        // If -v
        if (opt_v) {
            print_time();
            fprintf(stderr, "[%d] S: %s", comm_fd, value_read.c_str());
        }
    }
}

void LRUCache::cput(string user, string filename, string old_value, string new_value, int comm_fd, int seq_num) {
    string value_read;
    if (get_helper(user, filename, value_read, comm_fd)) { // get will update the user's position in the linked list
        if (value_read.compare(old_value) == 0) {
        	user_size[user] -= old_value.size();
            memory_used -= old_value.size();
            put_helper(user, filename, new_value, comm_fd, true, true, seq_num);
        } else {
            const char* versioncheckfail = "-ERR Version check fail\r\n";
            servermsg(versioncheckfail, comm_fd);
        }
    }
}

void LRUCache::dele(string user, string filename, int comm_fd, int seq_num) {
    string value_read;
    if (get_helper(user, filename, value_read, comm_fd)) { // get will update the user's position in the linked list
    	user_size[user] -= tablets[user]->value[filename].size();
        memory_used -= tablets[user]->value[filename].size();
        tablets[user]->value.erase(filename);
        if (isPrimary) {
            const char* successmsg = "+OK File deleted\r\n";
            servermsg(successmsg, comm_fd);
        } else {
            string report = "DONE " + user + "," + to_string(seq_num) + "\r\n";
            send(comm_fd, report.c_str(), report.size(), 0);
        }

        // delete the node of empty user
        if (tablets[user]->value.size() == 0) {
            tablets.erase(user);
        }
    }
}

void LRUCache::getlist(string user, string type, int comm_fd){
    int counter = 0;
    for (auto it: tablets[user]->value) {
        string filename = it.first;     
        if ((type.compare("email") == 0 && filename.substr(0,2).compare("##") == 0) ||
            (type.compare("file") == 0 && filename.substr(0,2).compare("##") != 0)) {
            string value_read;
            if (get_helper(user, filename, value_read, comm_fd)) { // get will update the user's position in the linked list
                if (counter == 0) {
                    const char* feedback = "+OK list as follows: \r\n";
                    servermsg(feedback, comm_fd);
                }
                string fn_value = filename + "," + value_read + ",";
                send(comm_fd, fn_value.c_str(), fn_value.size(), 0);

                // If -v
                if (opt_v) {
                    print_time();
                    fprintf(stderr, "[%d] S: %s", comm_fd, fn_value.c_str());
                }
            }
        }
        counter += 1; 
    }
}


void LRUCache::getfile(string user, int comm_fd) {
    int counter = 0;
    for (auto it: tablets[user]->value) {
        string filename = it.first;     
        if (filename.substr(0,2).compare("##") != 0) {
            string value_read;
            if (get_helper(user, filename, value_read, comm_fd)) { // get will update the user's position in the linked list
                if (counter == 0) {
                    const char* feedback = "+OK list as follows: \r\n";
                    servermsg(feedback, comm_fd);
                }
                string fn_value = filename + ",";
                send(comm_fd, fn_value.c_str(), fn_value.size(), 0);

                // If -v
                if (opt_v) {
                    print_time();
                    fprintf(stderr, "[%d] S: %s\n", comm_fd, fn_value.c_str());
                }
            }
        }
        counter += 1; 
    }
}

void LRUCache::rename(string user, string old_filename, string new_filename, int comm_fd, int seq_num) {
    string value_read;
    if (get_helper(user, old_filename, value_read, comm_fd)) { // get will update the user's position in the linked list
        
        tablets[user]->value.erase(old_filename);
        tablets[user]->value[new_filename] = value_read;
        
        if (isPrimary) {
            const char* successmsg = "+OK File renamed\r\n";
            servermsg(successmsg, comm_fd);
        } else {
            string report = "DONE " + user + "," + to_string(seq_num) + "\r\n";
            send(comm_fd, report.c_str(), report.size(), 0);
        }
    }
}