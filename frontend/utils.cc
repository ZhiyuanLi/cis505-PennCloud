#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <streambuf>

#include "utils.h"

using namespace std;

/*wrapper function for fprintf, which takes in variable arguments and prints if
 * debug is enabled*/
void debug(int vflag, const char *format, ...) {
  if (!vflag)
    return;
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
}

/* read len from fd to buffer*/
int do_read(int fd, char *buf, int len) {
  int rcvd = 0;
  while (rcvd < len) {
    int n = read(fd, &buf[rcvd], len - rcvd);
    if (n <= 0) {
      return rcvd;
    }
    rcvd += n;
  }
  return rcvd;
}

/* write buffer with len into fd*/
int do_write(int fd, char *buf, int len) {
  int sent = 0;
  while (sent < len) {
    int n = write(fd, &buf[sent], len - sent);
    if (n < 0) {
      return 0;
    }
    sent += n;
  }
  return 1;
}

/* read a line from a fd, till \r\n or end of file*/
string read_line(int fd) {
  string line;
  char c;

read:
  do {
    if (do_read(fd, &c, sizeof(c)) <= 0) {
      return line;
    }
    line += c;
  } while (c != '\r' && c != '\n');

  if (c == '\r') {
    if (do_read(fd, &c, sizeof(c)) <= 0) {
      return line;
    }
    if (c == '\n') { //'\r\n' end of the message
      line += c;
    } else { //'\r' not followed with '\n', read again
      goto read;
    }
  } else if (c == '\n') { // only ends with '\n'
    line = line.substr(0, line.length() - 1);
    line += '\r';
    line += '\n';
  }
  line = line.substr(0, line.length() - 2);
  return line;
}

/* split a string by delimiter */
vector<string> split(const string &s, char delim) {
  vector<string> elems;
  stringstream ss;
  ss.str(s);
  string item;
  while (getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

/* extract uploaded file content from request body */
string extract_file_content(string req_body, int content_length) {
  req_body = req_body.substr(req_body.find("\r\n\r\n") + 4);
  return req_body.substr(0, req_body.find("------WebKitFormBoundary") - 2);
  // return req_body.substr(0, content_length);
}

/* extract uploaded file name from request body */
string extract_file_name(string req_body) {
  req_body = req_body.substr(req_body.find("filename=\"") + 10);
  return req_body.substr(0, req_body.find("\""));
}

/* create and store uploaded files */
void store_file(string dir, string filename, string file_content) {
  debug(1, (dir + filename).c_str());
  ofstream outfile(dir + filename);
  outfile << file_content;
  outfile.close();
}

/* list all files under a directory */
vector<string> list_all_files(string dir) {
  DIR *file_dir;
  struct dirent *entry;
  vector<string> files;
  if ((file_dir = opendir(dir.c_str()))) {
    while ((entry = readdir(file_dir))) {
      if ((entry->d_name)[0] != '.') {
        // cout << entry->d_name << "\n";
        files.push_back(entry->d_name);
      }
    }
    closedir(file_dir);
  }
  return files;
}

/* check if a given file exists */
bool is_file_exist(const char *filename) {
    ifstream infile(filename);
    return infile.good();
}

/* get file content as string from file name */
string get_file_content_as_string(const char *filename) {
    ifstream t(filename);
    string str((istreambuf_iterator<char>(t)), istreambuf_iterator<char>());
    return str;
}

/* replace all substring in a string */
void replace_all(string& str, const string& from, const string& to) {
    if (from.empty()) {
      return;
    }

    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}
