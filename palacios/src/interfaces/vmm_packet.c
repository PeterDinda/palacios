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
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>
#include <interfaces/vmm_packet.h>

static struct v3_packet_hooks * packet_hooks = 0;

void V3_Init_Packet(struct v3_packet_hooks * hooks) {
    packet_hooks = hooks;
    PrintDebug("V3 raw packet interface inited\n");

    return;
}

struct v3_packet * v3_packet_connect(struct v3_vm_info * vm, 
				     const char * host_nic, 
				     const char * vm_mac,
				     int (*input)(struct v3_packet * packet, uint8_t * buf, uint32_t len),
				     void * guest_packet_data) {
    struct v3_packet * packet = NULL;

    V3_ASSERT(packet_hooks != NULL);
    V3_ASSERT(packet_hooks->connect != NULL);

    packet = V3_Malloc(sizeof(struct v3_packet));

    memcpy(packet->dev_mac, vm_mac, ETH_ALEN);
    packet->input = input;
    packet->guest_packet_data = guest_packet_data;
    if(packet_hooks->connect(packet, host_nic, vm->host_priv_data) != 0){
	V3_Free(packet);
	return NULL;
    }

    return packet;
}

int v3_packet_send(struct v3_packet * packet, uint8_t * buf, uint32_t len) {
    V3_ASSERT(packet_hooks != NULL);
    V3_ASSERT(packet_hooks->send != NULL);
    
    return packet_hooks->send(packet, buf, len);
}

void v3_packet_close(struct v3_packet * packet) {
    V3_ASSERT(packet_hooks != NULL);
    V3_ASSERT(packet_hooks->close != NULL);
    
    packet_hooks->close(packet);
    V3_Free(packet);
}


