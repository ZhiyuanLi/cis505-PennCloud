#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "helper.h"
#include "request.h"

using namespace std;

/*read and parse one request*/
Request::Request(int fd) {
  string line = read_line(fd);
  if (line.empty()) {
    valid = false;
    return;
  }
  while (!line.empty()) {
    debug(1, line.c_str());
    debug(1, "\r\n");
    line = read_line(fd);
  }
  valid = true;
}
