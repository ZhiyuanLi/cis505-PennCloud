#ifndef REQUEST_H
#define REQUEST_H

#include <map>
#include <string>

using namespace std;

class Request {
public:
  string method;
  string path;
  string http_version;
  int content_length;
  map<string, string> headers;
  map<string, string> cookies;
  string body;
  bool valid;

public:
  Request(int fd);
};

#endif
