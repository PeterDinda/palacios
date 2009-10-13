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
 *            Yuan Tang <ytang@northwestern.edu>
 *		  Jack Lange <jarusl@cs.northwestern.edu> 
 *		  Peter Dinda <pdinda@northwestern.edu
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
#if 0

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


struct gen_queue * vnet_inpkt_q;

struct ethAddr{
  char addr[6];
};

typedef struct vnet_device{
	char name[50];
//	struct ethAddr device_addr;
	int (*input)(uchar_t * pkt, uint_t size);

	void *data;
	//....
}iface_t;

#define ROUTE 1
//#define ROUTE 0

#if ROUTE //new routing accrding to VNET-VTL, no hash --YT

//struct gen_queue *src_link_index_q;

#define SOCK int
#define MAX_LINKS 100
#define MAX_ROUTES 500
#define MAX_DEVICES 16

#define TCP_TYPE 0
#define UDP_TYPE 1

#define TCP_STR "TCP"
#define UDP_STR "UDP"

/*   
#define HANDLER_ERROR -1
#define HANDLER_SUCCESS 0
*/
/* These are the return codes for the Control Session */
/*
#define CTRL_DO_NOTHING 0
#define CTRL_CLOSE 1
#define CTRL_DELETE_TCP_SOCK 3
#define CTRL_ADD_TCP_SOCK 4
#define CTRL_EXIT 5
#define CTRL_ERROR -1
*/

#define ANY "any"
#define NOT "not"
#define NONE "none"
#define EMPTY "empty"

#define ANY_TYPE 0
#define NOT_TYPE 1
#define NONE_TYPE 2
#define EMPTY_TYPE 3

#define INTERFACE "INTERFACE"
#define EDGE "EDGE"
#define ANY_SRC "ANY"

#define INTERFACE_TYPE 0
#define EDGE_TYPE 1
#define ANY_SRC_TYPE 2

//This is the structure that stores the routing entries
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
//  int link_class;

  int use;
//  int authenticated;
  int type; //TCP=0, UDP=1,VTP=2, can be extended so on

  int next;
  int prev;
};

struct sock_list {
  SOCK sock;

  int next;
  int prev;
};

typedef struct sock_list con_t;

struct device_list {
  char *device_name;

  iface_t * vnet_device;

  int type;
  int use;

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

#endif

int V3_Send_pkt(uchar_t *buf, int length);
int V3_Register_pkt_event(int (*netif_input)(uchar_t * pkt, uint_t size));


int vnet_send_pkt(char *buf, int length);
int vnet_register_pkt_event(char *dev_name, int (*netif_input)(uchar_t * pkt, uint_t size), void *data);

//check if there are incoming packet in the queue for VNIC, if yes, send the packet to the VNIC
//this should put in the svm exit handler
int vnet_check();

void vnet_init();

#endif
#endif

