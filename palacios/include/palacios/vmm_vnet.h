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

#ifdef __V3VEE__

#include <palacios/vmm.h>


#define V3_VNET_POLLING_VECTOR	50

typedef enum {MAC_ANY=0, MAC_NOT, MAC_NONE, MAC_ADDR} mac_type_t; //for 'src_mac_qual' and 'dst_mac_qual'
typedef enum {LINK_INTERFACE=0, LINK_EDGE, LINK_ANY} link_type_t; //for 'type' and 'src_type' in struct routing


#define VNET_HASH_SIZE 17
#define ETHERNET_HEADER_LEN 14
#define ETHERNET_MTU   6000
#define ETHERNET_PACKET_LEN (ETHERNET_HEADER_LEN + ETHERNET_MTU)

//routing table entry
struct v3_vnet_route {
    uint8_t src_mac[6];
    uint8_t dst_mac[6];

    uint8_t src_mac_qual;
    uint8_t dst_mac_qual;

    uint32_t dst_id; //link[dest] is the link to be used to send pkt
    uint8_t dst_type; //EDGE|INTERFACE|ANY
 
    uint32_t src_id;
    uint8_t src_type; //EDGE|INTERFACE|ANY
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
	    uint8_t *data;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


#ifdef CONFIG_VNET_PROFILE
struct v3_vnet_profile{
    uint64_t  time_copy_from_guest;
    uint64_t  time_route_lookup;
    uint64_t  time_mallocfree;
    uint64_t  time_copy_to_guest;
    uint64_t  total_handle_time;
    uint64_t  memcpy_time;

    uint64_t  total_exit_time;
    bool print;

    uint64_t virtio_handle_start;
};
#endif

struct v3_vnet_bridge_input_args{
    struct v3_vm_info * vm;
    struct v3_vnet_pkt *vnet_pkts; 
    uint16_t pkt_num;
    void * private_data;
};

int v3_vnet_send_pkt(struct v3_vnet_pkt * pkt, void *private_data);

void v3_vnet_send_pkt_xcall(void * data);

int v3_vnet_add_route(struct v3_vnet_route route);

int V3_init_vnet();

int v3_vnet_add_bridge(struct v3_vm_info * vm,
		int (*input)(struct v3_vm_info * vm, struct v3_vnet_pkt pkt[], uint16_t pkt_num, void * private_data),
		void (*xcall_input)(void *data),
		int (*poll_pkt)(struct v3_vm_info * vm, void * private_data),
		uint16_t max_delayed_pkts,
		long max_latency,
		void * priv_data);

int v3_vnet_add_dev(struct v3_vm_info *info, uint8_t mac[6], 
		    int (*dev_input)(struct v3_vm_info * vm, struct v3_vnet_pkt * pkt, void * private_data), 
		    void * priv_data);

void v3_vnet_heartbeat(struct guest_info *core);


int v3_vnet_disable_bridge();
int v3_vnet_enable_bridge();

void v3_vnet_polling();

int v3_vnet_rx(uchar_t *buf, uint16_t size, uint16_t src_id, uint8_t src_type);


#endif

#endif


