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

#include <devices/lnx_virtio.h>
#include <devices/pci.h>



struct blk_config {
    uint64_t capacity;
    uint32_t max_size;
    uint32_t max_seg;
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors;
} __attribute__((packed));

struct blk_state {

    struct blk_config cfg;

    struct vm_device * pci_bus;
};



static int virtio_pci_write(uint16_t port, void * src, uint_t length, struct vm_device * dev) {
    return -1;
}


static int virtio_pci_read(uint16_t port, void * dst, uint_t length, struct vm_device * dev) {
    return -1;
}



static int virtio_free(struct vm_device * dev) {
    return -1;
}



static struct v3_device_ops dev_ops = {
    .free = virtio_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};





static int virtio_init(struct guest_info * vm, void * cfg_data) {
    struct blk_state * virtio = NULL;
    struct vm_device * pci_bus = (struct vm_device *)cfg_data;


    PrintDebug("Initializing VIRTIO Block device\n");

    if (pci_bus == NULL) {
	PrintError("VirtIO requires a PCI bus\n");
	return -1;
    }

    virtio = (struct blk_state *)V3_Malloc(sizeof(struct blk_state));

    virtio->pci_bus = pci_bus;

    struct vm_device * dev = v3_allocate_device("VIRTIO_BLK", &dev_ops, virtio);
    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", "LNX_VIRTIO_BLK");
	return -1;
    }



    if (virtio->pci_bus != NULL) {
	struct v3_pci_bar bars[6];
	struct pci_device * pci_dev = NULL;
	int i;
	int num_ports_pow2 = 1;
	int num_ports = sizeof(struct virtio_config) + sizeof(struct blk_config);
	

	// This gets the number of ports, rounded up to a power of 2
	while (num_ports > 0) {
	    num_ports >>= 1;
	    num_ports_pow2 <<= 1;
	}
	
	// reset num_ports 
	num_ports = sizeof(struct virtio_config) + sizeof(struct blk_config);

	if (num_ports  & ((num_ports_pow2 >> 1) - 1)) {
	    num_ports_pow2 >>= 1;
	}


	for (i = 0; i < 6; i++) {
	    bars[i].type = PCI_BAR_NONE;
	}

	bars[0].type = PCI_BAR_IO;
	bars[0].default_base_port = -1;
	bars[0].num_ports = num_ports_pow2;

	bars[0].io_read = virtio_pci_read;
	bars[0].io_write = virtio_pci_write;

	pci_dev = v3_pci_register_device(virtio->pci_bus, PCI_STD_DEVICE, 
					 0, PCI_AUTO_DEV_NUM, 0,
					 "VIRTIO-BLK", bars,
					 NULL, NULL, NULL, dev);

	if (!pci_dev) {
	    PrintError("Could not register PCI Device\n");
	    return -1;
	}
	
	pci_dev->config_header.vendor_id = VIRTIO_VENDOR_ID;
	pci_dev->config_header.device_id = VIRTIO_BLOCK_DEV_ID;
	pci_dev->config_header.class = PCI_CLASS_STORAGE;
	pci_dev->config_header.subclass = PCI_STORAGE_SUBCLASS_OTHER;

	pci_dev->config_header.subsystem_vendor_id = VIRTIO_SUBVENDOR_ID;
	pci_dev->config_header.subsystem_id = VIRTIO_BLOCK_SUBDEVICE_ID;

	pci_dev->config_header.max_latency = 1; // ?? (qemu does it...)
    }



    return -1;
}


device_register("LNX_VIRTIO_BLK", virtio_init)
