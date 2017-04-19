#ifndef CHUNKSERVER_H
#define CHUNKSERVER_H

#include <string.h>
#include <vector>
#include "chunkserver.h"

using namespace std;

class Chunkserver {
    
  int opt_v;

  public:
  	Chunkserver(int num);
    void vput(char* &line, int comm_fd);
    void vget(char *line, int comm_fd);
    void cput(char* &line, int comm_fd);
    void dele(char* &line, int comm_fd);
    void servermsg(const char* msg, int comm_fd);
    void error(int comm_fd);

  private:
    void parse_line(string s, vector<string> &arguments, int num_of_args);
    void write_to_file(string user, string filename, string value);
    string read_file(string user, string filename);
    void rewrite_file(string user, string filename, string value);
    string string_to_binary(string value);
    string binary_to_string(string binary_val);
    void create_user_dir(string user);
    bool check_user_dir(string user);
    bool check_file(string filename, vector<string> files, int num_of_files);
    void delete_file(string user, string filename, int comm_fd);
    void read_directory(string user, vector<string> &files, int &num_of_files);
    void store_value(string user, string filename, string value, int comm_fd);
    void get_value(string user, string filename, int comm_fd);
    void conditional_store(string user, string filename, string old_value, string new_value, int comm_fd);
    void dele_value(string user, string filename, int comm_fd);
};

#endif
