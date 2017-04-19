#ifndef HELPER_H
#define HELPER_H

#include <string>

using namespace std;

/*wrapper function for fprintf, which takes in variable arguments and prints if
 * debug is enabled*/
void debug(int vflag, const char *format, ...);

/* read len from fd to buffer*/
int do_read(int fd, char *buf, int len);

/* write buffer with len into fd*/
int do_write(int fd, char *buf, int len);

/* read a line from a fd, till \r\n or end of file*/
string read_line(int fd);

/* split a string by delimiter */
vector<string> split(const string &s, char delim);

/* extract uploaded file content from request body */
string extract_file_content(string req_body);

/* extract uploaded file name from request body */
string extract_file_name(string req_body);

/* create and store uploaded files */
void store_file(string dir, string filename, string file_content);

/* list all files under a directory */
vector<string> list_all_files(string dir);

#endif
