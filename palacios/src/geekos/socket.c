#include <geekos/socket.h>

#define MAX_SOCKS 1024

struct socket sockets[1024];


void init_network() {
  int i = 0;

  for (i = 0; i < MAX_SOCKS; i++) {
    sockets[i].in_use = 0;
    init_queue(&(sockets[i].send_queue))
    init_queue(&(sockets[i].recv_queue))
  }

  // set up interrupt handler
  // set up device driver


}

static int allocate_socket_fd() {
  int i = 0;
  
  for (i = 0; i < MAX_SOCKS; i++) {
    if (sockets[i].in_use == 0) {
      sockets[i].in_use = 1;
      return i;
    }
  }

  return -1;
}


struct socket * get_socket_from_fd(int fd) {
  return &(sockets[fd]);
}




int connect(const uint_t ip_addr) {
  int sockfd = -1;
  sockfd = allocate_socket_fd();

  if (sockfd == -1) {
    return -1;
  }




}
