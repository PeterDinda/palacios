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
#include <devices/lnx_virtio_blk.h>
#include <devices/block_dev.h>
#include <palacios/vm_guest_mem.h>

#include <devices/pci.h>


#define BLK_CAPACITY_PORT     20
#define BLK_MAX_SIZE_PORT     28
#define BLK_MAX_SEG_PORT      32
#define BLK_CYLINDERS_PORT    36
#define BLK_HEADS_PORT        38
#define BLK_SECTS_PORT        39

#define BLK_IN_REQ            0
#define BLK_OUT_REQ           1
#define BLK_SCSI_CMD          2

#define BLK_BARRIER_FLAG     0x80000000

#define BLK_STATUS_OK             0
#define BLK_STATUS_ERR            1
#define BLK_STATUS_NOT_SUPPORTED  2


struct blk_config {
    uint64_t capacity;
    uint32_t max_size;
    uint32_t max_seg;
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors;
} __attribute__((packed));



struct blk_op_hdr {
    uint32_t type;
    uint32_t prior;
    uint64_t sector;
} __attribute__((packed));

#define QUEUE_SIZE 128

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
    
    struct virtio_queue queue;

    union {
	struct v3_cd_ops * cd_ops;
	struct v3_hd_ops * hd_ops;
    };

    v3_block_type_t block_type;
    void * backend_data;

    int io_range_size;
};


static int virtio_free(struct vm_device * dev) {
    return -1;
}

static int virtio_reset(struct vm_device * dev) {
    struct virtio_blk_state * virtio = (struct virtio_blk_state *)dev->private_data;

    virtio->queue.ring_desc_addr = 0;
    virtio->queue.ring_avail_addr = 0;
    virtio->queue.ring_used_addr = 0;
    virtio->queue.pfn = 0;
    virtio->queue.cur_avail_idx = 0;

    virtio->virtio_cfg.status = 0;
    virtio->virtio_cfg.pci_isr = 0;

    return 0;
}

static int handle_read_op(struct vm_device * dev, uint8_t * buf, uint64_t sector, uint32_t len) {
    struct virtio_blk_state * virtio = (struct virtio_blk_state *)dev->private_data; 

    if (virtio->block_type == BLOCK_DISK) {
	if (len % HD_SECTOR_SIZE) {
	    PrintError("Write of something that is not a sector len %d, mod=%d\n", len, len % HD_SECTOR_SIZE);
	    return -1;
	}


	PrintDebug("Reading Disk\n");

	return virtio->hd_ops->read(buf, len / HD_SECTOR_SIZE, sector * HD_SECTOR_SIZE, virtio->backend_data);
    } else if (virtio->block_type == BLOCK_CDROM) {
	if (len % ATAPI_BLOCK_SIZE) {
	    PrintError("Write of something that is not an ATAPI block len %d, mod=%d\n", len, len % ATAPI_BLOCK_SIZE);
	    return -1;
	}

	return virtio->cd_ops->read(buf, len / ATAPI_BLOCK_SIZE, sector * ATAPI_BLOCK_SIZE, virtio->backend_data);

    }

    return -1;
}


static int handle_write_op(struct vm_device * dev, uint8_t * buf, uint64_t sector, uint32_t len) {
    struct virtio_blk_state * virtio = (struct virtio_blk_state *)dev->private_data; 


    if (virtio->block_type == BLOCK_DISK) {
	if (len % HD_SECTOR_SIZE) {
	    PrintError("Write of something that is not a sector len %d, mod=%d\n", len, len % HD_SECTOR_SIZE);
	    return -1;
	}

	PrintDebug("Writing Disk\n");

	return virtio->hd_ops->write(buf, len / HD_SECTOR_SIZE, sector * HD_SECTOR_SIZE, virtio->backend_data);
    }

    return -1;
}


static int handle_block_op(struct vm_device * dev, struct vring_desc * hdr_desc, 
			   struct vring_desc * buf_desc, struct vring_desc * status_desc) {
    struct virtio_blk_state * virtio = (struct virtio_blk_state *)dev->private_data;    
    struct blk_op_hdr * hdr = NULL;
    uint8_t * buf = NULL;
    uint8_t * status = NULL;


    PrintDebug("Handling Block op\n");

    if (guest_pa_to_host_va(dev->vm, hdr_desc->addr_gpa, (addr_t *)&(hdr)) == -1) {
	PrintError("Could not translate block header address\n");
	return -1;
    }


    if (guest_pa_to_host_va(dev->vm, buf_desc->addr_gpa, (addr_t *)&(buf)) == -1) {
	PrintError("Could not translate buffer address\n");
	return -1;
    }

    if (guest_pa_to_host_va(dev->vm, status_desc->addr_gpa, (addr_t *)&(status)) == -1) {
	PrintError("Could not translate status address\n");
	return -1;
    }

    if (hdr->type == BLK_IN_REQ) {
	if (virtio->block_type != BLOCK_NONE) {
	    if (handle_read_op(dev, buf, hdr->sector, buf_desc->length) == -1) {
		*status = BLK_STATUS_ERR;
	    } else {
		*status = BLK_STATUS_OK;
	    }
	} else {
	    *status = BLK_STATUS_NOT_SUPPORTED;
	} 

    } else if (hdr->type == BLK_OUT_REQ) {
	if (virtio->block_type == BLOCK_DISK) {
	    if (handle_write_op(dev, buf, hdr->sector, buf_desc->length) == -1) {
		*status = BLK_STATUS_ERR;
	    } else {
		*status = BLK_STATUS_OK;
	    }
	} else {
	    *status = BLK_STATUS_NOT_SUPPORTED;
	}
    } else if (hdr->type == BLK_SCSI_CMD) {
	*status = BLK_STATUS_NOT_SUPPORTED;
    }




    return 0;
}



static int handle_kick(struct vm_device * dev) {
    struct virtio_blk_state * virtio = (struct virtio_blk_state *)dev->private_data;    
    struct virtio_queue * q = &(virtio->queue);


    PrintDebug("VIRTIO KICK: cur_index=%d, avail_index=%d\n", q->cur_avail_idx, q->avail->index);

    while (q->cur_avail_idx < q->avail->index) {
	struct vring_desc * hdr_desc = NULL;
	struct vring_desc * buf_desc = NULL;
	struct vring_desc * status_desc = NULL;
	uint16_t chain_idx = q->avail->ring[q->cur_avail_idx];
	uint32_t req_len = 0;
	int chained = 1;

	PrintDebug("chained=%d, Chain Index=%d\n", chained, chain_idx);

	while (chained) {
	    hdr_desc = &(q->desc[chain_idx]);
	    
	    PrintDebug("Header Descriptor gpa=%p, len=%d, flags=%x, next=%d\n", 
		       (void *)(hdr_desc->addr_gpa), hdr_desc->length, hdr_desc->flags, hdr_desc->next);

	
	    if (!(hdr_desc->flags & VIRTIO_NEXT_FLAG)) {
		PrintError("Block operations must chain a buffer descriptor\n");
		return -1;
	    }

	    buf_desc = &(q->desc[hdr_desc->next]);
	    

	    PrintDebug("Buffer  Descriptor gpa=%p, len=%d, flags=%x, next=%d\n", 
		       (void *)(buf_desc->addr_gpa), buf_desc->length, buf_desc->flags, buf_desc->next);
	    

	    if (!(buf_desc->flags & VIRTIO_NEXT_FLAG)) {
		PrintError("Block operatoins must chain a status descriptor\n");
		return -1;
	    }
	    
	    status_desc = &(q->desc[buf_desc->next]);

	    // We detect whether we are chained here...
	    if (status_desc->flags & VIRTIO_NEXT_FLAG) {
		chained = 1;
		chain_idx = status_desc->next; 
	    } else {
		chained = 0;
	    }

	    PrintDebug("Status  Descriptor gpa=%p, len=%d, flags=%x, next=%d\n", 
		       (void *)(status_desc->addr_gpa), status_desc->length, status_desc->flags, status_desc->next);
	    

	    if (handle_block_op(dev, hdr_desc, buf_desc, status_desc) == -1) {
		PrintError("Error handling block operation\n");
		return -1;
	    }

	    req_len += (buf_desc->length + status_desc->length);

	}

	q->used->ring[q->used->index].id = q->cur_avail_idx;
	q->used->ring[q->used->index].length = req_len; // What do we set this to????

	q->used->index = (q->used->index + 1) % (QUEUE_SIZE * sizeof(struct vring_desc));;


	q->cur_avail_idx = (q->cur_avail_idx + 1) % (QUEUE_SIZE * sizeof(struct vring_desc));
    }

    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	PrintDebug("Raising IRQ %d\n",  virtio->pci_dev->config_header.intr_line);
	v3_pci_raise_irq(virtio->pci_bus, 0, virtio->pci_dev);
    }

    return 0;
}

static int virtio_io_write(uint16_t port, void * src, uint_t length, struct vm_device * dev) {
    struct virtio_blk_state * virtio = (struct virtio_blk_state *)dev->private_data;
    int port_idx = port % virtio->io_range_size;


    PrintDebug("VIRTIO BLOCK Write for port %d (index=%d) len=%d, value=%x\n", 
	       port, port_idx,  length, *(uint32_t *)src);



    switch (port_idx) {
	case GUEST_FEATURES_PORT:
	    if (length != 4) {
		PrintError("Illegal write length for guest features\n");
		return -1;
	    }
	    
	    virtio->virtio_cfg.guest_features = *(uint32_t *)src;

	    break;
	case VRING_PG_NUM_PORT:
	    if (length == 4) {
		addr_t pfn = *(uint32_t *)src;
		addr_t page_addr = (pfn << VIRTIO_PAGE_SHIFT);


		virtio->queue.pfn = pfn;
		
		virtio->queue.ring_desc_addr = page_addr ;
		virtio->queue.ring_avail_addr = page_addr + (QUEUE_SIZE * sizeof(struct vring_desc));
		virtio->queue.ring_used_addr = ( virtio->queue.ring_avail_addr + \
						 sizeof(struct vring_avail)    + \
						 (QUEUE_SIZE * sizeof(uint16_t)));
		
		// round up to next page boundary.
		virtio->queue.ring_used_addr = (virtio->queue.ring_used_addr + 0xfff) & ~0xfff;

		if (guest_pa_to_host_va(dev->vm, virtio->queue.ring_desc_addr, (addr_t *)&(virtio->queue.desc)) == -1) {
		    PrintError("Could not translate ring descriptor address\n");
		    return -1;
		}


		if (guest_pa_to_host_va(dev->vm, virtio->queue.ring_avail_addr, (addr_t *)&(virtio->queue.avail)) == -1) {
		    PrintError("Could not translate ring available address\n");
		    return -1;
		}


		if (guest_pa_to_host_va(dev->vm, virtio->queue.ring_used_addr, (addr_t *)&(virtio->queue.used)) == -1) {
		    PrintError("Could not translate ring used address\n");
		    return -1;
		}

		PrintDebug("RingDesc=%p, Avail=%p, Used=%p\n",
			   (void *)(virtio->queue.ring_desc_addr),
			   (void *)(virtio->queue.ring_avail_addr),
			   (void *)(virtio->queue.ring_used_addr));

	    } else {
		PrintError("Illegal write length for page frame number\n");
		return -1;
	    }
	    break;
	case VRING_Q_SEL_PORT:
	    virtio->virtio_cfg.vring_queue_selector = *(uint16_t *)src;

	    if (virtio->virtio_cfg.vring_queue_selector != 0) {
		PrintError("Virtio Block device only uses 1 queue, selected %d\n", 
			   virtio->virtio_cfg.vring_queue_selector);
		return -1;
	    }

	    break;
	case VRING_Q_NOTIFY_PORT:
	    PrintDebug("Handling Kick\n");
	    if (handle_kick(dev) == -1) {
		PrintError("Could not handle Block Notification\n");
		return -1;
	    }
	    break;
	case VIRTIO_STATUS_PORT:
	    virtio->virtio_cfg.status = *(uint8_t *)src;

	    if (virtio->virtio_cfg.status == 0) {
		PrintDebug("Resetting device\n");
		virtio_reset(dev);
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
	case HOST_FEATURES_PORT:
	    if (length != 4) {
		PrintError("Illegal read length for host features\n");
		return -1;
	    }

	    *(uint32_t *)dst = virtio->virtio_cfg.host_features;
	
	    break;
	case VRING_PG_NUM_PORT:
	    if (length != 4) {
		PrintError("Illegal read length for page frame number\n");
		return -1;
	    }

	    *(uint32_t *)dst = virtio->queue.pfn;

	    break;
	case VRING_SIZE_PORT:
	    if (length != 2) {
		PrintError("Illegal read length for vring size\n");
		return -1;
	    }
		
	    *(uint16_t *)dst = virtio->queue.queue_size;

	    break;

	case VIRTIO_STATUS_PORT:
	    if (length != 1) {
		PrintError("Illegal read length for status\n");
		return -1;
	    }

	    *(uint8_t *)dst = virtio->virtio_cfg.status;
	    break;

	default:
	    if ( (port_idx >= sizeof(struct virtio_config)) && 
		 (port_idx < (sizeof(struct virtio_config) + sizeof(struct blk_config))) ) {

		uint8_t * cfg_ptr = (uint8_t *)&(virtio->block_cfg);
		memcpy(dst, cfg_ptr, length);
		
	    } else {
		PrintError("Read of Unhandled Virtio Read\n");
		return -1;
	    }
	  
	    break;
    }




    return length;
}




static struct v3_device_ops dev_ops = {
    .free = virtio_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};




int v3_virtio_register_cdrom(struct vm_device * dev, struct v3_cd_ops * ops, void * private_data) {
    struct virtio_blk_state * virtio = (struct virtio_blk_state *)dev->private_data;
    
    virtio->block_type = BLOCK_CDROM;
    virtio->cd_ops = ops;
    virtio->backend_data = private_data;

    virtio->block_cfg.capacity = ops->get_capacity(private_data);

    return 0;
}


int v3_virtio_register_harddisk(struct vm_device * dev, struct v3_hd_ops * ops, void * private_data) {
    struct virtio_blk_state * virtio = (struct virtio_blk_state *)dev->private_data;

    virtio->block_type = BLOCK_DISK;
    virtio->hd_ops = ops;
    virtio->backend_data = private_data;

    virtio->block_cfg.capacity = ops->get_capacity(private_data);

    return 0;
}



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
    }

    /* Block configuration */
    virtio_state->virtio_cfg.host_features = VIRTIO_SEG_MAX;

    // Virtio Block only uses one queue
    virtio_state->queue.queue_size = QUEUE_SIZE;

    virtio_reset(dev);

    virtio_state->backend_data = NULL;
    virtio_state->block_type = BLOCK_NONE;
    virtio_state->hd_ops = NULL;

    return 0;
}


device_register("LNX_VIRTIO_BLK", virtio_init)
