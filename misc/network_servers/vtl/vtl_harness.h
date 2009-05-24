#ifndef VTL_HARNESS_H
#define VTL_HARNESS_H

#include "vtl.h"

#ifdef linux
#include <fcntl.h>
#define POLL_TIMEOUT 0
#define MAX_VTL_CONS 100

#define HOME "./"
#define VTP_FIFO_SENDFILE HOME "vtp_fifo_send"
#define VTP_FIFO_RECVFILE HOME "vtp_fifo_recv"
#endif

struct VTL_CON {
  /*  unsigned long rem_seq_num;
      unsigned long dest_ip;
      unsigned long src_ip;
      unsigned short src_port;
      unsigned short dest_port;
      RawEthernetPacket ack_template;
      unsigned short ip_id;
      unsigned long tcp_timestamp;
  */
  
  vtl_model_t con_model;

  bool in_use;
  int next;
  int prev;
};

#define FOREACH_VTL_CON(iter, cons) for (iter = g_first_vtl; iter != -1; iter = cons[iter].next) 



/* Sending Layers Need to implement these */
int vtl_init();
int vtl_send(RawEthernetPacket * p, unsigned long serv_addr);
int vtl_recv(RawEthernetPacket ** p);
int vtl_connect(unsigned long serv_addr, unsigned short serv_port);
int vtl_close(unsigned long serv_addr);


int register_fd(SOCK fd);
int unregister_fd(SOCK fd);

#endif
