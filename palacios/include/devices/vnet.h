/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Lei Xia <lxia@northwestern.edu> 
 * Copyright (c) 2009, Yuan Tang <ytang@northwestern.edu> 
 * Copyright (c) 2009, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2009, Peter Dinda <pdinda@northwestern.edu
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu>
 *		  Yuan Tang <ytang@northwestern.edu>
 *		  Jack Lange <jarusl@cs.northwestern.edu> 
 *		  Peter Dinda <pdinda@northwestern.edu
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VNET_H__
#define __VNET_H__

#include <palacios/vmm.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm_queue.h>
#include <palacios/vmm_socket.h>
#include <palacios/vmm_hashtable.h>


#define ETHERNET_HEADER_LEN 14
#define ETHERNET_DATA_MIN   46
#define ETHERNET_DATA_MAX   1500
#define ETHERNET_PACKET_LEN (ETHERNET_HEADER_LEN+ETHERNET_DATA_MAX)

struct ethAddr{
  char addr[6];
};

#define SOCK int

#define TCP_TYPE 0
#define UDP_TYPE 1

#define TCP_STR "TCP"
#define UDP_STR "UDP"

/*   
#define HANDLER_ERROR -1
#define HANDLER_SUCCESS 0
*/

//the routing entry
struct routing {
  char src_mac[6];
  char dest_mac[6];

  int src_mac_qual;
  int dest_mac_qual;

  int dest;
  int type; //EDGE_TYPE|INTERFACE_TYPE
 
  int src;
  int src_type;

  int use;

  int next;
  int prev;
};

 //This is the structure that stores the topology 
struct topology {
  SOCK link_sock;

  unsigned long dest;

  // Port for UDP
  unsigned short remote_port;

  // LINK OR GATEWAY
  // int link_class;

  int use;
  int type; //TCP=0, UDP=1,VTP=2, can be extended so on

  int next;
  int prev;
};

struct sock_list {
  SOCK sock;

  int next;
  int prev;
};


#define GENERAL_NIC 0

struct vnet_if_device{
	char name[50];
	struct ethAddr device_addr;

	int (*input)(uchar_t * pkt, uint_t size);

	void *data;
};


struct device_list {
  struct vnet_if_device *device;

  int use;
  int type;

  int next;
  int prev;
};

// 14 (ethernet frame) + 20 bytes
struct HEADERS {
  char ethernetdest[6];
  char ethernetsrc[6];
  unsigned char ethernettype[2]; // indicates layer 3 protocol type
  char ip[20];
};

#define FOREACH(iter, list, start) for (iter = start; iter != -1; iter = list[iter].next)
#define FOREACH_SOCK(iter, socks, start) FOREACH(iter, socks, start)
#define FOREACH_LINK(iter, links, start) FOREACH(iter, links, start)
#define FOREACH_ROUTE(iter, routes, start) FOREACH(iter, routes, start)
#define FOREACH_DEVICE(iter, devices, start) FOREACH(iter, devices, start)


int V3_Send_pkt(uchar_t *buf, int length);
int V3_Register_pkt_event(int (*netif_input)(uchar_t * pkt, uint_t size));


int vnet_send_pkt(char *buf, int length);
int vnet_register_pkt_event(char *dev_name, int (*netif_input)(uchar_t * pkt, uint_t size), void *data);

int vnet_pkt_process();

void vnet_init();

#endif


