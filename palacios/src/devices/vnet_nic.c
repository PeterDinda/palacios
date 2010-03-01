/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
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

#ifndef CONFIG_DEBUG_VNET_NIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


struct vnet_nic_state {
    char mac[6];
    struct guest_info * core;
    struct v3_dev_net_ops net_ops;
};

//used when virtio_nic get a packet from guest and send it to the backend
static int vnet_send(uint8_t * buf, uint32_t len, void * private_data, struct vm_device *dest_dev){
    struct v3_vnet_pkt * pkt = NULL;
    struct guest_info *core  = (struct guest_info *)private_data; 

    PrintDebug("Virtio VNET-NIC: send pkt size: %d\n", len);
#ifdef CONFIG_DEBUG_VNET_NIC
    v3_hexdump(buf, len, NULL, 0);
#endif

#ifdef CONFIG_VNET_PROFILE
    uint64_t start, end;
    rdtscll(start);
#endif

    pkt = (struct v3_vnet_pkt *)V3_Malloc(sizeof(struct v3_vnet_pkt));

    if(pkt == NULL){
	PrintError("Malloc() fails");
	return -1;
    }
	
#ifdef CONFIG_VNET_PROFILE
    {
    	rdtscll(end);
    	core->vnet_times.time_mallocfree = end - start;
    }
#endif


    pkt->size = len;
    memcpy(pkt->data, buf, pkt->size);

#ifdef CONFIG_VNET_PROFILE
    {
    	rdtscll(start);
    	core->vnet_times.time_copy_from_guest = start - end;
    }
#endif

    v3_vnet_send_pkt(pkt, (void *)core);

#ifdef CONFIG_VNET_PROFILE
    {
    	rdtscll(end);
    	core->vnet_times.vnet_handle_time = end - start;
    }
#endif

    V3_Free(pkt);

#ifdef CONFIG_VNET_PROFILE
    {
    	rdtscll(start);
    	core->vnet_times.time_mallocfree += start - end;
    }
#endif

    return 0;
}


static int virtio_input(struct v3_vm_info *info, struct v3_vnet_pkt * pkt, void * private_data){
    struct vnet_nic_state *vnetnic = (struct vnet_nic_state *)private_data;
	
    PrintDebug("Vnet-nic: In input: vnet_nic state %p\n", vnetnic);	

    return vnetnic->net_ops.recv(pkt->data, pkt->size, vnetnic->net_ops.frontend_data);
}

static int register_to_vnet(struct v3_vm_info * vm,
		     struct vnet_nic_state *vnet_nic,
		     char *dev_name,
		     uchar_t mac[6]) { 
   
    PrintDebug("Vnet-nic: register Vnet-nic device %s, state %p to VNET\n", dev_name, vnet_nic);
	
    return v3_vnet_add_dev(vm, mac, virtio_input, (void *)vnet_nic);
}

static int vnet_nic_free(struct vm_device * dev) {
    return 0;
}

static struct v3_device_ops dev_ops = {
    .free = vnet_nic_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};


static int str2mac(char *macstr, char mac[6]){
    char hex[2], *s = macstr;
    int i = 0;

    while(s){
	memcpy(hex, s, 2);
	mac[i++] = (char)atox(hex);
	if (i == 6) return 0;
	s=strchr(s, ':');
	if(s) s++;
    }

    return -1;
}

static int vnet_nic_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vnet_nic_state * vnetnic = NULL;
    char * name = v3_cfg_val(cfg, "name");
    char * macstr;
    char mac[6];
    int vnet_dev_id;

    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");
    macstr = v3_cfg_val(frontend_cfg, "mac");
    str2mac(macstr, mac);

    vnetnic = (struct vnet_nic_state *)V3_Malloc(sizeof(struct vnet_nic_state));
    memset(vnetnic, 0, sizeof(struct vnet_nic_state));

    struct vm_device * dev = v3_allocate_device(name, &dev_ops, vnetnic);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", name);
	return -1;
    }

    vnetnic->net_ops.send = vnet_send;
    memcpy(vnetnic->mac, mac, 6);
	
    if (v3_dev_connect_net(vm, v3_cfg_val(frontend_cfg, "tag"), 
			   &(vnetnic->net_ops), frontend_cfg, vnetnic) == -1) {
	PrintError("Could not connect %s to frontend %s\n", 
		   name, v3_cfg_val(frontend_cfg, "tag"));
	return -1;
    }

    PrintDebug("Vnet-nic: Connect %s to frontend %s\n", 
		   name, v3_cfg_val(frontend_cfg, "tag"));

    if ((vnet_dev_id = register_to_vnet(vm, vnetnic, name, vnetnic->mac)) == -1) {
	return -1;
    }

    PrintDebug("Vnet-nic device %s (mac: %s, %ld) initialized\n", name, macstr, *((ulong_t *)vnetnic->mac));


//for temporary hack
#if 1	
    {
	 //uchar_t tapmac[6] = {0x00,0x02,0x55,0x67,0x42,0x39}; //for Intel-VT test HW
    	uchar_t tapmac[6] = {0x6e,0xa8,0x75,0xf4,0x82,0x95};
    	uchar_t dstmac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
    	uchar_t zeromac[6] = {0,0,0,0,0,0};

	struct v3_vnet_route route;
       route.dst_id = vnet_dev_id;
	route.dst_type = LINK_INTERFACE;
	route.src_id = -1;
	route.src_type = LINK_ANY;

	if(!strcmp(name, "vnet_nicdom0")){
	    memcpy(route.dst_mac, tapmac, 6);
	    route.dst_mac_qual = MAC_NONE;
	    memcpy(route.src_mac, zeromac, 6);
	    route.src_mac_qual = MAC_ANY;
	   
	    v3_vnet_add_route(route);

	    memcpy(route.dst_mac, dstmac, 6);
	    route.dst_mac_qual = MAC_NONE;
	    memcpy(route.src_mac, tapmac, 6);
	    route.src_mac_qual = MAC_NOT;

	    v3_vnet_add_route(route);
	}

	if (!strcmp(name, "vnet_nic0")){
	    memcpy(route.dst_mac, zeromac, 6);
	    route.dst_mac_qual = MAC_ANY;
	    memcpy(route.src_mac, tapmac, 6);
	    route.src_mac_qual = MAC_NONE;
	   
	    v3_vnet_add_route(route);
	}
    }

#endif

    return 0;
}

device_register("VNET_NIC", vnet_nic_init)
