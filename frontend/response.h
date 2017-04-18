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
  bool is_login;

public:
  Response(Request req);
  void reply(int fd);

private:
  void reg(Request req);
  void login(Request req);
  void upload(Request req);
  void handle_upload(Request req);
  void download(Request req);
};

#endif
