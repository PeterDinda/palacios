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
#include <palacios/vmm_vnet.h>
#include <palacios/vmm_sprintf.h>
#include <devices/pci.h>


#ifndef CONFIG_DEBUG_VNET_BRG
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define QUEUE_SIZE 128
#define NUM_QUEUES 3

struct vnet_config {
    uint32_t num_devs;
    uint32_t num_routes;
} __attribute__((packed));


#define CTRL_QUEUE 0
#define RECV_QUEUE 2
#define XMIT_QUEUE 1

struct virtio_vnet_state {
    struct v3_vm_info *vm;
    struct vnet_config vnet_cfg;
    struct virtio_config virtio_cfg;

    struct vm_device * pci_bus;
    struct pci_device * pci_dev;
	
    struct virtio_queue queue[NUM_QUEUES];

    struct virtio_queue * cur_queue;

    int io_range_size;
    v3_lock_t lock;
};

#define VNET_GET_ROUTES 10
#define VNET_ADD_ROUTE 11
#define VNET_DEL_ROUTE 12

#define VNET_GET_LINKS 20
#define VNET_ADD_LINK 21
#define VNET_DEL_LINK 22

// structure of the vnet command header
struct vnet_ctrl_hdr {
    uint8_t cmd_type;
    uint32_t num_cmds;
} __attribute__((packed));

static int virtio_reset(struct virtio_vnet_state * vnet_brg) {

    memset(vnet_brg->queue, 0, sizeof(struct virtio_queue) * 2);

    vnet_brg->cur_queue = &(vnet_brg->queue[0]);

    vnet_brg->virtio_cfg.status = 0;
    vnet_brg->virtio_cfg.pci_isr = 0;

    vnet_brg->queue[0].queue_size = QUEUE_SIZE;
    vnet_brg->queue[1].queue_size = QUEUE_SIZE;
    vnet_brg->queue[2].queue_size = QUEUE_SIZE;

    memset(&(vnet_brg->vnet_cfg), 0, sizeof(struct vnet_config));
    v3_lock_init(&(vnet_brg->lock));

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




static int handle_cmd_kick(struct guest_info * core, struct virtio_vnet_state * vnet_state) {
    struct virtio_queue * q = &(vnet_state->queue[0]);
    
    PrintDebug("VNET Bridge: Handling command  queue\n");

    while (q->cur_avail_idx < q->avail->index) {
	struct vring_desc * hdr_desc = NULL;
	struct vring_desc * buf_desc = NULL;
	struct vring_desc * status_desc = NULL;
	uint16_t desc_idx = q->avail->ring[q->cur_avail_idx % QUEUE_SIZE];
	uint16_t desc_cnt = get_desc_count(q, desc_idx);
	struct vnet_ctrl_hdr * hdr = NULL;
	int i;
	int xfer_len = 0;
	uint8_t * status_ptr = NULL;
	uint8_t status = 0;


	PrintDebug("Descriptor Count=%d, index=%d, desc_idx=%d\n", desc_cnt, q->cur_avail_idx % QUEUE_SIZE, desc_idx);

	if (desc_cnt < 3) {
	    PrintError("VNET Bridge cmd must include at least 3 descriptors (cnt=%d)\n", desc_cnt);
	    return -1;
	}
	
	hdr_desc = &(q->desc[desc_idx]);

	if (guest_pa_to_host_va(core, hdr_desc->addr_gpa, (addr_t *)&hdr) == -1) {
	    PrintError("Could not translate VirtioVNET header address\n");
	    return -1;
	}

	desc_idx = hdr_desc->next;
	
	if (hdr->cmd_type == VNET_ADD_ROUTE) {
	    
	    for (i = 0; i < hdr->num_cmds; i++) {
		uint8_t tmp_status = 0;
		struct v3_vnet_route * route = NULL;
		
		buf_desc = &(q->desc[desc_idx]);

		if (guest_pa_to_host_va(core, buf_desc->addr_gpa, (addr_t *)&(route)) == -1) {
		    PrintError("Could not translate route address\n");
		    return -1;
		}

		// add route
		PrintDebug("Adding VNET Route\n");

		tmp_status = v3_vnet_add_route(*route);

		PrintDebug("VNET Route Added\n");

		if (tmp_status != 0) {
		    PrintError("Error adding VNET ROUTE\n");
		    status = tmp_status;
		}

		xfer_len += buf_desc->length;
		desc_idx = buf_desc->next;
	    }

	} 



	status_desc = &(q->desc[desc_idx]);

	if (guest_pa_to_host_va(core, status_desc->addr_gpa, (addr_t *)&status_ptr) == -1) {
	    PrintError("VirtioVNET Error could not translate status address\n");
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
	PrintDebug("Raising IRQ %d\n",  vnet_state->pci_dev->config_header.intr_line);
	v3_pci_raise_irq(vnet_state->pci_bus, 0, vnet_state->pci_dev);
	vnet_state->virtio_cfg.pci_isr = 1;
    }


    return 0;
}

static int copy_data_to_desc(struct guest_info *core, 
					struct vring_desc * desc, 
					uchar_t * buf, 
					uint_t buf_len) 
{
    uint32_t len;
    uint8_t * desc_buf = NULL;

    if (guest_pa_to_host_va(core, desc->addr_gpa, (addr_t *)&(desc_buf)) == -1) {
	PrintError("Could not translate buffer address\n");
	return -1;
    }
    len = (desc->length < buf_len)?desc->length:buf_len;
    memcpy(desc_buf, buf, len);

    return len;
}


static int vnet_brg_rx(struct v3_vnet_pkt *pkt, void * private_data) {
    struct virtio_vnet_state * vnet_brg = (struct virtio_vnet_state *)private_data;
    struct virtio_queue * q = &(vnet_brg->queue[RECV_QUEUE]);
    char *buf = (char *)pkt;
    int ret_val;
    int pkt_head_len = ((addr_t)pkt->data) - ((addr_t)pkt); 
    uint32_t data_len = sizeof(struct v3_vnet_pkt); //TODO: should not transfer the whole struct, only the data size 
    unsigned long flags;

    flags = v3_lock_irqsave(vnet_brg->lock);
	
    PrintDebug("VNET Bridge: RX: pkt sent to guest size: %d\n, pkt_header_len: %d\n", data_len, pkt_head_len);
    v3_hexdump(buf, 100, NULL, 0);

    if (q->ring_avail_addr == 0) {
	PrintError("Queue is not set\n");
	ret_val = -1;
	goto exit;
    }

    if (q->last_avail_idx > q->avail->index)
	q->idx_overflow = true;
    q->last_avail_idx = q->avail->index;

    if (q->cur_avail_idx < q->avail->index || (q->idx_overflow && q->cur_avail_idx < q->avail->index+65536)){
	uint16_t buf_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];
	uint32_t len = 0;
	uint32_t offset = 0;
	struct vring_desc * buf_desc = NULL;

	buf_desc = &(q->desc[buf_idx]);
	PrintDebug("VNET Bridge RX: buffer desc len: %d\n", buf_desc->length);

	len = copy_data_to_desc(&(vnet_brg->vm->cores[0]), buf_desc, buf + offset, data_len - offset);
	if (len == -1) {
	    ret_val = -1;
	    goto exit;
	}
	offset += len;
	buf_desc->length = len;
	
	if (offset >= data_len) {
	    buf_desc->flags &= ~VIRTIO_NEXT_FLAG;
	}else {
	    PrintDebug("VNET Bridge: RX: Pkt not fit into one desc buf\n");
	}

	q->used->ring[q->used->index % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];
	q->used->ring[q->used->index % q->queue_size].length = data_len; // This should be the total length of data sent to guest (header+pkt_data)
	q->used->index++;

	int last_idx = q->cur_avail_idx;
	q->cur_avail_idx++;
	if (q->cur_avail_idx < last_idx)
	    q->idx_overflow = false;
    } else {
	PrintDebug("VNET Bridge: Handle RX: Fails to send, no available buffer: current_idx:%d, availIdx: %d\n", q->cur_avail_idx, q->avail->index);
    }

    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	v3_pci_raise_irq(vnet_brg->pci_bus, 0, vnet_brg->pci_dev);
	vnet_brg->virtio_cfg.pci_isr = 0x1;
	PrintDebug("Raising IRQ %d\n",  vnet_brg->pci_dev->config_header.intr_line);
    }

    ret_val = 0;

exit:

    v3_unlock_irqrestore(vnet_brg->lock, flags);
 
    return ret_val;
}

static int vnet_send(struct v3_vnet_pkt *pkt, int pkt_len, void * private_data){
    struct guest_info *core  = (struct guest_info *)private_data; 

#ifdef CONFIG_DEBUG_VNET_BRG
    {
    	PrintDebug("VNET Bridge: send pkt size: %d\n", pkt->size);
    	v3_hexdump(pkt->data,pkt->size, NULL, 0);
    }
#endif

#ifdef CONFIG_VNET_PROFILE
    uint64_t start, end;
    rdtscll(start);
    core->vnet_times.time_copy_from_guest = start - core->vnet_times.virtio_handle_start;
#endif

    pkt->src_type = LINK_EDGE;

    return v3_vnet_send_pkt(pkt, (void *)core);
}

static int pkt_tx(struct guest_info *core, struct virtio_vnet_state * vnet_brg, struct vring_desc * buf_desc) 
{
    uint8_t * buf = NULL;
    uint32_t len = buf_desc->length;
    struct v3_vnet_pkt *pkt;

    if (guest_pa_to_host_va(core, buf_desc->addr_gpa, (addr_t *)&(buf)) == -1) {
	PrintError("Could not translate buffer address\n");
	return -1;
    }
 
    pkt = (struct v3_vnet_pkt *)buf;
    if (vnet_send(pkt, len, (void *)core) == -1) {
	return -1;
    }

    return 0;
}

static int handle_pkt_tx(struct guest_info *core, struct virtio_vnet_state * vnet_brg) 
{
    struct virtio_queue * q = &(vnet_brg->queue[XMIT_QUEUE]);

    if (q->avail->index < q->last_avail_idx) {
	q->idx_overflow = true;
    }

    q->last_avail_idx = q->avail->index;

    PrintDebug("VNET Bridge Device: Handle TX\n");

    while (q->cur_avail_idx < q->avail->index || 
	   (q->idx_overflow && q->cur_avail_idx < (q->avail->index + 65536))) {
	struct vring_desc * buf_desc = NULL;
	uint16_t desc_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];

	buf_desc = &(q->desc[desc_idx]);

	PrintDebug("VNET Bridge: Handle TX buf_len: %d\n", buf_desc->length);

	if (pkt_tx(core, vnet_brg, buf_desc) == -1) {
		PrintError("Error handling nic operation\n");
		return -1;
	}
	 
	q->used->ring[q->used->index % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];
	q->used->ring[q->used->index % q->queue_size].length = buf_desc->length; // What do we set this to????
	q->used->index++;

	int last_idx = q->cur_avail_idx;
	q->cur_avail_idx ++;
	if (q->cur_avail_idx < last_idx) {
	    q->idx_overflow = false;
	}
    }

    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	v3_pci_raise_irq(vnet_brg->pci_bus, 0, vnet_brg->pci_dev);
	vnet_brg->virtio_cfg.pci_isr = 0x1;
    }

#ifdef CONFIG_VNET_PROFILE
    uint64_t time;
    rdtscll(time);
    core->vnet_times.total_handle_time = time - core->vnet_times.virtio_handle_start;
    core->vnet_times.print = true;
#endif

    return 0;
}

static int virtio_io_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * private_data) {
    struct virtio_vnet_state * vnet_brg = (struct virtio_vnet_state *)private_data;
    int port_idx = port % vnet_brg->io_range_size;


    PrintDebug("VNET Bridge: VIRTIO VNET Write for port %d len=%d, value=%x\n", 
	       port, length, *(uint32_t *)src);
    PrintDebug("VNET Bridge: port idx=%d\n", port_idx);


    switch (port_idx) {
	case GUEST_FEATURES_PORT:
	    if (length != 4) {
		PrintError("Illegal write length for guest features\n");
		return -1;
	    }    
	    vnet_brg->virtio_cfg.guest_features = *(uint32_t *)src;

	    break;
	case VRING_PG_NUM_PORT:
	    if (length == 4) {
		addr_t pfn = *(uint32_t *)src;
		addr_t page_addr = (pfn << VIRTIO_PAGE_SHIFT);

		vnet_brg->cur_queue->pfn = pfn;
		
		vnet_brg->cur_queue->ring_desc_addr = page_addr ;
		vnet_brg->cur_queue->ring_avail_addr = page_addr + (QUEUE_SIZE * sizeof(struct vring_desc));
		vnet_brg->cur_queue->ring_used_addr = ( vnet_brg->cur_queue->ring_avail_addr + \
						 sizeof(struct vring_avail)    + \
						 (QUEUE_SIZE * sizeof(uint16_t)));
		
		// round up to next page boundary.
		vnet_brg->cur_queue->ring_used_addr = (vnet_brg->cur_queue->ring_used_addr + 0xfff) & ~0xfff;

		if (guest_pa_to_host_va(core, vnet_brg->cur_queue->ring_desc_addr, (addr_t *)&(vnet_brg->cur_queue->desc)) == -1) {
		    PrintError("Could not translate ring descriptor address\n");
		    return -1;
		}

		if (guest_pa_to_host_va(core, vnet_brg->cur_queue->ring_avail_addr, (addr_t *)&(vnet_brg->cur_queue->avail)) == -1) {
		    PrintError("Could not translate ring available address\n");
		    return -1;
		}

		if (guest_pa_to_host_va(core, vnet_brg->cur_queue->ring_used_addr, (addr_t *)&(vnet_brg->cur_queue->used)) == -1) {
		    PrintError("Could not translate ring used address\n");
		    return -1;
		}

		PrintDebug("VNET Bridge: RingDesc_addr=%p, Avail_addr=%p, Used_addr=%p\n",
			   (void *)(vnet_brg->cur_queue->ring_desc_addr),
			   (void *)(vnet_brg->cur_queue->ring_avail_addr),
			   (void *)(vnet_brg->cur_queue->ring_used_addr));

		PrintDebug("VNET Bridge: RingDesc=%p, Avail=%p, Used=%p\n", 
			   vnet_brg->cur_queue->desc, vnet_brg->cur_queue->avail, vnet_brg->cur_queue->used);

	    } else {
		PrintError("Illegal write length for page frame number\n");
		return -1;
	    }
	    break;
	case VRING_Q_SEL_PORT:
	    vnet_brg->virtio_cfg.vring_queue_selector = *(uint16_t *)src;

	    if (vnet_brg->virtio_cfg.vring_queue_selector > NUM_QUEUES) {
		PrintError("VNET Bridge device has no qeueues. Selected %d\n", 
			   vnet_brg->virtio_cfg.vring_queue_selector);
		return -1;
	    }
	    
	    vnet_brg->cur_queue = &(vnet_brg->queue[vnet_brg->virtio_cfg.vring_queue_selector]);

	    break;
	case VRING_Q_NOTIFY_PORT: {
	    uint16_t queue_idx = *(uint16_t *)src;

	    PrintDebug("VNET Bridge: Handling Kick\n");

	    if (queue_idx == 0) {
		if (handle_cmd_kick(core, vnet_brg) == -1) {
		    PrintError("Could not handle Virtio VNET Control command\n");
		    return -1;
		}
	    } else if (queue_idx == 1) {
		if (handle_pkt_tx(core, vnet_brg) == -1){
		    PrintError("Could not handle Virtio VNET TX\n");
		    return -1;
		}
	    } else if (queue_idx == 2) {
		PrintDebug("VNET Bridge: receive kick on RX Queue\n");
	    } else {
		PrintError("VNET Bridge: Kick on invalid queue (%d)\n", queue_idx);
		return -1;
	    }

	    break;
	}
	case VIRTIO_STATUS_PORT:
	    vnet_brg->virtio_cfg.status = *(uint8_t *)src;

	    if (vnet_brg->virtio_cfg.status == 0) {
		PrintDebug("VNET Bridge: Resetting device\n");
		virtio_reset(vnet_brg);
	    }

	    break;

	case VIRTIO_ISR_PORT:
	    vnet_brg->virtio_cfg.pci_isr = *(uint8_t *)src;
	    break;
	default:
	    return -1;
	    break;
    }

    return length;
}


static int virtio_io_read(struct guest_info * core, uint16_t port, void * dst, uint_t length, void * private_data) {

    struct virtio_vnet_state * vnet_state = (struct virtio_vnet_state *)private_data;
    int port_idx = port % vnet_state->io_range_size;

/*
    PrintDebug("VirtioVNET: VIRTIO SYMBIOTIC Read  for port %d (index =%d), length=%d\n", 
	       port, port_idx, length);
*/
    switch (port_idx) {
	case HOST_FEATURES_PORT:
	    if (length != 4) {
		PrintError("Illegal read length for host features\n");
		return -1;
	    }

	    *(uint32_t *)dst = vnet_state->virtio_cfg.host_features;
	
	    break;
	case VRING_PG_NUM_PORT:
	    if (length != 4) {
		PrintError("Illegal read length for page frame number\n");
		return -1;
	    }

	    *(uint32_t *)dst = vnet_state->cur_queue->pfn;

	    break;
	case VRING_SIZE_PORT:
	    if (length != 2) {
		PrintError("Illegal read length for vring size\n");
		return -1;
	    }
		
	    *(uint16_t *)dst = vnet_state->cur_queue->queue_size;

	    break;

	case VIRTIO_STATUS_PORT:
	    if (length != 1) {
		PrintError("Illegal read length for status\n");
		return -1;
	    }

	    *(uint8_t *)dst = vnet_state->virtio_cfg.status;
	    break;

	case VIRTIO_ISR_PORT:
	    *(uint8_t *)dst = vnet_state->virtio_cfg.pci_isr;
	    vnet_state->virtio_cfg.pci_isr = 0;
	    v3_pci_lower_irq(vnet_state->pci_bus, 0, vnet_state->pci_dev);
	    break;

	default:
	    if ( (port_idx >= sizeof(struct virtio_config)) && 
		 (port_idx < (sizeof(struct virtio_config) + sizeof(struct vnet_config))) ) {
		int cfg_offset = port_idx - sizeof(struct virtio_config);
		uint8_t * cfg_ptr = (uint8_t *)&(vnet_state->vnet_cfg);

		memcpy(dst, cfg_ptr + cfg_offset, length);
		
	    } else {
		PrintError("Read of Unhandled Virtio Read\n");
		return -1;
	    }
	  
	    break;
    }

    return length;
}

static int vnet_brg_input(struct v3_vm_info * vm, 
				struct v3_vnet_pkt * pkt, 
				void * private_data){

    return vnet_brg_rx(pkt, private_data);
}

static struct v3_device_ops dev_ops = {
    .free = NULL,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};


static int vnet_brg_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vm_device * pci_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    struct virtio_vnet_state * vbrg_state = NULL;
    struct pci_device * pci_dev = NULL;
    char * name = v3_cfg_val(cfg, "name");

    PrintDebug("VNET Bridge: Initializing VNET Bridge Control device: %s\n", name);

    if (pci_bus == NULL) {
	PrintError("VNET Bridge device require a PCI Bus");
	return -1;
    }
    
    vbrg_state  = (struct virtio_vnet_state *)V3_Malloc(sizeof(struct virtio_vnet_state));
    memset(vbrg_state, 0, sizeof(struct virtio_vnet_state));
	
    vbrg_state->vm = vm;

    struct vm_device * dev = v3_allocate_device(name, &dev_ops, vbrg_state);

    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", name);
	return -1;
    }


    // PCI initialization
    {
	struct v3_pci_bar bars[6];
	int num_ports = sizeof(struct virtio_config) + sizeof(struct vnet_config);
	int tmp_ports = num_ports;
	int i;

	// This gets the number of ports, rounded up to a power of 2
	vbrg_state->io_range_size = 1; // must be a power of 2

	while (tmp_ports > 0) {
	    tmp_ports >>= 1;
	    vbrg_state->io_range_size <<= 1;
	}
	
	// this is to account for any low order bits being set in num_ports
	// if there are none, then num_ports was already a power of 2 so we shift right to reset it
	if ((num_ports & ((vbrg_state->io_range_size >> 1) - 1)) == 0) {
	    vbrg_state->io_range_size >>= 1;
	}

	for (i = 0; i < 6; i++) {
	    bars[i].type = PCI_BAR_NONE;
	}

	bars[0].type = PCI_BAR_IO;
	bars[0].default_base_port = -1;
	bars[0].num_ports = vbrg_state->io_range_size;
	bars[0].io_read = virtio_io_read;
	bars[0].io_write = virtio_io_write;
	bars[0].private_data = vbrg_state;

	pci_dev = v3_pci_register_device(pci_bus, PCI_STD_DEVICE, 
					 0, PCI_AUTO_DEV_NUM, 0,
					 "LNX_VIRTIO_VNET", bars,
					 NULL, NULL, NULL, vbrg_state);

	if (!pci_dev) {
	    PrintError("Could not register PCI Device\n");
	    return -1;
	}
	
	pci_dev->config_header.vendor_id = VIRTIO_VENDOR_ID;
	pci_dev->config_header.subsystem_vendor_id = VIRTIO_SUBVENDOR_ID;
	pci_dev->config_header.device_id = VIRTIO_VNET_DEV_ID;
	pci_dev->config_header.class = PCI_CLASS_MEMORY;
	pci_dev->config_header.subclass = PCI_MEM_SUBCLASS_RAM;
	pci_dev->config_header.subsystem_id = VIRTIO_VNET_SUBDEVICE_ID;
	pci_dev->config_header.intr_pin = 1;
	pci_dev->config_header.max_latency = 1; // ?? (qemu does it...)


	vbrg_state->pci_dev = pci_dev;
	vbrg_state->pci_bus = pci_bus;
    }

    virtio_reset(vbrg_state);

    v3_vnet_add_bridge(vm, vnet_brg_input, (void *)vbrg_state);

//for temporary hack
#if 1	
    {
    	uchar_t dstmac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
    	uchar_t zeromac[6] = {0,0,0,0,0,0};

	struct v3_vnet_route route;
	route.dst_id = 0;
	route.dst_type = LINK_EDGE;
	route.src_id = -1;
	route.src_type = LINK_ANY;

	memcpy(route.dst_mac, dstmac, 6);
	route.dst_mac_qual = MAC_NONE;
	memcpy(route.src_mac, zeromac, 6);
	route.src_mac_qual = MAC_ANY;
	   
	v3_vnet_add_route(route);
    }
#endif

    return 0;
}


device_register("LNX_VNET_BRG", vnet_brg_init)
