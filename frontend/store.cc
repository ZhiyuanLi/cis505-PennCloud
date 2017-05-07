#include <arpa/inet.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../master/config.h"
#include "constants.h"
#include "store.h"
#include "utils.h"

using namespace std;

// send one message to backend
vector<string> send_to_backend(string message, string username) {

  int udp_fd = socket(PF_INET, SOCK_DGRAM, 0);
  if (udp_fd < 0) {
    debug(1, "Cannot open socket!\n");
    exit(-1);
  }

  // associate with master IP address and port number
  struct sockaddr_in dest;
  bzero(&dest, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_port = htons(MASTER_PORT);
  inet_pton(AF_INET, MASTER_IP, &(dest.sin_addr));

  // ask master for backend's ip:port
  string contact_master = "?" + username;
  sendto(udp_fd, contact_master.c_str(), contact_master.size(), 0,
         (struct sockaddr *)&dest, sizeof(dest));
  cout << "To master: " << contact_master << endl;

  struct sockaddr_in src;
  socklen_t srcSize = sizeof(src);
  char feedback[50];
  int rlen = recvfrom(udp_fd, feedback, sizeof(feedback) - 1, 0,
                      (struct sockaddr *)&src, &srcSize);
  feedback[rlen] = 0;
  cout << "From master: " << feedback << endl;

  close(udp_fd);
  vector<string> tokens = split(feedback, ':');

  struct sockaddr_in backend;
  bzero(&dest, sizeof(backend));
  backend.sin_family = AF_INET;
  backend.sin_port = htons(atoi(tokens.at(1).c_str()));
  inet_pton(AF_INET, tokens.at(0).c_str(), &(backend.sin_addr));

  int sockfd = socket(PF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    debug(1, "Fail to open a socket!\n");
  }
  connect(sockfd, (struct sockaddr *)&backend, sizeof(backend));
  char m[message.length() + 1];
  strcpy(m, message.c_str());

  debug(1, "[%d] Send to backend: %s", sockfd, m);
  do_write(sockfd, m, strlen(m));

  vector<string> rep;
  string line = read_line(sockfd);

  debug(1, "[%d] Receive from backend:\n", sockfd);

  while (!line.empty()) {
    debug(1, "%d:[%s]\n", line.length(), line.c_str());
    rep.push_back(line);
    line = read_line(sockfd);
  }

  debug(1, "==================\n");
  close(sockfd);

  return rep;
}

void add_user(string username, string password) {
  string message = "put " + username + ",pwd," + password + "\r\n";
  send_to_backend(message, username);
}

bool is_user_exist(string username) {
  string message = "get " + username + ",pwd\r\n";
  vector<string> rep = send_to_backend(message, username);
  if (rep.at(0).compare(0, 3, "+OK") == 0) {
    return true;
  }
  return false;
}

bool is_login_valid(string username, string password) {
  string message = "get " + username + ",pwd\r\n";
  vector<string> rep = send_to_backend(message, username);
  if (rep.at(0).compare(0, 3, "+OK") == 0) {
    if (rep.at(1) == password) {
      return true;
    }
  }
  return false;
}

void add_session(string id) { sessions[id] = time(NULL); }

bool is_session_valid(string id) {
  return sessions.count(id) == 1 ? true : false;
}
