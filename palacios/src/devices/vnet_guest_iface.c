/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Peter Dinda <pdindal@northwestern.edu>
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vm_guest_mem.h>
#include <vnet/vnet.h>
#include <palacios/vmm_sprintf.h>
#include <devices/pci.h>


/*
  The purpose of this device is to act as a back-end for 
  in-guest implementations of the VNET forwarding and encapsulation engine.

  Via the hypercall hanging off this device, the guest can
  ask for the appropriate header to use for encapsulation (if any).

  All control-plane work remains in the VNET implementation in Palacios and 
  the host.  
*/

#ifndef V3_CONFIG_DEBUG_VNET_GUEST_IFACE
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


/*
  Calling convention:

64 bit only:
  rax = hcall number
  rbx = 64|32 tag
  rcx = src
  rdx = dest_mac
  rsi = header_ptr
  rdi = header_len
  r8  = 0 => send, 1=>recv

returns rax=negative on error. 0 if successful

*/
static int handle_header_query_hcall(struct guest_info * info, uint_t hcall_id, void * priv_data) 
{
    addr_t src_mac_gva;
    addr_t dest_mac_gva;
    addr_t header_ptr_gva;

    uint8_t src_mac[6];
    uint8_t dest_mac[6];
    uint32_t header_len;
    int      recv; 
    uint32_t copy_len;


    struct v3_vnet_header vnet_header;

    if (hcall_id != VNET_HEADER_QUERY_HCALL) { 
	PrintError("Unknown hcall 0x%x in vnet_stub\n",hcall_id);
	return -1;
    }

    src_mac_gva = info->vm_regs.rcx;
    dest_mac_gva = info->vm_regs.rdx;
    header_ptr_gva = info->vm_regs.rsi;
    header_len = info->vm_regs.rdi;
    recv = info->vm_regs.r8;

    if (v3_read_gva_memory(info,src_mac_gva,6,src_mac)!=6) { 
	PrintError("Cannot read src mac in query\n");
	info->vm_regs.rax=-1;
	return 0;
    }

    if (v3_read_gva_memory(info,(addr_t)dest_mac_gva,6,dest_mac)!=6) { 
	PrintError("Cannot read src mac in query\n");
	info->vm_regs.rax=-1;
	return 0;
    }

    if (v3_vnet_query_header(src_mac,dest_mac,recv,&vnet_header) < 0 ) { 
	PrintError("Failed to lookup header\n");
	info->vm_regs.rax=-1;
	return 0;
    }

    copy_len = (sizeof(vnet_header)<header_len) ? sizeof(vnet_header) : header_len;

    if (v3_write_gva_memory(info,header_ptr_gva,copy_len,(uchar_t*)&vnet_header) != copy_len) { 
	PrintError("Failed to write back header\n");
	info->vm_regs.rax=-1;
	return 0;
    }

    info->vm_regs.rax=0;
    return 0;

}

static int vnet_guest_iface_free(void *priv_data) {

    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = vnet_guest_iface_free,
};

static int dev_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) 
{
    char * name = v3_cfg_val(cfg, "name");

    PrintDebug("VNET guest interface: Initializing as device: %s\n", name);

    struct vm_device * dev = v3_add_device(vm, name, &dev_ops, NULL);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", name);
	return -1;
    }

    v3_register_hypercall(vm, VNET_HEADER_QUERY_HCALL, handle_header_query_hcall, NULL);

    return 0;
}


device_register("VNET_GUEST_IFACE", dev_init)
