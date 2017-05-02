#ifndef TOOLS_H
#define TOOLS_H

#include <string.h>
#include <vector>
#include <map>

using namespace std;

void write_file(string dest, char* value);

void write_file(string dest, string value);

string read_file(string path);

void rewrite_file(string user, string filename, string value);

string string_to_binary(string value);

string binary_to_string(string binary_val);

void create_dir(string dir);

bool check_dir(string user);

bool check_file(string filename, vector<string> files, int num_of_files);

void delete_file(string user, string filename, int comm_fd);

void read_dir(string user, vector<string> &files, int &num_of_files);

void clear_dir(string directory);

void print_time();

#endif
