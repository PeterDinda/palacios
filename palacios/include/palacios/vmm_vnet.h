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
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu>
 *		  Yuan Tang <ytang@northwestern.edu>
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
#include <palacios/vmm_hashtable.h>
#include <palacios/vmm_sprintf.h>

typedef enum {MAC_ANY, MAC_NOT, MAC_NONE} mac_type_t; //for 'src_mac_qual' and 'dst_mac_qual'
typedef enum {LINK_INTERFACE, LINK_EDGE, LINK_ANY} link_type_t; //for 'type' and 'src_type' in struct routing

//routing table entry
struct v3_vnet_route {
    char src_mac[6];
    char dest_mac[6];

    mac_type_t src_mac_qual;
    mac_type_t dest_mac_qual;

    int link_idx; //link[dest] is the link to be used to send pkt
    link_type_t link_type; //EDGE|INTERFACE|ANY
 
    int src_link_idx;
    link_type_t src_type; //EDGE|INTERFACE|ANY
};


int v3_vnet_send_rawpkt(uchar_t *buf, int len, void *private_data);
int v3_vnet_send_udppkt(uchar_t *buf, int len, void *private_data);

int v3_vnet_add_route(struct v3_vnet_route *route);

//int v3_vnet_del_route();
//int v3_vnet_get_routes();


//int v3_vnet_add_link(struct v3_vnet_link link);

// int v3_vnet_del_link();
//int v3_vnet_get_link(int idx, struct vnet_link * link);


int v3_init_vnet();

//int v3_vnet_add_bridge(struct v3_vm_info * vm, uint8_t mac[6]);

int v3_vnet_add_node(struct v3_vm_info *info, 
	           char * dev_name, 
	           uchar_t mac[6], 
		    int (*netif_input)(struct v3_vm_info * vm, uchar_t * pkt, uint_t size, void * private_data), 
		    void * priv_data);


// temporary hack
int v3_vnet_pkt_process();


#endif


