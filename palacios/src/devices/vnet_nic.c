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
//backend device for Virtio NIC

#include <palacios/vmm_vnet.h>
#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <devices/lnx_virtio_pci.h>
#include <palacios/vm_guest_mem.h>
#include <devices/pci.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_ethernet.h>

#ifndef CONFIG_DEBUG_VNET_NIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

struct vnet_nic_state {
    struct v3_vm_info * vm;
    struct v3_dev_net_ops net_ops;
    int vnet_dev_id;
};


/* called by frontend, send pkt to VNET */
static int vnet_nic_send(uint8_t * buf, uint32_t len, 
			 int synchronize, void * private_data) {
    struct vnet_nic_state * vnetnic = (struct vnet_nic_state *)private_data;

    struct v3_vnet_pkt pkt;
    pkt.size = len;
    pkt.src_type = LINK_INTERFACE;
    pkt.src_id = vnetnic->vnet_dev_id;
    memcpy(pkt.header, buf, ETHERNET_HEADER_LEN);
    pkt.data = buf;

    V3_Net_Print(2, "VNET-NIC: send pkt (size: %d, src_id: %d, src_type: %d)\n", 
		   pkt.size, pkt.src_id, pkt.src_type);
    if(v3_net_debug >= 4){
	v3_hexdump(buf, len, NULL, 0);
    }

    return v3_vnet_send_pkt(&pkt, NULL, synchronize);
}


/* send pkt to frontend device */
static int virtio_input(struct v3_vm_info * info, 
			struct v3_vnet_pkt * pkt, 
			void * private_data){
    struct vnet_nic_state *vnetnic = (struct vnet_nic_state *)private_data;

    V3_Net_Print(2, "VNET-NIC: receive pkt (size %d, src_id:%d, src_type: %d, dst_id: %d, dst_type: %d)\n", 
		pkt->size, pkt->src_id, pkt->src_type, pkt->dst_id, pkt->dst_type);
	
    return vnetnic->net_ops.recv(pkt->data, pkt->size,
				 vnetnic->net_ops.frontend_data);
}


static int vnet_nic_free(struct vnet_nic_state * vnetnic) {

    v3_vnet_del_dev(vnetnic->vnet_dev_id);
    V3_Free(vnetnic);
	
    return 0;
}

static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))vnet_nic_free,

};

static struct v3_vnet_dev_ops vnet_dev_ops = {
    .input = virtio_input,
};


static int vnet_nic_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vnet_nic_state * vnetnic = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    int vnet_dev_id;

    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");

    vnetnic = (struct vnet_nic_state *)V3_Malloc(sizeof(struct vnet_nic_state));
    memset(vnetnic, 0, sizeof(struct vnet_nic_state));

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, vnetnic);
    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(vnetnic);
	return -1;
    }

    vnetnic->net_ops.send = vnet_nic_send;
    vnetnic->vm = vm;
	
    if (v3_dev_connect_net(vm, v3_cfg_val(frontend_cfg, "tag"), 
			   &(vnetnic->net_ops), frontend_cfg, vnetnic) == -1) {
	PrintError("Could not connect %s to frontend %s\n", 
		   dev_id, v3_cfg_val(frontend_cfg, "tag"));
	v3_remove_device(dev);
	return -1;
    }

    PrintDebug("Vnet-nic: Connect %s to frontend %s\n", 
	      dev_id, v3_cfg_val(frontend_cfg, "tag"));

    if ((vnet_dev_id = v3_vnet_add_dev(vm, vnetnic->net_ops.fnt_mac, &vnet_dev_ops, (void *)vnetnic)) == -1) {
	PrintError("Vnet-nic device %s fails to registered to VNET\n", dev_id);
	
	v3_remove_device(dev);
	return 0;
    }
    vnetnic->vnet_dev_id = vnet_dev_id;

    return 0;
}

device_register("VNET_NIC", vnet_nic_init)
