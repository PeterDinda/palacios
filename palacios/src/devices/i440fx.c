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
#include <devices/i440fx.h>
#include <devices/pci.h>

struct i440_state {
    struct vm_device * pci;
};


static int io_read(ushort_t port, void * dst, uint_t length, struct vm_device * dev) {
    PrintError("Unhandled read on port %x\n", port);
    return -1;
}

static int io_write(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
    PrintError("Unhandled write on port %x\n", port);
    return -1;
}



static int i440_init(struct vm_device * dev) {
    struct i440_state * state = (struct i440_state *)(dev->private_data);
    struct pci_device * pci_dev = NULL;
    struct v3_pci_bar bars[6];
    int i;

    for (i = 0; i < 4; i++) {
	v3_dev_hook_io(dev, 0x0cf8 + i,
		       &io_read, &io_write);
	v3_dev_hook_io(dev, 0x0cfc + i,
		       &io_read, &io_write);
    }

    for (i = 0; i < 6; i++) {
	bars[i].type = PCI_BAR_NONE;
    }    

    pci_dev = v3_pci_register_device(state->pci, PCI_STD_DEVICE, 0, 0, 0, "i440FX", bars,
				     NULL, NULL, NULL, dev);

    if (!pci_dev) {
 	return -1;
    }

    pci_dev->config_header.vendor_id = 0x8086;
    pci_dev->config_header.device_id = 0x1237;
    pci_dev->config_header.revision = 0x0002;
    pci_dev->config_header.subclass = 0x00; //  SubClass: host2pci
    pci_dev->config_header.class = 0x06;    // Class: PCI bridge

    pci_dev->config_space[0x72] = 0x02; // SMRAM (?)

    return 0;
}


static int i440_deinit(struct vm_device * dev) {
    return 0;
}

static struct vm_device_ops dev_ops = {
    .init = i440_init, 
    .deinit = i440_deinit,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};



struct vm_device * v3_create_i440fx(struct vm_device * pci) {
    struct i440_state * state = NULL;

    state = (struct i440_state *)V3_Malloc(sizeof(struct i440_state));

    state->pci = pci;
	
    struct vm_device * i440_dev = v3_create_device("i440FX", &dev_ops, state);

    return i440_dev;
}
