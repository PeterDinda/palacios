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
#include <devices/lnx_virtio_pci.h>


#include <devices/pci.h>


struct blk_config {
    uint64_t capacity;
    uint32_t max_size;
    uint32_t max_seg;
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors;
} __attribute__((packed));




/* Host Feature flags */
#define VIRTIO_BARRIER       0x01       /* Does host support barriers? */
#define VIRTIO_SIZE_MAX      0x02       /* Indicates maximum segment size */
#define VIRTIO_SEG_MAX       0x04       /* Indicates maximum # of segments */
#define VIRTIO_LEGACY_GEOM   0x10       /* Indicates support of legacy geometry */



struct virtio_blk_state {
    struct blk_config block_cfg;
    struct virtio_config virtio_cfg;

    struct vm_device * pci_bus;
    struct pci_device * pci_dev;

    struct virtio_device * virtio_dev; // the virtio device struction for _this_ device
    

    int io_range_size;
};




static int virtio_io_write(uint16_t port, void * src, uint_t length, struct vm_device * dev) {
    struct virtio_blk_state * virtio = (struct virtio_blk_state *)dev->private_data;
    int port_idx = port % virtio->io_range_size;


    PrintDebug("VIRTIO BLOCK Write for port %d (index=%d) len=%d, value=%x\n", 
	       port, port_idx,  length, *(uint32_t *)src);



    switch (port_idx) {
	case VRING_Q_NOTIFY_PORT:
	    // handle output
	    PrintError("Notification\n");
	    return -1;
	    break;
	case VIRTIO_STATUS_PORT:
	    if (virtio->virtio_cfg.status == 0) {
		PrintDebug("Resetting device\n");
		return -1;
		//reset
	    }
	    break;
	default:
	    return -1;
	    break;
    }




    return length;
}


static int virtio_io_read(uint16_t port, void * dst, uint_t length, struct vm_device * dev) {
    struct virtio_blk_state * virtio = (struct virtio_blk_state *)dev->private_data;
    int port_idx = port % virtio->io_range_size;


    PrintDebug("VIRTIO BLOCK Read  for port %d (index =%d), length=%d\n", 
	       port, port_idx, length);

    switch (port_idx) {
	// search for device....
	// call and return dev config read
	default:
	return -1;
    }


    return length;
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
    struct vm_device * pci_bus = v3_find_dev(vm, (char *)cfg_data);
    struct virtio_blk_state * virtio_state = NULL;
    struct pci_device * pci_dev = NULL;

    PrintDebug("Initializing VIRTIO Block device\n");

    if (pci_bus == NULL) {
	PrintError("VirtIO devices require a PCI Bus");
	return -1;
    }

    
    virtio_state  = (struct virtio_blk_state *)V3_Malloc(sizeof(struct virtio_blk_state));
    memset(virtio_state, 0, sizeof(struct virtio_blk_state));


    struct vm_device * dev = v3_allocate_device("LNX_VIRTIO_BLK", &dev_ops, virtio_state);
    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", "LNX_VIRTIO_BLK");
	return -1;
    }


    // PCI initialization
    {
	struct v3_pci_bar bars[6];
	int num_ports = sizeof(struct virtio_config) + sizeof(struct blk_config);
	int tmp_ports = num_ports;
	int i;



	// This gets the number of ports, rounded up to a power of 2
	virtio_state->io_range_size = 1; // must be a power of 2

	while (tmp_ports > 0) {
	    tmp_ports >>= 1;
	    virtio_state->io_range_size <<= 1;
	}
	
	// this is to account for any low order bits being set in num_ports
	// if there are none, then num_ports was already a power of 2 so we shift right to reset it
	if ((num_ports & ((virtio_state->io_range_size >> 1) - 1)) == 0) {
	    virtio_state->io_range_size >>= 1;
	}


	for (i = 0; i < 6; i++) {
	    bars[i].type = PCI_BAR_NONE;
	}

	bars[0].type = PCI_BAR_IO;
	bars[0].default_base_port = -1;
	bars[0].num_ports = virtio_state->io_range_size;

	bars[0].io_read = virtio_io_read;
	bars[0].io_write = virtio_io_write;

	pci_dev = v3_pci_register_device(pci_bus, PCI_STD_DEVICE, 
					 0, PCI_AUTO_DEV_NUM, 0,
					 "LNX_VIRTIO_BLK", bars,
					 NULL, NULL, NULL, dev);

	if (!pci_dev) {
	    PrintError("Could not register PCI Device\n");
	    return -1;
	}
	
	pci_dev->config_header.vendor_id = VIRTIO_VENDOR_ID;
	pci_dev->config_header.subsystem_vendor_id = VIRTIO_SUBVENDOR_ID;
	

	pci_dev->config_header.device_id = VIRTIO_BLOCK_DEV_ID;
	pci_dev->config_header.class = PCI_CLASS_STORAGE;
	pci_dev->config_header.subclass = PCI_STORAGE_SUBCLASS_OTHER;
    
	pci_dev->config_header.subsystem_id = VIRTIO_BLOCK_SUBDEVICE_ID;


	pci_dev->config_header.intr_pin = 1;

	pci_dev->config_header.max_latency = 1; // ?? (qemu does it...)


	virtio_state->pci_dev = pci_dev;
	virtio_state->pci_bus = pci_bus;

	/* Block configuration */
    }
    
    return 0;
}


device_register("LNX_VIRTIO_BLK", virtio_init)
