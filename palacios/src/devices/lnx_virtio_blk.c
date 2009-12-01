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
#include <palacios/vm_guest_mem.h>

#include <devices/pci.h>



#ifndef CONFIG_DEBUG_VIRTIO_BLK
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


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


struct virtio_dev_state {
    struct vm_device * pci_bus;
    struct list_head dev_list;
    struct guest_info * vm;
};

struct virtio_blk_state {

    struct pci_device * pci_dev;
    struct blk_config block_cfg;
    struct virtio_config virtio_cfg;

    
    struct virtio_queue queue;

    struct v3_dev_blk_ops * ops;

    void * backend_data;

    int io_range_size;

    struct virtio_dev_state * virtio_dev;

    struct list_head dev_link;
};


static int virtio_free(struct vm_device * dev) {
    return -1;
}

static int blk_reset(struct virtio_blk_state * virtio) {

    virtio->queue.ring_desc_addr = 0;
    virtio->queue.ring_avail_addr = 0;
    virtio->queue.ring_used_addr = 0;
    virtio->queue.pfn = 0;
    virtio->queue.cur_avail_idx = 0;

    virtio->virtio_cfg.status = 0;
    virtio->virtio_cfg.pci_isr = 0;
    return 0;
}


static int virtio_reset(struct vm_device * dev) {
    struct virtio_dev_state * dev_state = (struct virtio_dev_state *)(dev->private_data);
    struct virtio_blk_state * blk_state = NULL;

    list_for_each_entry(blk_state, &(dev_state->dev_list), dev_link) {
	blk_reset(blk_state);
    }

    return 0;
}

static int handle_read_op(struct virtio_blk_state * blk_state, uint8_t * buf, uint64_t * sector, uint64_t len) {
    int ret = -1;

    PrintDebug("Reading Disk\n");
    ret = blk_state->ops->read(buf, *sector, len, (void *)(blk_state->backend_data));
    *sector += len;

    return ret;
}


static int handle_write_op(struct virtio_blk_state * blk_state, uint8_t * buf, uint64_t * sector, uint64_t len) {
    int ret = -1;

    PrintDebug("Writing Disk\n");
    ret = blk_state->ops->write(buf, *sector, len, (void *)(blk_state->backend_data));
    *sector += len;

    return ret;
}



// multiple block operations need to increment the sector 

static int handle_block_op(struct virtio_blk_state * blk_state, struct blk_op_hdr * hdr, 
			   struct vring_desc * buf_desc, uint8_t * status) {
    uint8_t * buf = NULL;

    PrintDebug("Handling Block op\n");
    if (guest_pa_to_host_va(blk_state->virtio_dev->vm, buf_desc->addr_gpa, (addr_t *)&(buf)) == -1) {
	PrintError("Could not translate buffer address\n");
	return -1;
    }

    PrintDebug("Sector=%p Length=%d\n", (void *)(addr_t)(hdr->sector), buf_desc->length);

    if (hdr->type == BLK_IN_REQ) {
	if (handle_read_op(blk_state, buf, &(hdr->sector), buf_desc->length) == -1) {
	    *status = BLK_STATUS_ERR;
	    return -1;
	} else {
	    *status = BLK_STATUS_OK;
	}
    } else if (hdr->type == BLK_OUT_REQ) {
	if (handle_write_op(blk_state, buf, &(hdr->sector), buf_desc->length) == -1) {
	    *status = BLK_STATUS_ERR;
	    return -1;
	} else {
	    *status = BLK_STATUS_OK;
	}
    } else if (hdr->type == BLK_SCSI_CMD) {
	PrintError("VIRTIO: SCSI Command Not supported!!!\n");
	*status = BLK_STATUS_NOT_SUPPORTED;
	return -1;
    }

    PrintDebug("Returning Status: %d\n", *status);

    return 0;
}

static int get_desc_count(struct virtio_queue * q, int index) {
    struct vring_desc * tmp_desc = &(q->desc[index]);
    int cnt = 1;
    
    while (tmp_desc->flags & VIRTIO_NEXT_FLAG) {
	tmp_desc = &(q->desc[tmp_desc->next]);
	cnt++;
    }

    return cnt;
}



static int handle_kick(struct virtio_blk_state * blk_state) {  
    struct virtio_queue * q = &(blk_state->queue);

    PrintDebug("VIRTIO KICK: cur_index=%d (mod=%d), avail_index=%d\n", 
	       q->cur_avail_idx, q->cur_avail_idx % QUEUE_SIZE, q->avail->index);

    while (q->cur_avail_idx < q->avail->index) {
	struct vring_desc * hdr_desc = NULL;
	struct vring_desc * buf_desc = NULL;
	struct vring_desc * status_desc = NULL;
	struct blk_op_hdr hdr;
	addr_t hdr_addr = 0;
	uint16_t desc_idx = q->avail->ring[q->cur_avail_idx % QUEUE_SIZE];
	int desc_cnt = get_desc_count(q, desc_idx);
	int i = 0;
	uint8_t * status_ptr = NULL;
	uint8_t status = BLK_STATUS_OK;
	uint32_t req_len = 0;

	PrintDebug("Descriptor Count=%d, index=%d\n", desc_cnt, q->cur_avail_idx % QUEUE_SIZE);

	if (desc_cnt < 3) {
	    PrintError("Block operations must include at least 3 descriptors\n");
	    return -1;
	}

	hdr_desc = &(q->desc[desc_idx]);


	PrintDebug("Header Descriptor (ptr=%p) gpa=%p, len=%d, flags=%x, next=%d\n", hdr_desc, 
		   (void *)(hdr_desc->addr_gpa), hdr_desc->length, hdr_desc->flags, hdr_desc->next);	

	if (guest_pa_to_host_va(blk_state->virtio_dev->vm, hdr_desc->addr_gpa, &(hdr_addr)) == -1) {
	    PrintError("Could not translate block header address\n");
	    return -1;
	}

	// We copy the block op header out because we are going to modify its contents
	memcpy(&hdr, (void *)hdr_addr, sizeof(struct blk_op_hdr));
	
	PrintDebug("Blk Op Hdr (ptr=%p) type=%d, sector=%p\n", (void *)hdr_addr, hdr.type, (void *)hdr.sector);

	desc_idx = hdr_desc->next;

	for (i = 0; i < desc_cnt - 2; i++) {
	    uint8_t tmp_status = BLK_STATUS_OK;

	    buf_desc = &(q->desc[desc_idx]);

	    PrintDebug("Buffer Descriptor (ptr=%p) gpa=%p, len=%d, flags=%x, next=%d\n", buf_desc, 
		       (void *)(buf_desc->addr_gpa), buf_desc->length, buf_desc->flags, buf_desc->next);

	    if (handle_block_op(blk_state, &hdr, buf_desc, &tmp_status) == -1) {
		PrintError("Error handling block operation\n");
		return -1;
	    }

	    if (tmp_status != BLK_STATUS_OK) {
		status = tmp_status;
	    }

	    req_len += buf_desc->length;
	    desc_idx = buf_desc->next;
	}

	status_desc = &(q->desc[desc_idx]);

	PrintDebug("Status Descriptor (ptr=%p) gpa=%p, len=%d, flags=%x, next=%d\n", status_desc, 
		   (void *)(status_desc->addr_gpa), status_desc->length, status_desc->flags, status_desc->next);

	if (guest_pa_to_host_va(blk_state->virtio_dev->vm, status_desc->addr_gpa, (addr_t *)&(status_ptr)) == -1) {
	    PrintError("Could not translate status address\n");
	    return -1;
	}

	req_len += status_desc->length;
	*status_ptr = status;

	q->used->ring[q->used->index % QUEUE_SIZE].id = q->avail->ring[q->cur_avail_idx % QUEUE_SIZE];
	q->used->ring[q->used->index % QUEUE_SIZE].length = req_len; // What do we set this to????

	q->used->index++;
	q->cur_avail_idx++;
    }

    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	PrintDebug("Raising IRQ %d\n",  blk_state->pci_dev->config_header.intr_line);
	v3_pci_raise_irq(blk_state->virtio_dev->pci_bus, 0, blk_state->pci_dev);
	blk_state->virtio_cfg.pci_isr = 1;
    }

    return 0;
}

static int virtio_io_write(uint16_t port, void * src, uint_t length, void * private_data) {
    struct virtio_blk_state * blk_state = (struct virtio_blk_state *)private_data;
    int port_idx = port % blk_state->io_range_size;


    PrintDebug("VIRTIO BLOCK Write for port %d (index=%d) len=%d, value=%x\n", 
	       port, port_idx,  length, *(uint32_t *)src);



    switch (port_idx) {
	case GUEST_FEATURES_PORT:
	    if (length != 4) {
		PrintError("Illegal write length for guest features\n");
		return -1;
	    }
	    
	    blk_state->virtio_cfg.guest_features = *(uint32_t *)src;
	    PrintDebug("Setting Guest Features to %x\n", blk_state->virtio_cfg.guest_features);

	    break;
	case VRING_PG_NUM_PORT:
	    if (length == 4) {
		addr_t pfn = *(uint32_t *)src;
		addr_t page_addr = (pfn << VIRTIO_PAGE_SHIFT);


		blk_state->queue.pfn = pfn;
		
		blk_state->queue.ring_desc_addr = page_addr ;
		blk_state->queue.ring_avail_addr = page_addr + (QUEUE_SIZE * sizeof(struct vring_desc));
		blk_state->queue.ring_used_addr = ( blk_state->queue.ring_avail_addr + \
						 sizeof(struct vring_avail)    + \
						 (QUEUE_SIZE * sizeof(uint16_t)));
		
		// round up to next page boundary.
		blk_state->queue.ring_used_addr = (blk_state->queue.ring_used_addr + 0xfff) & ~0xfff;

		if (guest_pa_to_host_va(blk_state->virtio_dev->vm, blk_state->queue.ring_desc_addr, (addr_t *)&(blk_state->queue.desc)) == -1) {
		    PrintError("Could not translate ring descriptor address\n");
		    return -1;
		}


		if (guest_pa_to_host_va(blk_state->virtio_dev->vm, blk_state->queue.ring_avail_addr, (addr_t *)&(blk_state->queue.avail)) == -1) {
		    PrintError("Could not translate ring available address\n");
		    return -1;
		}


		if (guest_pa_to_host_va(blk_state->virtio_dev->vm, blk_state->queue.ring_used_addr, (addr_t *)&(blk_state->queue.used)) == -1) {
		    PrintError("Could not translate ring used address\n");
		    return -1;
		}

		PrintDebug("RingDesc_addr=%p, Avail_addr=%p, Used_addr=%p\n",
			   (void *)(blk_state->queue.ring_desc_addr),
			   (void *)(blk_state->queue.ring_avail_addr),
			   (void *)(blk_state->queue.ring_used_addr));

		PrintDebug("RingDesc=%p, Avail=%p, Used=%p\n", 
			   blk_state->queue.desc, blk_state->queue.avail, blk_state->queue.used);

	    } else {
		PrintError("Illegal write length for page frame number\n");
		return -1;
	    }
	    break;
	case VRING_Q_SEL_PORT:
	    blk_state->virtio_cfg.vring_queue_selector = *(uint16_t *)src;

	    if (blk_state->virtio_cfg.vring_queue_selector != 0) {
		PrintError("Virtio Block device only uses 1 queue, selected %d\n", 
			   blk_state->virtio_cfg.vring_queue_selector);
		return -1;
	    }

	    break;
	case VRING_Q_NOTIFY_PORT:
	    PrintDebug("Handling Kick\n");
	    if (handle_kick(blk_state) == -1) {
		PrintError("Could not handle Block Notification\n");
		return -1;
	    }
	    break;
	case VIRTIO_STATUS_PORT:
	    blk_state->virtio_cfg.status = *(uint8_t *)src;

	    if (blk_state->virtio_cfg.status == 0) {
		PrintDebug("Resetting device\n");
		blk_reset(blk_state);
	    }

	    break;

	case VIRTIO_ISR_PORT:
	    blk_state->virtio_cfg.pci_isr = *(uint8_t *)src;
	    break;
	default:
	    return -1;
	    break;
    }

    return length;
}


static int virtio_io_read(uint16_t port, void * dst, uint_t length, void * private_data) {
    struct virtio_blk_state * blk_state = (struct virtio_blk_state *)private_data;
    int port_idx = port % blk_state->io_range_size;


    PrintDebug("VIRTIO BLOCK Read  for port %d (index =%d), length=%d\n", 
	       port, port_idx, length);

    switch (port_idx) {
	case HOST_FEATURES_PORT:
	    if (length != 4) {
		PrintError("Illegal read length for host features\n");
		return -1;
	    }

	    *(uint32_t *)dst = blk_state->virtio_cfg.host_features;
	
	    break;
	case VRING_PG_NUM_PORT:
	    if (length != 4) {
		PrintError("Illegal read length for page frame number\n");
		return -1;
	    }

	    *(uint32_t *)dst = blk_state->queue.pfn;

	    break;
	case VRING_SIZE_PORT:
	    if (length != 2) {
		PrintError("Illegal read length for vring size\n");
		return -1;
	    }
		
	    *(uint16_t *)dst = blk_state->queue.queue_size;

	    break;

	case VIRTIO_STATUS_PORT:
	    if (length != 1) {
		PrintError("Illegal read length for status\n");
		return -1;
	    }

	    *(uint8_t *)dst = blk_state->virtio_cfg.status;
	    break;

	case VIRTIO_ISR_PORT:
	    *(uint8_t *)dst = blk_state->virtio_cfg.pci_isr;
	    blk_state->virtio_cfg.pci_isr = 0;
	    v3_pci_lower_irq(blk_state->virtio_dev->pci_bus, 0, blk_state->pci_dev);
	    break;

	default:
	    if ( (port_idx >= sizeof(struct virtio_config)) && 
		 (port_idx < (sizeof(struct virtio_config) + sizeof(struct blk_config))) ) {
		int cfg_offset = port_idx - sizeof(struct virtio_config);
		uint8_t * cfg_ptr = (uint8_t *)&(blk_state->block_cfg);

		memcpy(dst, cfg_ptr + cfg_offset, length);
		
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
    .reset = virtio_reset,
    .start = NULL,
    .stop = NULL,
};





static int register_dev(struct virtio_dev_state * virtio, struct virtio_blk_state * blk_state) {
    // initialize PCI
    struct pci_device * pci_dev = NULL;
    struct v3_pci_bar bars[6];
    int num_ports = sizeof(struct virtio_config) + sizeof(struct blk_config);
    int tmp_ports = num_ports;
    int i;



    // This gets the number of ports, rounded up to a power of 2
    blk_state->io_range_size = 1; // must be a power of 2
    
    while (tmp_ports > 0) {
	tmp_ports >>= 1;
	blk_state->io_range_size <<= 1;
    }
	
    // this is to account for any low order bits being set in num_ports
    // if there are none, then num_ports was already a power of 2 so we shift right to reset it
    if ((num_ports & ((blk_state->io_range_size >> 1) - 1)) == 0) {
	blk_state->io_range_size >>= 1;
    }
    
    
    for (i = 0; i < 6; i++) {
	bars[i].type = PCI_BAR_NONE;
    }
    
    PrintDebug("Virtio-BLK io_range_size = %d\n", blk_state->io_range_size);
    
    bars[0].type = PCI_BAR_IO;
    bars[0].default_base_port = -1;
    bars[0].num_ports = blk_state->io_range_size;
    
    bars[0].io_read = virtio_io_read;
    bars[0].io_write = virtio_io_write;
    bars[0].private_data = blk_state;
    
    pci_dev = v3_pci_register_device(virtio->pci_bus, PCI_STD_DEVICE, 
				     0, PCI_AUTO_DEV_NUM, 0,
				     "LNX_VIRTIO_BLK", bars,
				     NULL, NULL, NULL, blk_state);
    
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
    
    
    blk_state->pci_dev = pci_dev;
    
    /* Block configuration */
    blk_state->virtio_cfg.host_features = VIRTIO_SEG_MAX;
    blk_state->block_cfg.max_seg = QUEUE_SIZE - 2;

    // Virtio Block only uses one queue
    blk_state->queue.queue_size = QUEUE_SIZE;

    blk_state->virtio_dev = virtio;

    blk_reset(blk_state);


    return 0;
}


static int connect_fn(struct guest_info * info, 
		      void * frontend_data, 
		      struct v3_dev_blk_ops * ops, 
		      v3_cfg_tree_t * cfg, 
		      void * private_data) {

    struct virtio_dev_state * virtio = (struct virtio_dev_state *)frontend_data;

    struct virtio_blk_state * blk_state  = (struct virtio_blk_state *)V3_Malloc(sizeof(struct virtio_blk_state));
    memset(blk_state, 0, sizeof(struct virtio_blk_state));

    register_dev(virtio, blk_state);

    blk_state->ops = ops;
    blk_state->backend_data = private_data;

    blk_state->block_cfg.capacity = ops->get_capacity(private_data);

    PrintDebug("Virtio Capacity = %d -- 0x%p\n", (int)(virtio->block_cfg.capacity), 
	       (void *)(addr_t)(virtio->block_cfg.capacity));

    return 0;
}


static int virtio_init(struct guest_info * vm, v3_cfg_tree_t * cfg) {
    struct vm_device * pci_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    struct virtio_dev_state * virtio_state = NULL;
    char * name = v3_cfg_val(cfg, "name");

    PrintDebug("Initializing VIRTIO Block device\n");

    if (pci_bus == NULL) {
	PrintError("VirtIO devices require a PCI Bus");
	return -1;
    }


    virtio_state  = (struct virtio_dev_state *)V3_Malloc(sizeof(struct virtio_dev_state));
    memset(virtio_state, 0, sizeof(struct virtio_dev_state));

    INIT_LIST_HEAD(&(virtio_state->dev_list));
    virtio_state->pci_bus = pci_bus;
    virtio_state->vm = vm;

    struct vm_device * dev = v3_allocate_device(name, &dev_ops, virtio_state);
    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", name);
	return -1;
    }

    if (v3_dev_add_blk_frontend(vm, name, connect_fn, (void *)virtio_state) == -1) {
	PrintError("Could not register %s as block frontend\n", name);
	return -1;
    }

    return 0;
}


device_register("LNX_VIRTIO_BLK", virtio_init)
