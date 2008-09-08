#ifndef GEEKOS_SOCKET_H
#define GEEKOS_SOCKET_H

#include <geekos/ring_buffer.h>
#include <uip/uip.h>
#include <geekos/kthread.h>

struct socket {
  int in_use;
  struct Thread_Queue recv_wait_queue;
  struct ring_buffer *send_buf;
  struct ring_buffer *recv_buf;
  struct uip_conn *con;
};


void init_network();

int connect(const uchar_t ip_addr[4], ushort_t port);
int close(const int sockfd);
int recv(int sockfd, void * buf, uint_t len);
int send(int sockfd, void * buf, uint_t len);

#endif
