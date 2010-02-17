/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2008, Cui Zheng <cuizheng@cs.unm.edu>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu>
 *             Cui Zheng <cuizheng@cs.unm.edu>
 * 		 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
 
#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <devices/lnx_virtio_pci.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_vnet.h>
#include <palacios/vmm_lock.h>

#include <devices/pci.h>


#ifndef CONFIG_DEBUG_VIRTIO_NET
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define VIRTIO_NIC_PROFILE

#define VIRTIO_NET_S_LINK_UP	1	/* Link is up */
#define VIRTIO_NET_MAX_BUFSIZE (sizeof(struct virtio_net_hdr) + (64 << 10))

struct virtio_net_hdr {
	uint8_t flags;

#define VIRTIO_NET_HDR_GSO_NONE		0	/* Not a GSO frame */
	uint8_t gso_type;
	uint16_t hdr_len;		/* Ethernet + IP + tcp/udp hdrs */
	uint16_t gso_size;		/* Bytes to append to hdr_len per frame */
	uint16_t csum_start;	/* Position to start checksumming from */
	uint16_t csum_offset;	/* Offset after that to place checksum */
}__attribute__((packed));

	
#define QUEUE_SIZE 1024
#define CTRL_QUEUE_SIZE 64
#define ETH_ALEN 6

struct virtio_net_config
{
    uint8_t mac[ETH_ALEN]; //VIRTIO_NET_F_MAC
    uint16_t status;
} __attribute__((packed));

struct virtio_dev_state {
    struct vm_device * pci_bus;
    struct list_head dev_list;
    struct guest_info *vm;
};

struct virtio_net_state {
    struct virtio_net_config net_cfg;
    struct virtio_config virtio_cfg;

    struct vm_device * dev;
    struct pci_device * pci_dev; 
    int io_range_size;
    
    struct virtio_queue rx_vq;   //index 0, rvq in Linux virtio driver, handle packet to guest
    struct virtio_queue tx_vq;   //index 1, svq in Linux virtio driver, handle packet from guest
    struct virtio_queue ctrl_vq; //index 2, ctrol info from guest

    ulong_t pkt_sent, pkt_recv, pkt_drop;

    struct v3_dev_net_ops * net_ops;

    void * backend_data;
    struct virtio_dev_state * virtio_dev;
    struct list_head dev_link;
};

static int virtio_free(struct vm_device * dev) 
{
	
    return -1;
}

static int virtio_init_state(struct virtio_net_state * virtio) 
{
    virtio->rx_vq.ring_desc_addr = 0;
    virtio->rx_vq.ring_avail_addr = 0;
    virtio->rx_vq.ring_used_addr = 0;
    virtio->rx_vq.pfn = 0;
    virtio->rx_vq.cur_avail_idx = 0;

    virtio->tx_vq.ring_desc_addr = 0;
    virtio->tx_vq.ring_avail_addr = 0;
    virtio->tx_vq.ring_used_addr = 0;
    virtio->tx_vq.pfn = 0;
    virtio->tx_vq.cur_avail_idx = 0;

    virtio->ctrl_vq.ring_desc_addr = 0;
    virtio->ctrl_vq.ring_avail_addr = 0;
    virtio->ctrl_vq.ring_used_addr = 0;
    virtio->ctrl_vq.pfn = 0;
    virtio->ctrl_vq.cur_avail_idx = 0;

    virtio->virtio_cfg.host_features = 0;
    //virtio->virtio_cfg.status = VIRTIO_NET_S_LINK_UP;
    virtio->virtio_cfg.pci_isr = 0;

    virtio->pkt_sent = virtio->pkt_recv = virtio->pkt_drop = 0;

    return 0;
}

static int pkt_tx(struct virtio_net_state * virtio, struct vring_desc * buf_desc) 
{
    uint8_t * buf = NULL;
    uint32_t len = buf_desc->length;

    PrintDebug("Virtio NIC: Handling Virtio Write, net_state: %p\n", virtio);

    if (guest_pa_to_host_va(virtio->virtio_dev->vm, buf_desc->addr_gpa, (addr_t *)&(buf)) == -1) {
	PrintError("Could not translate buffer address\n");
	return -1;
    }

    if (virtio->net_ops->send(buf, len, (void *)virtio, NULL) == -1) {
	return -1;
    }

    return 0;
}

static int build_receive_header(struct virtio_net_hdr * hdr, const void * buf, int raw) {
    hdr->flags = 0;

    if (!raw) {
        memcpy(hdr, buf, sizeof(struct virtio_net_hdr));
    } else {
        memset(hdr, 0, sizeof(struct virtio_net_hdr));
    }

    return 0;
}

static int copy_data_to_desc(struct virtio_net_state * virtio_state, struct vring_desc * desc, uchar_t * buf, uint_t buf_len) 
{
    uint32_t len;
    uint8_t * desc_buf = NULL;

    if (guest_pa_to_host_va(virtio_state->virtio_dev->vm, desc->addr_gpa, (addr_t *)&(desc_buf)) == -1) {
	PrintError("Could not translate buffer address\n");
	return -1;
    }
    len = (desc->length < buf_len)?desc->length:buf_len;
    memcpy(desc_buf, buf, len);

    return len;
}

//send data to guest
int send_pkt_to_guest(struct virtio_net_state * virtio, uchar_t * buf, uint_t size, int raw, void * private_data) 
{
    struct virtio_queue * q = &(virtio->rx_vq);
    struct virtio_net_hdr hdr;
    uint32_t hdr_len = sizeof(struct virtio_net_hdr);
    uint32_t data_len = size;
    uint32_t offset = 0;
	
    PrintDebug("VIRTIO NIC:  sending packet to virtio nic %p, size:%d", virtio, size);

    virtio->pkt_recv ++;
    if (!raw) {
	data_len -= hdr_len;
    }

    build_receive_header(&hdr, buf, 1);

    if (q->ring_avail_addr == 0) {
	PrintError("Queue is not set\n");
	return -1;
    }

    if (q->last_avail_idx > q->avail->index)
	q->idx_overflow = true;
    q->last_avail_idx = q->avail->index;

    if (q->cur_avail_idx < q->avail->index || (q->idx_overflow && q->cur_avail_idx < q->avail->index+65536)){
	addr_t hdr_addr = 0;
	uint16_t hdr_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];
	uint16_t buf_idx = 0;
	struct vring_desc * hdr_desc = NULL;

	hdr_desc = &(q->desc[hdr_idx]);
	if (guest_pa_to_host_va(virtio->virtio_dev->vm, hdr_desc->addr_gpa, &(hdr_addr)) == -1) {
	    PrintError("Could not translate receive buffer address\n");
	    return -1;
	}

	memcpy((void *)hdr_addr, &hdr, sizeof(struct virtio_net_hdr));
	if (offset >= data_len) {
	    hdr_desc->flags &= ~VIRTIO_NEXT_FLAG;
	}

	for (buf_idx = hdr_desc->next; offset < data_len; buf_idx = q->desc[hdr_idx].next) {
	    struct vring_desc * buf_desc = &(q->desc[buf_idx]);
	    uint32_t len = 0;

	    len = copy_data_to_desc(virtio, buf_desc, buf + offset, data_len - offset);	    
	    offset += len;
	    if (offset < data_len) {
		buf_desc->flags = VIRTIO_NEXT_FLAG;		
	    }
	    buf_desc->length = len;
	}
	
	q->used->ring[q->used->index % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];
	q->used->ring[q->used->index % q->queue_size].length = data_len + hdr_len; // This should be the total length of data sent to guest (header+pkt_data)
	q->used->index++;

	int last_idx = q->cur_avail_idx;
	q->cur_avail_idx++;
	if (q->cur_avail_idx < last_idx)
	    q->idx_overflow = false;
    } else {
	virtio->pkt_drop++;

#ifdef VIRTIO_NIC_PROFILE
	PrintError("Virtio NIC: %p, one pkt dropped receieved: %ld, dropped: %ld, sent: %ld curidx: %d, avaiIdx: %d\n", 
		virtio, virtio->pkt_recv, virtio->pkt_drop, virtio->pkt_sent, q->cur_avail_idx, q->avail->index);
#endif
    }

    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	PrintDebug("Raising IRQ %d\n",  virtio->pci_dev->config_header.intr_line);
	v3_pci_raise_irq(virtio->virtio_dev->pci_bus, 0, virtio->pci_dev);
	virtio->virtio_cfg.pci_isr = 0x1;
    }

#ifdef VIRTIO_NIC_PROFILE
    if ((virtio->pkt_recv % 10000) == 0){
	PrintError("Virtio NIC: %p, receieved: %ld, dropped: %ld, sent: %ld\n", 
		virtio, virtio->pkt_recv, virtio->pkt_drop, virtio->pkt_sent);
    }
#endif

    
    return offset;
}

int virtio_dev_send(struct v3_vm_info *info,
                                   uchar_t * buf,
                                   uint32_t size,
                                   void *private_data) {
    struct virtio_net_state *virtio_state = (struct virtio_net_state *)private_data;

    return send_pkt_to_guest(virtio_state, buf, size, 1, NULL);
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

static int handle_ctrl(struct virtio_net_state * dev) {
    return 0;
}

static int handle_pkt_tx(struct virtio_net_state * virtio_state) 
{
    struct virtio_queue * q = &(virtio_state->tx_vq);
    struct virtio_net_hdr * hdr = NULL;

    if (q->avail->index < q->last_avail_idx)
	q->idx_overflow = true;
    q->last_avail_idx = q->avail->index;

    while (q->cur_avail_idx < q->avail->index || 
		 (q->idx_overflow && q->cur_avail_idx < (q->avail->index + 65536))) {
	struct vring_desc * hdr_desc = NULL;
	addr_t hdr_addr = 0;
	uint16_t desc_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];
	int desc_cnt = get_desc_count(q, desc_idx);
	uint32_t req_len = 0;
	int i = 0;

	hdr_desc = &(q->desc[desc_idx]);
	if (guest_pa_to_host_va(virtio_state->virtio_dev->vm, hdr_desc->addr_gpa, &(hdr_addr)) == -1) {
	    PrintError("Could not translate block header address\n");
	    return -1;
	}

	hdr = (struct virtio_net_hdr*)hdr_addr;
	desc_idx = hdr_desc->next;
	
	for (i = 0; i < desc_cnt - 1; i++) {	
	    struct vring_desc * buf_desc = &(q->desc[desc_idx]);
	    if (pkt_tx(virtio_state, buf_desc) == -1) {
		PrintError("Error handling nic operation\n");
		return -1;
	    }

	    req_len += buf_desc->length;
	    desc_idx = buf_desc->next;
	}
	virtio_state->pkt_sent ++;

	q->used->ring[q->used->index % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];
	q->used->ring[q->used->index % q->queue_size].length = req_len; // What do we set this to????
	q->used->index++;

	int last_idx = q->cur_avail_idx;
	q->cur_avail_idx ++;
	if (q->cur_avail_idx < last_idx)
	    q->idx_overflow = false;
    }

    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	v3_pci_raise_irq(virtio_state->virtio_dev->pci_bus, 0, virtio_state->pci_dev);
	virtio_state->virtio_cfg.pci_isr = 0x1;
    }

#ifdef VIRTIO_NIC_PROFILE
    if(virtio_state->pkt_sent % 10000 == 0)
 	PrintError("Virtio NIC: %p, pkt_sent: %ld\n", virtio_state, virtio_state->pkt_sent);
#endif	

    return 0;
}

static int virtio_setup_queue(struct virtio_net_state * virtio_state, struct virtio_queue * queue, addr_t pfn, addr_t page_addr) {
    queue->pfn = pfn;
		
    queue->ring_desc_addr = page_addr;
    queue->ring_avail_addr = page_addr + (queue->queue_size * sizeof(struct vring_desc));
    queue->ring_used_addr = ((queue->ring_avail_addr) + 
			     (sizeof(struct vring_avail)) + 
			     (queue->queue_size * sizeof(uint16_t)));
		
    // round up to next page boundary.
    queue->ring_used_addr = (queue->ring_used_addr + 0xfff) & ~0xfff;
    if (guest_pa_to_host_va(virtio_state->virtio_dev->vm, queue->ring_desc_addr, (addr_t *)&(queue->desc)) == -1) {
        PrintError("Could not translate ring descriptor address\n");
	 return -1;
    }
 
    if (guest_pa_to_host_va(virtio_state->virtio_dev->vm, queue->ring_avail_addr, (addr_t *)&(queue->avail)) == -1) {
        PrintError("Could not translate ring available address\n");
        return -1;
    }

    if (guest_pa_to_host_va(virtio_state->virtio_dev->vm, queue->ring_used_addr, (addr_t *)&(queue->used)) == -1) {
        PrintError("Could not translate ring used address\n");
        return -1;
    }

    PrintDebug("RingDesc_addr=%p, Avail_addr=%p, Used_addr=%p\n",
	       (void *)(queue->ring_desc_addr),
	       (void *)(queue->ring_avail_addr),
	       (void *)(queue->ring_used_addr));
    
    PrintDebug("RingDesc=%p, Avail=%p, Used=%p\n", 
	       queue->desc, queue->avail, queue->used);
    
    return 0;
}

static int virtio_io_write(struct guest_info *core, uint16_t port, void * src, uint_t length, void * private_data) 
{
    struct virtio_net_state * virtio = (struct virtio_net_state *)private_data;
    int port_idx = port % virtio->io_range_size;

    PrintDebug("VIRTIO NIC %p Write for port %d (index=%d) len=%d, value=%x\n", private_data,
	       port, port_idx,  length, *(uint32_t *)src);

    switch (port_idx) {
	case GUEST_FEATURES_PORT:
	    if (length != 4) {
		PrintError("Illegal write length for guest features\n");
		return -1;
	    }	    
	    virtio->virtio_cfg.guest_features = *(uint32_t *)src;
	    PrintDebug("Setting Guest Features to %x\n", virtio->virtio_cfg.guest_features);
	    break;
		
	case VRING_PG_NUM_PORT:
	    if (length != 4) {
		PrintError("Illegal write length for page frame number\n");
		return -1;
	    }
	    addr_t pfn = *(uint32_t *)src;
	    addr_t page_addr = (pfn << VIRTIO_PAGE_SHIFT);
	    uint16_t queue_idx = virtio->virtio_cfg.vring_queue_selector;
	    switch (queue_idx) {
		case 0:
		    virtio_setup_queue(virtio, &virtio->rx_vq, pfn, page_addr);
		    break;
		case 1:
		    virtio_setup_queue(virtio, &virtio->tx_vq, pfn, page_addr);
		    break;
		case 2:
		    virtio_setup_queue(virtio, &virtio->ctrl_vq, pfn, page_addr);
		    break;	    
		default:
		    break;
	    }
	    break;
		
	case VRING_Q_SEL_PORT:
	    virtio->virtio_cfg.vring_queue_selector = *(uint16_t *)src;
	    if (virtio->virtio_cfg.vring_queue_selector > 2) {
		PrintError("Virtio NIC device only uses 3 queue, selected %d\n", 
			   virtio->virtio_cfg.vring_queue_selector);
		return -1;
	    }
	    break;
		
	case VRING_Q_NOTIFY_PORT: 
	    {
		uint16_t queue_idx = *(uint16_t *)src;	   		
		if (queue_idx == 0){
		    PrintDebug("receive queue notification 0, packet get by Guest\n");
		} else if (queue_idx == 1){
		    if (handle_pkt_tx(virtio) == -1) {
			PrintError("Could not handle NIC Notification\n");
			return -1;
		    }
		} else if (queue_idx == 2){
		    if (handle_ctrl(virtio) == -1) {
			PrintError("Could not handle NIC Notification\n");
			return -1;
		    }
		} else {
		    PrintError("Virtio NIC device only uses 3 queue, selected %d\n", 
			       queue_idx);
		}	
		break;		
	    }
	
	case VIRTIO_STATUS_PORT:
	    virtio->virtio_cfg.status = *(uint8_t *)src;
	    if (virtio->virtio_cfg.status == 0) {
		PrintDebug("Resetting device\n");
		virtio_init_state(virtio);
	    }
	    break;

	case VIRTIO_ISR_PORT:
	    virtio->virtio_cfg.pci_isr = *(uint8_t *)src;
	    break;
	default:
	    return -1;
	    break;
    }

    return length;
}

static int virtio_io_read(struct guest_info *core, uint16_t port, void * dst, uint_t length, void * private_data) 
{
    struct virtio_net_state * virtio = (struct virtio_net_state *)private_data;
    int port_idx = port % virtio->io_range_size;
    uint16_t queue_idx = virtio->virtio_cfg.vring_queue_selector;

    PrintDebug("Virtio NIC %p: Read  for port %d (index =%d), length=%d", private_data,
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
 	    switch (queue_idx) {
	        case 0:
		    *(uint32_t *)dst = virtio->rx_vq.pfn;
		    break;
		case 1:
		    *(uint32_t *)dst = virtio->tx_vq.pfn;
		    break;	
		case 2:
		    *(uint32_t *)dst = virtio->ctrl_vq.pfn;
		    break;
		default:
		    break;
	    }
	    break;

	case VRING_SIZE_PORT:
	    if (length != 2) {
		PrintError("Illegal read length for vring size\n");
		return -1;
	    }
	    switch (queue_idx) {
	        case 0:
		    *(uint16_t *)dst = virtio->rx_vq.queue_size;
		    break;
		case 1:
		    *(uint16_t *)dst = virtio->tx_vq.queue_size;
		    break;	
		case 2:
		    *(uint16_t *)dst = virtio->ctrl_vq.queue_size;
		    break;
		default:
		    break;
	    }
	    PrintDebug("queue index: %d, value=0x%x\n", (int)queue_idx, *(uint16_t *)dst);
	    break;

	case VIRTIO_STATUS_PORT:
	    if (length != 1) {
		PrintError("Illegal read length for status\n");
		return -1;
	    }
	    *(uint8_t *)dst = virtio->virtio_cfg.status;
	    break;
		
	case VIRTIO_ISR_PORT:
	    *(uint8_t *)dst = virtio->virtio_cfg.pci_isr;
	    virtio->virtio_cfg.pci_isr = 0;
	    v3_pci_lower_irq(virtio->virtio_dev->pci_bus, 0, virtio->pci_dev);
	    break;

	default:
	    PrintError("Virtio NIC: Read of Unhandled Virtio Read\n");
	    return -1;
    }

    return length;
}


static struct v3_device_ops dev_ops = {
    .free = virtio_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};

static int register_dev(struct virtio_dev_state * virtio, struct virtio_net_state * net_state) 
{
    struct pci_device * pci_dev = NULL;
    struct v3_pci_bar bars[6];
    int num_ports = sizeof(struct virtio_config);
    int tmp_ports = num_ports;
    int i;

    // This gets the number of ports, rounded up to a power of 2
    net_state->io_range_size = 1; // must be a power of 2
    while (tmp_ports > 0) {
	tmp_ports >>= 1;
	net_state->io_range_size <<= 1;
    }
	
    // this is to account for any low order bits being set in num_ports
    // if there are none, then num_ports was already a power of 2 so we shift right to reset it
    if ((num_ports & ((net_state->io_range_size >> 1) - 1)) == 0) {
	net_state->io_range_size >>= 1;
    }
    
    for (i = 0; i < 6; i++) {
	bars[i].type = PCI_BAR_NONE;
    }
    
    PrintDebug("Virtio-NIC io_range_size = %d\n", net_state->io_range_size);
    
    bars[0].type = PCI_BAR_IO;
    bars[0].default_base_port = -1;
    bars[0].num_ports = net_state->io_range_size;
    bars[0].io_read = virtio_io_read;
    bars[0].io_write = virtio_io_write;
    bars[0].private_data = net_state;
    
    pci_dev = v3_pci_register_device(virtio->pci_bus, PCI_STD_DEVICE, 
				     0, PCI_AUTO_DEV_NUM, 0,
				     "LNX_VIRTIO_NIC", bars,
				     NULL, NULL, NULL, net_state);
    
    if (!pci_dev) {
	PrintError("Virtio NIC: Could not register PCI Device\n");
	return -1;
    }

    PrintDebug("Virtio NIC:  registered to PCI bus\n");
    
    pci_dev->config_header.vendor_id = VIRTIO_VENDOR_ID;
    pci_dev->config_header.subsystem_vendor_id = VIRTIO_SUBVENDOR_ID;
	

    pci_dev->config_header.device_id = VIRTIO_NET_DEV_ID;
    pci_dev->config_header.class = PCI_CLASS_NETWORK;
    pci_dev->config_header.subclass = PCI_NET_SUBCLASS_OTHER;  
    pci_dev->config_header.subsystem_id = VIRTIO_NET_SUBDEVICE_ID;
    pci_dev->config_header.intr_pin = 1;
    pci_dev->config_header.max_latency = 1; // ?? (qemu does it...)

    net_state->pci_dev = pci_dev;   
    net_state->virtio_cfg.host_features = 0; //no features support now
    net_state->rx_vq.queue_size = QUEUE_SIZE;
    net_state->tx_vq.queue_size = QUEUE_SIZE;
    net_state->ctrl_vq.queue_size = CTRL_QUEUE_SIZE;
    net_state->virtio_dev = virtio;
   
    virtio_init_state(net_state);

    return 0;
}

static int connect_fn(struct v3_vm_info * info, 
		      void * frontend_data, 
		      struct v3_dev_net_ops * ops, 
		      v3_cfg_tree_t * cfg, 
		      void * private_data) {
    struct virtio_dev_state * virtio = (struct virtio_dev_state *)frontend_data;
    struct virtio_net_state * net_state  = (struct virtio_net_state *)V3_Malloc(sizeof(struct virtio_net_state));

    memset(net_state, 0, sizeof(struct virtio_net_state));
    register_dev(virtio, net_state);

    net_state->net_ops = ops;
    net_state->backend_data = private_data;

    //register input callback to the backend
    net_state->net_ops->register_input(net_state->backend_data, virtio_dev_send, (void *)net_state);

    return 0;
}

static int virtio_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vm_device * pci_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    struct virtio_dev_state * virtio_state = NULL;
    char * name = v3_cfg_val(cfg, "name");

    PrintDebug("Virtio NIC: Initializing VIRTIO Network device: %s\n", name);

    if (pci_bus == NULL) {
	PrintError("Virtio NIC: VirtIO devices require a PCI Bus");
	return -1;
    }

    virtio_state  = (struct virtio_dev_state *)V3_Malloc(sizeof(struct virtio_dev_state));
    memset(virtio_state, 0, sizeof(struct virtio_dev_state));

    INIT_LIST_HEAD(&(virtio_state->dev_list));
    virtio_state->pci_bus = pci_bus;

    struct vm_device * dev = v3_allocate_device(name, &dev_ops, virtio_state);
    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Virtio NIC: Could not attach device %s\n", name);
	return -1;
    }

    if (v3_dev_add_net_frontend(vm, name, connect_fn, (void *)virtio_state) == -1) {
	PrintError("Virtio NIC: Could not register %s as net frontend\n", name);
	return -1;
    }
	
    return 0;
}

device_register("LNX_VIRTIO_NIC", virtio_init)
	
