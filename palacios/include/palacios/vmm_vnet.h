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

#include <palacios/vmm.h>
#include <palacios/vmm_ethernet.h>

#define MAC_ANY 	0
#define MAC_NOT 	1
#define MAC_NONE 	2 
#define MAC_ADDR 	3

#define LINK_INTERFACE 	0
#define LINK_EDGE 	1 
#define LINK_ANY 	2

#define VNET_HASH_SIZE 	17

//routing table entry
struct v3_vnet_route {
    uint8_t src_mac[ETH_ALEN];
    uint8_t dst_mac[ETH_ALEN];

    uint8_t src_mac_qual;
    uint8_t dst_mac_qual;

    uint32_t dst_id;
    uint8_t dst_type;
 
    uint32_t src_id;
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
int v3_vnet_add_route(struct v3_vnet_route route);
int v3_vnet_send_pkt(struct v3_vnet_pkt * pkt, void * private_data);

#ifdef __V3VEE__

#define VMM_DRIVERN 	1
#define GUEST_DRIVERN 	0

struct v3_vnet_dev_ops {
    int (*input)(struct v3_vm_info * vm, 
		struct v3_vnet_pkt * pkt, 
		void * dev_data);
    void (*poll) (struct v3_vm_info * vm, void * dev_data);
};

int v3_init_vnet(void);	
void v3_deinit_vnet(void);

void v3_vnet_poll(struct v3_vm_info * vm);

int v3_vnet_add_dev(struct v3_vm_info * info, uint8_t * mac, 
		    struct v3_vnet_dev_ops * ops,
		    void * priv_data);
int v3_vnet_del_dev(int dev_id);

#endif

#endif


