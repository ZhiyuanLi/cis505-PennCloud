#ifndef STORE_H
#define STORE_H

#include <map>

using namespace std;

void add_user(string username, string password);

bool is_user_exsit(string username);

bool is_login_valid(string username, string password);

void add_session(string id);

bool is_session_valid(string id);

#endif
