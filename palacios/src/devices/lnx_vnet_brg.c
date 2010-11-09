/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Lei Xia <lxia@cs.northwestern.edu>
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
 
 /* VNET backend bridge for Linux host
 */
#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_vnet.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_socket.h>

#ifndef CONFIG_DEBUG_VNET_LNX_BRIGE
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

typedef enum {SOCK_UDP, SOCK_TCP, SOCK_OTHER} sock_type_t; 

const uint16_t vnet_udp_port = 20003;

struct vnet_link {
    uint32_t dst_ip;
    uint16_t dst_port;
    
    int socket;
    sock_type_t type;
    int link_idx;

    struct list_head node;
};

struct vnet_brg_state {
    uint32_t num_links; 

    struct list_head link_list;

    int serv_sock;
    sock_type_t serv_sock_type;
    int serv_port;
    
    /* The thread recving pkts from sockets. */
    int serv_thread;

    v3_lock_t lock;

    unsigned long pkt_sent, pkt_recv, pkt_drop;
};

static struct vnet_brg_state lnxbrg_state;

static int vnet_lnxbrg_reset(struct vnet_brg_state * state) {
    memset(state, 0, sizeof(struct vnet_brg_state));

    state->num_links = 0;
    state->serv_sock = -1;
    state->serv_port = vnet_udp_port;
    state->serv_sock_type = SOCK_UDP;

    if(v3_lock_init(&(state->lock)) < 0){
	PrintError("VNET Linux Bridge: error to initiate vnet lock\n");
    }

    return 0;
}


struct vnet_link * link_by_ip(struct vnet_brg_state * state, uint32_t ip) {
    struct vnet_link * link = NULL;

    list_for_each_entry(link, &(state->link_list), node) {
	if (link->dst_ip == ip) {
	    return link;
	}
    }

    return NULL;
}

struct vnet_link * link_by_idx(struct vnet_brg_state * state, int idx) {
    struct vnet_link * link = NULL;

    list_for_each_entry(link, &(state->link_list), node) {
	if (link->link_idx == idx) {
	    return link;
	}
    }
    return NULL;
}

static int
udp_send(int sockid, uint32_t dst_ip, 
 			uint16_t dst_port, uchar_t * pkt, 
 			uint16_t len){
    if(dst_ip > 0 && dst_port > 0){
	return V3_SendTo_IP(sockid, dst_ip, dst_port, pkt, len);
    }

    return V3_Send(sockid, pkt, len);
}


static int 
udp_recv(int sockid, uint32_t * src_ip, 
			uint16_t * src_port, uchar_t *buf, 
			uint16_t len) {

    return V3_Recv(sockid, buf, len);
}



static int 
brg_send(struct vnet_brg_state * state,
			struct v3_vm_info * vm,  
			struct v3_vnet_pkt * vnet_pkt, 
			void * private_data){
    struct vnet_link * link = NULL;
	
    #ifdef CONFIG_DEBUG_VNET_LNX_BRIGE
    {
	PrintDebug("vnet_brg_send... pkt size: %d, link: %d, struct len: %d\n",
			vnet_pkt->size,
			vnet_pkt->dst_id,
			sizeof(struct v3_vnet_pkt));
    }
    #endif

    state->pkt_recv ++;
    link = link_by_idx(vnet_pkt->dst_id);
    if (link != NULL) {
	if(link->type == SOCK_TCP){
		
	}else if(link->type == SOCK_UDP){
	    udp_send(link->socket, 0, 0, vnet_pkt->data, vnet_pkt->size);
	    state->pkt_sent ++;
	}else {
	    PrintError("VNET Linux Bridge: wrong link type\n"); 
	    return -1;
	}
    } else {
	PrintDebug("VNET Linux Bridge: wrong dst link, idx: %d, discards the packet\n", vnet_pkt->dst_id);
	state->pkt_drop ++;
    }

    return 0;
}


static int init_serv(struct vnet_brg_state * state) {
    int sock, err;

    if(state->serv_sock_type == SOCK_UDP){
    	sock = V3_Create_UDP_Socket();
    	if (sock < 0) {
	    PrintError("Could not create socket, Initiate VNET server error\n");
	    return -1;
    	}

    	err = V3_Bind_Socket(sock, state->serv_port);
    	if(err < 0){
	    PrintError("Error to bind VNET Linux bridge receiving UDP socket\n");
	    return -1;
    	}
	state->serv_sock = sock;
    }

    return 0;
}

static int vnet_server(void * arg) {
    struct v3_vnet_pkt pkt;
    struct vnet_link *link;
    struct vnet_brg_state * state = (struct vnet_brg_state *)arg;
    uchar_t buf[ETHERNET_MTU];
    uint32_t ip = 0;
    uint16_t port = 0;
    int len;

    while (1) {
	len = udp_recv(state->serv_sock, &ip, &port, buf, ETHERNET_MTU);
	if(len < 0) {
 	    PrintError("VNET Linux Bridge: receive error\n");
	    continue;
	}

	link = link_by_ip(ip);
	if (link != NULL) {
	    pkt.src_id= link->link_idx;
	}
	else { 
	    pkt.src_id= -1;
	}

	pkt.size = len;
	pkt.src_type = LINK_EDGE;
	memcpy(pkt.header, buf, ETHERNET_HEADER_LEN);
	pkt.data = buf;
	
	#ifdef CONFIG_DEBUG_VNET_LNX_BRIGE
    	{
	    PrintDebug("VNET Linux Bridge: recv pkt size: %d, pkt src_id: %d\n", 
			len,  pkt.src_id);
	    v3_hexdump(buf, len, NULL, 0);
	}
	#endif

	v3_vnet_send_pkt(&pkt, NULL);
	
	state->pkt_recv ++;
    }

    return 0;
}


static int vnet_lnxbrg_init() {
    struct v3_vnet_bridge_ops brg_ops;
    struct vnet_brg_state * state;
	
    state = (struct vnet_brg_state *)V3_Malloc(sizeof(struct vnet_brg_state));
    V3_ASSERT(state != NULL);

    vnet_lnxbrg_reset(state);

    brg_ops.input = brg_send;

    v3_vnet_add_bridge(NULL, &brg_ops, state);

    init_serv();
    V3_CREATE_THREAD(vnet_server, state, "VNET_LNX_BRIDGE");

    PrintDebug("VNET Linux Bridge initiated\n");

    return 0;
}


device_register("LNX_VNET_BRIDGE", vnet_lnxbrg_init)
