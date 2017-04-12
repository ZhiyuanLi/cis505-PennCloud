#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <unistd.h>

#include "helper.h"

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
