#ifndef GEEKOS_SOCKET_H
#define GEEKOS_SOCKET_H

#include <geekos/queue.h>


struct socket {
  int in_use;
  struct gen_queue send_queue;
  struct gen_queue recv_queue;
};



void init_network();

int connect(const uint_t ip_addr);
int close(const int sockfd);
int recv(int sockfd, void * buf, uint_t len);
int send(int sockfd, void * buf, uint_t len);

#endif
