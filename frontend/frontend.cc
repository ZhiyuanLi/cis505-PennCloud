#include <algorithm>
#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>
#include <set>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

#include "helper.h"
#include "request.h"
#include "response.h"

#define MAX_CON 100

using namespace std;

int vflag = 0; // flag to show debug info
set<int> fds;  // active clients' file descriptors

/*open a stream socket*/
int open_socket(int &listen_fd, struct sockaddr_in &servaddr,
                unsigned short port) {
  // create socket
  listen_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    debug(1, "Fail to open a socket!\n");
    return -1;
  }

  // bind to port and listen
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);
  
  if (bind(listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    debug(1, "Fail to bind the socket!\n");
    return -2;
  }

  if (listen(listen_fd, 100) < 0) {
    debug(1, "Fail to Listen to the socket!\n");
    return -3;
  }

  return 0;
}

/*thread function to handle each client*/
void *client_handler(void *comm_fd) {
  int fd = *(int *)comm_fd;

  // Not support Persistent Connections
  Request req(fd);
  // if(req.valid){
  //   Response rep(req);
  //   rep.reply(fd);
  // }

  fds.erase(fd);
  close(fd);
  pthread_exit(NULL);
}

int main(int argc, char *argv[]) {

  /*read command-line arguments*/
  int pflag = 0;
  char *pvalue = NULL;
  int c;

  /*-p, -a or -v options*/
  while ((c = getopt(argc, argv, "p:v")) != -1) {
    switch (c) {
    case 'p':
      pflag = 1;
      pvalue = optarg;
      break;
    case 'v':
      vflag = 1;
      break;
    case '?':
      if (optopt == 'p')
        debug(1, "Option -%c requires an argument.\n", optopt);
      else if (isprint(optopt))
        debug(1, "Unknown option `-%c'.\n", optopt);
      else
        debug(1, "Unknown option character `\\x%x'.\n", optopt);
      exit(1);
    default:
      abort();
    }
  }

  // check unknown arguments
  for (int index = optind; index < argc; index++)
    debug(1, "Non-option argument %s\n", argv[index]);

  // check port validation
  int tmp = atoi(pvalue);
  if (tmp < 0 || tmp > 65535) {
    debug(1, "Invalid port number!(should be 0~65535)\n");
    exit(2);
  }
  unsigned short port = (unsigned short)tmp;

  int listen_fd;
  struct sockaddr_in servaddr;
  if (open_socket(listen_fd, servaddr, port) < 0) {
    exit(3);
  };

  debug(vflag, "Listening to port %d\n", port);

  while (true) {
    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);
    // comming fd
    int comm_fd =
        accept(listen_fd, (struct sockaddr *)&clientaddr, &clientaddrlen);
    if (comm_fd < 0) {
      debug(1, "Cannot connect to %s:%d!\n", inet_ntoa(clientaddr.sin_addr),
            ntohs(clientaddr.sin_port));
      continue;
    }
    debug(vflag, "[%d] New connection from %s:%d\n", comm_fd,
          inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

    fds.insert(comm_fd);
    pthread_t thread;
    pthread_create(&thread, NULL, client_handler, &comm_fd);
  }

  return 0;
}
