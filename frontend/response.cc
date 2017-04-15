#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "constants.h"
#include "helper.h"
#include "request.h"
#include "response.h"

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
  stringstream ss;
  ss << "<!DOCTYPE html>\n";
  ss << "<html>\n";
  ss << "<head><title>PennCloud</title></head>\n";
  ss << "<body bgcolor=\"#f0f0f0\">\n<h1 align=\"center\">Register Page</h1>\n";
  ss << "<form method=\"post\">\n";
  ss << "username: <input type=\"text\" name=\"username\" required><br/>\n";
  ss << "password: <input type=\"password\" name=\"password\" required><br/>\n";
  ss << "<input type=\"submit\" value=\"Register\"></form>\n";
  ss << "<a href=\"/login\">Already have an account? Login here!</a>\n";
  ss << "</body></html>\n";
  this->body = ss.str();
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* login */
void Response::login(Request req) {
    this->status = OK;
    (this->headers)[CONTENT_TYPE] = "text/html";
    stringstream ss;
    ss << "<!DOCTYPE html>\n";
    ss << "<html>\n";
    ss << "<head><title>PennCloud</title></head>\n";
    ss << "<body bgcolor=\"#f0f0f0\">\n<h1 align=\"center\">Login Page</h1>\n";
    ss << "<form method=\"post\">\n";
    ss << "username: <input type=\"text\" name=\"username\" required><br/>\n";
    ss << "password: <input type=\"password\" name=\"password\" required><br/>\n";
    ss << "<input type=\"submit\" value=\"Login\"></form>\n";
    ss << "<a href=\"/register\">New user? Register here!</a>\n";
    ss << "</body></html>\n";
    this->body = ss.str();
    (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}
