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

private:
  void reg(Request req);
  void login(Request req);
  bool is_already_login(map<string, string> cookies, string &username);
  void upload(Request req);
  void handle_upload(Request req);
  void download(Request req);
  void file(const char* filename);
  void create_new_folder(Request req);
  void rename_file(Request req);
  void delete_file(Request req);
  void delete_folder(Request req);
  void move_file(Request req);
  void download_file(Request req);
  void send_email(Request req);
  void inbox(Request req);
  void view_email(Request req);
  void handle_send_email(Request req);
};

#endif
