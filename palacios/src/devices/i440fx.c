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

#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <devices/pci.h>

#include <palacios/vmm_io.h>


// We Have to setup some sort of PIC interrupt mapping here....

struct i440_state {
    struct vm_device * pci;
};


static int io_read(struct guest_info * core, ushort_t port, void * dst, uint_t length, void * priv_data) {
    PrintError("Unhandled read on port %x\n", port);
    return -1;
}

static int io_write(struct guest_info * core, ushort_t port, void * src, uint_t length, void * priv_data) {
    PrintError("Unhandled write on port %x\n", port);
    return -1;
}





static int i440_free(struct i440_state * state) {

    // unregister from PCI

    V3_Free(state);

    return 0;
}

static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))i440_free,

};




static int i440_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct pci_device * pci_dev = NULL;
    struct v3_pci_bar bars[6];
    int i;
    struct i440_state * state = NULL;
    struct vm_device * pci = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    char * dev_id = v3_cfg_val(cfg, "ID");
    int ret = 0;

    if (!pci) {
	PrintError("could not find PCI Device\n");
	return -1;
    }

    state = (struct i440_state *)V3_Malloc(sizeof(struct i440_state));

    if (!state) {
	PrintError("Cannot allocate state\n");
	return -1;
    }

    state->pci = pci;
	
    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, state);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(state);
	return -1;
    }

    for (i = 0; i < 4; i++) {
	ret |= v3_dev_hook_io(dev, 0x0cf8 + i, &io_read, &io_write);
	ret |= v3_dev_hook_io(dev, 0x0cfc + i, &io_read, &io_write);
    }

    /*
    if (ret != 0) {
	PrintError("Error hooking i440FX io ports\n");
	v3_remove_device(dev);
	return -1;
    }
    */

    for (i = 0; i < 6; i++) {
	bars[i].type = PCI_BAR_NONE;
    }    

    pci_dev = v3_pci_register_device(state->pci, PCI_STD_DEVICE, 
				     0, 0, 0, "i440FX", bars,
				     NULL, NULL, NULL, NULL, state);

    if (!pci_dev) {
	v3_remove_device(dev);
 	return -1;
    }

    pci_dev->config_header.vendor_id = 0x8086;
    pci_dev->config_header.device_id = 0x1237;
    pci_dev->config_header.revision = 0x02;
    pci_dev->config_header.subclass = 0x00; //  SubClass: host2pci
    pci_dev->config_header.class = PCI_CLASS_BRIDGE;    // Class: PCI bridge

    pci_dev->config_space[0x72] = 0x02; // SMRAM (?)

    return 0;
}

device_register("i440FX", i440_init);
