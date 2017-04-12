#ifndef HELPER_H
#define HELPER_H

#include <string>

using namespace std;

/*wrapper function for fprintf, which takes in variable arguments and prints if
 * debug is enabled*/
void debug(int vflag, const char *format, ...);

/* read len from fd to buffer*/
int do_read(int fd, char *buf, int len);

/* read a line from a fd, till \r\n or end of file*/
string read_line(int fd);

#endif
