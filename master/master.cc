#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <map>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../frontend/utils.h"
#include "config.h"
#include "conhash.h"

using namespace std;

// Server class to represent a backend server
class Server {

public:
  int id;
  string ip;
  int port;
  bool running;
};

// Server Pair
class Pair {

public:
  Server primary;
  Server secondary;
};

// backend servers
map<int, Pair> servers;

/*
 * check server state
 */
bool check_server_state(string ip, int port) {
  bool state;
  int sockfd = socket(PF_INET, SOCK_STREAM, 0);
  struct sockaddr_in servaddr;
  inet_aton(ip.c_str(), &(servaddr.sin_addr));
  servaddr.sin_port = htons(port);
  servaddr.sin_family = AF_INET;

  if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0) {
    state = true;
    cout << "server " << ip << ":" << port << " is active" << endl;
    char m[] = "quit\r\n";

    debug(1, "[%d] Send to backend: %s", sockfd, m);
    do_write(sockfd, m, strlen(m));
  } else {
    state = false;
    cout << "server " << ip << ":" << port << " is down" << endl;
  }

  close(sockfd);
  return state;
}

// get corresponding next-higher backend server id
int get_server_id(int key) {
  if (servers.empty())
    return -1;
  map<int, Pair>::iterator it = servers.begin();
  int head = it->first;
  if (0 <= key && key < head) {
    return head;
  }
  int prev = head;
  int current = prev;
  ++it;
  for (; it != servers.end(); ++it) {
    current = it->first;
    if (prev <= key && key < current) {
      return current;
    }
    prev = current;
  }
  return head;
}

// get server ip:port
string get_backend_info(int key) {
  int server_id = get_server_id(key);
  Pair pair = servers[server_id];

  if (!check_server_state(pair.primary.ip, pair.primary.port)) {
    pair.primary = pair.secondary;
    Server dummy;
    pair.secondary = dummy;
  }
  servers[server_id] = pair;
  string res = pair.primary.ip + ":" + to_string(pair.primary.port);
  return res;
}

/*
 * Main
 */
int main(int argc, char *argv[]) {

  // create UDP socket
  int sock = socket(PF_INET, SOCK_DGRAM, 0);

  struct sockaddr_in servaddr;
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  inet_aton(MASTER_IP, &(servaddr.sin_addr));
  servaddr.sin_port = htons(MASTER_PORT);
  bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr));

  while (true) {
    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);
    char buf[1024];
    int rlen =
        recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&src, &srclen);
    if (rlen <= 0)
      continue;

    buf[rlen] = 0;
    std::string message(buf);
    string rep;

    printf("Receive [%s] from %s:%d\n", buf, inet_ntoa(src.sin_addr),
           ntohs(src.sin_port));

    string command = message.substr(0, 1);
    string content = message.substr(1, message.length() - 1);

    if (command.compare("?") == 0) {
      int key = hash_str(content.c_str());
      rep = get_backend_info(key);
      cout << "Object key:" << key << "|"
           << "Server" << rep << endl;
    } else if (command.compare("!") == 0) {
      int id = atoi(content.c_str());
      Pair pair;
      Server server;
      server.id = id;
      string ip_addr(inet_ntoa(src.sin_addr));
      server.ip = ip_addr;
      server.port = ntohs(src.sin_port);
      server.running = true;
      if (servers.count(id) == 0) {
        pair.primary = server;
        rep = "P";
      } else {
        pair = servers[id];
        pair.secondary = server;
        rep = "P=" + pair.primary.ip + ":" + to_string(pair.primary.port);
      }
      servers[id] = pair;
    }

    sendto(sock, rep.c_str(), rep.length(), 0, (struct sockaddr *)&src,
           sizeof(src));
  }

  return 0;
}
