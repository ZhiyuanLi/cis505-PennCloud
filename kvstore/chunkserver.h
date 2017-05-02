#ifndef CHUNKSERVER_H
#define CHUNKSERVER_H

#include <string.h>
#include <vector>
#include <map>

#include "lrucache.h"

using namespace std;

class Chunkserver {

public: 
    int capacity;
    LRUCache *lru;
    
public:
  	Chunkserver(int size);
    void put(char* &line, int comm_fd);
    void get(char *line, int comm_fd);
    void cput(char* &line, int comm_fd);
    void dele(char* &line, int comm_fd);
    void error(int comm_fd);
    void checkpointing(); 
    void load_checkpointing();

private:
    void parse_line(string s, vector<string> &arguments, int num_of_args);
    void write_log(char* request); // force write
    void replay_log();   
    void write_cp_meta();
    void write_virmem_meta();
    void write_chunk_info();
    void write_user_size();
    vector<int> rebuild_vector(string s);
    void parse_meta(string s, string type);
    void load_cp_vm_meta(string path, string type);
    void load_chunk_info();
    void load_user_size();

};

#endif
