/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef GEEKOS_SOCKET_H
#define GEEKOS_SOCKET_H

#include <geekos/ring_buffer.h>
#include <geekos/kthread.h>

#ifdef UIP
#include <uip/uip.h>


typedef enum {WAITING, CLOSED, LISTEN, ESTABLISHED} sock_state_t;

struct socket {
  int in_use;
  struct Thread_Queue recv_wait_queue;
  struct ring_buffer *send_buf;
  struct ring_buffer *recv_buf;
  struct uip_conn *con;

  sock_state_t state;

};


void init_socket_layer();

int connect(const uchar_t ip_addr[4], ushort_t port);
int close(const int sockfd);
int recv(int sockfd, void * buf, uint_t len);
int send(int sockfd, void * buf, uint_t len);

void set_ip_addr(uchar_t addr[4]);

#endif // UIP

#endif
