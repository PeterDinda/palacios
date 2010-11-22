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
#include <palacios/vmm_packet.h>

#ifndef CONFIG_DEBUG_NIC_BRIDGE
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

struct nic_bridge_state {
    struct v3_vm_info * vm;
    struct v3_dev_net_ops net_ops;
};

static int bridge_send(uint8_t * buf, uint32_t len, 
		       void * private_data, struct vm_device *dev){
    //struct nic_bridge_state *bridge = (struct nic_bridge_state *)private_data;

#ifdef CONFIG_DEBUG_NIC_BRIDGE
    {
    	PrintDebug("NIC Bridge: send pkt size: %d\n", len);
    	v3_hexdump(buf, len, NULL, 0);
    }
#endif

    return V3_send_raw(buf, len);
}


static int packet_input(struct v3_vm_info * vm,
			struct v3_packet_event * evt, 
			void * private_data) {
    struct nic_bridge_state *bridge = (struct nic_bridge_state *)private_data;

    PrintDebug("NIC_BRIDGE: Incoming packet size: %d\n", evt->size);

    return bridge->net_ops.recv(evt->pkt, 
	evt->size, 
	bridge->net_ops.frontend_data);
}


static int vnet_nic_free(struct vm_device * dev) {
    struct nic_bridge_state * bridge = dev->private_data;

    /*detach from front device */

    V3_Free(bridge);
	
    return 0;
}

static struct v3_device_ops dev_ops = {
    .free = vnet_nic_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};

static int vnet_nic_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct nic_bridge_state * bridge = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");

    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");
	
    bridge = (struct nic_bridge_state *)V3_Malloc(sizeof(struct nic_bridge_state));
    memset(bridge, 0, sizeof(struct nic_bridge_state));

    struct vm_device * dev = v3_allocate_device(dev_id, &dev_ops, bridge);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", dev_id);
	return -1;
    }

    bridge->net_ops.send = bridge_send;
    bridge->vm = vm;
	
    if (v3_dev_connect_net(vm, v3_cfg_val(frontend_cfg, "tag"), 
			   &(bridge->net_ops), frontend_cfg, bridge) == -1) {
	PrintError("Could not connect %s to frontend %s\n", 
		   dev_id, v3_cfg_val(frontend_cfg, "tag"));
	return -1;
    }

    PrintDebug("NIC-Bridge: Connect %s to frontend %s\n", 
	      dev_id, v3_cfg_val(frontend_cfg, "tag"));

    v3_hook_host_event(vm, HOST_PACKET_EVT, V3_HOST_EVENT_HANDLER(packet_input), dev);

    return 0;
}

device_register("NIC_BRIDGE", vnet_nic_init)
