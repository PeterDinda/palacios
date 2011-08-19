/* 
 * Palacios VNET Host Bridge
 * (c) Lei Xia  2010
 */ 

#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/inet.h>
#include <linux/kthread.h>

#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/string.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <asm/msr.h>

#include <vnet/vnet.h>
#include <vnet/vnet_hashtable.h>
#include "palacios-vnet.h"

#define VNET_SERVER_PORT 9000

struct vnet_link {
    uint32_t dst_ip;
    uint16_t dst_port;
    
    struct socket * sock;
    struct sockaddr_in sock_addr;
    vnet_brg_proto_t sock_proto;

    struct nic_statistics stats;

    uint32_t idx;

    struct list_head node;
};


struct vnet_brg_state {
    uint8_t status;

    uint32_t num_links;
    uint32_t link_idx;
    struct list_head link_list;
    struct hashtable *ip2link;

    spinlock_t lock;

    struct socket * serv_sock;
    struct sockaddr_in serv_addr;
    vnet_brg_proto_t serv_proto;
	
    struct task_struct * serv_thread;

    void * brg_data; /* private data from vnet_core */

    struct vnet_brg_stats stats;
};


static struct vnet_brg_state vnet_brg_s;


int vnet_brg_stats(struct vnet_brg_stats * stats){
    memcpy(stats, &(vnet_brg_s.stats), sizeof(*stats));

    return 0;
}

static inline struct vnet_link * _link_by_ip(uint32_t ip) {
    return (struct vnet_link *)vnet_htable_search(vnet_brg_s.ip2link, (addr_t)&ip);
}

static inline struct vnet_link * _link_by_idx(int idx) {
    struct vnet_link * link = NULL;

    list_for_each_entry(link, &(vnet_brg_s.link_list), node) {
		
	if (link->idx == idx) {
	    return link;
	}
    }
    return NULL;
}


static void _delete_link(struct vnet_link * link){
    unsigned long flags;

    link->sock->ops->release(link->sock);

    spin_lock_irqsave(&(vnet_brg_s.lock), flags);
    list_del(&(link->node));
    vnet_htable_remove(vnet_brg_s.ip2link, (addr_t)&(link->dst_ip), 0);
    vnet_brg_s.num_links --;
    spin_unlock_irqrestore(&(vnet_brg_s.lock), flags);

    printk("VNET Bridge: Link deleted, ip 0x%x, port: %d, idx: %d\n", 
	   link->dst_ip, 
	   link->dst_port, 
	   link->idx);

    kfree(link);
    link = NULL;
}

void vnet_brg_delete_link(uint32_t idx){
    struct vnet_link * link = _link_by_idx(idx);

    if(link){
	_delete_link(link);
    }
}

static void deinit_links_list(void){
    struct vnet_link * link;

    list_for_each_entry(link, &(vnet_brg_s.link_list), node) {
     	_delete_link(link);
    }
}

static uint32_t _create_link(struct vnet_link * link) {
    int err;
    unsigned long flags;
    int protocol;

    switch(link->sock_proto){
	case UDP:
	    protocol = IPPROTO_UDP;
	    break;
    	case TCP:
	    protocol = IPPROTO_TCP;
    	    break;

	default:
           printk("Unsupported VNET Server Protocol\n");
	    return -1;
    }
   
    if ((err = sock_create(AF_INET, SOCK_DGRAM, protocol, &link->sock)) < 0) {
	printk("Could not create socket for VNET Link, error %d\n", err);
	return -1;
    }

    memset(&link->sock_addr, 0, sizeof(struct sockaddr));

    link->sock_addr.sin_family = AF_INET;
    link->sock_addr.sin_addr.s_addr = link->dst_ip;
    link->sock_addr.sin_port = htons(link->dst_port);

    if ((err = link->sock->ops->connect(link->sock, (struct sockaddr *)&(link->sock_addr), sizeof(struct sockaddr), 0) < 0)) {
	printk("Could not connect to remote VNET Server, error %d\n", err);
	return -1;
    }

    spin_lock_irqsave(&(vnet_brg_s.lock), flags);
    list_add(&(link->node), &(vnet_brg_s.link_list));
    vnet_brg_s.num_links ++;
    link->idx = ++ vnet_brg_s.link_idx;
    vnet_htable_insert(vnet_brg_s.ip2link, (addr_t)&(link->dst_ip), (addr_t)link);
    spin_unlock_irqrestore(&(vnet_brg_s.lock), flags);

    printk("VNET Bridge: Link created, ip 0x%x, port: %d, idx: %d, link: %p, protocol: %s\n", 
	   link->dst_ip, 
	   link->dst_port, 
	   link->idx, 
	   link,
	   ((link->sock_proto==UDP)?"UDP":"TCP"));

    return link->idx;
}


uint32_t vnet_brg_add_link(uint32_t ip, uint16_t port, vnet_brg_proto_t proto){
     struct vnet_link * new_link = NULL;
     uint32_t idx;

     new_link = kmalloc(sizeof(struct vnet_link), GFP_KERNEL);
     if (!new_link) {
	return -1;
     }
     memset(new_link, 0, sizeof(struct vnet_link));

     new_link->dst_ip = ip;
     new_link->dst_port = port;
     new_link->sock_proto = proto;

     idx = _create_link(new_link);
     if (idx < 0) {
	printk("Could not create link\n");
	kfree(new_link);
	return -1;
     }

     return idx;
}


int vnet_brg_link_stats(uint32_t link_idx, struct nic_statistics * stats){
     struct vnet_link * link;

     link = _link_by_idx(link_idx);
     if(!link){
	 return -1;
     }

     memcpy(stats, &(link->stats), sizeof(*stats));

     return 0;
}
     

static int 
_udp_send(struct socket * sock, 
	 struct sockaddr_in * addr,
	 unsigned char * buf,  int len) {
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int size = 0;

	  
    if (sock->sk == NULL) {
	return 0;
    }

    iov.iov_base = buf;
    iov.iov_len = len;

    msg.msg_flags = 0;
    msg.msg_name = addr;
    msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    size = sock_sendmsg(sock, &msg, len);
    set_fs(oldfs);

    return size;
}



static int 
_udp_recv(struct socket * sock, 
	 struct sockaddr_in * addr,
	 unsigned char * buf, int len) {
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int size = 0;
    
    if (sock->sk == NULL) {
	return 0;
    }

    iov.iov_base = buf;
    iov.iov_len = len;
    
    msg.msg_flags = 0;
    msg.msg_name = addr;
    msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    size = sock_recvmsg(sock, &msg, len, msg.msg_flags);
	
    set_fs(oldfs);
    
    return size;
}

/* send packets to VNET core */
static int 
send_to_palacios(unsigned char * buf, 
		 int len,
		 int link_id){
    struct v3_vnet_pkt pkt;
    pkt.size = len;
    pkt.src_type = LINK_EDGE;
    pkt.src_id = link_id;
    memcpy(pkt.header, buf, ETHERNET_HEADER_LEN);
    pkt.data = buf;

    if(net_debug >= 2){
    	printk("VNET Lnx Bridge: send pkt to VNET core (size: %d, src_id: %d, src_type: %d)\n", 
			pkt.size,  pkt.src_id, pkt.src_type);
    	if(net_debug >= 4){
	    print_hex_dump(NULL, "pkt_data: ", 0, 20, 20, pkt.data, pkt.size, 0);
    	}
    }

    vnet_brg_s.stats.pkt_to_vmm ++;

    return v3_vnet_send_pkt(&pkt, NULL);
}


/* send packet to extern network */
static int 
bridge_send_pkt(struct v3_vm_info * vm, 
		struct v3_vnet_pkt * pkt, 
		void * private_data) {
    struct vnet_link * link;

    if(net_debug >= 2){
	printk("VNET Lnx Host Bridge: packet received from VNET Core ... pkt size: %d, link: %d\n",
			pkt->size,
			pkt->dst_id);
    	if(net_debug >= 4){
	    print_hex_dump(NULL, "pkt_data: ", 0, 20, 20, pkt->data, pkt->size, 0);
    	}
    }

    vnet_brg_s.stats.pkt_from_vmm ++;

    link = _link_by_idx(pkt->dst_id);
    if (link != NULL) {
	switch(link->sock_proto){
	    case UDP:
 	    	_udp_send(link->sock, &(link->sock_addr), pkt->data, pkt->size);
		vnet_brg_s.stats.pkt_to_phy ++;
		break;
	    case TCP:
		vnet_brg_s.stats.pkt_to_phy ++;
		break;	

	    default:
		printk("VNET Server: Invalid Link Protocol\n");
		vnet_brg_s.stats.pkt_drop_vmm ++;
	}
	link->stats.tx_bytes += pkt->size;
	link->stats.tx_pkts ++;
    } else {
	printk("VNET Bridge Linux Host: wrong dst link, idx: %d, discards the packet\n", pkt->dst_id);
	vnet_brg_s.stats.pkt_drop_vmm ++;
    }

    return 0;
}


static int init_vnet_serv(void) {
    int protocol;
    int err;

    switch(vnet_brg_s.serv_proto){
	case UDP:
	    protocol = IPPROTO_UDP;
	    break;
    	case TCP:
	    protocol = IPPROTO_TCP;
    	    break;

	default:
           printk("Unsupported VNET Server Protocol\n");
	    return -1;
    }
	 
    if ((err = sock_create(AF_INET, SOCK_DGRAM, protocol, &vnet_brg_s.serv_sock)) < 0) {
	printk("Could not create VNET server socket, error: %d\n", err);
	return -1;
    }

    memset(&vnet_brg_s.serv_addr, 0, sizeof(struct sockaddr));

    vnet_brg_s.serv_addr.sin_family = AF_INET;
    vnet_brg_s.serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    vnet_brg_s.serv_addr.sin_port = htons(VNET_SERVER_PORT);

    if ((err = vnet_brg_s.serv_sock->ops->bind(vnet_brg_s.serv_sock, (struct sockaddr *)&(vnet_brg_s.serv_addr), sizeof(struct sockaddr))) < 0) {
	printk("Could not bind VNET server socket to port %d, error: %d\n", VNET_SERVER_PORT, err);
	return -1;
    }

    printk("VNET server bind to port: %d\n", VNET_SERVER_PORT);

    if(vnet_brg_s.serv_proto == TCP){
	if((err = vnet_brg_s.serv_sock->ops->listen(vnet_brg_s.serv_sock, 32)) < 0){
	    printk("VNET Server error listening on port %d, error %d\n", VNET_SERVER_PORT, err);
	    return -1;
	}
    }

    return 0;
}

static int _udp_server(void * arg) {
    unsigned char * pkt;
    struct sockaddr_in pkt_addr;
    struct vnet_link * link = NULL;
    int len;

    printk("Palacios VNET Bridge: UDP receiving server ..... \n");

    pkt = kmalloc(MAX_PACKET_LEN, GFP_KERNEL);
    while (!kthread_should_stop()) {
	
    	len = _udp_recv(vnet_brg_s.serv_sock, &pkt_addr, pkt, MAX_PACKET_LEN); 
	if(len < 0) {
	    printk("Receive error: Could not get packet, error %d\n", len);
	    continue;
	}

	link = _link_by_ip(pkt_addr.sin_addr.s_addr);
	if (link == NULL){
	    printk("VNET Server: No VNET Link match the src IP\n");
	    vnet_brg_s.stats.pkt_drop_phy ++;
	    continue;
	}
	
	vnet_brg_s.stats.pkt_from_phy ++;
	link->stats.rx_bytes += len;
	link->stats.rx_pkts ++;

	send_to_palacios(pkt, len, link->idx);
    }

    kfree(pkt);

    return 0;
}


static int _rx_server(void * arg) {
    
    if(vnet_brg_s.serv_proto == UDP){
    	_udp_server(NULL);
    }else if(vnet_brg_s.serv_proto == TCP) {
	//accept new connection
	//use select to receive pkt from physical network
	//or create new kthread to handle each connection?
    }else {
    	printk ("VNET Server: Unsupported Protocol\n");
	return -1;
    }

    return 0;
}

static inline unsigned int hash_fn(addr_t hdr_ptr) {    
    return vnet_hash_buffer((uint8_t *)hdr_ptr, sizeof(uint32_t));
}

static inline int hash_eq(addr_t key1, addr_t key2) {	
    return (memcmp((uint8_t *)key1, (uint8_t *)key2, sizeof(uint32_t)) == 0);
}


int vnet_bridge_init(void) {
    struct v3_vnet_bridge_ops bridge_ops;

    if(vnet_brg_s.status != 0) {
	return -1;
    }	
    vnet_brg_s.status = 1;	
	
    memset(&vnet_brg_s, 0, sizeof(struct vnet_brg_state));

    INIT_LIST_HEAD(&(vnet_brg_s.link_list));
    spin_lock_init(&(vnet_brg_s.lock));

    vnet_brg_s.serv_proto = UDP;

    vnet_brg_s.ip2link = vnet_create_htable(10, hash_fn, hash_eq);
    if(vnet_brg_s.ip2link == NULL){
	printk("Failure to initiate VNET link hashtable\n");
	return -1;
    }
	
    if(init_vnet_serv() < 0){
	printk("Failure to initiate VNET server\n");
	return -1;
    }

    vnet_brg_s.serv_thread = kthread_run(_rx_server, NULL, "vnet_brgd");

    bridge_ops.input = bridge_send_pkt;
    bridge_ops.poll = NULL;
	
    if( v3_vnet_add_bridge(NULL, &bridge_ops, HOST_LNX_BRIDGE, NULL) < 0){
	printk("VNET LNX Bridge: Fails to register bridge to VNET core");
    }

    printk("VNET Linux Bridge initiated\n");

    return 0;
}


void vnet_bridge_deinit(void){

    v3_vnet_del_bridge(HOST_LNX_BRIDGE);

    kthread_stop(vnet_brg_s.serv_thread);
    vnet_brg_s.serv_sock->ops->release(vnet_brg_s.serv_sock);

    deinit_links_list();

    vnet_free_htable(vnet_brg_s.ip2link, 0, 0);

    vnet_brg_s.status = 0;
}


