#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <map>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "conhash.h"

using namespace std;

// Server class
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

map<int, Pair> servers;

/*
 * check server state every 5 second
 */
// void *check_server_state(void *arg) {
//   while (true) {
//     for (int i = 1; i <= NUM_OF_SERVERS; i++) {
//       int sockfd = socket(PF_INET, SOCK_STREAM, 0);
//       struct sockaddr_in servaddr;
//       inet_aton(servers[i].ip.c_str(), &(servaddr.sin_addr));
//       servaddr.sin_port = htons(servers[i].port);
//       servaddr.sin_family = AF_INET;
//
//       if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) ==
//           0) {
//         pthread_mutex_lock(&mutex_lock);
//         servers[i].running = true;
//         pthread_mutex_unlock(&mutex_lock);
//
//         cout << "server #" << i << " is active" << endl;
//       } else {
//         pthread_mutex_lock(&mutex_lock);
//         servers[i].running = false;
//         pthread_mutex_unlock(&mutex_lock);
//
//         cout << "server #" << i << " is down" << endl;
//       }
//
//       close(sockfd);
//     }
//
//     // sleep 5 second
//     sleep(5);
//   }
//
//   return NULL;
// }

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

    if (command.compare("?") == 0) {

    } else if (command.compare("!") == 0) {
      int id = atoi(message.substr(1, message.length() - 1).c_str());
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
