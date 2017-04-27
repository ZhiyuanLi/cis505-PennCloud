#ifndef TOOLS_H
#define TOOLS_H

#include <string.h>
#include <vector>
#include <map>

using namespace std;

/* Assume that storage space for a single user cannot exceed this max capacity. */
long long static MAX_CAPACITY = 500 * 1024 * 1024;

long long static CHUNK_SIZE = 64 * 1024 * 1024;

void write_file(string dest, char* value);

void write_file(string dest, string value);

string read_file(string user, string filename);

void rewrite_file(string user, string filename, string value);

string string_to_binary(string value);

string binary_to_string(string binary_val);

void create_dir(string dir);

bool check_dir(string user);

bool check_file(string filename, vector<string> files, int num_of_files);

void delete_file(string user, string filename, int comm_fd);

void read_directory(string user, vector<string> &files, int &num_of_files);

#endif
