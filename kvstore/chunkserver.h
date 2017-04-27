#ifndef CHUNKSERVER_H
#define CHUNKSERVER_H

#include <string.h>
#include <vector>
#include <map>

#include "lrucache.h"

using namespace std;

class Chunkserver {

public: 
    int opt_v;
    int capacity;
    LRUCache *lru;
    
public:
  	Chunkserver(int v, int size);
    void put(char* &line, int comm_fd);
    void get(char *line, int comm_fd);
    void cput(char* &line, int comm_fd);
    void dele(char* &line, int comm_fd);
    void error(int comm_fd);

private:
    void parse_line(string s, vector<string> &arguments, int num_of_args);
    void write_log(char* request); // force write
    void replay_log();
    void checkpointing(); 
    void load_checkpointing();

};

#endif
