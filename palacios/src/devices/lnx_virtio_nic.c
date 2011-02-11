/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2010, Cui Zheng <cuizheng@cs.unm.edu>
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu>
 *         Cui Zheng <cuizheng@cs.unm.edu>
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
#include <palacios/vmm_util.h>
#include <devices/pci.h>
#include <palacios/vmm_ethernet.h>


#ifndef CONFIG_DEBUG_VIRTIO_NET
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define VIRTIO_NET_S_LINK_UP	1	/* Link is up */
#define VIRTIO_NET_MAX_BUFSIZE (sizeof(struct virtio_net_hdr) + (64 << 10))


struct virtio_net_hdr {
	uint8_t flags;

#define VIRTIO_NET_HDR_GSO_NONE		0	/* Not a GSO frame */
	uint8_t gso_type;
	uint16_t hdr_len;		/* Ethernet + IP + tcp/udp hdrs */
	uint16_t gso_size;		/* Bytes to append to hdr_len per frame */
	uint16_t csum_start;		/* Position to start checksumming from */
	uint16_t csum_offset;		/* Offset after that to place checksum */
}__attribute__((packed));


/* This is the version of the header to use when the MRG_RXBUF
 * feature has been negotiated. */
struct virtio_net_hdr_mrg_rxbuf {
	struct virtio_net_hdr hdr;
	uint16_t num_buffers;	/* Number of merged rx buffers */
};

	
#define TX_QUEUE_SIZE 64
#define RX_QUEUE_SIZE 1024
#define CTRL_QUEUE_SIZE 64

#define VIRTIO_NET_F_MRG_RXBUF	15	/* Host can merge receive buffers. */
#define VIRTIO_NET_F_MAC	5	/* Host has given MAC address. */
#define VIRTIO_NET_F_GSO	6	/* Host handles pkts w/ any GSO type */
#define VIRTIO_NET_F_HOST_TSO4	11	/* Host can handle TSOv4 in. */

/* this is not how virtio supposed to be,
 * we may need a separately implemented virtio_pci
 * In order to make guest to get virtio MAC from host
 * I added it here  -- Lei
 */
 #define VIRTIO_NET_CONFIG 20  


struct virtio_net_config
{
    uint8_t mac[ETH_ALEN]; 	/* VIRTIO_NET_F_MAC */
    uint16_t status;
} __attribute__((packed));

struct virtio_dev_state {
    struct vm_device * pci_bus;
    struct list_head dev_list;
    struct v3_vm_info *vm;

    uint8_t mac[ETH_ALEN];
};

struct virtio_net_state {
    struct virtio_net_config net_cfg;
    struct virtio_config virtio_cfg;

    struct vm_device * dev;
    struct pci_device * pci_dev; 
    int io_range_size;
    
    struct virtio_queue rx_vq;   	/* idx 0, pkts to guest */
    struct virtio_queue tx_vq;   	/* idx 1, pkts from guest */
    struct virtio_queue ctrl_vq;  	/* idx 2 */

    int buffed_rx;
    int tx_disabled; 			/* stop TX pkts from guest */

    struct nic_statistics statistics;

    struct v3_dev_net_ops * net_ops;
    v3_lock_t rx_lock, tx_lock;

    void * backend_data;
    struct virtio_dev_state * virtio_dev;
    struct list_head dev_link;
};

/* virtio nic error type */
#define ERR_VIRTIO_OTHER  1
#define ERR_VIRTIO_RXQ_FULL  2
#define ERR_VIRTIO_RXQ_NOSET  3
#define ERR_VIRTIO_TXQ_NOSET 4
#define ERR_VIRTIO_TXQ_FULL 5
#define ERR_VIRTIO_TXQ_DISABLED 6




static int virtio_init_state(struct virtio_net_state * virtio) 
{
    virtio->rx_vq.queue_size = RX_QUEUE_SIZE;
    virtio->tx_vq.queue_size = TX_QUEUE_SIZE;
    virtio->ctrl_vq.queue_size = CTRL_QUEUE_SIZE;

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

    virtio->virtio_cfg.pci_isr = 0;
	
    virtio->virtio_cfg.host_features = 0 | (1 << VIRTIO_NET_F_MAC);

    if ((v3_lock_init(&(virtio->rx_lock)) == -1) ||
	(v3_lock_init(&(virtio->tx_lock)) == -1)){
        PrintError("Virtio NIC: Failure to init locks for net_state\n");
    }

    virtio->buffed_rx = 0;

    return 0;
}

static int 
pkt_tx(struct guest_info * core, 
       struct virtio_net_state * virtio, 
       struct vring_desc * buf_desc) 
{
    uint8_t * buf = NULL;
    uint32_t len = buf_desc->length;

    if (v3_gpa_to_hva(core, buf_desc->addr_gpa, (addr_t *)&(buf)) == -1) {
	PrintError("Could not translate buffer address\n");
	return -ERR_VIRTIO_OTHER;
    }

    if(virtio->net_ops->send(buf, len, virtio->backend_data) >= 0){
	virtio->statistics.tx_pkts ++;
	virtio->statistics.tx_bytes += len;

	return 0;
    }

    virtio->statistics.tx_dropped ++;

    return -1;
}


static int 
copy_data_to_desc(struct guest_info * core, 
		  struct virtio_net_state * virtio_state, 
		  struct vring_desc * desc, 
		  uchar_t * buf, 
		  uint_t buf_len,
		  uint_t offset)
{
    uint32_t len;
    uint8_t * desc_buf = NULL;

    if (v3_gpa_to_hva(core, desc->addr_gpa, (addr_t *)&(desc_buf)) == -1) {
	PrintError("Could not translate buffer address\n");
	return -1;
    }
    len = (desc->length < buf_len)?(desc->length - offset):buf_len;
    memcpy(desc_buf+offset, buf, len);

    return len;
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

static inline void enable_cb(struct virtio_queue *queue){
    queue->used->flags &= ~ VRING_NO_NOTIFY_FLAG;
}

static inline void disable_cb(struct virtio_queue *queue) {
    queue->used->flags |= VRING_NO_NOTIFY_FLAG;
}


/* interrupt the guest, so the guest core get EXIT to Palacios
 * this happens when there are either incoming pkts for the guest
 * or the guest can start TX pkts again */
static inline void notify_guest(struct virtio_net_state * virtio){
    v3_interrupt_cpu(virtio->virtio_dev->vm, virtio->virtio_dev->vm->cores[0].cpu_id, 0);
}


/* guest free some pkts from rx queue */
static int handle_rx_kick(struct guest_info *core, 
			  struct virtio_net_state * virtio) 
{
    unsigned long flags;

    flags = v3_lock_irqsave(virtio->rx_lock);

    if(virtio->net_ops->start_rx != NULL){
	virtio->net_ops->start_rx(virtio->backend_data);
    }
    //disable_cb(&virtio->rx_vq);

    v3_unlock_irqrestore(virtio->rx_lock, flags);
	
    return 0;
}


static int handle_ctrl(struct guest_info *core, 
		       struct virtio_net_state * virtio) {
	
    return 0;
}

static int handle_pkt_tx(struct guest_info *core, 
			 struct virtio_net_state * virtio_state) 
{
    struct virtio_queue * q = &(virtio_state->tx_vq);
    struct virtio_net_hdr * hdr = NULL;
    int recved = 0;
    unsigned long flags;

    if (!q->ring_avail_addr) 
	return -ERR_VIRTIO_TXQ_NOSET;

    if(virtio_state->tx_disabled)
	return -ERR_VIRTIO_TXQ_DISABLED;

    flags = v3_lock_irqsave(virtio_state->tx_lock);
    while (q->cur_avail_idx != q->avail->index) {
	struct vring_desc * hdr_desc = NULL;
	addr_t hdr_addr = 0;
	uint16_t desc_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];
	int desc_cnt = get_desc_count(q, desc_idx);
	uint32_t req_len = 0;
	int i = 0;

	hdr_desc = &(q->desc[desc_idx]);
	if (v3_gpa_to_hva(core, hdr_desc->addr_gpa, &(hdr_addr)) == -1) {
	    PrintError("Could not translate block header address\n");
	    goto exit_error;
	}

	hdr = (struct virtio_net_hdr*)hdr_addr;
	desc_idx = hdr_desc->next;

	if(desc_cnt > 2){
	    PrintError("VNIC: merged rx buffer not supported\n");
	    goto exit_error;
	}

	/* here we assumed that one ethernet pkt is not splitted into multiple virtio buffer */
	for (i = 0; i < desc_cnt - 1; i++) {	
	    struct vring_desc * buf_desc = &(q->desc[desc_idx]);
	    if (pkt_tx(core, virtio_state, buf_desc) == -1) {
		PrintError("Error handling nic operation\n");
		goto exit_error;
	    }

	    req_len += buf_desc->length;
	    desc_idx = buf_desc->next;
	}

	q->used->ring[q->used->index % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];
	q->used->ring[q->used->index % q->queue_size].length = req_len; // What do we set this to????
	q->used->index ++;
	
	q->cur_avail_idx ++;
    }

    v3_unlock_irqrestore(virtio_state->tx_lock, flags);

    if(!recved)
	return 0;
	
    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	v3_pci_raise_irq(virtio_state->virtio_dev->pci_bus, 0, virtio_state->pci_dev);
	virtio_state->virtio_cfg.pci_isr = 0x1;

	virtio_state->statistics.interrupts ++;
    }

    return 0;

exit_error:
	
    v3_unlock_irqrestore(virtio_state->tx_lock, flags);
    return -ERR_VIRTIO_OTHER;
}


static int virtio_setup_queue(struct guest_info *core, 
			      struct virtio_net_state * virtio_state, 
			      struct virtio_queue * queue, 
			      addr_t pfn, addr_t page_addr) {
    queue->pfn = pfn;
		
    queue->ring_desc_addr = page_addr;
    queue->ring_avail_addr = page_addr + (queue->queue_size * sizeof(struct vring_desc));
    queue->ring_used_addr = ((queue->ring_avail_addr) + 
			     (sizeof(struct vring_avail)) + 
			     (queue->queue_size * sizeof(uint16_t)));

    // round up to next page boundary.
    queue->ring_used_addr = (queue->ring_used_addr + 0xfff) & ~0xfff;
    if (v3_gpa_to_hva(core, queue->ring_desc_addr, (addr_t *)&(queue->desc)) == -1) {
        PrintError("Could not translate ring descriptor address\n");
	 return -1;
    }
 
    if (v3_gpa_to_hva(core, queue->ring_avail_addr, (addr_t *)&(queue->avail)) == -1) {
        PrintError("Could not translate ring available address\n");
        return -1;
    }

    if (v3_gpa_to_hva(core, queue->ring_used_addr, (addr_t *)&(queue->used)) == -1) {
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

static int virtio_io_write(struct guest_info *core, 
			   uint16_t port, void * src, 
			   uint_t length, void * private_data) 
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
		    virtio_setup_queue(core, virtio, &virtio->rx_vq, pfn, page_addr);
		    //disable_cb(&virtio->rx_vq);
		    break;
		case 1:
		    virtio_setup_queue(core, virtio, &virtio->tx_vq, pfn, page_addr);
		    //disable_cb(&virtio->tx_vq);
		    break;
		case 2:
		    virtio_setup_queue(core, virtio, &virtio->ctrl_vq, pfn, page_addr);
		    break;	    
		default:
		    break;
	    }
	    break;
		
	case VRING_Q_SEL_PORT:
	    virtio->virtio_cfg.vring_queue_selector = *(uint16_t *)src;
	    if (virtio->virtio_cfg.vring_queue_selector > 2) {
		PrintError("Virtio NIC: wrong queue idx: %d\n", 
			   virtio->virtio_cfg.vring_queue_selector);
		return -1;
	    }
	    break;
		
	case VRING_Q_NOTIFY_PORT: 
	    {
		uint16_t queue_idx = *(uint16_t *)src;	   		
		if (queue_idx == 0){
		    handle_rx_kick(core, virtio);
		} else if (queue_idx == 1){
		    if (handle_pkt_tx(core, virtio) == -1) {
			PrintError("Could not handle NIC Notification\n");
			return -1;
		    }
		} else if (queue_idx == 2){
		    if (handle_ctrl(core, virtio) == -1) {
			PrintError("Could not handle NIC Notification\n");
			return -1;
		    }
		} else {
		    PrintError("Wrong queue index %d\n", queue_idx);
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

static int virtio_io_read(struct guest_info *core, 
			  uint16_t port, void * dst, 
			  uint_t length, void * private_data) 
{
    struct virtio_net_state * virtio = (struct virtio_net_state *)private_data;
    int port_idx = port % virtio->io_range_size;
    uint16_t queue_idx = virtio->virtio_cfg.vring_queue_selector;

    PrintDebug("Virtio NIC %p: Read  for port 0x%x (index =%d), length=%d\n", private_data,
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

	case VIRTIO_NET_CONFIG ... VIRTIO_NET_CONFIG + ETH_ALEN:
	    *(uint8_t *)dst = virtio->net_cfg.mac[port_idx-VIRTIO_NET_CONFIG];
	    break;

	default:
	    PrintError("Virtio NIC: Read of Unhandled Virtio Read:%d\n", port_idx);
	    return -1;
    }

    return length;
}


static int virtio_rx(uint8_t * buf, uint32_t size, void * private_data) {
    struct virtio_net_state * virtio = (struct virtio_net_state *)private_data;
    struct virtio_queue * q = &(virtio->rx_vq);
    struct virtio_net_hdr_mrg_rxbuf hdr;
    uint32_t hdr_len = sizeof(struct virtio_net_hdr_mrg_rxbuf);
    uint32_t data_len = size;
    uint32_t offset = 0;
    unsigned long flags;
    int ret_val = -ERR_VIRTIO_OTHER;
    int raw = 1;

#ifndef CONFIG_DEBUG_VIRTIO_NET
   {
    	PrintDebug("Virtio-NIC: virtio_rx: size: %d\n", size);	
    	//v3_hexdump(buf, size, NULL, 0);
   }
#endif

    flags = v3_lock_irqsave(virtio->rx_lock);

    if (!raw)
    	data_len -= hdr_len;

    if (!raw)
        memcpy(&hdr, buf, sizeof(struct virtio_net_hdr_mrg_rxbuf));
    else
        memset(&hdr, 0, sizeof(struct virtio_net_hdr_mrg_rxbuf));

    if (q->ring_avail_addr == 0) {
	PrintError("Queue is not set\n");
	ret_val = -ERR_VIRTIO_RXQ_NOSET;
	goto exit;
    }

    if (q->cur_avail_idx != q->avail->index){
	addr_t hdr_addr = 0;
	uint16_t hdr_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];
	uint16_t buf_idx = 0;
	struct vring_desc * hdr_desc = NULL;

	hdr_desc = &(q->desc[hdr_idx]);
	if (v3_gpa_to_hva(&(virtio->virtio_dev->vm->cores[0]), hdr_desc->addr_gpa, &(hdr_addr)) == -1) {
	    PrintError("Could not translate receive buffer address\n");
	    goto exit;
	}
	hdr.num_buffers = 1;
	memcpy((void *)hdr_addr, &hdr, sizeof(struct virtio_net_hdr_mrg_rxbuf));
	if (offset >= data_len) {
	    hdr_desc->flags &= ~VIRTIO_NEXT_FLAG;
	}

	struct vring_desc * buf_desc = NULL;
	for (buf_idx = hdr_desc->next; offset < data_len; buf_idx = q->desc[hdr_idx].next) {
	    uint32_t len = 0;
	    buf_desc = &(q->desc[buf_idx]);

	    len = copy_data_to_desc(&(virtio->virtio_dev->vm->cores[0]), virtio, buf_desc, buf + offset, data_len - offset, 0);	    
	    offset += len;
	    if (offset < data_len) {
		buf_desc->flags = VIRTIO_NEXT_FLAG;		
	    }
	    buf_desc->length = len;
	}
	buf_desc->flags &= ~VIRTIO_NEXT_FLAG;
	
	q->used->ring[q->used->index % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];
	q->used->ring[q->used->index % q->queue_size].length = data_len + hdr_len; /* This should be the total length of data sent to guest (header+pkt_data) */
	q->used->index++;
	q->cur_avail_idx++;

	/* if there are certain num of pkts in the RX queue, notify guest 
	  * so guest will exit to palacios
         * when it returns, guest gets the virtio rx interrupt */
	if((++virtio->buffed_rx > q->queue_size/5) &&
	    (q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	    if(virtio->virtio_dev->vm->cores[0].cpu_id != V3_Get_CPU()){
		  notify_guest(virtio);
	    }
	    virtio->buffed_rx = 0;
	}

 	virtio->statistics.rx_pkts ++;
	virtio->statistics.rx_bytes += size;
    } else {
	virtio->statistics.rx_dropped ++;
	
	/* RX queue is full,  tell backend to stop RX on this device */
	virtio->net_ops->stop_rx(virtio->backend_data);
	enable_cb(&virtio->rx_vq);
	
	ret_val = -ERR_VIRTIO_RXQ_FULL;
	goto exit;
    }

    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	PrintDebug("Raising IRQ %d\n",  virtio->pci_dev->config_header.intr_line);
	v3_pci_raise_irq(virtio->virtio_dev->pci_bus, 0, virtio->pci_dev);
	virtio->virtio_cfg.pci_isr = 0x1;

	virtio->statistics.interrupts ++;
    }

    ret_val = offset;

exit:

    v3_unlock_irqrestore(virtio->rx_lock, flags);
 
    return ret_val;
}

static int virtio_free(struct virtio_dev_state * virtio) {
    struct virtio_net_state * backend = NULL;
    struct virtio_net_state * tmp = NULL;


    list_for_each_entry_safe(backend, tmp, &(virtio->dev_list), dev_link) {

	// unregister from PCI

	list_del(&(backend->dev_link));
	V3_Free(backend);
    }

    V3_Free(virtio);
    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))virtio_free,
};


/* TODO: Issue here: which vm info it needs? calling VM or the device's own VM? */
static void virtio_nic_poll(struct v3_vm_info * vm, void * data){
    struct virtio_net_state * virtio = (struct virtio_net_state *)data;
	
    handle_pkt_tx(&(vm->cores[0]), virtio);
}

static void virtio_start_tx(void * data){
    struct virtio_net_state * virtio = (struct virtio_net_state *)data;
    unsigned long flags;

    flags = v3_lock_irqsave(virtio->tx_lock);
    virtio->tx_disabled = 0;

    /* notify the device's guest to start sending pkt */
    if(virtio->virtio_dev->vm->cores[0].cpu_id != V3_Get_CPU()){
	notify_guest(virtio);
    }
    v3_unlock_irqrestore(virtio->tx_lock, flags);	
}

static void virtio_stop_tx(void * data){
    struct virtio_net_state * virtio = (struct virtio_net_state *)data;
    unsigned long flags;

    flags = v3_lock_irqsave(virtio->tx_lock);
    virtio->tx_disabled = 1;

    /* stop the guest to exit to palacios for sending pkt? */
    if(virtio->virtio_dev->vm->cores[0].cpu_id != V3_Get_CPU()){
       disable_cb(&virtio->tx_vq);
    }

    v3_unlock_irqrestore(virtio->tx_lock, flags);
}

	


static int register_dev(struct virtio_dev_state * virtio, 
			struct virtio_net_state * net_state) 
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
				     0, 4/*PCI_AUTO_DEV_NUM*/, 0,
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
    net_state->virtio_dev = virtio;

    memcpy(net_state->net_cfg.mac, virtio->mac, 6);                           
	
    virtio_init_state(net_state);

    /* Add backend to list of devices */
    list_add(&(net_state->dev_link), &(virtio->dev_list));

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
    net_state->virtio_dev = virtio;
	

    ops->recv = virtio_rx;
    ops->poll = virtio_nic_poll;
    ops->start_tx = virtio_start_tx;
    ops->stop_tx = virtio_stop_tx;
    ops->frontend_data = net_state;
    memcpy(ops->fnt_mac, virtio->mac, ETH_ALEN);

    return 0;
}

static int virtio_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vm_device * pci_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    struct virtio_dev_state * virtio_state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    char * macstr = v3_cfg_val(cfg, "mac");

    if (pci_bus == NULL) {
	PrintError("Virtio NIC: VirtIO devices require a PCI Bus");
	return -1;
    }

    virtio_state  = (struct virtio_dev_state *)V3_Malloc(sizeof(struct virtio_dev_state));
    memset(virtio_state, 0, sizeof(struct virtio_dev_state));

    INIT_LIST_HEAD(&(virtio_state->dev_list));
    virtio_state->pci_bus = pci_bus;
    virtio_state->vm = vm;

    if (macstr != NULL && !str2mac(macstr, virtio_state->mac)) {
	PrintDebug("Virtio NIC: Mac specified %s\n", macstr);
    }else {
    	PrintDebug("Virtio NIC: MAC not specified\n");
	random_ethaddr(virtio_state->mac);
    }

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, virtio_state);

    if (dev == NULL) {
	PrintError("Virtio NIC: Could not attach device %s\n", dev_id);
	V3_Free(virtio_state);
	return -1;
    }

    if (v3_dev_add_net_frontend(vm, dev_id, connect_fn, (void *)virtio_state) == -1) {
	PrintError("Virtio NIC: Could not register %s as net frontend\n", dev_id);
	v3_remove_device(dev);
	return -1;
    }
	
    return 0;
}

device_register("LNX_VIRTIO_NIC", virtio_init)
	
