#include <map>
#include <set>
#include <stdio.h>
#include <time.h>

#include "store.h"
#include "constants.h"

using namespace std;

void add_user(string username, string password){
  users[username] = password;
}

bool is_user_exsit(string username) {
  return users.count(username) == 1 ? true : false;
}

bool is_login_valid(string username, string password) {
  if (users.count(username) == 1) {
    return users[username].compare(password) == 0;
  }
  return false;
}

void add_session(string id) { sessions[id] = time(NULL); }

bool is_session_valid(string id) {
  return sessions.count(id) == 1 ? true : false;
}
