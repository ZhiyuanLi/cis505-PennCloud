#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <bits/stdc++.h>
#include <ctype.h>
#include <csignal>
#include <signal.h>
#include <vector>
#include <dirent.h>
#include <time.h>
#include <map>
#include <bitset>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>

#include "tools.h"

using namespace std;

/* Write char to the dest. (append) */
void write_file(string dest, char* value)
{
	ofstream ofs;
	ofs.open(dest, ofstream::out | ofstream::app); // append
	ofs << value;
	ofs.close();
}

/* Write char to the dest. (append) */
void write_file(string dest, string value)
{
	ofstream ofs;
	ofs.open(dest, ofstream::out | ofstream::app); // append
	ofs << value;
	ofs.close();
}

/* Read file contents to a string. */
string read_file(string user, string filename)
{
	string path = "storage/" + user + "/" + filename;
	string content;
	string line;
	ifstream myfile (path);
	if (myfile.is_open()) {
		while ( getline (myfile,line) ) {
			content += line;
		}
		myfile.close();
	} else cout << "Unable to open file";
	return content;
}

/* Rewrite the new value to current user's file. */
void rewrite_file(string user, string filename, string value)
{
	string path = "storage/" + user + "/" + filename;
	ofstream ofs;
	ofs.open(path, ofstream::out | ofstream::trunc);	// rewrite, in contrast to append
	ofs << value;
	ofs.close();
}

/* Transfer string to binary. */
string string_to_binary(string value)
{
	string binary_output;

	char* data = new char [value.size() + 1];
	strcpy(data, value.c_str());
	data[value.size()] = '\0';

	for (size_t i = 0; i < strlen(data); i++) {
		bitset<8> b(data[i]);
		binary_output += b.to_string();
	}
	delete [] data;
	return binary_output;
}

/* Transfer binary to string. */
string binary_to_string(string binary_value)
{
    stringstream sstream(binary_value);
    string output;
    while(sstream.good())
    {
        bitset<8> bits;
        sstream >> bits;
        char c = char(bits.to_ulong());
        output += c;
    }
    return output;
}

/* Create a directory named after user if not exist already. */
void create_dir(string dir)
{
    int status;
    status = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

/* Check whether this user exists.*/
bool check_dir(string path)
{
	DIR *dir;
	struct dirent *ent;

	if ((dir = opendir (path.c_str())) != NULL) {
		closedir (dir);
		return true;
	} else return false;
}

/* Check whether this file exists. */
bool check_file(string filename, vector<string> files, int num_of_files)
{
	for (int i = 0; i < num_of_files; i++) {
		if (files[i].compare(filename) == 0){
			return true;
		}
	}
	return false;
}

/* Delete the file. */
void delete_file(string user, string filename, int comm_fd)
{
	string path = "storage/" + user + "/" + filename;
	if(remove(path.c_str()) != 0 ) {
	 	cout << "-ERR Delete failed" << endl;
	} else {
	 	cout << "+OK File deleted" << endl;
	}
}

void read_directory(string user, vector<string> &files, int &num_of_files)
{
	string directory = "storage/" + user;
	DIR *dir;
	struct dirent *ent;

	if ((dir = opendir (directory.c_str())) != NULL) {
		/* Stores the names of all files in a vector. */
		num_of_files = 0;
		while ((ent = readdir (dir)) != NULL) {
			if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
				continue;
			}
			files.push_back(ent->d_name);
			num_of_files += 1;
		}
		closedir (dir);
	} else {
	  fprintf(stderr, "Unable to open the directory\n");
	  exit(-1);
	}
}