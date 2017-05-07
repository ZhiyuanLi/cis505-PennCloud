#ifndef STORE_H
#define STORE_H

#include <string>
#include <vector>

using namespace std;

// find the backend server that should store the user info
struct sockaddr_in find_backend(string username);

//send one message to backend
vector<string> send_to_backend(string message, struct sockaddr_in backend);

void add_user(string username, string password);

bool is_user_exist(string username);

bool is_login_valid(string username, string password);

void add_session(string id);

bool is_session_valid(string id);

#endif
