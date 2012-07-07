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
#include <palacios/vm_guest_mem.h>
#include <devices/lnx_virtio_pci.h>
#include <palacios/vmm_symmod.h>
#include <palacios/vmm_hashtable.h>

#include <devices/pci.h>


#define QUEUE_SIZE 128
#define NUM_QUEUES 2

struct sym_config {
    uint32_t avail_mods;
    uint32_t loaded_mods;
} __attribute__((packed));



struct virtio_sym_state {
    struct sym_config sym_cfg;
    struct virtio_config virtio_cfg;


    struct vm_device * pci_bus;
    struct pci_device * pci_dev;
    struct v3_vm_info * vm;
    struct v3_symmod_state * symmod_state;
    

#define NOTIFY_QUEUE 0
#define LOADER_QUEUE 1
    struct virtio_queue queue[NUM_QUEUES];

    struct virtio_queue * cur_queue;

    int notifier_active;

    int io_range_size;
};



struct symmod_cmd {
#define CMD_INV  0
#define CMD_LOAD 1
#define CMD_LIST 2
    uint32_t cmd;
    uint32_t num_cmds;
} __attribute__((packed));


// structure of the symmod notifier ring structures
struct symmod_hdr {
    uint32_t num_bytes;
    char name[32];
    union {
	uint32_t flags;
	struct {
#define V3_SYMMOD_INV (0x00)
#define V3_SYMMOD_LNX (0x01)
#define V3_SYMMOD_MOD (0x02)
#define V3_SYMMOD_SEC (0x03)
	    uint8_t type;

#define V3_SYMMOD_ARCH_INV     (0x00)
#define V3_SYMMOD_ARCH_i386    (0x01)
#define V3_SYMMOD_ARCH_x86_64  (0x02)
	    uint8_t arch;

#define V3_SYMMOD_ACT_INV       (0x00)
#define V3_SYMMOD_ACT_ADVERTISE (0x01)
#define V3_SYMMOD_ACT_LOAD      (0x02)
	    uint8_t action;

	    uint8_t rsvd;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


static int virtio_reset(struct virtio_sym_state * virtio) {

    memset(virtio->queue, 0, sizeof(struct virtio_queue) * 2);

    virtio->cur_queue = &(virtio->queue[0]);

    virtio->virtio_cfg.status = 0;
    virtio->virtio_cfg.pci_isr = 0;

    virtio->queue[0].queue_size = QUEUE_SIZE;
    virtio->queue[1].queue_size = QUEUE_SIZE;


    virtio->sym_cfg.avail_mods = virtio->symmod_state->num_avail_capsules;
    virtio->sym_cfg.loaded_mods = virtio->symmod_state->num_loaded_capsules;

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




static int handle_xfer_kick(struct guest_info * core, struct virtio_sym_state * sym_state) {
    struct virtio_queue * q = sym_state->cur_queue;
    
    PrintDebug("SYMMOD: VIRTIO SYMMOD Kick on loader queue\n");

    while (q->cur_avail_idx != q->avail->index) {
	struct vring_desc * cmd_desc = NULL;
	struct symmod_cmd * cmd = NULL;
	uint16_t desc_idx = q->avail->ring[q->cur_avail_idx % QUEUE_SIZE];
	uint16_t desc_cnt = get_desc_count(q, desc_idx);
	struct vring_desc * status_desc = NULL;
	uint8_t status = 0;
	uint8_t * status_ptr = NULL;
	int i;
	uint32_t xfer_len = 0;

	cmd_desc = &(q->desc[desc_idx]);
	
	if (v3_gpa_to_hva(core, cmd_desc->addr_gpa, (addr_t *)&cmd) == -1) {
	    PrintError("Could not translate SYMMOD header address\n");
	    return -1;
	}
 
	desc_idx = cmd_desc->next;

	if (cmd->cmd == CMD_LOAD) {
	    struct vring_desc * name_desc = NULL;
	    struct vring_desc * buf_desc = NULL;
	    char * name = NULL;
	    struct v3_sym_capsule * capsule = NULL;
	    uint32_t offset = 0;
    

	    PrintDebug("Descriptor Count=%d, index=%d\n", desc_cnt, q->cur_avail_idx % QUEUE_SIZE);
    
	    if (desc_cnt < 3) {
		PrintError("Symmod loads must include at least 3 descriptors (cnt=%d)\n", desc_cnt);
		return -1;
	    }
	
	    name_desc = &(q->desc[desc_idx]);

	    if (v3_gpa_to_hva(core, name_desc->addr_gpa, (addr_t *)&name) == -1) {
		PrintError("Could not translate SYMMOD header address\n");
		return -1;
	    }

	    desc_idx = name_desc->next;

	    capsule = v3_get_sym_capsule(core->vm_info, name);

	    for (i = 0; i < desc_cnt - 3; i++) {
		uint8_t tmp_status = 0;
		uint8_t * buf = NULL;

		buf_desc = &(q->desc[desc_idx]);

		if (v3_gpa_to_hva(core, buf_desc->addr_gpa, (addr_t *)&(buf)) == -1) {
		    PrintError("Could not translate buffer address\n");
		    return -1;
		}

		memcpy(buf, capsule->start_addr + offset, buf_desc->length);
		PrintDebug("Copying module to virtio buffers: SRC=%p, DST=%p, len=%d\n",
			   (void *)(capsule->start_addr + offset), (void *)buf, buf_desc->length);

		if (tmp_status != 0) {
		    PrintError("Error loading module segment\n");
		    status = tmp_status;
		}


		offset += buf_desc->length;
		xfer_len += buf_desc->length;
		desc_idx = buf_desc->next;
	    }
	} else {
	    PrintError("Invalid SYMMOD Loader command\n");
	    return -1;
	}

	status_desc = &(q->desc[desc_idx]);

	if (v3_gpa_to_hva(core, status_desc->addr_gpa, (addr_t *)&status_ptr) == -1) {
	    PrintError("SYMMOD Error could not translate status address\n");
	    return -1;
	}

	xfer_len += status_desc->length;
	*status_ptr = status;

	PrintDebug("Transferred %d bytes (xfer_len)\n", xfer_len);
	q->used->ring[q->used->index % QUEUE_SIZE].id = q->avail->ring[q->cur_avail_idx % QUEUE_SIZE];
	q->used->ring[q->used->index % QUEUE_SIZE].length = xfer_len; // set to total inbound xfer length

	q->used->index++;
	q->cur_avail_idx++;

    }


    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	PrintDebug("Raising IRQ %d\n",  sym_state->pci_dev->config_header.intr_line);
	v3_pci_raise_irq(sym_state->pci_bus, sym_state->pci_dev, 0);
	sym_state->virtio_cfg.pci_isr = 1;
    }


    return 0;
}




static int handle_notification_kick(struct guest_info * core, struct virtio_sym_state * sym_state) {
    //    struct virtio_queue * q = sym_state->cur_queue;
    struct virtio_queue * q = &(sym_state->queue[NOTIFY_QUEUE]);
    struct hashtable_iter * capsule_iter = NULL;

    PrintDebug("SYMMOD: VIRTIO SYMMOD Kick on notification queue\n");

    capsule_iter = v3_create_htable_iter(sym_state->symmod_state->capsule_table);

    do {
 	uint16_t desc_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];
	struct vring_desc * hdr_desc = NULL;
	struct symmod_hdr * hdr = NULL;
	struct v3_sym_capsule * capsule = NULL;


	capsule = (struct v3_sym_capsule *)v3_htable_get_iter_value(capsule_iter);


	PrintDebug("SYMMOD: Advertising Capsule %s\n", capsule->name);

	if (capsule->type != V3_SYMMOD_LNX) {
	    continue;
	}
	


	if (q->cur_avail_idx == q->avail->index) {
	    PrintError("Notification Queue Too SMALL\n");
	    return -1;
	}

	hdr_desc = &(q->desc[desc_idx]);

	if (v3_gpa_to_hva(core, hdr_desc->addr_gpa, (addr_t *)&hdr) == -1) {
	    PrintError("Could not translate SYMMOD header address\n");
	    return -1;
	}

	memset(hdr, 0, sizeof(struct symmod_hdr));


	memcpy(hdr->name, capsule->name, strlen(capsule->name));
	hdr->num_bytes = capsule->size;
	hdr->flags = capsule->flags;
	hdr->action = V3_SYMMOD_ACT_ADVERTISE;

	q->used->ring[q->used->index % QUEUE_SIZE].id = q->avail->ring[q->cur_avail_idx % QUEUE_SIZE];
	q->used->ring[q->used->index % QUEUE_SIZE].length = sizeof(struct symmod_hdr) ; // set to total inbound xfer length
	
	q->used->index++;
	q->cur_avail_idx++;

    } while (v3_htable_iter_advance(capsule_iter));


    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	PrintDebug("Raising IRQ %d\n",  sym_state->pci_dev->config_header.intr_line);
	v3_pci_raise_irq(sym_state->pci_bus, sym_state->pci_dev, 0);
	sym_state->virtio_cfg.pci_isr = 1;
    }


    return 0;
}


static int virtio_io_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * private_data) {
    struct virtio_sym_state * sym_state = (struct virtio_sym_state *)private_data;
    int port_idx = port % sym_state->io_range_size;


    PrintDebug("SYMMOD: VIRTIO SYMMOD Write for port %d len=%d, value=%x\n", 
	       port, length, *(uint32_t *)src);
    PrintDebug("SYMMOD: port idx=%d\n", port_idx);


    switch (port_idx) {
	case GUEST_FEATURES_PORT:
	    if (length != 4) {
		PrintError("Illegal write length for guest features\n");
		return -1;
	    }
	    
	    sym_state->virtio_cfg.guest_features = *(uint32_t *)src;

	    break;
	case VRING_PG_NUM_PORT:
	    if (length == 4) {
		addr_t pfn = *(uint32_t *)src;
		addr_t page_addr = (pfn << VIRTIO_PAGE_SHIFT);

		sym_state->cur_queue->pfn = pfn;
		
		sym_state->cur_queue->ring_desc_addr = page_addr ;
		sym_state->cur_queue->ring_avail_addr = page_addr + (QUEUE_SIZE * sizeof(struct vring_desc));
		sym_state->cur_queue->ring_used_addr = ( sym_state->cur_queue->ring_avail_addr + \
						 sizeof(struct vring_avail)    + \
						 (QUEUE_SIZE * sizeof(uint16_t)));
		
		// round up to next page boundary.
		sym_state->cur_queue->ring_used_addr = (sym_state->cur_queue->ring_used_addr + 0xfff) & ~0xfff;

		if (v3_gpa_to_hva(core, sym_state->cur_queue->ring_desc_addr, (addr_t *)&(sym_state->cur_queue->desc)) == -1) {
		    PrintError("Could not translate ring descriptor address\n");
		    return -1;
		}


		if (v3_gpa_to_hva(core, sym_state->cur_queue->ring_avail_addr, (addr_t *)&(sym_state->cur_queue->avail)) == -1) {
		    PrintError("Could not translate ring available address\n");
		    return -1;
		}


		if (v3_gpa_to_hva(core, sym_state->cur_queue->ring_used_addr, (addr_t *)&(sym_state->cur_queue->used)) == -1) {
		    PrintError("Could not translate ring used address\n");
		    return -1;
		}

		PrintDebug("SYMMOD: RingDesc_addr=%p, Avail_addr=%p, Used_addr=%p\n",
			   (void *)(sym_state->cur_queue->ring_desc_addr),
			   (void *)(sym_state->cur_queue->ring_avail_addr),
			   (void *)(sym_state->cur_queue->ring_used_addr));

		PrintDebug("SYMMOD: RingDesc=%p, Avail=%p, Used=%p\n", 
			   sym_state->cur_queue->desc, sym_state->cur_queue->avail, sym_state->cur_queue->used);

	    } else {
		PrintError("Illegal write length for page frame number\n");
		return -1;
	    }
	    break;
	case VRING_Q_SEL_PORT:
	    sym_state->virtio_cfg.vring_queue_selector = *(uint16_t *)src;

	    if (sym_state->virtio_cfg.vring_queue_selector > NUM_QUEUES) {
		PrintError("Virtio Symbiotic device has no qeueues. Selected %d\n", 
			   sym_state->virtio_cfg.vring_queue_selector);
		return -1;
	    }
	    
	    sym_state->cur_queue = &(sym_state->queue[sym_state->virtio_cfg.vring_queue_selector]);

	    break;
	case VRING_Q_NOTIFY_PORT: {
	    uint16_t queue_idx = *(uint16_t *)src;
	    
	    PrintDebug("SYMMOD: Handling Kick\n");
	    
	    if (queue_idx == 0) {
		if (handle_notification_kick(core, sym_state) == -1) {
		    PrintError("Could not handle Notification Kick\n");
		    return -1;
		}
		
		sym_state->notifier_active = 1;
		
	    } else if (queue_idx == 1) {
		if (handle_xfer_kick(core, sym_state) == -1) {
		    PrintError("Could not handle Symbiotic Notification\n");
		    return -1;
		}
	    } else {
		PrintError("Kick on invalid queue (%d)\n", queue_idx);
		return -1;
	    }
	    
	    break;
	}
	case VIRTIO_STATUS_PORT:
	    sym_state->virtio_cfg.status = *(uint8_t *)src;

	    if (sym_state->virtio_cfg.status == 0) {
		PrintDebug("SYMMOD: Resetting device\n");
		virtio_reset(sym_state);
	    }

	    break;

	case VIRTIO_ISR_PORT:
	    sym_state->virtio_cfg.pci_isr = *(uint8_t *)src;
	    break;
	default:
	    return -1;
	    break;
    }

    return length;
}


static int virtio_io_read(struct guest_info * core, uint16_t port, void * dst, uint_t length, void * private_data) {

    struct virtio_sym_state * sym_state = (struct virtio_sym_state *)private_data;
    int port_idx = port % sym_state->io_range_size;

/*
    PrintDebug("SYMMOD: VIRTIO SYMBIOTIC Read  for port %d (index =%d), length=%d\n", 
	       port, port_idx, length);
*/
    switch (port_idx) {
	case HOST_FEATURES_PORT:
	    if (length != 4) {
		PrintError("Illegal read length for host features\n");
		return -1;
	    }

	    *(uint32_t *)dst = sym_state->virtio_cfg.host_features;
	
	    break;
	case VRING_PG_NUM_PORT:
	    if (length != 4) {
		PrintError("Illegal read length for page frame number\n");
		return -1;
	    }

	    *(uint32_t *)dst = sym_state->cur_queue->pfn;

	    break;
	case VRING_SIZE_PORT:
	    if (length != 2) {
		PrintError("Illegal read length for vring size\n");
		return -1;
	    }
		
	    *(uint16_t *)dst = sym_state->cur_queue->queue_size;

	    break;

	case VIRTIO_STATUS_PORT:
	    if (length != 1) {
		PrintError("Illegal read length for status\n");
		return -1;
	    }

	    *(uint8_t *)dst = sym_state->virtio_cfg.status;
	    break;

	case VIRTIO_ISR_PORT:
	    *(uint8_t *)dst = sym_state->virtio_cfg.pci_isr;
	    sym_state->virtio_cfg.pci_isr = 0;
	    v3_pci_lower_irq(sym_state->pci_bus, sym_state->pci_dev, 0);
	    break;

	default:
	    if ( (port_idx >= sizeof(struct virtio_config)) && 
		 (port_idx < (sizeof(struct virtio_config) + sizeof(struct sym_config))) ) {
		int cfg_offset = port_idx - sizeof(struct virtio_config);
		uint8_t * cfg_ptr = (uint8_t *)&(sym_state->sym_cfg);

		memcpy(dst, cfg_ptr + cfg_offset, length);

		V3_Print("Reading SymConfig at idx %d (val=%x)\n", cfg_offset, *(uint32_t *)cfg_ptr);
		
	    } else {
		PrintError("Read of Unhandled Virtio Read\n");
		return -1;
	    }
	  
	    break;
    }

    return length;
}




static int virtio_load_capsule(struct v3_vm_info * vm, struct v3_sym_capsule * mod, void * priv_data) {
    struct virtio_sym_state * virtio = (struct virtio_sym_state *)priv_data;
    //   struct virtio_queue * q = virtio->cur_queue;
    struct virtio_queue * q = &(virtio->queue[NOTIFY_QUEUE]);


    if (strlen(mod->name) >= 32) {
	PrintError("Capsule name is too long... (%d bytes) limit is 32\n", (uint32_t)strlen(mod->name));
	return -1;
    }

    PrintDebug("SYMMOD: VIRTIO SYMMOD Loader: Loading Capsule (size=%d)\n", mod->size);

    //queue is not set yet
    if (q->ring_avail_addr == 0) {
	PrintError("Queue is not set\n");
	return -1;
    }

    
    if (q->cur_avail_idx != q->avail->index) {
	uint16_t notifier_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];
	struct symmod_hdr * notifier = NULL;
	struct vring_desc * notifier_desc = NULL;

	PrintDebug("SYMMOD: Descriptor index=%d\n", q->cur_avail_idx % q->queue_size);

	notifier_desc = &(q->desc[notifier_idx]);

	PrintDebug("SYMMOD: Notifier Descriptor (ptr=%p) gpa=%p, len=%d, flags=%x, next=%d\n", 
		   notifier_desc, (void *)(addr_t)(notifier_desc->addr_gpa), 
		   notifier_desc->length, notifier_desc->flags, 
		   notifier_desc->next);	

	if (v3_gpa_to_hva(&(vm->cores[0]), notifier_desc->addr_gpa, (addr_t *)&(notifier)) == -1) {
	    PrintError("Could not translate receive buffer address\n");
	    return -1;
	}

	// clear the notifier
	memset(notifier, 0, sizeof(struct symmod_hdr));

	// set the capsule name
	memcpy(notifier->name, mod->name, strlen(mod->name));

	// set capsule length
	notifier->num_bytes = mod->size;
	notifier->flags = mod->flags;
	notifier->action =  V3_SYMMOD_ACT_LOAD;

	
	q->used->ring[q->used->index % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];

	q->used->ring[q->used->index % q->queue_size].length = sizeof(struct symmod_hdr);

	q->used->index++;
	q->cur_avail_idx++;
    }

    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	PrintDebug("SYMMOD: Raising IRQ %d\n",  virtio->pci_dev->config_header.intr_line);
	v3_pci_raise_irq(virtio->pci_bus, virtio->pci_dev, 0);
	virtio->virtio_cfg.pci_isr = 0x1;
    }


    return 0;
}


static int virtio_free(struct virtio_sym_state * virtio_state) {
    // unregister from PCI

    V3_Free(virtio_state);
    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))virtio_free,
};



static struct v3_symmod_loader_ops loader_ops = {
    .load_capsule = virtio_load_capsule,
};


static int virtio_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vm_device * pci_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    struct virtio_sym_state * virtio_state = NULL;
    struct v3_symmod_state * symmod_state = &(vm->sym_vm_state.symmod_state);
    struct pci_device * pci_dev = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");

    PrintDebug("SYMMOD: Initializing VIRTIO Symbiotic Module device\n");

    if (pci_bus == NULL) {
	PrintError("VirtIO devices require a PCI Bus");
	return -1;
    }
    
    virtio_state  = (struct virtio_sym_state *)V3_Malloc(sizeof(struct virtio_sym_state));

    if (!virtio_state) {
	PrintError("Cannot allocate in init\n");
	return -1;
    }

    memset(virtio_state, 0, sizeof(struct virtio_sym_state));

    virtio_state->vm = vm;
    virtio_state->symmod_state = symmod_state;




    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, virtio_state);

    if (dev == NULL) {
	PrintError("Could not attach device %s\n", dev_id);
	V3_Free(virtio_state);
	return -1;
    }


    // PCI initialization
    {
	struct v3_pci_bar bars[6];
	int num_ports = sizeof(struct virtio_config) + sizeof(struct sym_config);
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
	bars[0].private_data = virtio_state;

	pci_dev = v3_pci_register_device(pci_bus, PCI_STD_DEVICE, 
					 0, PCI_AUTO_DEV_NUM, 0,
					 "LNX_VIRTIO_SYMMOD", bars,
					 NULL, NULL, NULL, NULL, virtio_state);

	if (!pci_dev) {
	    PrintError("Could not register PCI Device\n");
	    v3_remove_device(dev);
	    return -1;
	}
	
	pci_dev->config_header.vendor_id = VIRTIO_VENDOR_ID;
	pci_dev->config_header.subsystem_vendor_id = VIRTIO_SUBVENDOR_ID;
	

	pci_dev->config_header.device_id = VIRTIO_SYMMOD_DEV_ID;
	pci_dev->config_header.class = PCI_CLASS_MEMORY;
	pci_dev->config_header.subclass = PCI_MEM_SUBCLASS_RAM;
    
	pci_dev->config_header.subsystem_id = VIRTIO_SYMMOD_SUBDEVICE_ID;


	pci_dev->config_header.intr_pin = 1;

	pci_dev->config_header.max_latency = 1; // ?? (qemu does it...)


	virtio_state->pci_dev = pci_dev;
	virtio_state->pci_bus = pci_bus;
    }
    

    V3_Print("SYMMOD: %d available sym modules\n", virtio_state->sym_cfg.avail_mods);

    virtio_reset(virtio_state);

    v3_set_symmod_loader(vm, &loader_ops, virtio_state);

    return 0;
}


device_register("LNX_VIRTIO_SYMMOD", virtio_init)
