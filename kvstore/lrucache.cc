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
#include "tools.h"

extern map<string, vector<int>> chunk_info;
extern map<string, vector<vector<int>>> virmem_meta; 
// virtual memory part is not finished at this stage, please test with 500M in-memory space

LRUCache::LRUCache(int v, int capacity) {
    this->opt_v = v;
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
        fprintf(stderr, "[%d] S: %s", comm_fd, msg);
    }
}

void LRUCache::move_to_tail(Node *current) {
    current->prev = tail->prev;
    tail->prev = current;
    current->prev->next = current;
    current->next = tail;
}

void LRUCache::update_virmem_metadata(string user, string str, int size) {
    string dest = "virtual memory/chunk" + to_string(chunk_info["virtual memory"].at(0));
    write_file(dest, str);
    vector<int> record;
    record.push_back(chunk_info["virtual memory"].at(0)); // #chunk 
    record.push_back(chunk_info["virtual memory"].at(1)); // start index
    record.push_back(size); // length
    virmem_meta[user].push_back(record);
    chunk_info["virtual memory"].at(1) += size;
}

int LRUCache::write_lru_user_to_disk(Node *lru_user) {
    string user = lru_user->key;
    string row = user + ",";
    int value_size = 0;
    for (map<string, string>::iterator it = lru_user->value.begin(); it != lru_user->value.end(); ++it) {
        string line = it->first + "," + it->second + ","; // value should be binary here, easy to parse with comma when read back to memory
        row += line;
        value_size += it->second.size();
    }
    if (CHUNK_SIZE - chunk_info["virtual memory"].at(1) >= row.size()) {    
        update_virmem_metadata(user, row, row.size());    
    } else {
        // how many chunks needed
        int current_chunk_left = CHUNK_SIZE - chunk_info["virtual memory"].at(1);
        int chunk_needed = ceil(float(row.size() - current_chunk_left) / float(CHUNK_SIZE)) + 1;
        int start = 0;
        for (int i = 0; i < chunk_needed; i++) {
            if (i == 0) {
                string temp = row.substr(start, current_chunk_left);
                update_virmem_metadata(user, temp, current_chunk_left);
                start = current_chunk_left;
            } else if (i == chunk_needed - 1) {
                string temp = row.substr(start, row.size() - start);
                chunk_info["virtual memory"].at(0) += 1;
                update_virmem_metadata(user, temp, row.size() - start);
            } else {
                string temp = row.substr(start, CHUNK_SIZE);
                chunk_info["virtual memory"].at(0) += 1;
                update_virmem_metadata(user, temp, CHUNK_SIZE);
                start += CHUNK_SIZE;
            }
        }
    } 
    return value_size;
}

void LRUCache::use_virtual_memory() {
    Node *lru_user = head->next;
    int size = write_lru_user_to_disk(lru_user);
    tablets.erase(head->next->key);
    head->next = head->next->next;
    head->next->prev = head;
    memory_used -= size;
}

bool LRUCache::put_helper(string user, string filename, string value, int comm_fd) {

    if (memory_used + value.size() <= capacity) {

        bool isNotInMemory = tablets.find(user) == tablets.end();
        bool isNotInVirMem = virmem_meta.find(user) == virmem_meta.end();

        if (isNotInMemory && isNotInVirMem) {  // check both in-memory and virtual memory
            Node *insert = new Node(user); // user not exists
            insert->value[filename] = value;
            tablets[user] = insert;
            move_to_tail(insert);

        } 

        if (!isNotInMemory) {
            tablets[user]->value[filename] += value; 
            move_to_tail(tablets[user]);          
        }

        if (!isNotInVirMem) {
            //TODO: read from chunk based on metadata, update virmem_meta, delete on disk, store in temp in memory with new value
            //then, call put: put will write other user into disk   
        }

        memory_used += value.size();
        const char* feedback = "+OK Value stored\r\n";
        servermsg(feedback, comm_fd);
        return true;
    }
    return false;
}

bool LRUCache::get_helper(string user, string filename, string &value, int comm_fd) {

    if (tablets.find(user) != tablets.end()) {

        // remove current
        Node *current = tablets[user];
        current->prev->next = current->next;
        current->next->prev = current->prev;

        // move current to tail
        move_to_tail(current);

        if (tablets[user]->value.find(filename) != tablets[user]->value.end()) {
            value = current->value[filename];
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

void LRUCache::put(string user, string filename, string value, int comm_fd) {
    while (!put_helper(user, filename, value, comm_fd)) {
        use_virtual_memory();
    }
}

void LRUCache::get(string user, string filename, int comm_fd) {
    string value_read;
    if (get_helper(user, filename, value_read, comm_fd)) {
        const char* feedback = "+OK value as follows: \r\n";
        servermsg(feedback, comm_fd);
        send(comm_fd, value_read.c_str(), value_read.size() + 1, 0);

        // If -v
        if (opt_v) {
            fprintf(stderr, "[%d] S: %s", comm_fd, value_read.c_str());
        }
    }
}

void LRUCache::cput(string user, string filename, string old_value, string new_value, int comm_fd) {
    string value_read;
    if (get_helper(user, filename, value_read, comm_fd)) { // get will update the user's position in the linked list
        if (value_read.compare(old_value) == 0) {
            tablets[user]->value[filename] = new_value;
            const char* feedback = "+OK New value stored\r\n";
            servermsg(feedback, comm_fd);
        } else {
            const char* versioncheckfail = "-ERR Version check fail\r\n";
            servermsg(versioncheckfail, comm_fd);
        }
    }
}

void LRUCache::dele(string user, string filename, int comm_fd) {
    string value_read;
    if (get_helper(user, filename, value_read, comm_fd)) { // get will update the user's position in the linked list
        tablets[user]->value.erase(filename);
        const char* successmsg = "+OK File deleted\r\n";
        servermsg(successmsg, comm_fd);
    }
}
