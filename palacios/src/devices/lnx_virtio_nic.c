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
 * Copyright (c) 2008, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *		  Lei Xia <lxia@northwestern.edu>
 * 		 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
 
#include <palacios/vmm.h>
#include <palacios/vmm_dev_mgr.h>
#include <devices/lnx_virtio_pci.h>
#include <devices/lnx_virtio_nic.h>
#include <palacios/vm_guest_mem.h>

#include <devices/pci.h>


#ifndef CONFIG_DEBUG_VIRTIO_BLK
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define NIC_STATUS_OK             0
#define NIC_STATUS_ERR            1
#define NIC_STATUS_NOT_SUPPORTED  2


/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM	0	/* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM	1	/* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_MAC	5	/* Host has given MAC address. */
#define VIRTIO_NET_F_GSO	6	/* Host handles pkts w/ any GSO type */
#define VIRTIO_NET_F_GUEST_TSO4	7	/* Guest can handle TSOv4 in. */
#define VIRTIO_NET_F_GUEST_TSO6	8	/* Guest can handle TSOv6 in. */
#define VIRTIO_NET_F_GUEST_ECN	9	/* Guest can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_GUEST_UFO	10	/* Guest can handle UFO in. */
#define VIRTIO_NET_F_HOST_TSO4	11	/* Host can handle TSOv4 in. */
#define VIRTIO_NET_F_HOST_TSO6	12	/* Host can handle TSOv6 in. */
#define VIRTIO_NET_F_HOST_ECN	13	/* Host can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_HOST_UFO	14	/* Host can handle UFO in. */
#define VIRTIO_NET_F_MRG_RXBUF	15	/* Host can merge receive buffers. */
#define VIRTIO_NET_F_STATUS	16	/* virtio_net_config.status available */
#define VIRTIO_NET_F_CTRL_VQ	17	/* Control channel available */
#define VIRTIO_NET_F_CTRL_RX	18	/* Control channel RX mode support */
#define VIRTIO_NET_F_CTRL_VLAN	19	/* Control channel VLAN filtering */
#define VIRTIO_NET_F_CTRL_RX_EXTRA 20	/* Extra RX mode control support */
#define VIRTIO_NET_S_LINK_UP	1	/* Link is up */

/* Maximum packet size we can receive from tap device: header + 64k */
#define VIRTIO_NET_MAX_BUFSIZE (sizeof(struct virtio_net_hdr) + (64 << 10))


struct virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM	1	/* Use csum_start, csum_offset */
	uint8_t flags;

#define VIRTIO_NET_HDR_GSO_NONE		0	/* Not a GSO frame */
#define VIRTIO_NET_HDR_GSO_TCPV4	1	/* GSO frame, IPv4 TCP (TSO) */
#define VIRTIO_NET_HDR_GSO_UDP		3	/* GSO frame, IPv4 UDP (UFO) */
#define VIRTIO_NET_HDR_GSO_TCPV6	4	/* GSO frame, IPv6 TCP */
#define VIRTIO_NET_HDR_GSO_ECN		0x80	/* TCP has ECN set */
	uint8_t gso_type;

	uint16_t hdr_len;		/* Ethernet + IP + tcp/udp hdrs */
	uint16_t gso_size;		/* Bytes to append to hdr_len per frame */
	uint16_t csum_start;	/* Position to start checksumming from */
	uint16_t csum_offset;	/* Offset after that to place checksum */
}__attribute__((packed));

	


#define QUEUE_SIZE 256
#define CTRL_QUEUE_SIZE 64


struct v3_net_ops {
    int (*send)(uint8_t * buf, uint32_t count, void * private_data);
    int (*receive)(uint8_t * buf, uint32_t count, void * private_data);
};


#define ETH_ALEN 6

struct virtio_net_config
{
    uint8_t mac[ETH_ALEN];
    // See VIRTIO_NET_F_STATUS and VIRTIO_NET_S_* above
    uint16_t status;
} __attribute__((packed));

struct virtio_net_state {
    struct virtio_net_config net_cfg;
    struct virtio_config virtio_cfg;

    struct vm_device * pci_bus;
    struct pci_device * pci_dev;
    
    struct virtio_queue rx_vq;   //index 0, rvq in Linux virtio driver, handle packet to guest
    struct virtio_queue tx_vq;   //index 1, svq in Linux virtio driver, handle packet from guest
    struct virtio_queue ctrl_vq; //index 2, ctrol info from guest

    struct v3_net_ops * net_ops;

    int io_range_size;

    void *private_data;
};


static int virtio_free(struct vm_device * dev) 
{
	
    return -1;
}

static int virtio_reset(struct vm_device * dev) 
{
    struct virtio_net_state * virtio = (struct virtio_net_state *)dev->private_data;

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

    virtio->virtio_cfg.status = VIRTIO_NET_S_LINK_UP;
    virtio->virtio_cfg.pci_isr = 0;
    virtio->private_data = NULL;

    return 0;
}

static int read_op(struct vm_device * dev, uint8_t * buf, uint32_t len) 
{
    struct virtio_net_state * virtio = (struct virtio_net_state *)dev->private_data; 
    int ret = -1;

    PrintDebug("Receving pkt from guest\n");

    ret = virtio->net_ops->receive(buf, len, virtio->private_data);
    return ret;
}


static int write_op(struct vm_device *dev, uint8_t *buf, uint32_t len) 
{
    struct virtio_net_state * virtio = (struct virtio_net_state *)dev->private_data; 
    int ret = -1;

    PrintDebug("Receving pkt from guest\n");

    ret = virtio->net_ops->send(buf, len, virtio->private_data);

    return ret;
}


//sending guest's packet to network sink
static int handle_pkt_write(struct vm_device *dev, struct virtio_net_hdr *hdr, 
			   struct vring_desc *buf_desc, uint8_t *status) 
{
    //struct virtio_net_state * virtio = (struct virtio_net_state *)dev->private_data;    
    uint8_t * buf = NULL;

    PrintDebug("Handling Virtio Net write\n");

    if (guest_pa_to_host_va(dev->vm, buf_desc->addr_gpa, (addr_t *)&(buf)) == -1) {
	PrintError("Could not translate buffer address\n");
	return -1;
    }

    PrintDebug("Length=%d\n", buf_desc->length);

    if (write_op(dev, buf, buf_desc->length) == -1) {
	*status = NIC_STATUS_ERR;
	return -1;
    } else {
	*status = NIC_STATUS_OK;
    }

    PrintDebug("Returning Status: %d\n", *status);

    return 0;
}



//get packet from network, and send to guest
static int handle_pkt_read(struct vm_device *dev, struct virtio_net_hdr *hdr, 
			   struct vring_desc *buf_desc, uint8_t *status) 
{
    //struct virtio_net_state * virtio = (struct virtio_net_state *)dev->private_data;    
    uint8_t * buf = NULL;

    PrintDebug("Handling Virtio Net read\n");

    if (guest_pa_to_host_va(dev->vm, buf_desc->addr_gpa, (addr_t *)&(buf)) == -1) {
	PrintError("Could not translate buffer address\n");
	return -1;
    }

    PrintDebug("Length=%d\n", buf_desc->length);

    if (read_op(dev, buf, buf_desc->length) == -1) {
	*status = NIC_STATUS_ERR;
	return -1;
    } else {
	*status = NIC_STATUS_OK;
    }

    PrintDebug("Returning Status: %d\n", *status);

    return 0;
}

static int get_desc_count(struct virtio_queue * q, int index) 
{
    struct vring_desc * tmp_desc = &(q->desc[index]);
    int cnt = 1;
    
    while (tmp_desc->flags & VIRTIO_NEXT_FLAG) {
	tmp_desc = &(q->desc[tmp_desc->next]);
	cnt++;
    }

    return cnt;
}


static int handle_ctrl(struct vm_device * dev) {


    return 0;
}

// TODO: handle receiving, not done yet
//send packet to guest
static int handle_pkt_rx(struct vm_device * dev) 
{

    if (handle_pkt_read(dev, NULL, 0, NULL) == -1) {
		PrintError("Error handling nic operation\n");
		return -1;
	    }

    return 0;
}

//get packet from guest
static int handle_pkt_tx(struct vm_device * dev) 
{
    struct virtio_net_state *virtio = (struct virtio_net_state *)dev->private_data;    
    struct virtio_queue *q = &(virtio->rx_vq);

    PrintDebug("VIRTIO NIC KICK: cur_index=%d (mod=%d), avail_index=%d\n", 
	       q->cur_avail_idx, q->cur_avail_idx % QUEUE_SIZE, q->avail->index);

    while (q->cur_avail_idx < q->avail->index) {
	struct vring_desc * hdr_desc = NULL;
	struct vring_desc * buf_desc = NULL;
	struct vring_desc * status_desc = NULL;
	struct virtio_net_hdr hdr;
	addr_t hdr_addr = 0;
	uint16_t desc_idx = q->avail->ring[q->cur_avail_idx % QUEUE_SIZE];
	int desc_cnt = get_desc_count(q, desc_idx);
	int i = 0;
	uint8_t * status_ptr = NULL;
	uint8_t status = NIC_STATUS_OK;
	uint32_t req_len = 0;

	PrintDebug("Descriptor Count=%d, index=%d\n", desc_cnt, q->cur_avail_idx % QUEUE_SIZE);

	hdr_desc = &(q->desc[desc_idx]);

	PrintDebug("Header Descriptor (ptr=%p) gpa=%p, len=%d, flags=%x, next=%d\n", hdr_desc, 
		   (void *)(hdr_desc->addr_gpa), hdr_desc->length, hdr_desc->flags, hdr_desc->next);	

	if (guest_pa_to_host_va(dev->vm, hdr_desc->addr_gpa, &(hdr_addr)) == -1) {
	    PrintError("Could not translate block header address\n");
	    return -1;
	}

	// We copy the block op header out because we are going to modify its contents
	memcpy(&hdr, (void *)hdr_addr, sizeof(struct virtio_net_hdr));
	
	PrintDebug("NIC Op Hdr (ptr=%p) type=%d, sector=%p\n", (void *)hdr_addr, hdr.hdr_len, (void *)hdr.csum_start);

	desc_idx = hdr_desc->next;

	for (i = 0; i < desc_cnt - 2; i++) {
	    uint8_t tmp_status = NIC_STATUS_OK;

	    buf_desc = &(q->desc[desc_idx]);

	    PrintDebug("Buffer Descriptor (ptr=%p) gpa=%p, len=%d, flags=%x, next=%d\n", buf_desc, 
		       (void *)(buf_desc->addr_gpa), buf_desc->length, buf_desc->flags, buf_desc->next);

	    if (handle_pkt_write(dev, &hdr, buf_desc, &tmp_status) == -1) {
		PrintError("Error handling nic operation\n");
		return -1;
	    }

	    if (tmp_status != NIC_STATUS_OK) {
		status = tmp_status;
	    }

	    req_len += buf_desc->length;
	    desc_idx = buf_desc->next;
	}

	status_desc = &(q->desc[desc_idx]);

	PrintDebug("Status Descriptor (ptr=%p) gpa=%p, len=%d, flags=%x, next=%d\n", status_desc, 
		   (void *)(status_desc->addr_gpa), status_desc->length, status_desc->flags, status_desc->next);

	if (guest_pa_to_host_va(dev->vm, status_desc->addr_gpa, (addr_t *)&(status_ptr)) == -1) {
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
	PrintDebug("Raising IRQ %d\n",  virtio->pci_dev->config_header.intr_line);
	v3_pci_raise_irq(virtio->pci_bus, 0, virtio->pci_dev);
	virtio->virtio_cfg.pci_isr = 0x1;
    }

    return 0;
}


static int virtio_setup_queue(struct vm_device * dev, struct virtio_queue *queue, addr_t pfn, addr_t page_addr)
{
    queue->pfn = pfn;
		
    queue->ring_desc_addr = page_addr ;
    queue->ring_avail_addr = page_addr + (QUEUE_SIZE * sizeof(struct vring_desc));
    queue->ring_used_addr = (queue->ring_avail_addr + \
						 sizeof(struct vring_avail)    + \
						 (QUEUE_SIZE * sizeof(uint16_t)));
		
    // round up to next page boundary.
    queue->ring_used_addr = (queue->ring_used_addr + 0xfff) & ~0xfff;

    if (guest_pa_to_host_va(dev->vm, queue->ring_desc_addr, (addr_t *)&(queue->desc)) == -1) {
        PrintError("Could not translate ring descriptor address\n");
	 return -1;
    }

 
    if (guest_pa_to_host_va(dev->vm, queue->ring_avail_addr, (addr_t *)&(queue->avail)) == -1) {
        PrintError("Could not translate ring available address\n");
        return -1;
    }


    if (guest_pa_to_host_va(dev->vm, queue->ring_used_addr, (addr_t *)&(queue->used)) == -1) {
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



static int virtio_io_write(uint16_t port, void * src, uint_t length, void * private_data) 
{
    struct vm_device * dev = (struct vm_device *)private_data;
    struct virtio_net_state * virtio = (struct virtio_net_state *)dev->private_data;
    int port_idx = port % virtio->io_range_size;


    PrintDebug("VIRTIO NIC Write for port %d (index=%d) len=%d, value=%x\n", 
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
	    if (length == 4) {
		addr_t pfn = *(uint32_t *)src;
		addr_t page_addr = (pfn << VIRTIO_PAGE_SHIFT);

		uint16_t queue_idx = virtio->virtio_cfg.vring_queue_selector;
		switch (queue_idx) {
		    case 0:
		        virtio_setup_queue(dev, &virtio->rx_vq, pfn, page_addr);
		        break;
                  case 1:
 		        virtio_setup_queue(dev, &virtio->tx_vq, pfn, page_addr);
			 break;
		    case 2:
			 virtio_setup_queue(dev, &virtio->ctrl_vq, pfn, page_addr);
			 break;

		    default:
			 break;
		}
	    } else {
		PrintError("Illegal write length for page frame number\n");
		return -1;
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
	    PrintDebug("Handling Kick\n");
	    uint16_t queue_idx = *(uint16_t *)src;
	    if (queue_idx == 0){
		    if (handle_pkt_rx(dev) == -1) {
			PrintError("Could not handle NIC Notification\n");
			return -1;
		    }
	    }else if (queue_idx == 1){
		    if (handle_pkt_tx(dev) == -1) {
			PrintError("Could not handle NIC Notification\n");
			return -1;
		    }
	    }else if (queue_idx == 2){
		    if (handle_ctrl(dev) == -1) {
			PrintError("Could not handle NIC Notification\n");
			return -1;
		    }
	    }else {
	        PrintError("Virtio NIC device only uses 3 queue, selected %d\n", 
			   queue_idx);
	    }
	    
	    break;
	case VIRTIO_STATUS_PORT:
	    virtio->virtio_cfg.status = *(uint8_t *)src;

	    if (virtio->virtio_cfg.status == 0) {
		PrintDebug("Resetting device\n");
		virtio_reset(dev);
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


static int virtio_io_read(uint16_t port, void * dst, uint_t length, void * private_data) 
{
    struct vm_device * dev = (struct vm_device *)private_data;
    struct virtio_net_state * virtio = (struct virtio_net_state *)dev->private_data;
    int port_idx = port % virtio->io_range_size;
    uint16_t queue_idx = virtio->virtio_cfg.vring_queue_selector;


    PrintDebug("VIRTIO NIC Read  for port %d (index =%d), length=%d\n", 
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
	              *(uint32_t *)dst = virtio->tx_vq.queue_size;
			break;	
		 case 2:
	              *(uint32_t *)dst = virtio->ctrl_vq.queue_size;
			break;
		 default:
		 	break;
           }

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
	    v3_pci_lower_irq(virtio->pci_bus, 0, virtio->pci_dev);
	    break;

	default:
	    PrintError("Read of Unhandled Virtio Read\n");
           return -1;
    }

    return length;
}


static struct v3_device_ops dev_ops = {
    .free = virtio_free,
    .reset = virtio_reset,
    .start = NULL,
    .stop = NULL,
};


int v3_virtio_register_nic(struct vm_device *dev, struct v3_net_ops *ops, void *private_data) {
    struct virtio_net_state * virtio = (struct virtio_net_state *)dev->private_data;
    
    virtio->net_ops = ops;

    return 0;
}


static int virtio_init(struct guest_info * vm, void *cfg_data) {
    struct vm_device * pci_bus = v3_find_dev(vm, (char *)cfg_data);
    struct virtio_net_state * virtio_state = NULL;
    struct pci_device * pci_dev = NULL;

    PrintDebug("Initializing VIRTIO Network device\n");

    if (pci_bus == NULL) {
	PrintError("VirtIO network devices require a PCI Bus");
	return -1;
    }
    
    virtio_state  = (struct virtio_net_state *)V3_Malloc(sizeof(struct virtio_net_state));
    memset(virtio_state, 0, sizeof(struct virtio_net_state));

    struct vm_device * dev = v3_allocate_device("LNX_VIRTIO_NIC", &dev_ops, virtio_state);
    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", "LNX_VIRTIO_NIC");
	return -1;
    }


    // PCI initialization
    {
	struct v3_pci_bar bars[6];
	int num_ports = sizeof(struct virtio_config);
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

	PrintDebug("Virtio-NIC io_range_size = %d\n", virtio_state->io_range_size);

	bars[0].type = PCI_BAR_IO;
	bars[0].default_base_port = -1;
	bars[0].num_ports = virtio_state->io_range_size;

	bars[0].io_read = virtio_io_read;
	bars[0].io_write = virtio_io_write;
	bars[0].private_data = dev;

	pci_dev = v3_pci_register_device(pci_bus, PCI_STD_DEVICE, 
					 0, PCI_AUTO_DEV_NUM, 0,
					 "LNX_VIRTIO_NIC", bars,
					 NULL, NULL, NULL, dev, NULL);

	if (!pci_dev) {
	    PrintError("Could not register PCI Device\n");
	    return -1;
	}
	
	pci_dev->config_header.vendor_id = VIRTIO_VENDOR_ID;
	pci_dev->config_header.subsystem_vendor_id = VIRTIO_SUBVENDOR_ID;
	

	pci_dev->config_header.device_id = VIRTIO_NET_DEV_ID;
	pci_dev->config_header.class = PCI_CLASS_NETWORK;
	pci_dev->config_header.subclass = PCI_NET_SUBCLASS_OTHER;

	// TODO:how to define new one for virtio net device
	pci_dev->config_header.subsystem_id = VIRTIO_BLOCK_SUBDEVICE_ID;


	pci_dev->config_header.intr_pin = 1;

	pci_dev->config_header.max_latency = 1; // ?? (qemu does it...)


	virtio_state->pci_dev = pci_dev;
	virtio_state->pci_bus = pci_bus;
    }

    virtio_state->virtio_cfg.host_features = 0; //no features support now

    virtio_state->rx_vq.queue_size = QUEUE_SIZE;
    virtio_state->tx_vq.queue_size = QUEUE_SIZE;
    virtio_state->ctrl_vq.queue_size = CTRL_QUEUE_SIZE;
   

    virtio_reset(dev);

// TODO: net ops
    virtio_state->net_ops = NULL;

    return 0;
}


device_register("LNX_VIRTIO_NIC", virtio_init)

