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
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
//backend device for Virtio NIC, Direct Network Bridge

#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_sprintf.h>
#include <interfaces/vmm_packet.h>

#ifndef V3_CONFIG_DEBUG_NIC_BRIDGE
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

struct nic_bridge_state {
    struct v3_vm_info * vm;
    struct v3_dev_net_ops net_ops;
    struct v3_packet * packet_state;
};

static int bridge_send(uint8_t * buf, uint32_t len, 
		       void * private_data) {
    struct nic_bridge_state * bridge = (struct nic_bridge_state *)private_data;
    
#ifdef V3_CONFIG_DEBUG_NIC_BRIDGE
    {
    	PrintDebug("NIC Bridge: send pkt size: %d\n", len);
    	v3_hexdump(buf, len, NULL, 0);
    }
#endif
    
    return v3_packet_send(bridge->packet_state, buf, len);
}

static int packet_input(struct v3_packet * packet_state, uint8_t * pkt, uint32_t size) {
    struct nic_bridge_state * bridge = (struct nic_bridge_state *)packet_state->guest_packet_data;
    
#ifdef V3_CONFIG_DEBUG_NIC_BRIDGE
    {
    	PrintDebug("NIC Bridge: recv pkt size: %d\n", size);
    	v3_hexdump(pkt, size, NULL, 0);
    }
#endif
    
    return bridge->net_ops.recv(pkt, 
				size, 
				bridge->net_ops.config.frontend_data);
}


static int nic_bridge_free(struct nic_bridge_state * bridge) {
    /*TODO: detach from front device */
    
    v3_packet_close(bridge->packet_state);
    V3_Free(bridge);
	
    return 0;
}

static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))nic_bridge_free,
    
};

static int nic_bridge_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct nic_bridge_state * bridge = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * host_nic;
    
    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");
    
    v3_cfg_tree_t * hostnic_cfg = v3_cfg_subtree(cfg, "hostnic");
    host_nic = v3_cfg_val(hostnic_cfg, "name"); 
    if(host_nic == NULL) {
	host_nic = "eth0";
    }
    
    bridge = (struct nic_bridge_state *)V3_Malloc(sizeof(struct nic_bridge_state));

    if (!bridge) {
	PrintError("Cannot allocate in init\n");
	return -1;
    }

    memset(bridge, 0, sizeof(struct nic_bridge_state));
    
    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, bridge);
    
    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(bridge);
	return -1;
    }
    
    bridge->net_ops.send = bridge_send;
    bridge->vm = vm;
    
    if (v3_dev_connect_net(vm, v3_cfg_val(frontend_cfg, "tag"), 
			   &(bridge->net_ops), frontend_cfg, bridge) == -1) {
	PrintError("Could not connect %s to frontend %s\n", 
		   dev_id, v3_cfg_val(frontend_cfg, "tag"));
	v3_remove_device(dev);
	return -1;
    }
    
    PrintDebug("NIC-Bridge: Connect %s to frontend %s\n", 
	       dev_id, v3_cfg_val(frontend_cfg, "tag"));
    
    bridge->packet_state = v3_packet_connect(vm, host_nic, 
					     bridge->net_ops.config.fnt_mac, 
					     packet_input, 
					     (void *)bridge);
    
    if(bridge->packet_state == NULL){
	PrintError("NIC-Bridge: Error to connect to host ethernet device\n");
	return -1;
    }
    
    return 0;
}

device_register("NIC_BRIDGE", nic_bridge_init)
