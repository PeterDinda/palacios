/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it under the terms of the GNU General Public License
 * Version 2 (GPLv2).  The accompanying COPYING file contains the
 * full text of the license.
 */
/* 
 * Palacios Raw Packet Interface Implementation
 */
#include <asm/uaccess.h>
#include <linux/inet.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/if_packet.h>
#include <linux/errno.h>
#include <asm/msr.h>
 
#include <interfaces/vmm_packet.h>
#include <palacios/vmm_host_events.h>
#include <vnet/vnet.h>


#define __V3VEE__
#include <palacios/vmm_hashtable.h>
#include <palacios/vmm_ethernet.h>
#undef __V3VEE__

#include "palacios.h"
#include "linux-exts.h"

struct palacios_packet_state {
    struct socket * raw_sock;
    uint8_t inited;
	
    struct hashtable * mac_vm_cache;
    struct task_struct * server_thread;
};

static struct palacios_packet_state packet_state;

static inline uint_t hash_fn(addr_t hdr_ptr) {    
    uint8_t * hdr_buf = (uint8_t *)hdr_ptr;

    return v3_hash_buffer(hdr_buf, ETH_ALEN);
}

static inline int hash_eq(addr_t key1, addr_t key2) {	
    return (memcmp((uint8_t *)key1, (uint8_t *)key2, ETH_ALEN) == 0);
}


static int palacios_packet_add_recver(const char * mac, 
					struct v3_vm_info * vm){
    char * key;

    key = (char *)kmalloc(ETH_ALEN, GFP_KERNEL);					
    memcpy(key, mac, ETH_ALEN);    

    if (v3_htable_insert(packet_state.mac_vm_cache, (addr_t)key, (addr_t)vm) == 0) {
	printk("Palacios Packet: Failed to insert new mac entry to the hash table\n");
	return -1;
    }

    printk("Packet: Add MAC: %2x:%2x:%2x:%2x:%2x:%2x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    return 0;
}

static int palacios_packet_del_recver(const char * mac,
				      struct v3_vm_info * vm){

    return 0;
}

static int init_raw_socket(const char * eth_dev){
    int err;
    struct sockaddr_ll sock_addr;
    struct ifreq if_req;
    int dev_idx;

    err = sock_create(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL), &(packet_state.raw_sock)); 

    if (err < 0) {
	printk(KERN_WARNING "Could not create a PF_PACKET Socket, err %d\n", err);
	return -1;
    }

    if (eth_dev == NULL){
	eth_dev = "eth0"; /* default "eth0" */
    }

    memset(&if_req, 0, sizeof(if_req));
    strncpy(if_req.ifr_name, eth_dev, IFNAMSIZ);  //sizeof(if_req.ifr_name));

    err = packet_state.raw_sock->ops->ioctl(packet_state.raw_sock, SIOCGIFINDEX, (long)&if_req);

    if (err < 0){
	printk(KERN_WARNING "Palacios Packet: Unable to get index for device %s, error %d\n", 
	       if_req.ifr_name, err);
	dev_idx = 2; /* match ALL  2:"eth0" */
    } else {
	dev_idx = if_req.ifr_ifindex;
    }

    printk(KERN_INFO "Palacios Packet: bind to device index: %d\n", dev_idx);

    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sll_family = PF_PACKET;
    sock_addr.sll_protocol = htons(ETH_P_ALL);
    sock_addr.sll_ifindex = dev_idx;

    err = packet_state.raw_sock->ops->bind(packet_state.raw_sock, 
					   (struct sockaddr *)&sock_addr, 
					   sizeof(sock_addr));

    if (err < 0){
	printk(KERN_WARNING "Error binding raw packet to device %s, %d\n", eth_dev, err);
	return -1;
    }

    printk(KERN_INFO "Bind palacios raw packet interface to device %s\n", eth_dev);

    return 0;
}


static int
palacios_packet_send(const char * pkt, unsigned int len, void * private_data) {
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int size = 0;	
	
    iov.iov_base = (void *)pkt;
    iov.iov_len = (__kernel_size_t)len;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_flags = 0;

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    size = sock_sendmsg(packet_state.raw_sock, &msg, len);
    set_fs(oldfs);


    return size;
}


static struct v3_packet_hooks palacios_packet_hooks = {
    .send	= palacios_packet_send,
    .add_recver = palacios_packet_add_recver,
    .del_recver = palacios_packet_del_recver,
};


static int 
recv_pkt(char * pkt, int len) {
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int size = 0;
    
    if (packet_state.raw_sock == NULL) {
	return -1;
    }

    iov.iov_base = pkt;
    iov.iov_len = len;
    
    msg.msg_flags = 0;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    size = sock_recvmsg(packet_state.raw_sock, &msg, len, msg.msg_flags);
    set_fs(oldfs);
    
    return size;
}


static void
send_raw_packet_to_palacios(char * pkt,
			       int len,
			       struct v3_vm_info * vm) {
    struct v3_packet_event event;
    char data[ETHERNET_PACKET_LEN];

    /* one memory copy */
    memcpy(data, pkt, len);
    event.pkt = data;
    event.size = len;
	
    v3_deliver_packet_event(vm, &event);
}

static int packet_server(void * arg) {
    char pkt[ETHERNET_PACKET_LEN];
    int size;
    struct v3_vm_info *vm;

    printk("Palacios Raw Packet Bridge: Staring receiving server\n");

    while (!kthread_should_stop()) {
	size = recv_pkt(pkt, ETHERNET_PACKET_LEN);

	if (size < 0) {
	    printk(KERN_WARNING "Palacios raw packet receive error, Server terminated\n");
	    break;
	}


	/* if VNET is enabled, send to VNET */
	// ...


	/* if it is broadcast or multicase packet */
	// ...


	vm = (struct v3_vm_info *)v3_htable_search(packet_state.mac_vm_cache, (addr_t)pkt);

	if (vm != NULL){
	    printk("Find destinated VM 0x%p\n", vm);
   	    send_raw_packet_to_palacios(pkt, size, vm);
	}
    }

    return 0;
}


static int packet_init( void ) {

    const char * eth_dev = NULL;

    if (packet_state.inited == 0){
	packet_state.inited = 1;

	if (init_raw_socket(eth_dev) == -1){
	    printk("Error to initiate palacios packet interface\n");
	    return -1;
	}
	
	V3_Init_Packet(&palacios_packet_hooks);

	packet_state.mac_vm_cache = v3_create_htable(0, &hash_fn, &hash_eq);

	packet_state.server_thread = kthread_run(packet_server, NULL, "raw-packet-server");
    }
	

    // REGISTER GLOBAL CONTROL to add interfaces...

    return 0;
}

static int packet_deinit( void ) {


    kthread_stop(packet_state.server_thread);
    packet_state.raw_sock->ops->release(packet_state.raw_sock);
    v3_free_htable(packet_state.mac_vm_cache, 0, 1);
    packet_state.inited = 0;

    return 0;
}



static struct linux_ext pkt_ext = {
    .name = "PACKET_INTERFACE",
    .init = packet_init,
    .deinit = packet_deinit,
    .guest_init = NULL,
    .guest_deinit = NULL
};


register_extension(&pkt_ext);
