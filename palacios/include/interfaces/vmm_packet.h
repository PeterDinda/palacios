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
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu> 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMM_PACKET_H__
#define __VMM_PACKET_H__

#include <palacios/vmm.h>
#include <palacios/vmm_ethernet.h>

#define V3_ETHINT_NAMELEN 126

struct v3_packet {
    void * host_packet_data;
    void * guest_packet_data;

    char dev_mac[ETH_ALEN];
    int (*input)(struct v3_packet * packet, uint8_t * buf, uint32_t len);

    struct list_head node;
};

#ifdef __V3VEE__
#include <palacios/vmm.h>

struct v3_packet * v3_packet_connect(struct v3_vm_info * vm, const char * host_nic,
				     const char * mac,
				     int (*input)(struct v3_packet * packet, uint8_t * buf, uint32_t len),
				     void * guest_packet_data);

int v3_packet_send(struct v3_packet * packet, uint8_t * buf, uint32_t len);
void v3_packet_close(struct v3_packet * packet);

#endif

struct v3_packet_hooks {
    int (*connect)(struct v3_packet * packet, const char * host_nic, void * host_vm_data);
    int (*send)(struct v3_packet * packet, uint8_t * buf, uint32_t len);
    void (*close)(struct v3_packet * packet);
};

extern void V3_Init_Packet(struct v3_packet_hooks * hooks);

#endif
