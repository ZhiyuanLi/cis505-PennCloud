#ifndef RESPONSE_H
#define RESPONSE_H

#include <map>
#include <string>

#include "request.h"

using namespace std;

class Response {
public:
  string http_version;
  string status;
  map<string, string> headers;
  string body;

public:
  Response(Request req);
  void reply(int fd);
};

#endif
