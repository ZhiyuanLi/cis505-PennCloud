#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "constants.h"
#include "utils.h"
#include "request.h"
#include "response.h"
#include "store.h"

using namespace std;

Response::Response(Request req) {
  this->http_version = req.http_version;
  (this->headers)[CONNECTION] = "close";

  // static file
  const char* filename = ("." + req.path).c_str();
  if (is_file_exist(filename)) {
      file(filename);
  }

  // register
  else if (req.path == REGISTER_URL) {
    reg(req);
  }

  // login
  else if (req.path == LOGIN_URL) {
    login(req);
  }

  // upload
  else if (req.path == UPLOAD_URL) {
    if (req.method == "GET") {
      upload(req);
    } else if (req.method == "POST") {
      handle_upload(req);
    }
  }

  // download
  else if (req.path == DOWNLOAD_URL) {
    download(req);
  }
}

void Response::reply(int fd) {
  string rep;
  rep += http_version + " " + status + "\r\n";

  for (map<string, string>::iterator it = headers.begin(); it != headers.end();
       ++it) {
    rep += it->first + it->second + "\r\n";
  }
  rep += "\r\n";
  rep += body;

  char crep[rep.length() + 1];
  strcpy(crep, rep.c_str());

  debug(1, "[%d] S: %s", fd, crep);
  do_write(fd, crep, strlen(crep));
}

/* register */
void Response::reg(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";

  // GET
  if (req.method.compare("GET") == 0) {
    this->body = get_file_content_as_string("html/register.html");
    (this->headers)[CONTENT_LEN] = to_string((this->body).length());
  }

  // POST
  else if (req.method.compare("POST") == 0) {
    vector<string> parameter_tokens = split(req.body.c_str(), '&');
    string username = split(parameter_tokens.at(0), '=').at(1);
    string password = split(parameter_tokens.at(1), '=').at(1);

    if (is_user_exist(username)) {
      this->body = get_file_content_as_string("html/user-already-exist.html");
      (this->headers)[CONTENT_LEN] = to_string((this->body).length());
    } else { // new user
      add_user(username, password);
      this->body = get_file_content_as_string("html/new-user.html");
      (this->headers)[CONTENT_LEN] = to_string((this->body).length());
    }
  }
}

/* login */
void Response::login(Request req) {
  string username;
  bool already_login = is_already_login(req.cookies, username);

  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";

  // GET
  if (req.method.compare("GET") == 0 && !already_login) {
    this->body = get_file_content_as_string("html/login.html");
    (this->headers)[CONTENT_LEN] = to_string((this->body).length());
  }

  // POST
  else if (req.method.compare("POST") == 0) {
    vector<string> parameter_tokens = split(req.body.c_str(), '&');
    username = split(parameter_tokens.at(0), '=').at(1);
    string password = split(parameter_tokens.at(1), '=').at(1);

    if (is_login_valid(username, password)) {
      add_session(username);
      already_login = true;
    }
  }

  if (already_login) {
    stringstream ss;
    ss << "<!DOCTYPE html>\n";
    ss << "<html>\n";
    ss << "<head><title>PennCloud</title></head>\n";
    ss << "<body>\n<h1 align=\"center\">Hi " << username << "</h1>\n";
    ss << "</body></html>\n";

    this->body = ss.str();
    this->body = get_file_content_as_string("html/user-home.html");
    replace_all(this->body, "$username", username);
    (this->headers)[CONTENT_LEN] = to_string((this->body).length());
    (this->headers)[SET_COOKIE] = "sessionid=" + username;
  }
}

/*tell is login or not from sessions*/
bool Response::is_already_login(map<string, string> cookies, string &username) {
  if (cookies.count("sessionid") == 1) {
    if (is_session_valid(cookies["sessionid"])) {
      username = cookies["sessionid"];
      return true;
    }
  }
  return false;
}

/* file upload */
void Response::upload(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  this->body = get_file_content_as_string("html/upload.html");
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* handle file upload */
void Response::handle_upload(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/plain";
  // (this->headers)[CONTENT_TYPE] = "application/pdf";
  string dir(UPLOADED_DIR);
  string filename(extract_file_name(req.body));
  this->body = extract_file_content(req.body);
  store_file(dir, filename, this->body);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* download all uploaded files */
void Response::download(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  string dir(UPLOADED_DIR);
  vector<string> files(list_all_files(dir));
  stringstream ss;
  for (vector<string>::iterator it = files.begin(); it != files.end(); ++it) {
    string file_path(dir + *it);
    string saved_filename(*it);
    debug(1, "[download filepath]: ");
    debug(1, file_path.c_str());
    debug(1, "\r\n");
    ss << "<li><a href=\"" << file_path;
    ss << "\" download=\"" << saved_filename << "\">";
    ss << *it;
    ss << "</a></li>";
  }
  string all_download_files = ss.str();
  this->body = get_file_content_as_string("html/download.html");
  replace_all(this->body, "$allDownloadFiles", all_download_files);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* display file content */
void Response::file(const char* filename) {
    debug(1, "[file exists]: ");
    debug(1, filename);
    debug(1, "\r\n");
    string file_content(get_file_content_as_string(filename));
    this->status = OK;
    (this->headers)[CONTENT_TYPE] = "text/plain";
    this->body = file_content;
    (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}
