/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2009, Chang Seok Bae <jhuell@gmail.com>
 * Copyright (c) 2009, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author:  Lei Xia <lxia@northwestern.edu>
 *          Chang Seok Bae <jhuell@gmail.com>
 *          Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */ 
 

#include <palacios/vmm.h>
#include <devices/pci.h>
#include <devices/southbridge.h>



static int reset_piix3(struct vm_device * dev) {
    struct v3_southbridge * piix3 = (struct v3_southbridge *)(dev->private_data);
    struct pci_device * pci_dev = piix3->southbridge_pci;

    pci_dev->config_header.command = 0x0007; // master, memory and I/O
    pci_dev->config_header.status = 0x0200;

    pci_dev->config_space[0x4c] = 0x4d;
    pci_dev->config_space[0x4e] = 0x03;
    pci_dev->config_space[0x4f] = 0x00;
    pci_dev->config_space[0x60] = 0x80;
    pci_dev->config_space[0x69] = 0x02;
    pci_dev->config_space[0x70] = 0x80;
    pci_dev->config_space[0x76] = 0x0c;
    pci_dev->config_space[0x77] = 0x0c;
    pci_dev->config_space[0x78] = 0x02;
    pci_dev->config_space[0x79] = 0x00;
    pci_dev->config_space[0x80] = 0x00;
    pci_dev->config_space[0x82] = 0x00;
    pci_dev->config_space[0xa0] = 0x08;
    pci_dev->config_space[0xa2] = 0x00;
    pci_dev->config_space[0xa3] = 0x00;
    pci_dev->config_space[0xa4] = 0x00;
    pci_dev->config_space[0xa5] = 0x00;
    pci_dev->config_space[0xa6] = 0x00;
    pci_dev->config_space[0xa7] = 0x00;
    pci_dev->config_space[0xa8] = 0x0f;
    pci_dev->config_space[0xaa] = 0x00;
    pci_dev->config_space[0xab] = 0x00;
    pci_dev->config_space[0xac] = 0x00;
    pci_dev->config_space[0xae] = 0x00;

    return 0;
}





static int piix_free(struct vm_device * dev) {
    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = piix_free,
    .reset = reset_piix3,
    .start = NULL,
    .stop = NULL,
};




static int setup_pci(struct vm_device * dev) {
    struct v3_southbridge * piix3 = (struct v3_southbridge *)(dev->private_data);
    struct pci_device * pci_dev = NULL;
    struct v3_pci_bar bars[6];
    int i;

    for (i = 0; i < 6; i++) {
	bars[i].type = PCI_BAR_NONE;
    }

    pci_dev = v3_pci_register_device(piix3->pci_bus, PCI_MULTIFUNCTION, 
				     0, -1, 0, 
				     "PIIX3", bars, 
				     NULL, NULL, NULL, dev);
    if (pci_dev == NULL) {
	PrintError("Could not register PCI Device for PIIX3\n");
	return -1;
    }

    pci_dev->config_header.vendor_id = 0x8086;
    pci_dev->config_header.device_id = 0x7000; // PIIX4 is 0x7001
    pci_dev->config_header.class = PCI_CLASS_BRIDGE;
    pci_dev->config_header.subclass = PCI_BRIDGE_SUBCLASS_PCI_ISA; 

    piix3->southbridge_pci = pci_dev;

    reset_piix3(dev);

    return 0;
}

static int piix3_init(struct guest_info * vm, void * cfg_data) {
    struct v3_southbridge * piix3 = (struct v3_southbridge *)V3_Malloc(sizeof(struct v3_southbridge));
    struct vm_device * dev = NULL;
    struct vm_device * pci = v3_find_dev(vm, (char *)cfg_data);

    if (!pci) {
	PrintError("Could not find PCI device\n");
	return -1;
    }

    piix3->pci_bus = pci;
    piix3->type = V3_SB_PIIX3;
    
    dev = v3_allocate_device("PIIX3", &dev_ops, piix3);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", "PIIX3");
	return -1;
    }

    PrintDebug("Created PIIX3\n");

    return setup_pci(dev);
}


device_register("PIIX3", piix3_init)
