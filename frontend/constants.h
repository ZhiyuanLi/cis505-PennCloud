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
const static char *LOGOUT_URL = "/logout";
const static char *STORAGE_URL = "/storage";
const static char *EMAIL_URL = "/email";
const static char *UPLOAD_URL = "/upload";
const static char *DOWNLOAD_URL = "/download";
const static char *NEW_FOLDER_URL = "/newfolder";
const static char *RENAME_FILE_URL = "/renamefile";
const static char *DELETE_FILE_URL = "/deletefile";
const static char *DELETE_FOLDER_URL = "/deletefolder";
const static char *MOVE_FILE_URL = "/movefile";
const static char *SEND_EMAIL_URL = "/sendemail";
const static char *INBOX_URL = "/inbox";
const static char *VIEW_EMAIL_URL = "/viewemail";
const static char *FORWARD_EMAIL_URL = "/forward";
const static char *REPLY_EMAIL_URL = "/replyemail";

using namespace std;

// static map<string, string> users; // key: username; value:password
// store session id and its creation time, session id same as username
static map<string, time_t> sessions;

#endif
