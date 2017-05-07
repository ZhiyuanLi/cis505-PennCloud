#ifndef LRUCACHE_H
#define LRUCACHE_H

#include <string.h>
#include <map>

#include "node.h"

using namespace std;

/* Least recently used user */

class LRUCache {

public:
    int capacity;
    int memory_used;
    Node *head = new Node();
    Node *tail = new Node();
    map<string, Node*> tablets; // key: user (in memory)
                                // value: (filename, value) pairs
    map<string, int> user_size; // key: user (both in memory and virtual memory)
                                // value: user size
    
public:
    LRUCache(int capacity);   
    void put(string user, string filename, string value, int comm_fd, bool external, int seq_num);
    void get(string user, string filename, int comm_fd);
    void cput(string user, string filename, string old_value, string new_value, int comm_fd, int seq_num);
    void getlist(string user, string type, int comm_fd);
    void dele(string user, string filename, int comm_fd, int seq_num);
    void servermsg(const char* msg, int comm_fd);
    int format_node(Node *user_node, string &row);  
    void write_to_chunk(string user, string row, string type);
    void load_user_from_disk(string type, string user, int comm_fd);

private:
    void move_to_tail(Node *current);
    bool put_helper(string user, string filename, string value, int comm_fd, bool external, bool isCPUT, int seq_num);
    bool get_helper(string user, string filename, string &value, int comm_fd);
    void clear_metadata(string type, string user);
    void update_metadata(string type, string user, string str, int size);   
    void use_virtual_memory();
    void restore_user(string user, string s, int comm_fd);    
    void print_memory_status(int value_size, int comm_fd);
    
};

#endif
