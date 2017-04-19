#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "constants.h"
#include "helper.h"
#include "request.h"
#include "response.h"
#include "store.h"

using namespace std;

Response::Response(Request req) {
  this->http_version = req.http_version;
  (this->headers)[CONNECTION] = "close";

  // register
  if (req.path == REGISTER_URL) {
    reg(req);
  }

  // login
  else if (req.path == LOGIN_URL) {
    login(req);
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
    stringstream ss;
    ss << "<!DOCTYPE html>\n";
    ss << "<html>\n";
    ss << "<head><title>PennCloud</title></head>\n";
    ss << "<body bgcolor=\"#f0f0f0\">\n<h1 align=\"center\">Register "
          "Page</h1>\n";
    ss << "<form method=\"post\">\n";
    ss << "username: <input type=\"text\" name=\"username\" required><br/>\n";
    ss << "password: <input type=\"password\" name=\"password\" "
          "required><br/>\n";
    ss << "<input type=\"submit\" value=\"Register\"></form>\n";
    ss << "<a href=\"/login\">Already have an account? Login here!</a>\n";
    ss << "</body></html>\n";
    this->body = ss.str();
    (this->headers)[CONTENT_LEN] = to_string((this->body).length());
  }

  // POST
  else if (req.method.compare("POST") == 0) {
    vector<string> parameter_tokens = split(req.body.c_str(), '&');
    string username = split(parameter_tokens.at(0), '=').at(1);
    string password = split(parameter_tokens.at(1), '=').at(1);

    if (is_user_exsit(username)) {
      stringstream ss;
      ss << "<!DOCTYPE html>\n";
      ss << "<html>\n";
      ss << "<head><title>PennCloud</title></head>\n";
      ss << "<body bgcolor=\"#f0f0f0\">\n<h1 align=\"center\">Register "
            "Page</h1>\n";
      ss << "<h2>Username aldready exsits!</h2>\n";
      ss << "<form method=\"post\">\n";
      ss << "username: <input type=\"text\" name=\"username\" required><br/>\n";
      ss << "password: <input type=\"password\" name=\"password\" "
            "required><br/>\n";
      ss << "<input type=\"submit\" value=\"Register\"></form>\n";
      ss << "<a href=\"/login\">Already have an account? Login here!</a>\n";
      ss << "</body></html>\n";
      this->body = ss.str();
      (this->headers)[CONTENT_LEN] = to_string((this->body).length());
    } else { // new user
      stringstream ss;
      ss << "<!DOCTYPE html>\n";
      ss << "<html>\n";
      ss << "<head><title>PennCloud</title></head>\n";
      ss << "<body bgcolor=\"#f0f0f0\">\n<h1 align=\"center\">Register "
            "Page</h1>\n";
      ss << "<h2>Register successful!</h2>\n";
      ss << "<form method=\"post\">\n";
      ss << "<a href=\"/login\">Login here!</a>\n";
      ss << "</body></html>\n";
      add_user(username, password);
      this->body = ss.str();
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
    stringstream ss;
    ss << "<!DOCTYPE html>\n";
    ss << "<html>\n";
    ss << "<head><title>PennCloud</title></head>\n";
    ss << "<body bgcolor=\"#f0f0f0\">\n<h1 align=\"center\">Login Page</h1>\n";
    ss << "<form method=\"post\">\n";
    ss << "username: <input type=\"text\" name=\"username\" required><br/>\n";
    ss << "password: <input type=\"password\" name=\"password\" "
          "required><br/>\n";
    ss << "<input type=\"submit\" value=\"Login\"></form>\n";
    ss << "<a href=\"/register\">New user? Register here!</a>\n";
    ss << "</body></html>\n";
    this->body = ss.str();
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
    ss << "<body bgcolor=\"#f0f0f0\">\n<h1 align=\"center\">Hi " << username
       << "</h1>\n";
    ss << "</body></html>\n";

    this->body = ss.str();
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
