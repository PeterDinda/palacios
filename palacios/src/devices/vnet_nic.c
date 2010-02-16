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

struct vnet_nic_state {
	char mac[6];


};

//used when virtio_nic get a packet from guest and send it to the backend
// send packet to all of the virtio nic devices other than the sender
static int send(uint8_t * buf, uint32_t len, void * private_data, struct vm_device *dest_dev){
    PrintDebug("Virito NIC: In vnet_send: guest net state %p\n", private_data);

    v3_vnet_send_rawpkt(buf, len, private_data);
    return 0;
}

static int receive(uint8_t * buf, uint32_t count, void * private_data, struct vm_device *src_dev){

    return 0;
}


static struct v3_dev_net_ops net_ops = {
    .send = send, 
    .receive = receive,
};


#if 0
static int input(struct v3_vm_info *info, uchar_t * buf, uint_t len, void * private_data){
    PrintDebug("Virito NIC: In virtio_input: guest net state %p\n", private_data);

    return __virtio_dev_send(buf, len, private_data);
}


//register a virtio device to the vnet as backend
void register_to_vnet(struct vm_device  *dev, 
						char *dev_name,
						uchar_t mac[6]){
    struct virtio_net_state * net_state;
    struct virtio_dev_state *virtio_state =  (struct virtio_dev_state *)dev->private_data;
    uchar_t tapmac[6] = {0x00,0x02,0x55,0x67,0x42,0x39}; //for Intel-VT test HW
    //uchar_t tapmac[6] = {0x6e,0xa8,0x75,0xf4,0x82,0x95};
    uchar_t dstmac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
    uchar_t zeromac[6] = {0,0,0,0,0,0};


    net_state  = (struct virtio_net_state *)V3_Malloc(sizeof(struct virtio_net_state));
    memset(net_state, 0, sizeof(struct virtio_net_state));
    net_state->net_ops = (struct v3_dev_net_ops *)V3_Malloc(sizeof(struct v3_dev_net_ops));
    net_state->net_ops->send = &vnet_send;
    net_state->net_ops->receive = &vnet_receive;
    net_state->dev = dev;

    register_dev(virtio_state, net_state);

    PrintDebug("Virtio NIC register Device %s: queue size: %d, %d\n", dev->name,
	       net_state->rx_vq.queue_size, net_state->tx_vq.queue_size);
    PrintDebug("Virtio NIC: connect virtio device %s, state %p, to vnet\n", dev->name, net_state);
	
    int idx = vnet_register_device(dev, dev_name, mac, &virtio_input, net_state);
    //vnet_add_route_entry(zeromac, dstmac, MAC_ANY, MAC_NONE, idx, LINK_INTERFACE, -1, LINK_INTERFACE);
    if (!strcmp(dev_name, "net_virtiodom0")){
    	vnet_add_route_entry(zeromac, tapmac, MAC_ANY, MAC_NONE, idx, LINK_INTERFACE, -1, LINK_INTERFACE);
	vnet_add_route_entry(zeromac, dstmac, MAC_ANY, MAC_NONE, idx, LINK_INTERFACE, -1, LINK_INTERFACE);
    }
    if (!strcmp(dev_name, "net_virtio"))
    	vnet_add_route_entry(tapmac, zeromac, MAC_NONE, MAC_ANY, idx, LINK_INTERFACE, -1, LINK_INTERFACE);


    v3_vnet_add_node(dev_name, mac, input, priv_data);
    struct v3_vnet_route route;
 //add default route
    memset(&route, 0, sizeof(struct v3_vnet_route));
    memcpy(&route.dest_mac, mac, 6);
    route.src_mac_qual = MAC_ANY;
    route.dest_mac_qual = MAC_NONE;
    route.link_idx = idx;
    route.link_type = LINK_EDGE;
    route.src_link_idx = -1;
    route.src_type = LINK_ANY;
    v3_vnet_add_route(&route);

    char mac
    memset(&route, 0, sizeof(struct v3_vnet_route));
    memcpy(&route.dest_mac, mac, 6);
    route.src_mac_qual = MAC_ANY;
    route.dest_mac_qual = MAC_NONE;
    route.link_idx = idx;
    route.link_type = LINK_EDGE;
    route.src_link_idx = -1;
    route.src_type = LINK_ANY;
    v3_vnet_add_route(&route);
    
}


/*
  *called in svm/vmx handler
  *iteative handled the unsent packet in incoming packet queues for
  *all virtio nic devices in this guest
  */
int v3_vnetnic_pktprocess(struct guest_info * info)
{
 
    return 0;
}

#endif

static int vnet_nic_free(struct vm_device * dev) {
    return 0;
}

static struct v3_device_ops dev_ops = {
    .free = vnet_nic_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};

static int vnet_nic_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vnet_nic_state * vnetnic = NULL;
    char * name = v3_cfg_val(cfg, "name");

    v3_cfg_tree_t * frontend_cfg = v3_cfg_subtree(cfg, "frontend");

    vnetnic = (struct vnet_nic_state *)V3_Malloc(sizeof(struct vnet_nic_state));
    memset(vnetnic, 0, sizeof(struct vnet_nic_state));


    PrintDebug("Registering vnet_nic device at\n");

    struct vm_device * dev = v3_allocate_device(name, &dev_ops, vnetnic);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", name);
	return -1;
    }

    if (v3_dev_connect_net(vm, v3_cfg_val(frontend_cfg, "tag"), 
			   &net_ops, frontend_cfg, vnetnic) == -1) {
	PrintError("Could not connect %s to frontend %s\n", 
		   name, v3_cfg_val(frontend_cfg, "tag"));
	return -1;
    }
    

    return 0;
}

device_register("VNET_NIC", vnet_nic_init)
