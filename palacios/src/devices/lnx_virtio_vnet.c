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
 * Copyright (c) 2008, Lei Xia <lxia@cs.northwestern.edu>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *            Lei Xia <lxia@cs.northwestern.edu>
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


#ifndef CONFIG_DEBUG_LINUX_VIRTIO_VNET
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define QUEUE_SIZE 4096
#define CMD_QUEUE_SIZE 128
#define NUM_QUEUES 3

struct vnet_config {
    uint32_t num_devs;
    uint32_t num_routes;
} __attribute__((packed));


#define CTRL_QUEUE 0
#define XMIT_QUEUE 1
#define RECV_QUEUE 2

struct virtio_vnet_state {
    struct v3_vm_info * vm;
    struct vnet_config vnet_cfg;
    struct virtio_config virtio_cfg;

    struct vm_device * pci_bus;
    struct pci_device * pci_dev;
	
    struct virtio_queue queue[NUM_QUEUES];

    struct virtio_queue * cur_queue;

    int io_range_size;
    v3_lock_t lock;

    ulong_t pkt_sent, pkt_recv, pkt_drop, tx_exit, rx_exit, total_exit;
    int ready;
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


struct vnet_bridge_pkt {
    uint32_t link_id;
    uint32_t pkt_size;
    uint8_t pkt[ETHERNET_PACKET_LEN];
}__attribute__((packed));


static int virtio_reset(struct virtio_vnet_state * vnet_state) {

    memset(vnet_state->queue, 0, sizeof(struct virtio_queue) * NUM_QUEUES);

    vnet_state->cur_queue = &(vnet_state->queue[0]);

    vnet_state->virtio_cfg.status = 0;
    vnet_state->virtio_cfg.pci_isr = 0;

    vnet_state->queue[0].queue_size = CMD_QUEUE_SIZE;
    vnet_state->queue[1].queue_size = QUEUE_SIZE;
    vnet_state->queue[2].queue_size = QUEUE_SIZE;

    memset(&(vnet_state->vnet_cfg), 0, sizeof(struct vnet_config));
    v3_lock_init(&(vnet_state->lock));

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

    while (q->cur_avail_idx != q->avail->index) {
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


	PrintDebug("VNET Bridge: CMD: Descriptor Count=%d, index=%d, desc_idx=%d\n", desc_cnt, q->cur_avail_idx % QUEUE_SIZE, desc_idx);

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
		PrintDebug("VNET Bridge: Adding VNET Route\n");

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


static int vnet_pkt_input_cb(struct v3_vm_info * vm,  struct v3_vnet_pkt vnet_pkts[], uint16_t pkt_num, void * private_data){
    struct virtio_vnet_state * vnet_state = (struct virtio_vnet_state *)private_data;
    struct virtio_queue * q = &(vnet_state->queue[RECV_QUEUE]);
    int ret_val = -1;
    unsigned long flags;
    uint16_t sent;
    struct v3_vnet_pkt *pkt;

    if(pkt_num <= 0)
	return 0;

    flags = v3_lock_irqsave(vnet_state->lock);
	
    if (q->ring_avail_addr == 0) {
	PrintError("Queue is not set\n");
	goto exit;
    }

    PrintDebug("VNET Bridge: RX: running on cpu: %d, num of pkts: %d\n", V3_Get_CPU(), pkt_num);

    for(sent = 0; sent < pkt_num; sent ++) {
	pkt = &vnet_pkts[sent];
	vnet_state->pkt_recv ++;

    	if (q->cur_avail_idx != q->avail->index) {
	    uint16_t pkt_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];
	    struct vring_desc * pkt_desc = NULL;
	    struct vnet_bridge_pkt * virtio_pkt = NULL;

	    pkt_desc = &(q->desc[pkt_idx]);
	    PrintDebug("VNET Bridge RX: buffer desc len: %d\n", pkt_desc->length);

	    if (guest_pa_to_host_va(&(vm->cores[0]), pkt_desc->addr_gpa, (addr_t *)&(virtio_pkt)) == -1) {
	        PrintError("Could not translate buffer address\n");
	        goto exit;
	    }

	    PrintDebug("VNET Bridge: RX: pkt sent to guest pkt size: %d, dst link: %d\n", pkt->size, pkt->dst_id);

	    // Fill in dst packet buffer
	    virtio_pkt->link_id = pkt->dst_id;
	    virtio_pkt->pkt_size = pkt->size;
	    memcpy(virtio_pkt->pkt, pkt->data, pkt->size);
	
	    q->used->ring[q->used->index % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];
	    q->used->ring[q->used->index % q->queue_size].length = sizeof(struct vnet_bridge_pkt); 

	    q->used->index++;
	    q->cur_avail_idx++;
	} else {
	    vnet_state->pkt_drop ++;
	    v3_vnet_disable_bridge();
	}
    }

    if(sent == 0){
	goto exit;
    }

    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	v3_pci_raise_irq(vnet_state->pci_bus, 0, vnet_state->pci_dev);
	vnet_state->virtio_cfg.pci_isr = 0x1;
	PrintDebug("Raising IRQ %d\n",  vnet_state->pci_dev->config_header.intr_line);
    }

    ret_val = 0;

    	
#ifdef CONFIG_VNET_PROFILE
    if (vnet_state->pkt_recv % 200000 == 0)
	PrintError("Vnet Bridge: sent: %ld, rxed: %ld, dropped: %ld, total exit: %ld, tx exit: %ld, rx exit: %ld\n",
			vnet_state->pkt_sent,
			vnet_state->pkt_recv,
			vnet_state->pkt_drop, 
			vnet_state->total_exit,
			vnet_state->tx_exit,
			vnet_state->rx_exit);
#endif

exit:

    v3_unlock_irqrestore(vnet_state->lock, flags);
 
    return ret_val;
}

static void vnet_pkt_input_xcall(void *data){
    struct v3_vnet_bridge_input_args *args = (struct v3_vnet_bridge_input_args *)data;
	
    vnet_pkt_input_cb(args->vm, args->vnet_pkts, args->pkt_num, args->private_data);
}

static int handle_pkt_kick(struct guest_info *core, struct virtio_vnet_state * vnet_state) 
{
    struct virtio_queue * q = &(vnet_state->queue[XMIT_QUEUE]);
    unsigned long flags = 0;
    int recvd = 0;
	
    flags = v3_lock_irqsave(vnet_state->lock);

    if (q->ring_avail_addr == 0) {
	goto exit;
    }

    while (q->cur_avail_idx != q->avail->index) {
	uint16_t desc_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];
	struct vring_desc * pkt_desc = NULL;
	struct vnet_bridge_pkt * virtio_pkt = NULL;

	pkt_desc = &(q->desc[desc_idx]);

	PrintDebug("VNET Bridge: Handle TX desc buf_len: %d\n", pkt_desc->length);
	
	if (guest_pa_to_host_va(core, pkt_desc->addr_gpa, (addr_t *)&(virtio_pkt)) == -1) {
	    PrintError("Could not translate buffer address\n");
	    return -1;
	}

	PrintDebug("VNET Bridge: TX: on cpu %d pkt size: %d, dst link: %d\n", cpu, virtio_pkt->pkt_size, virtio_pkt->link_id);
	
	v3_vnet_rx(virtio_pkt->pkt, virtio_pkt->pkt_size, virtio_pkt->link_id, LINK_EDGE);

	q->used->ring[q->used->index % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];
	q->used->ring[q->used->index % q->queue_size].length = pkt_desc->length; // What do we set this to????
	q->used->index++;

	vnet_state->pkt_sent ++;
	recvd ++;

	q->cur_avail_idx++;
    }

    if(recvd == 0){
	goto exit;
    }

    //PrintError("In polling get %d\n", recvd);
	
    //if on the dom0 core, interrupt the domU core to poll pkts
    //otherwise, call the polling directly
    int cpu = V3_Get_CPU();
    if(vnet_state->vm->cores[0].cpu_id == cpu){
    	cpu = (cpu == 0)?1:0;
    	v3_interrupt_cpu(vnet_state->vm, cpu, V3_VNET_POLLING_VECTOR);
    }else{
    	v3_vnet_polling();
    }

    if((vnet_state->pkt_sent % (QUEUE_SIZE/20)) == 0) { //optimized for guest's, batch the interrupts
	    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
		v3_pci_raise_irq(vnet_state->pci_bus, 0, vnet_state->pci_dev);
		vnet_state->virtio_cfg.pci_isr = 0x1;
	    }
    }

#ifdef CONFIG_VNET_PROFILE
    if (vnet_state->pkt_sent % 200000 == 0)
	PrintError("Vnet Bridge: sent: %ld, rxed: %ld, dropped: %ld, total exit: %ld, tx exit: %ld, rx exit: %ld\n",
			vnet_state->pkt_sent,
			vnet_state->pkt_recv,
			vnet_state->pkt_drop, 
			vnet_state->total_exit,
			vnet_state->tx_exit,
			vnet_state->rx_exit);
#endif

exit:
    v3_unlock_irqrestore(vnet_state->lock,flags);

    return 0;
}

static int polling_pkt_from_guest(struct v3_vm_info * vm, void *private_data){
    struct virtio_vnet_state * vnet_state = (struct virtio_vnet_state *)private_data;
	
    return handle_pkt_kick(&(vm->cores[0]), vnet_state);
}

static int handle_rx_kick(struct guest_info *core, struct virtio_vnet_state * vnet_state) 
{
    v3_vnet_enable_bridge();
	
    return 0;
}

static int vnet_virtio_io_write(struct guest_info * core, uint16_t port, void * src, uint_t length, void * private_data) {
    struct virtio_vnet_state * vnet_state = (struct virtio_vnet_state *)private_data;
    int port_idx = port % vnet_state->io_range_size;

    PrintDebug("VNET Bridge: VIRTIO VNET Write for port %d len=%d, value=%x\n", 
	       port, length, *(uint32_t *)src);
    PrintDebug("VNET Bridge: port idx=%d\n", port_idx);

    vnet_state->total_exit ++;

    switch (port_idx) {
	case GUEST_FEATURES_PORT:
	    if (length != 4) {
		PrintError("Illegal write length for guest features\n");
		return -1;
	    }    
	    vnet_state->virtio_cfg.guest_features = *(uint32_t *)src;

	    break;
	case VRING_PG_NUM_PORT:
	    if (length == 4) {
		addr_t pfn = *(uint32_t *)src;
		addr_t page_addr = (pfn << VIRTIO_PAGE_SHIFT);

		vnet_state->cur_queue->pfn = pfn;
		
		vnet_state->cur_queue->ring_desc_addr = page_addr ;
		vnet_state->cur_queue->ring_avail_addr = page_addr + (QUEUE_SIZE * sizeof(struct vring_desc));
		vnet_state->cur_queue->ring_used_addr = ( vnet_state->cur_queue->ring_avail_addr + \
						 sizeof(struct vring_avail)    + \
						 (QUEUE_SIZE * sizeof(uint16_t)));
		
		// round up to next page boundary.
		vnet_state->cur_queue->ring_used_addr = (vnet_state->cur_queue->ring_used_addr + 0xfff) & ~0xfff;

		if (guest_pa_to_host_va(core, vnet_state->cur_queue->ring_desc_addr, (addr_t *)&(vnet_state->cur_queue->desc)) == -1) {
		    PrintError("Could not translate ring descriptor address\n");
		    return -1;
		}

		if (guest_pa_to_host_va(core, vnet_state->cur_queue->ring_avail_addr, (addr_t *)&(vnet_state->cur_queue->avail)) == -1) {
		    PrintError("Could not translate ring available address\n");
		    return -1;
		}

		if (guest_pa_to_host_va(core, vnet_state->cur_queue->ring_used_addr, (addr_t *)&(vnet_state->cur_queue->used)) == -1) {
		    PrintError("Could not translate ring used address\n");
		    return -1;
		}

		PrintDebug("VNET Bridge: RingDesc_addr=%p, Avail_addr=%p, Used_addr=%p\n",
			   (void *)(vnet_state->cur_queue->ring_desc_addr),
			   (void *)(vnet_state->cur_queue->ring_avail_addr),
			   (void *)(vnet_state->cur_queue->ring_used_addr));

		PrintDebug("VNET Bridge: RingDesc=%p, Avail=%p, Used=%p\n", 
			   vnet_state->cur_queue->desc, vnet_state->cur_queue->avail, vnet_state->cur_queue->used);

		if(vnet_state->queue[RECV_QUEUE].avail != NULL){
		    vnet_state->ready = 1;
		}

		//No notify when there is pkt tx from guest
		if(vnet_state->queue[XMIT_QUEUE].used != NULL){
		    vnet_state->queue[XMIT_QUEUE].used->flags |= VRING_NO_NOTIFY_FLAG;
		}
		
	    } else {
		PrintError("Illegal write length for page frame number\n");
		return -1;
	    }
	    break;
	case VRING_Q_SEL_PORT:
	    vnet_state->virtio_cfg.vring_queue_selector = *(uint16_t *)src;

	    if (vnet_state->virtio_cfg.vring_queue_selector > NUM_QUEUES) {
		PrintError("VNET Bridge device has no qeueues. Selected %d\n", 
			   vnet_state->virtio_cfg.vring_queue_selector);
		return -1;
	    }
	    
	    vnet_state->cur_queue = &(vnet_state->queue[vnet_state->virtio_cfg.vring_queue_selector]);

	    break;
	case VRING_Q_NOTIFY_PORT: {
	    uint16_t queue_idx = *(uint16_t *)src;

	    PrintDebug("VNET Bridge: Handling Kick\n");

	    if (queue_idx == 0) {
		if (handle_cmd_kick(core, vnet_state) == -1) {
		    PrintError("Could not handle Virtio VNET Control command\n");
		    return -1;
		}
	    } else if (queue_idx == 1) {
		if (handle_pkt_kick(core, vnet_state) == -1){
		    PrintError("Could not handle Virtio VNET TX\n");
		    return -1;
		}
		vnet_state->tx_exit ++;
		//PrintError("Notify on TX\n");
	    } else if (queue_idx == 2) {
		if (handle_rx_kick(core, vnet_state) == -1){
		    PrintError("Could not handle Virtio RX buffer refills Kick\n");
		    return -1;
		}
		vnet_state->rx_exit ++;
	    } else {
		PrintError("VNET Bridge: Kick on invalid queue (%d)\n", queue_idx);
		return -1;
	    }

	    break;
	}
	case VIRTIO_STATUS_PORT:
	    vnet_state->virtio_cfg.status = *(uint8_t *)src;

	    if (vnet_state->virtio_cfg.status == 0) {
		PrintDebug("VNET Bridge: Resetting device\n");
		virtio_reset(vnet_state);
	    }

	    break;

	case VIRTIO_ISR_PORT:
	    vnet_state->virtio_cfg.pci_isr = *(uint8_t *)src;
	    break;
	default:
	    return -1;
	    break;
    }

    return length;
}


static int vnet_virtio_io_read(struct guest_info * core, uint16_t port, void * dst, uint_t length, void * private_data) {

    struct virtio_vnet_state * vnet_state = (struct virtio_vnet_state *)private_data;
    int port_idx = port % vnet_state->io_range_size;

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



static struct v3_device_ops dev_ops = {
    .free = NULL,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};

static int dev_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vm_device * pci_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    struct virtio_vnet_state * vnet_state = NULL;
    struct pci_device * pci_dev = NULL;
    char * name = v3_cfg_val(cfg, "name");

    PrintDebug("VNET Bridge: Initializing VNET Bridge Control device: %s\n", name);

    if (pci_bus == NULL) {
	PrintError("VNET Bridge device require a PCI Bus");
	return -1;
    }
    
    vnet_state  = (struct virtio_vnet_state *)V3_Malloc(sizeof(struct virtio_vnet_state));
    memset(vnet_state, 0, sizeof(struct virtio_vnet_state));
	
    vnet_state->vm = vm;

    struct vm_device * dev = v3_allocate_device(name, &dev_ops, vnet_state);

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
	vnet_state->io_range_size = 1; // must be a power of 2

	while (tmp_ports > 0) {
	    tmp_ports >>= 1;
	    vnet_state->io_range_size <<= 1;
	}
	
	// this is to account for any low order bits being set in num_ports
	// if there are none, then num_ports was already a power of 2 so we shift right to reset it
	if ((num_ports & ((vnet_state->io_range_size >> 1) - 1)) == 0) {
	    vnet_state->io_range_size >>= 1;
	}

	for (i = 0; i < 6; i++) {
	    bars[i].type = PCI_BAR_NONE;
	}

	bars[0].type = PCI_BAR_IO;
	bars[0].default_base_port = -1;
	bars[0].num_ports = vnet_state->io_range_size;
	bars[0].io_read = vnet_virtio_io_read;
	bars[0].io_write = vnet_virtio_io_write;
	bars[0].private_data = vnet_state;

	pci_dev = v3_pci_register_device(pci_bus, PCI_STD_DEVICE, 
					 0, PCI_AUTO_DEV_NUM, 0,
					 "LNX_VIRTIO_VNET", bars,
					 NULL, NULL, NULL, vnet_state);

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


	vnet_state->pci_dev = pci_dev;
	vnet_state->pci_bus = pci_bus;
    }

    virtio_reset(vnet_state);

    V3_Print("Registering Virtio device as vnet bridge\n");
    v3_vnet_add_bridge(vm, vnet_pkt_input_cb, vnet_pkt_input_xcall, polling_pkt_from_guest, 0, 500000, (void *)vnet_state);


    return 0;
}


device_register("LNX_VIRTIO_VNET", dev_init)
