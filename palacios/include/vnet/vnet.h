
/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Lei Xia <lxia@northwestern.edu> 
 * Copyright (c) 2009, Yuan Tang <ytang@northwestern.edu> 
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
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

#include <palacios/vmm_ethernet.h>
#include <vnet/vnet_base.h>
#include <vnet/vnet_host.h>
#include <vnet/vnet_vmm.h>

#define MAC_NOSET 	0
#define MAC_ANY 	11
#define MAC_NOT 	12
#define MAC_NONE 	13 
#define MAC_ADDR 	14

#define LINK_NOSET	0
#define LINK_INTERFACE 	11
#define LINK_EDGE 	12 
#define LINK_ANY 	13

#define VNET_HASH_SIZE 	17

struct v3_vnet_route {
    uint8_t src_mac[ETH_ALEN];
    uint8_t dst_mac[ETH_ALEN];

    uint8_t src_mac_qual;
    uint8_t dst_mac_qual;

    int dst_id;
    uint8_t dst_type;
 
    int src_id;
    uint8_t src_type;
} __attribute__((packed));


struct v3_vnet_pkt {
    uint32_t size; 
    
    uint8_t dst_type;
    uint32_t dst_id;

    /*
     * IMPORTANT The next three fields must be grouped and packed together
     *  They are used to generate a hash value 
     */
    union {
	uint8_t hash_buf[VNET_HASH_SIZE];
	struct {
	    uint8_t src_type;
	    uint32_t src_id;
	    uint8_t header[ETHERNET_HEADER_LEN];
	    uint8_t * data;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct vnet_stat{
    uint64_t tx_bytes;
    uint32_t tx_pkts;
    uint64_t rx_bytes;
    uint32_t rx_pkts;
};


struct v3_vnet_bridge_ops {
    int (*input)(struct v3_vm_info * vm, 
		 struct v3_vnet_pkt * pkt,
		 void * private_data);
    void (*poll)(struct v3_vm_info * vm,  
		 void * private_data);
};

#define HOST_LNX_BRIDGE 1
#define CTL_VM_BRIDGE 	2

int v3_vnet_add_bridge(struct v3_vm_info * vm,
		       struct v3_vnet_bridge_ops * ops,
		       uint8_t type,
		       void * priv_data);

void v3_vnet_del_bridge(uint8_t type);

int v3_vnet_add_route(struct v3_vnet_route route);
void v3_vnet_del_route(uint32_t route_idx);

int v3_vnet_send_pkt(struct v3_vnet_pkt * pkt, void * private_data);
int v3_vnet_find_dev(uint8_t  * mac);
int v3_vnet_stat(struct vnet_stat * stats);

#ifdef __V3VEE__

struct v3_vnet_dev_ops {
    int (*input)(struct v3_vm_info * vm, 
		 struct v3_vnet_pkt * pkt, 
		 void * dev_data);

    /* return >0 means there are more pkts in the queue to be sent */
    int (*poll)(struct v3_vm_info * vm,
		int quote,
		void * dev_data);
};

int v3_init_vnet(void);	
void v3_deinit_vnet(void);

int v3_vnet_add_dev(struct v3_vm_info * info, uint8_t * mac, 
		    struct v3_vnet_dev_ops * ops, int quote, int poll_state,
		    void * priv_data);
int v3_vnet_del_dev(int dev_id);


#endif

#endif


