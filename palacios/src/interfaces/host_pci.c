/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Jack Lange <jacklange@cs.pitt.edu>
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jacklange@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <interfaces/host_pci.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_mem.h>



static struct v3_host_pci_hooks * pci_hooks = NULL;



void V3_Init_Host_PCI(struct v3_host_pci_hooks * hooks) {
    pci_hooks = hooks;
    V3_Print(VM_NONE, VCORE_NONE, "V3 host PCI interface intialized\n");
    return;
}


/* This is ugly and should be abstracted out to a function in the memory manager */
int V3_get_guest_mem_region(struct v3_vm_info * vm, struct v3_guest_mem_region * region, uint64_t gpa) {
    struct v3_mem_region * v3_reg = NULL;
    
    memset(region, 0, sizeof(struct v3_guest_mem_region));

    if (!vm) {
        PrintError(vm, VCORE_NONE, "Tried to get a menregion from a NULL vm pointer\n");
	return -1;
    }


    v3_reg = v3_get_base_region(vm, gpa);

    if (v3_reg == NULL) {
	return 0;
    }

    region->start = v3_reg->host_addr;
    region->end = v3_reg->host_addr + (v3_reg->guest_end - v3_reg->guest_start);

    return 1;
}


struct v3_host_pci_dev * v3_host_pci_get_dev(struct v3_vm_info * vm, 
					     char * url, void * priv_data) {

    struct v3_host_pci_dev * host_dev = NULL;

    if ((!pci_hooks) || (!pci_hooks->request_device)) {
	PrintError(vm, VCORE_NONE, "Host PCI Hooks not initialized\n");
	return NULL;
    }

    host_dev = pci_hooks->request_device(url, vm);

    if (host_dev == NULL) {
	PrintError(vm, VCORE_NONE, "Could not find host PCI device (%s)\n", url);
	return NULL;
    }

    host_dev->guest_data = priv_data;
    
    return host_dev;
    
}


int v3_host_pci_config_write(struct v3_host_pci_dev * v3_dev, 
			     uint32_t reg_num, void * src, 
			     uint32_t length) {

    if ((!pci_hooks) || (!pci_hooks->config_write)) {
        PrintError(VM_NONE, VCORE_NONE, "Host PCI hooks not initialized\n");
	return -1;
    }

    return pci_hooks->config_write(v3_dev, reg_num, src, length);
}


int v3_host_pci_config_read(struct v3_host_pci_dev * v3_dev, 
			     uint32_t reg_num, void * dst, 
			     uint32_t length) {

    if ((!pci_hooks) || (!pci_hooks->config_read)) {
	PrintError(VM_NONE, VCORE_NONE, "Host PCI hooks not initialized\n");
	return -1;
    }

    return pci_hooks->config_read(v3_dev, reg_num, dst, length);
}

int v3_host_pci_ack_irq(struct v3_host_pci_dev * v3_dev, uint32_t vec_index) {

    if ((!pci_hooks) || (!pci_hooks->ack_irq)) {
	PrintError(VM_NONE, VCORE_NONE, "Host PCI hooks not initialized\n");
	return -1;
    }

    return pci_hooks->ack_irq(v3_dev, vec_index);
}



int v3_host_pci_cmd_update(struct v3_host_pci_dev * v3_dev, pci_cmd_t cmd, uint64_t arg ) {

    if ((!pci_hooks) || (!pci_hooks->pci_cmd)) {
	PrintError(VM_NONE, VCORE_NONE, "Host PCI hooks not initialized\n");
	return -1;
    }

    return pci_hooks->pci_cmd(v3_dev, cmd, arg);
}





int V3_host_pci_raise_irq(struct v3_host_pci_dev * v3_dev, uint32_t vec_index) {
    if (!v3_dev->irq_handler) {
	PrintError(VM_NONE, VCORE_NONE, "No interrupt registerd for host pci device\n");
	return -1;
    }

    return v3_dev->irq_handler(v3_dev->guest_data, vec_index);
}

