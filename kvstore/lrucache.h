#ifndef LRUCACHE_H
#define LRUCACHE_H

#include <string.h>
#include <map>

#include "node.h"

using namespace std;

/* Least recently used user */

class LRUCache {

public:
    int opt_v;
    int capacity;
    int memory_used;
    Node *head = new Node();
    Node *tail = new Node();
    map<string, Node*> tablets;
    
public:
    LRUCache(int v, int capacity);   
    void put(string user, string filename, string value, int comm_fd);
    void get(string user, string filename, int comm_fd);
    void cput(string user, string filename, string old_value, string new_value, int comm_fd);
    void dele(string user, string filename, int comm_fd);
    void servermsg(const char* msg, int comm_fd);

private:
    void move_to_tail(Node *current);
    bool put_helper(string user, string filename, string value, int comm_fd);
    bool get_helper(string user, string filename, string &value, int comm_fd);
    void update_virmem_metadata(string user, string str, int size);
    int write_lru_user_to_disk(Node *lru_user);
    void use_virtual_memory();
    
};

#endif
