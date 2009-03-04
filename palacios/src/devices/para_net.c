/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <devices/para_net.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_hypercall.h>

#define TX_HYPERCALL 0x300
#define RX_HYPERCALL 0x301
#define MACADDR_HYPERCALL 0x302


struct nic_state {    
    uchar_t mac_addr[6];
};



static int tx_call(struct guest_info * info, uint_t call_no, void * priv_data) {
    //    struct nic_state * nic = (struct nic_state *)priv_data;
    addr_t pkt_gpa = info->vm_regs.rbx;
    int pkt_len = info->vm_regs.rcx;
    uchar_t * pkt = V3_Malloc(pkt_len);
    
    PrintDebug("Transmitting Packet\n");
    
    if (read_guest_pa_memory(info, pkt_gpa, pkt_len, pkt) != -1) {
	return -1;
    }
    
    
    return -1;
}


static int rx_call(struct guest_info * info, uint_t call_no, void * priv_data) {
    //    struct nic_state * nic = (struct nic_state *)priv_data;
    addr_t pkt_gpa = info->vm_regs.rbx;
    uint_t pkt_len = 0;
    uchar_t * pkt = NULL;

    PrintDebug("Receiving Packet\n");
    return -1;

    if (write_guest_pa_memory(info, pkt_gpa, pkt_len, pkt) != -1) {
	return -1;
    }

    return -1;
}


static int macaddr_call(struct guest_info * info, uint_t call_no, void * priv_data) {
    struct nic_state * nic = (struct nic_state *)priv_data;
    addr_t mac_gpa = info->vm_regs.rbx;


    PrintDebug("Returning MAC ADDR\n");
    
    if (write_guest_pa_memory(info, mac_gpa, 6, nic->mac_addr) != 6) {
	PrintError("Could not write mac address\n");
	return -1;
    }

    return 0;
}

static int net_init(struct vm_device * dev) {
    struct nic_state * nic = (struct nic_state *)dev->private_data;

    v3_register_hypercall(dev->vm, TX_HYPERCALL, tx_call, nic);
    v3_register_hypercall(dev->vm, RX_HYPERCALL, rx_call, nic);
    v3_register_hypercall(dev->vm, MACADDR_HYPERCALL, macaddr_call, nic);

    nic->mac_addr[0] = 0x52;
    nic->mac_addr[1] = 0x54;
    nic->mac_addr[2] = 0x00;
    nic->mac_addr[3] = 0x12;
    nic->mac_addr[4] = 0x34;
    nic->mac_addr[5] = 0x56;

    return 0;
}

static int net_deinit(struct vm_device * dev) {

    return 0;
}


static struct vm_device_ops dev_ops = {
    .init = net_init,
    .deinit = net_deinit,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};


struct vm_device * v3_create_para_net() {
    struct nic_state * state = NULL;

    state = (struct nic_state *)V3_Malloc(sizeof(struct nic_state));

    PrintDebug("Creating VMNet Device\n");

    struct vm_device * device = v3_create_device("VMNET", &dev_ops, state);

    return device;
}
