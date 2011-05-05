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
#include <vnet/vnet.h>
#include <palacios/vmm_lock.h>
#include <palacios/vmm_util.h>
#include <devices/pci.h>
#include <palacios/vmm_ethernet.h>
#include <palacios/vmm_time.h>


#ifndef V3_CONFIG_DEBUG_VIRTIO_NET
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define TX_QUEUE_SIZE 4096
#define RX_QUEUE_SIZE 4096
#define CTRL_QUEUE_SIZE 64

/* The feature bitmap for virtio nic
  * from Linux */
#define VIRTIO_NET_F_CSUM       0       /* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM 1       /* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_MAC        5       /* Host has given MAC address. */
#define VIRTIO_NET_F_GSO        6       /* Host handles pkts w/ any GSO type */
#define VIRTIO_NET_F_GUEST_TSO4 7       /* Guest can handle TSOv4 in. */
#define VIRTIO_NET_F_GUEST_TSO6 8       /* Guest can handle TSOv6 in. */
#define VIRTIO_NET_F_GUEST_ECN  9       /* Guest can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_GUEST_UFO  10      /* Guest can handle UFO in. */
#define VIRTIO_NET_F_HOST_TSO4  11      /* Host can handle TSOv4 in. */
#define VIRTIO_NET_F_HOST_TSO6  12      /* Host can handle TSOv6 in. */
#define VIRTIO_NET_F_HOST_ECN   13      /* Host can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_HOST_UFO   14      /* Host can handle UFO in. */
#define VIRTIO_NET_F_MRG_RXBUF  15      /* Host can merge receive buffers. */
#define VIRTIO_NET_F_STATUS     16      /* virtio_net_config.status available */

/* Port to get virtio config */
#define VIRTIO_NET_CONFIG 20  

#define VIRTIO_NET_MAX_BUFSIZE (sizeof(struct virtio_net_hdr) + (64 << 10))

/* for gso_type in virtio_net_hdr */
#define VIRTIO_NET_HDR_GSO_NONE         0      
#define VIRTIO_NET_HDR_GSO_TCPV4        1     /* GSO frame, IPv4 TCP (TSO) */
#define VIRTIO_NET_HDR_GSO_UDP          3       /* GSO frame, IPv4 UDP (UFO) */
#define VIRTIO_NET_HDR_GSO_TCPV6        4       /* GSO frame, IPv6 TCP */
#define VIRTIO_NET_HDR_GSO_ECN          0x80    /* TCP has ECN set */	


/* for flags in virtio_net_hdr */
#define VIRTIO_NET_HDR_F_NEEDS_CSUM     1       /* Use csum_start, csum_offset */


/* First element of the scatter-gather list, used with GSO or CSUM features */
struct virtio_net_hdr
{
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;		/* Ethernet + IP + tcp/udp hdrs */
    uint16_t gso_size;		/* Bytes to append to hdr_len per frame */
    uint16_t csum_start;	/* Position to start checksumming from */
    uint16_t csum_offset;	/* Offset after that to place checksum */
}__attribute__((packed));


/* The header to use when the MRG_RXBUF 
 * feature has been negotiated. */
struct virtio_net_hdr_mrg_rxbuf {
    struct virtio_net_hdr hdr;
    uint16_t num_buffers;	/* Number of merged rx buffers */
};

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

    struct v3_vm_info * vm;
    struct vm_device * dev;
    struct pci_device * pci_dev; 
    int io_range_size;

    uint16_t status;
    
    struct virtio_queue rx_vq;   	/* idx 0*/
    struct virtio_queue tx_vq;   	/* idx 1*/
    struct virtio_queue ctrl_vq;  	/* idx 2*/

    uint8_t mergeable_rx_bufs;

    struct v3_timer * timer;
    struct vnet_thread * poll_thread;

    struct nic_statistics stats;

    struct v3_dev_net_ops * net_ops;
    v3_lock_t rx_lock, tx_lock;

    uint8_t tx_notify, rx_notify;
    uint32_t tx_pkts, rx_pkts;
    uint64_t past_us;

    void * backend_data;
    struct virtio_dev_state * virtio_dev;
    struct list_head dev_link;
};


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

    virtio->mergeable_rx_bufs = 0;
	
    virtio->virtio_cfg.host_features = 0 | (1 << VIRTIO_NET_F_MAC);
    if(virtio->mergeable_rx_bufs) {
	virtio->virtio_cfg.host_features |= (1 << VIRTIO_NET_F_MRG_RXBUF);
    }

    if ((v3_lock_init(&(virtio->rx_lock)) == -1) ||
	(v3_lock_init(&(virtio->tx_lock)) == -1)){
        PrintError("Virtio NIC: Failure to init locks for net_state\n");
    }

    return 0;
}

static int tx_one_pkt(struct guest_info * core, 
		      struct virtio_net_state * virtio, 
		      struct vring_desc * buf_desc) 
{
    uint8_t * buf = NULL;
    uint32_t len = buf_desc->length;
    int synchronize = virtio->tx_notify;

    if (v3_gpa_to_hva(core, buf_desc->addr_gpa, (addr_t *)&(buf)) == -1) {
	PrintDebug("Could not translate buffer address\n");
	return -1;
    }

    V3_Net_Print(2, "Virtio-NIC: virtio_tx: size: %d\n", len);
    if(v3_net_debug >= 4){
	v3_hexdump(buf, len, NULL, 0);
    }

    if(virtio->net_ops->send(buf, len, synchronize, virtio->backend_data) < 0){
	virtio->stats.tx_dropped ++;
	return -1;
    }

    virtio->stats.tx_pkts ++;
    virtio->stats.tx_bytes += len;

    return 0;
}


static inline int copy_data_to_desc(struct guest_info * core, 
		  struct virtio_net_state * virtio_state, 
		  struct vring_desc * desc, 
		  uchar_t * buf, 
		  uint_t buf_len,
		  uint_t dst_offset){
    uint32_t len;
    uint8_t * desc_buf = NULL;

    if (v3_gpa_to_hva(core, desc->addr_gpa, (addr_t *)&(desc_buf)) == -1) {
	PrintDebug("Could not translate buffer address\n");
	return -1;
    }
    len = (desc->length < buf_len)?(desc->length - dst_offset):buf_len;
    memcpy(desc_buf+dst_offset, buf, len);

    return len;
}


static inline int get_desc_count(struct virtio_queue * q, int index) {
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

static int handle_pkt_tx(struct guest_info * core, 
			 struct virtio_net_state * virtio_state) 
{
    struct virtio_queue *q = &(virtio_state->tx_vq);
    int txed = 0;
    unsigned long flags; 

    if (!q->ring_avail_addr) {
	return -1;
    }

    flags = v3_lock_irqsave(virtio_state->tx_lock);
    while (q->cur_avail_idx != q->avail->index) {
	struct virtio_net_hdr_mrg_rxbuf * hdr = NULL;
	struct vring_desc * hdr_desc = NULL;
	addr_t hdr_addr = 0;
	uint16_t desc_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];
	int desc_cnt = get_desc_count(q, desc_idx);

	if(desc_cnt > 2){
	    PrintError("VNIC: merged rx buffer not supported, desc_cnt %d\n", desc_cnt);
	    goto exit_error;
	}

	hdr_desc = &(q->desc[desc_idx]);
	if (v3_gpa_to_hva(core, hdr_desc->addr_gpa, &(hdr_addr)) == -1) {
	    PrintError("Could not translate block header address\n");
	    goto exit_error;
	}

	hdr = (struct virtio_net_hdr_mrg_rxbuf *)hdr_addr;
	desc_idx = hdr_desc->next;

	V3_Net_Print(2, "Virtio NIC: TX hdr count : %d\n", hdr->num_buffers);

	/* here we assumed that one ethernet pkt is not splitted into multiple buffer */	
	struct vring_desc * buf_desc = &(q->desc[desc_idx]);
	if (tx_one_pkt(core, virtio_state, buf_desc) == -1) {
	    PrintError("Virtio NIC: Error handling nic operation\n");
	    goto exit_error;
	}
	if(buf_desc->next & VIRTIO_NEXT_FLAG){
	    V3_Net_Print(2, "Virtio NIC: TX more buffer need to read\n");
	}
	    
	q->used->ring[q->used->index % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];
	q->used->ring[q->used->index % q->queue_size].length = buf_desc->length; /* What do we set this to???? */
	q->used->index ++;
	
	q->cur_avail_idx ++;

	txed ++;
    }

    v3_unlock_irqrestore(virtio_state->tx_lock, flags);

    if (txed && !(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	v3_pci_raise_irq(virtio_state->virtio_dev->pci_bus, 0, virtio_state->pci_dev);
	virtio_state->virtio_cfg.pci_isr = 0x1;

	virtio_state->stats.rx_interrupts ++;
    }

    if(txed > 0) {
	V3_Net_Print(2, "Virtio Handle TX: txed pkts: %d\n", txed);
    }

    return 0;

exit_error:
	
    v3_unlock_irqrestore(virtio_state->tx_lock, flags);
    return -1;
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
		    break;
		case 1:
		    virtio_setup_queue(core, virtio, &virtio->tx_vq, pfn, page_addr);
		    if(virtio->tx_notify == 0){
	 		disable_cb(&virtio->tx_vq);
			vnet_thread_wakeup(virtio->poll_thread);
    		    }
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
		    /* receive queue refill */
		    virtio->stats.tx_interrupts ++;
		} else if (queue_idx == 1){
		    if (handle_pkt_tx(core, virtio) == -1) {
			PrintError("Could not handle Virtio NIC tx kick\n");
			return -1;
		    }
		    virtio->stats.tx_interrupts ++;
		} else if (queue_idx == 2){
		    /* ctrl */
		} else {
		    PrintError("Wrong queue index %d\n", queue_idx);
		}	
		break;		
	    }
	
	case VIRTIO_STATUS_PORT:
	    virtio->virtio_cfg.status = *(uint8_t *)src;
	    if (virtio->virtio_cfg.status == 0) {
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
		//return -1;
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


/* receiving raw ethernet pkt from backend */
static int virtio_rx(uint8_t * buf, uint32_t size, void * private_data) {
    struct virtio_net_state * virtio = (struct virtio_net_state *)private_data;
    struct virtio_queue * q = &(virtio->rx_vq);
    struct virtio_net_hdr_mrg_rxbuf hdr;
    uint32_t data_len;
    unsigned long flags;

    V3_Net_Print(2, "Virtio-NIC: virtio_rx: size: %d\n", size);
    if(v3_net_debug >= 4){
	v3_hexdump(buf, size, NULL, 0);
    }

    flags = v3_lock_irqsave(virtio->rx_lock);

    data_len = size;
    memset(&hdr, 0, sizeof(struct virtio_net_hdr_mrg_rxbuf));

    if (q->ring_avail_addr == 0) {
	V3_Net_Print(2, "Virtio NIC: RX Queue not set\n");
	virtio->stats.rx_dropped ++;
	goto err_exit;
    }

    if (q->cur_avail_idx != q->avail->index){
	addr_t hdr_addr = 0;
	uint16_t buf_idx = 0;
	uint16_t hdr_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];
	struct vring_desc * hdr_desc = NULL;
	struct vring_desc * buf_desc = NULL;
	uint32_t hdr_len = 0;
	uint32_t len;

	hdr_desc = &(q->desc[hdr_idx]);
	if (v3_gpa_to_hva(&(virtio->virtio_dev->vm->cores[0]), hdr_desc->addr_gpa, &(hdr_addr)) == -1) {
	    V3_Net_Print(2, "Virtio NIC: Could not translate receive buffer address\n");
	    virtio->stats.rx_dropped ++;
	    goto err_exit;
	}

     	hdr_len = sizeof(struct virtio_net_hdr_mrg_rxbuf);

	if(virtio->mergeable_rx_bufs){/* merged buffer */
	    uint32_t offset = 0;
	    len = 0;
	    hdr.num_buffers = 0;

	    hdr_desc = &(q->desc[buf_idx]);
	    hdr_desc->flags &= ~VIRTIO_NEXT_FLAG;

	    len = copy_data_to_desc(&(virtio->virtio_dev->vm->cores[0]), virtio, hdr_desc, buf, data_len, hdr_len);
	    offset += len;

	    hdr.num_buffers ++;
	    q->used->ring[q->used->index % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];
	    q->used->ring[q->used->index % q->queue_size].length = hdr_len + len;
	    q->cur_avail_idx ++;

	    while(offset < data_len) {
		buf_idx = q->avail->ring[q->cur_avail_idx % q->queue_size];
		buf_desc = &(q->desc[buf_idx]);

		len = copy_data_to_desc(&(virtio->virtio_dev->vm->cores[0]), virtio, buf_desc, buf + offset, data_len - offset, 0);	
		if (len <= 0){
		    V3_Net_Print(2, "Virtio NIC:merged buffer, %d buffer size %d\n", hdr.num_buffers, data_len);
		    virtio->stats.rx_dropped ++;
	      	    goto err_exit;
		}
		offset += len;
		buf_desc->flags &= ~VIRTIO_NEXT_FLAG;

		hdr.num_buffers ++;
		q->used->ring[(q->used->index + hdr.num_buffers) % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];
		q->used->ring[(q->used->index + hdr.num_buffers) % q->queue_size].length = len;
		q->cur_avail_idx ++;   
	    }
	    q->used->index += hdr.num_buffers;
	    copy_data_to_desc(&(virtio->virtio_dev->vm->cores[0]), virtio, hdr_desc, (uchar_t *)&hdr, hdr_len, 0);
	}else{
	    hdr_desc = &(q->desc[buf_idx]);
	    copy_data_to_desc(&(virtio->virtio_dev->vm->cores[0]), virtio, hdr_desc, (uchar_t *)&hdr, hdr_len, 0);

	    buf_idx = hdr_desc->next;
	    buf_desc = &(q->desc[buf_idx]);
	    len = copy_data_to_desc(&(virtio->virtio_dev->vm->cores[0]), virtio, buf_desc, buf, data_len, 0);	    
	    if (len < data_len) {
	    	V3_Net_Print(2, "Virtio NIC: ring buffer len less than pkt size, merged buffer not supported, buffer size %d\n", len);
	    	virtio->stats.rx_dropped ++;
		
	    	goto err_exit;
	    }
	    buf_desc->flags &= ~VIRTIO_NEXT_FLAG;
		
	    q->used->ring[q->used->index % q->queue_size].id = q->avail->ring[q->cur_avail_idx % q->queue_size];
	    q->used->ring[q->used->index % q->queue_size].length = data_len + hdr_len; /* This should be the total length of data sent to guest (header+pkt_data) */
	    q->used->index++;
	    q->cur_avail_idx++;
	} 

 	virtio->stats.rx_pkts ++;
	virtio->stats.rx_bytes += size;
    } else {
	V3_Net_Print(2, "Virtio NIC: Guest RX queue is full\n");
	virtio->stats.rx_dropped ++;

 	/* kick guest to refill the queue */
	virtio->virtio_cfg.pci_isr = 0x1;	
	v3_pci_raise_irq(virtio->virtio_dev->pci_bus, 0, virtio->pci_dev);
	v3_interrupt_cpu(virtio->virtio_dev->vm, virtio->virtio_dev->vm->cores[0].cpu_id, 0);
	virtio->stats.rx_interrupts ++;
	
	goto err_exit;
    }

    if (!(q->avail->flags & VIRTIO_NO_IRQ_FLAG)) {
	V3_Net_Print(2, "Raising IRQ %d\n",  virtio->pci_dev->config_header.intr_line);

	virtio->virtio_cfg.pci_isr = 0x1;	
	v3_pci_raise_irq(virtio->virtio_dev->pci_bus, 0, virtio->pci_dev);

	virtio->stats.rx_interrupts ++;
    }

    v3_unlock_irqrestore(virtio->rx_lock, flags);

    /* notify guest if it is in guest mode */
    if(virtio->rx_notify == 1 && 
	V3_Get_CPU() != virtio->virtio_dev->vm->cores[0].cpu_id){
	v3_interrupt_cpu(virtio->virtio_dev->vm, virtio->virtio_dev->vm->cores[0].cpu_id, 0);
    }

    return 0;

err_exit:

    v3_unlock_irqrestore(virtio->rx_lock, flags);
 
    return -1;
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


static int virtio_tx_flush(void * args){
    struct virtio_net_state *virtio  = (struct virtio_net_state *)args;

    V3_Print("Virtio TX Poll Thread Starting for %s\n", virtio->vm->name);

    while(1){
    	if(virtio->tx_notify == 0){
    	    handle_pkt_tx(&(virtio->vm->cores[0]), virtio);
	    v3_yield(NULL);
    	}else {
	    vnet_thread_sleep(-1);
    	}
    }

    return 0;
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
	
    /* this is to account for any low order bits being set in num_ports
      * if there are none, then num_ports was already a power of 2 so we shift right to reset it
      */
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

#define RATE_UPPER_THRESHOLD 10  /* 10000 pkts per second, around 100Mbits */
#define RATE_LOWER_THRESHOLD 1
#define PROFILE_PERIOD 10000 /*us*/

static void virtio_nic_timer(struct guest_info * core, 
			     uint64_t cpu_cycles, uint64_t cpu_freq, 
			     void * priv_data) {
    struct virtio_net_state * net_state = (struct virtio_net_state *)priv_data;
    uint64_t period_us;
    static int profile_ms = 0;

    period_us = (1000*cpu_cycles)/cpu_freq;
    net_state->past_us += period_us;

    if(net_state->past_us > PROFILE_PERIOD){ 
	uint32_t tx_rate, rx_rate;
	
	tx_rate = (net_state->stats.tx_pkts - net_state->tx_pkts)/(net_state->past_us/1000); /* pkts/per ms */
	rx_rate = (net_state->stats.rx_pkts - net_state->rx_pkts)/(net_state->past_us/1000);

	net_state->tx_pkts = net_state->stats.tx_pkts;
	net_state->rx_pkts = net_state->stats.rx_pkts;

	if(tx_rate > RATE_UPPER_THRESHOLD && net_state->tx_notify == 1){
	    V3_Print("Virtio NIC: Switch TX to VMM driven mode\n");
	    disable_cb(&(net_state->tx_vq));
	    net_state->tx_notify = 0;
	    vnet_thread_wakeup(net_state->poll_thread);
	}

	if(tx_rate < RATE_LOWER_THRESHOLD && net_state->tx_notify == 0){
	    V3_Print("Virtio NIC: Switch TX to Guest  driven mode\n");
	    enable_cb(&(net_state->tx_vq));
	    net_state->tx_notify = 1;
	}

	if(rx_rate > RATE_UPPER_THRESHOLD && net_state->rx_notify == 1){
	    V3_Print("Virtio NIC: Switch RX to VMM None notify mode\n");
	    net_state->rx_notify = 0;
	}

	if(rx_rate < RATE_LOWER_THRESHOLD && net_state->rx_notify == 0){
	    V3_Print("Virtio NIC: Switch RX to VMM notify mode\n");
	    net_state->rx_notify = 1;
	}

	net_state->past_us = 0;
    }

    profile_ms += period_us/1000;
    if(profile_ms > 20000){
	V3_Net_Print(1, "Virtio NIC: TX: Pkt: %lld, Bytes: %lld\n\t\tRX Pkt: %lld. Bytes: %lld\n\t\tDropped: tx %lld, rx %lld\nInterrupts: tx %d, rx %d\nTotal Exit: %lld\n",
	    	net_state->stats.tx_pkts, net_state->stats.tx_bytes,
	    	net_state->stats.rx_pkts, net_state->stats.rx_bytes,
	    	net_state->stats.tx_dropped, net_state->stats.rx_dropped,
	    	net_state->stats.tx_interrupts, net_state->stats.rx_interrupts,
	    	net_state->vm->cores[0].num_exits);
	profile_ms = 0;
    }
}

static struct v3_timer_ops timer_ops = {
    .update_timer = virtio_nic_timer,
};


static int connect_fn(struct v3_vm_info * info, 
		      void * frontend_data, 
		      struct v3_dev_net_ops * ops, 
		      v3_cfg_tree_t * cfg, 
		      void * private_data) {
    struct virtio_dev_state * virtio = (struct virtio_dev_state *)frontend_data;
    struct virtio_net_state * net_state  = (struct virtio_net_state *)V3_Malloc(sizeof(struct virtio_net_state));

    memset(net_state, 0, sizeof(struct virtio_net_state));
    register_dev(virtio, net_state);

    net_state->vm = info;
    net_state->net_ops = ops;
    net_state->backend_data = private_data;
    net_state->virtio_dev = virtio;
    net_state->tx_notify = 0;
    net_state->rx_notify = 0;
	
    net_state->timer = v3_add_timer(&(info->cores[0]),&timer_ops,net_state);

    ops->recv = virtio_rx;
    ops->frontend_data = net_state;
    memcpy(ops->fnt_mac, virtio->mac, ETH_ALEN);

    net_state->poll_thread = vnet_start_thread(virtio_tx_flush, (void *)net_state, "Virtio_Poll");

    return 0;
}

static int virtio_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct vm_device * pci_bus = v3_find_dev(vm, v3_cfg_val(cfg, "bus"));
    struct virtio_dev_state * virtio_state = NULL;
    char * dev_id = v3_cfg_val(cfg, "ID");
    char macstr[128];
    char * str = v3_cfg_val(cfg, "mac");
    memcpy(macstr, str, strlen(str));

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
	
