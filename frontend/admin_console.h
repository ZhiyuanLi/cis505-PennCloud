/*
 * admin_console.cc
 *
 *  Created on: May 8, 2017
 *      Author: cis505
 */

#include "../master/config.h"
#include "utils.h"
#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <map>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace std;

class Server {

public:
  int id;
  string ip;
  int port;
  bool running;
  int backend_type; // 1: primary, 2: secondary
};

class Server_Info {

public:
  string servertype;
  int Num_of_Servers;
};

map<int, Server> frontend_servers;
map<int, Server> backend_servers;
map<int, string> user_list;
Server_Info server_list[2];

// mutex lock
pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;

int parse_frontend_servers(const char *filename) {
  cout << "here" << endl;
  ifstream ifs(filename);
  string line = "";
  int id = 1;

  // read file line by line
  while (getline(ifs, line)) {
    // get IP address and port number
    size_t colon = line.find(":");
    if (colon == string::npos) {
      fprintf(stderr, "Wrong IP address and port number format\n");
      exit(1);
    }
    string ip_addr = line.substr(0, colon);
    int port = atoi(line.substr(colon + 1).c_str());

    Server server;
    server.id = id;
    server.ip = ip_addr;
    server.port = port;
    server.running = false;
    frontend_servers[id++] = server;
  }

  return id - 1;
}

int parse_backend_servers(char *buf) {

  string data = buf;
  int id = 1;
  std::size_t found1 = 0;
  std::size_t found2 = data.find_first_of(":");
  std::size_t found3 = data.find_first_of(",");

  while (found2 != std::string::npos) {

    string ip_addr = data.substr(found1 + 1, found2 - found1 - 1);
    int port = atoi(data.substr(found2 + 1, found3 - found2 - 1).c_str());
    string backend_type = data.substr(found1, 1);

    cout << "ip_addr = " << ip_addr << endl;
    cout << "port = " << port << endl;
    cout << "backend_type = " << backend_type << endl;

    Server server;
    server.id = id;
    server.ip = ip_addr;
    server.port = port;
    server.running = false;
    if (backend_type.compare("P") == 0) {
      server.backend_type = 1;
    } else if (backend_type.compare("S") == 0) {
      server.backend_type = 2;
    } else
      server.backend_type = 3;

    backend_servers[id++] = server;

    found1 = found3 + 1;
    found2 = data.find_first_of(":", found2 + 1);
    found3 = data.find_first_of(",", found3 + 1);
  }

  return id - 1;
}

void *check_server_state(void *arg) {

  int server_index = *(int *)arg;
  while (true) {
    for (int i = 1; i <= server_list[server_index].Num_of_Servers; i++) {

      if (server_list[server_index].servertype.compare("frontend") == 0) {

        int sockfd = socket(PF_INET, SOCK_STREAM, 0);
        struct sockaddr_in servaddr;
        inet_aton(frontend_servers[i].ip.c_str(), &(servaddr.sin_addr));
        servaddr.sin_port = htons(frontend_servers[i].port);
        servaddr.sin_family = AF_INET;

        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) ==
            0) {
          pthread_mutex_lock(&mutex_lock);
          frontend_servers[i].running = true;
          pthread_mutex_unlock(&mutex_lock);

          cout << "frontend_server #" << i << " is active" << endl;
        } else {
          pthread_mutex_lock(&mutex_lock);
          frontend_servers[i].running = false;
          pthread_mutex_unlock(&mutex_lock);

          cout << "frontend_server #" << i << " is down" << endl;
        }
        close(sockfd);
      } else if (server_list[server_index].servertype.compare("backend") == 0) {

        int sockfd = socket(PF_INET, SOCK_STREAM, 0);
        struct sockaddr_in servaddr;
        inet_aton(backend_servers[i].ip.c_str(), &(servaddr.sin_addr));
        servaddr.sin_port = htons(backend_servers[i].port);
        servaddr.sin_family = AF_INET;

        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) ==
            0) {
          pthread_mutex_lock(&mutex_lock);
          backend_servers[i].running = true;
          pthread_mutex_unlock(&mutex_lock);

          cout << "backend_server #" << i << " is active" << endl;
        } else {
          pthread_mutex_lock(&mutex_lock);
          backend_servers[i].running = false;
          pthread_mutex_unlock(&mutex_lock);

          cout << "backend_server #" << i << " is down" << endl;
        }

        close(sockfd);
      }
    }

    // sleep 5 second
    sleep(5);
  }

  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "[Usage]: ./admin_console -p 10000 servers.txt\n");
    exit(1);
  }

  // local variables
  int frontend_port = 0;

  // parse command line arguments
  int c;
  while ((c = getopt(argc, argv, "p:")) != -1) {
    switch (c) {
    case 'p':
      frontend_port = atoi(optarg);
      break;
    default:
      exit(1);
    }
  }

  // parse server config file
  const char *frontend_filename = argv[optind];

  server_list[0].servertype = "frontend";
  cout << "frontend_filename = " << frontend_filename << endl;
  server_list[0].Num_of_Servers = parse_frontend_servers(frontend_filename);

  cout << "here!" << endl;
  cout << "number = " << server_list[0].Num_of_Servers << endl;
  pthread_t frontend_thread;
  int num = 0;
  // pthread_create(&frontend_thread, NULL, check_server_state, &num);

  // admin_console connect to master via UDP, and get the information of ip and
  // port of backend server

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

  //	ask master for backend's ip:port
  string contact_master = "A";
  sendto(udp_fd, contact_master.c_str(), contact_master.size(), 0,
         (struct sockaddr *)&dest, sizeof(dest));
  cout << "To master: " << contact_master << endl;

  struct sockaddr_in src;
  socklen_t srcSize = sizeof(src);
  char feedback[1024];
  int rlen = recvfrom(udp_fd, feedback, sizeof(feedback) - 1, 0,
                      (struct sockaddr *)&src, &srcSize);
  feedback[rlen] = 0;
  cout << "From master: " << feedback << endl;

  server_list[1].Num_of_Servers = parse_backend_servers(feedback);

  //	close(udp_fd);

  int nodeID;
  while (true) {
    cout << "input node number you want to check:" << endl;
    cin >> nodeID;

    string contact_master = "U" + backend_servers[nodeID].ip + ":" +
                            to_string(backend_servers[nodeID].port);
    sendto(udp_fd, contact_master.c_str(), contact_master.size(), 0,
           (struct sockaddr *)&dest, sizeof(dest));
    cout << "asking master username of node " << contact_master << endl;

    struct sockaddr_in src;
    socklen_t srcSize = sizeof(src);
    char feedback[5000];
    int rlen = recvfrom(udp_fd, feedback, sizeof(feedback) - 1, 0,
                        (struct sockaddr *)&src, &srcSize);
    feedback[rlen] = 0;
    cout << "user name list in this node: " << feedback << endl;

    string data = feedback;
    int id = 1;
    std::size_t found1 = 0;
    std::size_t found2 = data.find_first_of(",");

    while (found2 != std::string::npos) {

      user_list[id++] = data.substr(found1, found2 - 1);

      found1 = found2 + 1;
      found2 = data.find_first_of(",", found2 + 1);
    }

    cout << "Do you want to see raw data from one user?(y/n)" << endl;
    string reply;
    while (true) {
      cin >> reply;
      if (reply.compare("y") == 0) {
        string user;
        cin >> user;

        // get data from backend server
        struct sockaddr_in backend;
        bzero(&dest, sizeof(backend));
        backend.sin_family = AF_INET;
        backend.sin_port = htons(backend_servers[nodeID].port);
        inet_pton(AF_INET, backend_servers[nodeID].ip.c_str(),
                  &(backend.sin_addr));

        int sockfd = socket(PF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
          debug(1, "Fail to open a socket!\n");
        }
        connect(sockfd, (struct sockaddr *)&backend, sizeof(backend));
        char m[] = "getlist ";
        strcat(m, user.c_str());
        strcat(m, ",email\r\n");

        debug(1, "[%d] Send to backend: %s", sockfd, m);
        do_write(sockfd, m, strlen(m));

        string line = read_line(sockfd);
        cout << line << endl;

        debug(1, "[%d] Receive from backend:\n", sockfd);

        char msg[] = "getfile ";
        strcat(msg, user.c_str());
        strcat(msg, "\r\n");

        debug(1, "[%d] Send to backend: %s", sockfd, msg);
        do_write(sockfd, msg, strlen(msg));

        string line2 = read_line(sockfd);
        cout << line2 << endl;

        debug(1, "[%d] Receive from backend:\n", sockfd);

      } else if (reply.compare("n") == 0)
        continue;
      else {
        cout << "Please input correct response(y/n)!" << endl;
      }
    }
  }

  //	// create socket for frontend server
  //	int frontend_fd = socket(PF_INET, SOCK_STREAM, 0);
  //
  //	struct sockaddr_in frontend_servaddr;
  //	bzero(&frontend_servaddr, sizeof(frontend_servaddr));
  //	frontend_servaddr.sin_family = AF_INET;
  //	frontend_servaddr.sin_addr.s_addr = htons(INADDR_ANY);
  //	frontend_servaddr.sin_port = htons(frontend_port);
  //	bind(frontend_fd, (struct sockaddr*) &frontend_servaddr,
  //sizeof(frontend_servaddr));
  //
  //	// listen on frontend port number and accept connection
  //	listen(frontend_fd, 100);

  // check frontend server state
  //	pthread_t state_thread;
  //	pthread_create(&state_thread, NULL, check_server_state, NULL);

  // create socket for backend server
  //	int backend_fd = socket(PF_INET, SOCK_STREAM, 0);
  //
  //	struct sockaddr_in backend_servaddr;
  //	bzero(&backend_servaddr, sizeof(backend_servaddr));
  //	backend_servaddr.sin_family = AF_INET;
  //	backend_servaddr.sin_addr.s_addr = htons(INADDR_ANY);
  //	backend_servaddr.sin_port = htons(backend_port);
  //	bind(backend_fd, (struct sockaddr*) &backend_servaddr,
  //sizeof(backend_servaddr));
  //
  //	// listen on backend port number and accept connection
  //	listen(backend_fd, 100);

  // check backend server state

  pthread_t backend_thread;
  num = 1;
  pthread_create(&backend_thread, NULL, check_server_state, &num);

  //	close(frontend_fd);
  //	close(backend_fd);
  return 0;
}
