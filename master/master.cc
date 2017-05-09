#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <map>
#include <pthread.h>
#include <set>
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
// users metadata, key:node id, values: user name set
map<int, set<string>> users;

// ip:port to id
map<string, int> server_index;

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
    // char m[] = "quit\r\n";
    // debug(1, "[%d] Send to backend: %s", sockfd, m);
    // do_write(sockfd, m, strlen(m));
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
    string content = message.substr(1);

    if (command.compare("?") == 0) {
      int key = hash_str(content.c_str());
      int node_id = get_server_id(key);
      if (users.count(node_id) == 0) {
        set<string> l;
        l.insert(content);
        users[node_id] = l;
      } else {
        users[node_id].insert(content);
      }
      rep = get_backend_info(key);
      cout << "Object key:" << key << "|"
           << "Server" << rep << endl;
    }

    else if (command.compare("!") == 0) {
      int id = atoi(content.c_str());
      Pair pair;
      Server server;
      server.id = id;
      string ip_addr(inet_ntoa(src.sin_addr));
      server.ip = ip_addr;
      server.port = ntohs(src.sin_port);
      server.running = true;

      // store server index
      string address = server.ip + ":" + to_string(server.port);
      server_index[address] = id;

      if (servers.count(id) == 0) {
        pair.primary = server;
        if (users.empty()) { // Initialize
          rep = "P";
        } else { // dynamic membership
          int dest = get_server_id(id);
          set<string> list = users[dest];
          Server dest_server = servers[dest].primary;
          rep = "P " + dest_server.ip + ":" + to_string(dest_server.port) + ",";
          for (set<string>::iterator it = list.begin(); it != list.end();
               ++it) {
            int k = hash_str((*it).c_str());
            if (k < id) {
              rep += *it + ",";
              users[dest].erase(*it);
            }
          }
        }
      } else {
        pair = servers[id];
        if (!check_server_state(pair.primary.ip, pair.primary.port)) {
          if (pair.secondary.ip.empty()) {
            pair.primary = server;
            rep = "P";
          } else {
            pair.primary = pair.secondary;
            pair.secondary = server;
            rep = "P=" + pair.primary.ip + ":" + to_string(pair.primary.port);
          }
        } else {
          pair.secondary = server;
          rep = "P=" + pair.primary.ip + ":" + to_string(pair.primary.port);
        }
      }
      servers[id] = pair;
    }

    // return all backend servers
    else if (command.compare("A") == 0) {
      for (map<int, Pair>::iterator it = servers.begin(); it != servers.end();
           ++it) {
        Pair pair = it->second;
        rep += "P" + pair.primary.ip + ":" + to_string(pair.primary.port) + ",";
        if (!pair.secondary.ip.empty()) {
          rep += "S" + pair.secondary.ip + ":" +
                 to_string(pair.secondary.port) + ",";
        }
      }
    }

    else if (command.compare("U") == 0) {
      int node_id = server_index.at(content);
      set<string> list = users[node_id];
      for (set<string>::iterator it = list.begin(); it != list.end(); ++it) {
        rep += *it + ",";
      }
    }

    cout << rep << endl;
    sendto(sock, rep.c_str(), rep.length(), 0, (struct sockaddr *)&src,
           sizeof(src));
  }

  return 0;
}
