#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <bits/stdc++.h>
#include <ctype.h>
#include <csignal>
#include <signal.h>
#include <algorithm>
#include <vector>
#include <dirent.h>
#include <time.h>
#include <map>
#include <experimental/filesystem>
#include <bitset>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>

#include "chunkserver.h"

using namespace std;

Chunkserver::Chunkserver(int num){
	opt_v = num;
}

/* Sends the server message to the client. */
void Chunkserver::servermsg(const char* msg, int comm_fd)
{	
	send(comm_fd, msg, strlen(msg), 0);

	/* If option 'v' is given. */
	if (opt_v) {
		fprintf(stderr, "[%d] S: %s", comm_fd, msg);
	}
}

/* Sends the ERROR message to the client. */
void Chunkserver::error(int comm_fd)
{
	const char *errormsg = "-ERR Not supported\r\n";
	send(comm_fd, errormsg, strlen(errormsg), 0);

	/* If option 'v' is given. */
	if (opt_v) {
		fprintf(stderr, "[%d] S: %s", comm_fd, errormsg);
	}
}

void Chunkserver::parse_line(string s, vector<string> &arguments, int num_of_args)
{
    string delimiter = ",";  //TODO: what if there are "," in v1 when CPUT r,c,v1,v2 ? maybe use binary for v1
    int counter = 0;

    size_t pos = 0;
    string token;
    while ((pos = s.find(delimiter)) != string::npos && counter != num_of_args - 1) {
        arguments.push_back(s.substr(0, pos));
        s.erase(0, pos + delimiter.length());
        counter ++;
    }
    arguments.push_back(s);
}

/* Write value to the file under user's directory. (append) */
void Chunkserver::write_to_file(string user, string filename, string value)
{
	string path = "storage/" + user + "/" + filename;
	ofstream ofs;
	ofs.open(path, ofstream::out | ofstream::app); // append
	ofs << value;
	ofs.close();
}

/* Read file contents to a string. */
string Chunkserver::read_file(string user, string filename)
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
void Chunkserver::rewrite_file(string user, string filename, string value)
{
	string path = "storage/" + user + "/" + filename;
	ofstream ofs;
	ofs.open(path, ofstream::out | ofstream::trunc);	// rewrite, in contrast to append
	ofs << value;
	ofs.close();
}

/* Transfer string to binary. */
string Chunkserver::string_to_binary(string value)
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
string Chunkserver::binary_to_string(string binary_value)
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
void Chunkserver::create_user_dir(string user)
{
	string dir = "storage/" + user;
	int status;
	status = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

/* Check whether this user exists.*/
bool Chunkserver::check_user_dir(string user)
{
	string directory = "storage/" + user;
	DIR *dir;
	struct dirent *ent;

	if ((dir = opendir (directory.c_str())) != NULL) {
		closedir (dir);
		return true;
	} else return false;
}

/* Check whether this file exists. */
bool Chunkserver::check_file(string filename, vector<string> files, int num_of_files)
{
	for (int i = 0; i < num_of_files; i++) {
		if (files[i].compare(filename) == 0){
			return true;
		}
	}
	return false;
}

/* Delete the file. */
void Chunkserver::delete_file(string user, string filename, int comm_fd)
{
	string path = "storage/" + user + "/" + filename;
	if(remove(path.c_str()) != 0 ) {
	 	const char* failmsg = "-ERR Delete failed\r\n";
		servermsg(failmsg, comm_fd);
	} else {
	 	const char* successmsg = "+OK File deleted\r\n";
		servermsg(successmsg, comm_fd);
	}
}

void Chunkserver::read_directory(string user, vector<string> &files, int &num_of_files)
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

/* At this stage, we write everything into disk, should think of a way 
   maximizing the usage of memory to improve performance.
   For now, the name of the directory is the username; 
   the filename is the actual filename and the value is the contents 
   of the file. */
void Chunkserver::store_value(string user, string filename, string value, int comm_fd)
{
	create_user_dir(user);
	string binary_val = string_to_binary(value);
	write_to_file(user, filename, binary_val); // Do we need to check if file with the given name already exists?
	const char* feedback = "+OK Value stored\r\n";
	servermsg(feedback, comm_fd);
}

/* Get the value with given user and filename.  */
void Chunkserver::get_value(string user, string filename, int comm_fd)
{
	if (check_user_dir(user)) {
		vector<string> files;
		int num_of_files;
		read_directory(user, files, num_of_files);
		if (check_file(filename, files, num_of_files)) {

			const char* feedback = "+OK value as follows: \r\n";
			servermsg(feedback, comm_fd);

			string binary_value = read_file(user, filename);
			string value = binary_to_string(binary_value);
			send(comm_fd, value.c_str(), value.size() + 1, 0);

			/* If option 'v' is given. */
			if (opt_v) {
				fprintf(stderr, "[%d] S: %s", comm_fd, value.c_str());
			}

		} else {
			const char* nosuchfile = "-ERR No such file\r\n";
			servermsg(nosuchfile, comm_fd);
		}
	} else {
		const char* feedback = "-ERR User not exist\r\n";
		servermsg(feedback, comm_fd);
	}
}

/* Compare the old values and write new value to the file of given user.  */
void Chunkserver::conditional_store(string user, string filename, string old_value, string new_value, int comm_fd)
{
	if (check_user_dir(user)) {
		vector<string> files;
		int num_of_files;
		read_directory(user, files, num_of_files);
		if (check_file(filename, files, num_of_files)) {
			string binary_value = read_file(user, filename);
			string value_read = binary_to_string(binary_value);
			if (value_read.compare(old_value + '\0') == 0) {
				string new_binary_value = string_to_binary(new_value);
				rewrite_file(user, filename, new_binary_value);
				const char* feedback = "+OK New value stored\r\n";
				servermsg(feedback, comm_fd);
			} else {
				const char* versioncheckfail = "-ERR Version check fail\r\n";
				servermsg(versioncheckfail, comm_fd);
			}

		} else {
			const char* nosuchfile = "-ERR No such file\r\n";
			servermsg(nosuchfile, comm_fd);
		}
	} else {
		const char* feedback = "-ERR User not exist\r\n";
		servermsg(feedback, comm_fd);
	}
}

/* Delete the file with given username and filename. */
void Chunkserver::dele_value(string user, string filename, int comm_fd)
{
	if (check_user_dir(user)) {
		vector<string> files;
		int num_of_files;
		read_directory(user, files, num_of_files);
		if (check_file(filename, files, num_of_files)) {
			delete_file(user, filename, comm_fd);   // if the user folder is empty after the deletion, do we keep the user directory?
		} else {
			const char* nosuchfile = "-ERR No such file\r\n";
			servermsg(nosuchfile, comm_fd);
		}
	} else {
		const char* feedback = "-ERR User not exist\r\n";
		servermsg(feedback, comm_fd);
	}
}

/* VPUT r,c,v */
void Chunkserver::vput(char* &line, int comm_fd)
{
	char *arguments = new char[strlen(line) - 5 + 1];
	strncpy(arguments, line + 5, strlen(line) - 5);
	arguments[strlen(line) - 5] = '\0';

	string to_be_parsed(arguments);

	vector<string> rcv;
	parse_line(to_be_parsed, rcv, 3);
	if (rcv.size() == 3) {
		string user = rcv.at(0);
		string filename = rcv.at(1);
		string value = rcv.at(2);
		store_value(user, filename, value, comm_fd);
	} else {
		error(comm_fd);
	}
	delete [] arguments;
}

/* VGET r,c */
void Chunkserver::vget(char *line, int comm_fd)
{
	char *arguments = new char[strlen(line) - 5 + 1];
	strncpy(arguments, line + 5, strlen(line) - 5);
	arguments[strlen(line) - 5] = '\0';

	string to_be_parsed(arguments);

	vector<string> rc;
	parse_line(to_be_parsed, rc, 2);
	if (rc.size() == 2) {
		string user = rc.at(0);
		string filename;
		filename.assign(rc.at(1), 0, rc.at(1).size() - 2);
		// Get the value in the corresponding row and colummn
		get_value(user, filename, comm_fd);
	} else {
		error(comm_fd);
	}
	delete [] arguments;
}

/* CPUT r,c,v1,v2 */
void Chunkserver::cput(char* &line, int comm_fd)
{
	char *arguments = new char[strlen(line) - 5 + 1];
	strncpy(arguments, line + 5, strlen(line) - 5);
	arguments[strlen(line) - 5] = '\0';

	string to_be_parsed(arguments);

	vector<string> rcv2;
	parse_line(to_be_parsed, rcv2, 4);
	if (rcv2.size() == 4) {
		string user = rcv2.at(0);
		string filename = rcv2.at(1);
		string old_value = rcv2.at(2) + "\r\n";
		string new_value = rcv2.at(3);
		// Don't support "," in old_value now, old_value may be better transfered as binary
		conditional_store(user, filename, old_value, new_value, comm_fd);
	} else {
		error(comm_fd);
	}
	delete [] arguments;
}

/* DELE r,c */
void Chunkserver::dele(char* &line, int comm_fd)
{
	char *arguments = new char[strlen(line) - 5 + 1];
	strncpy(arguments, line + 5, strlen(line) - 5);
	arguments[strlen(line) - 5] = '\0';

	string to_be_parsed(arguments);

	vector<string> rc;
	parse_line(to_be_parsed, rc, 2);
	if (rc.size() == 2) {
		string user = rc.at(0);
		string filename;
		filename.assign(rc.at(1), 0, rc.at(1).size() - 2);
		//Get the value in the corresponding row and colummn
		dele_value(user, filename, comm_fd);
	} else {
		error(comm_fd);
	}
	delete [] arguments;
}
