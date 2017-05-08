#ifndef STORE_H
#define STORE_H

#include <string>
#include <vector>

using namespace std;

//send one message to backend
vector<string> send_to_backend(string message, string username);

void add_user(string username, string password);

bool is_user_exist(string username);

bool is_login_valid(string username, string password);

void add_session(string id);

void delete_session(string id);

bool is_session_valid(string id);

#endif
