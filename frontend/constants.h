#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <map>
#include <string>
#include <time.h>

const static char *CONTENT_TYPE = "Content-Type: ";
const static char *CONTENT_LEN = "Content-Length: ";
const static char *COOKIE = "Cookie: ";
const static char *SET_COOKIE = "Set-Cookie: ";
const static char *CONNECTION = "Connection: ";
const static char *OK = "200 OK";

const static char *HOME_URL = "/";
const static char *REGISTER_URL = "/register";
const static char *LOGIN_URL = "/login";
const static char *STORAGE_URL = "/storage";
const static char *EMAIL_URL = "/email";

using namespace std;

static map<string, string> users;    // key: username; value:password
static map<string, time_t> sessions; // store session id and its creation time, session
                              // id same as username

#endif
