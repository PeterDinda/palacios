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
#include <palacios/vmm_sprintf.h>


#define ETHERNET_HEADER_LEN 14
#define ETHERNET_DATA_MIN   46
#define ETHERNET_DATA_MAX   1500
#define ETHERNET_PACKET_LEN (ETHERNET_HEADER_LEN + ETHERNET_DATA_MAX)

#define SOCK int

//the routing entry
struct routing{
    char src_mac[6];
    char dest_mac[6];

    int src_mac_qual;
    int dest_mac_qual;

    int dest; //link[dest] is the link to be used to send pkt
    int type; //EDGE|INTERFACE|ANY
 
    int src;
    int src_type; //EDGE|INTERFACE|ANY

    int use;

    int next;
    int prev;
  //  struct list_head entry_list;
};

//struct  gen_route {
 //   uint_t num_entries;
  //  struct list_head entries;
//}

 //This is the structure that stores the topology 
struct topology {
    SOCK link_sock;

    unsigned long dest;

    // Port for UDP
    unsigned short remote_port;

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

struct ethAddr{
  char addr[6];
};

struct vnet_if_device {
    char name[50];
    struct ethAddr device_addr;
    
    int (*input)(uchar_t * pkt, uint_t size);
    
    void * data;
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


int v3_Send_pkt(uchar_t *buf, int length);
int v3_Register_pkt_event(int (*netif_input)(uchar_t * pkt, uint_t size));


int vnet_send_pkt(char *buf, int length);
int vnet_register_pkt_event(char *dev_name, int (*netif_input)(uchar_t * pkt, uint_t size), void *data);

int v3_vnet_pkt_process();

void v3_vnet_init();

#endif


