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
    struct v3_vm_info * vm;
    struct v3_dev_net_ops net_ops;
    int vnet_dev_id;
};


static int vnet_send(uint8_t * buf, uint32_t len, void * private_data, struct vm_device *dest_dev){
    struct vnet_nic_state *vnetnic = (struct vnet_nic_state *)private_data;

    struct v3_vnet_pkt pkt;
    pkt.size = len;
    pkt.src_type = LINK_INTERFACE;
    pkt.src_id = vnetnic->vnet_dev_id;
    memcpy(pkt.header, buf, ETHERNET_HEADER_LEN);
    pkt.data = buf;

#ifdef CONFIG_DEBUG_VNET_NIC
    {
    	PrintDebug("Virtio VNET-NIC: send pkt size: %d, pkt src_id: %d\n", 
			len,  vnetnic->vnet_dev_id);
    	v3_hexdump(buf, len, NULL, 0);
    }
#endif
/*
    v3_vnet_rx(buf, len, vnetnic->vnet_dev_id, LINK_INTERFACE);
	
    //if on the dom0 core, interrupt the domU core to poll pkts
    //otherwise, call the polling directly
    int cpu = V3_Get_CPU();
    cpu = (cpu == 0)?1:0;
    v3_interrupt_cpu(vnetnic->vm, cpu, V3_VNET_POLLING_VECTOR);
 */
 
    v3_vnet_send_pkt(&pkt, NULL);

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
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * macstr = NULL;
    char mac[6];
    int vnet_dev_id = 0;

    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");
    macstr = v3_cfg_val(frontend_cfg, "mac");

    if (macstr == NULL) {
	PrintDebug("Vnet-nic: No Mac specified\n");
    } else {
    	str2mac(macstr, mac);
    }

    vnetnic = (struct vnet_nic_state *)V3_Malloc(sizeof(struct vnet_nic_state));
    memset(vnetnic, 0, sizeof(struct vnet_nic_state));

    struct vm_device * dev = v3_allocate_device(dev_id, &dev_ops, vnetnic);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", dev_id);
	return -1;
    }

    vnetnic->net_ops.send = vnet_send;
    memcpy(vnetnic->mac, mac, 6);
    vnetnic->vm = vm;
	
    if (v3_dev_connect_net(vm, v3_cfg_val(frontend_cfg, "tag"), 
			   &(vnetnic->net_ops), frontend_cfg, vnetnic) == -1) {
	PrintError("Could not connect %s to frontend %s\n", 
		   dev_id, v3_cfg_val(frontend_cfg, "tag"));
	return -1;
    }

    PrintDebug("Vnet-nic: Connect %s to frontend %s\n", 
		   dev_id, v3_cfg_val(frontend_cfg, "tag"));

    if ((vnet_dev_id = register_to_vnet(vm, vnetnic, dev_id, vnetnic->mac)) == -1) {
	PrintError("Vnet-nic device %s (mac: %s) fails to registered to VNET\n", dev_id, macstr);
    }
    vnetnic->vnet_dev_id = vnet_dev_id;

    PrintDebug("Vnet-nic device %s (mac: %s, %ld) registered to VNET\n", dev_id, macstr, *((ulong_t *)vnetnic->mac));


//for temporary hack for vnet bridge test
#if 1
    {
    	uchar_t zeromac[6] = {0,0,0,0,0,0};
		
	if(!strcmp(dev_id, "vnet_nic")){
	    struct v3_vnet_route route;
		
	    route.dst_id = vnet_dev_id;
	    route.dst_type = LINK_INTERFACE;
	    route.src_id = 0;
	    route.src_type = LINK_EDGE;
	    memcpy(route.dst_mac, zeromac, 6);
	    route.dst_mac_qual = MAC_ANY;
	    memcpy(route.src_mac, zeromac, 6);
	    route.src_mac_qual = MAC_ANY;  
	    v3_vnet_add_route(route);


	    route.dst_id = 0;
	    route.dst_type = LINK_EDGE;
	    route.src_id = vnet_dev_id;
	    route.src_type = LINK_INTERFACE;
	    memcpy(route.dst_mac, zeromac, 6);
	    route.dst_mac_qual = MAC_ANY;
	    memcpy(route.src_mac, zeromac, 6);
	    route.src_mac_qual = MAC_ANY;

	    v3_vnet_add_route(route);
	}
    }
#endif

//for temporary hack for Linux bridge (w/o encapuslation) test
#if 0
    {
 	static int vnet_nic_guestid = -1;
	static int vnet_nic_dom0 = -1;
    	uchar_t zeromac[6] = {0,0,0,0,0,0};
		
	if(!strcmp(dev_id, "vnet_nic")){ //domu
	    vnet_nic_guestid = vnet_dev_id;
	}
	if (!strcmp(dev_id, "vnet_nic_dom0")){
	    vnet_nic_dom0 = vnet_dev_id;
	}

	if(vnet_nic_guestid != -1 && vnet_nic_dom0 !=-1){
	    struct v3_vnet_route route;
		
	    route.src_id = vnet_nic_guestid;
	    route.src_type = LINK_INTERFACE;
	    route.dst_id = vnet_nic_dom0;
	    route.dst_type = LINK_INTERFACE;
	    memcpy(route.dst_mac, zeromac, 6);
	    route.dst_mac_qual = MAC_ANY;
	    memcpy(route.src_mac, zeromac, 6);
	    route.src_mac_qual = MAC_ANY;  
	    v3_vnet_add_route(route);


	    route.src_id = vnet_nic_dom0;
	    route.src_type = LINK_INTERFACE;
	    route.dst_id = vnet_nic_guestid;
	    route.dst_type = LINK_INTERFACE;
	    memcpy(route.dst_mac, zeromac, 6);
	    route.dst_mac_qual = MAC_ANY;
	    memcpy(route.src_mac, zeromac, 6);
	    route.src_mac_qual = MAC_ANY;

	    v3_vnet_add_route(route);
	}
    }
#endif

    return 0;
}

device_register("VNET_NIC", vnet_nic_init)
