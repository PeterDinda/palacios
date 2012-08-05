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

#include <linux/net.h>
#include <linux/socket.h>
#include <net/sock.h>

#include <vnet/vnet.h>
#include <vnet/vnet_hashtable.h>
#include "palacios-vnet.h"
#include "palacios.h"



#define VNET_SERVER_PORT 9000

#define VNET_NOPROGRESS_LIMIT 1000

#define VNET_YIELD_TIME_USEC  1000

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
    unsigned long flags = 0;

    link->sock->ops->release(link->sock);

    spin_lock_irqsave(&(vnet_brg_s.lock), flags);
    list_del(&(link->node));
    vnet_htable_remove(vnet_brg_s.ip2link, (addr_t)&(link->dst_ip), 0);
    vnet_brg_s.num_links --;
    spin_unlock_irqrestore(&(vnet_brg_s.lock), flags);

    INFO("VNET Bridge: Link deleted, ip 0x%x, port: %d, idx: %d\n", 
	   link->dst_ip, 
	   link->dst_port, 
	   link->idx);

    palacios_free(link);
    link = NULL;
}

void vnet_brg_delete_link(uint32_t idx){
    struct vnet_link * link = _link_by_idx(idx);

    if(link){
	_delete_link(link);
    }
}

static void deinit_links_list(void){
    struct vnet_link * link = NULL, * tmp_link = NULL;

    list_for_each_entry_safe(link, tmp_link, &(vnet_brg_s.link_list), node) {
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
           WARNING("Unsupported VNET Server Protocol\n");
	    return -1;
    }
   
    if ((err = sock_create(AF_INET, SOCK_DGRAM, protocol, &link->sock)) < 0) {
	WARNING("Could not create socket for VNET Link, error %d\n", err);
	return -1;
    }

    if (link->sock_proto == UDP) { 
	// no UDP checksumming
	lock_sock(link->sock->sk);
	link->sock->sk->sk_no_check = 1;
	release_sock(link->sock->sk);
    }

    memset(&link->sock_addr, 0, sizeof(struct sockaddr));

    link->sock_addr.sin_family = AF_INET;
    link->sock_addr.sin_addr.s_addr = link->dst_ip;
    link->sock_addr.sin_port = htons(link->dst_port);


    if ((err = link->sock->ops->connect(link->sock, (struct sockaddr *)&(link->sock_addr), sizeof(struct sockaddr), 0) < 0)) {
	WARNING("Could not connect to remote VNET Server, error %d\n", err);
	return -1;
    }


    spin_lock_irqsave(&(vnet_brg_s.lock), flags);
    list_add(&(link->node), &(vnet_brg_s.link_list));
    vnet_brg_s.num_links ++;
    link->idx = ++ vnet_brg_s.link_idx;
    vnet_htable_insert(vnet_brg_s.ip2link, (addr_t)&(link->dst_ip), (addr_t)link);
    spin_unlock_irqrestore(&(vnet_brg_s.lock), flags);

    INFO("VNET Bridge: Link created, ip 0x%x, port: %d, idx: %d, link: %p, protocol: %s\n", 
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

     new_link = palacios_alloc(sizeof(struct vnet_link));
     if (!new_link) {
	return -1;
     }
     memset(new_link, 0, sizeof(struct vnet_link));

     new_link->dst_ip = ip;
     new_link->dst_port = port;
     new_link->sock_proto = proto;

     idx = _create_link(new_link);
     if (idx < 0) {
	WARNING("Could not create link\n");
	palacios_free(new_link);
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

    msg.msg_flags = MSG_NOSIGNAL;
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
	  unsigned char * buf, int len, int nonblocking) {
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int size = 0;
    
    if (sock->sk == NULL) {
	return 0;
    }

    iov.iov_base = buf;
    iov.iov_len = len;
    
    msg.msg_flags = MSG_NOSIGNAL | (nonblocking ? MSG_DONTWAIT : 0);
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
    memset(&pkt,0,sizeof(struct v3_vnet_pkt));
    pkt.size = len;
    pkt.dst_type = LINK_NOSET;
    pkt.src_type = LINK_EDGE;
    pkt.src_id = link_id;
    memcpy(pkt.header, buf, ETHERNET_HEADER_LEN);
    pkt.data = buf;

    if(net_debug >= 2){
    	DEBUG("VNET Lnx Bridge: send pkt to VNET core (size: %d, src_id: %d, src_type: %d)\n", 
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
    struct vnet_link * link = NULL;

    if(net_debug >= 2){
	DEBUG("VNET Lnx Host Bridge: packet received from VNET Core ... pkt size: %d, link: %d\n",
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
		WARNING("VNET Server: Invalid Link Protocol\n");
		vnet_brg_s.stats.pkt_drop_vmm ++;
	}
	link->stats.tx_bytes += pkt->size;
	link->stats.tx_pkts ++;
    } else {
	INFO("VNET Bridge Linux Host: wrong dst link, idx: %d, discarding the packet\n", pkt->dst_id);
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
           WARNING("Unsupported VNET Server Protocol\n");
	    return -1;
    }
	 
    if ((err = sock_create(AF_INET, SOCK_DGRAM, protocol, &vnet_brg_s.serv_sock)) < 0) {
	WARNING("Could not create VNET server socket, error: %d\n", err);
	return -1;
    }

    if (vnet_brg_s.serv_proto == UDP) { 
	// No UDP checksumming is done
	lock_sock(vnet_brg_s.serv_sock->sk);
	vnet_brg_s.serv_sock->sk->sk_no_check = 1;
	release_sock(vnet_brg_s.serv_sock->sk);
    }

    memset(&vnet_brg_s.serv_addr, 0, sizeof(struct sockaddr));

    vnet_brg_s.serv_addr.sin_family = AF_INET;
    vnet_brg_s.serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    vnet_brg_s.serv_addr.sin_port = htons(VNET_SERVER_PORT);

    if ((err = vnet_brg_s.serv_sock->ops->bind(vnet_brg_s.serv_sock, (struct sockaddr *)&(vnet_brg_s.serv_addr), sizeof(struct sockaddr))) < 0) {
	WARNING("Could not bind VNET server socket to port %d, error: %d\n", VNET_SERVER_PORT, err);
	return -1;
    }

    INFO("VNET server bind to port: %d\n", VNET_SERVER_PORT);

    if(vnet_brg_s.serv_proto == TCP){
	if((err = vnet_brg_s.serv_sock->ops->listen(vnet_brg_s.serv_sock, 32)) < 0){
	    WARNING("VNET Server error listening on port %d, error %d\n", VNET_SERVER_PORT, err);
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
    uint64_t noprogress_count;

    INFO("Palacios VNET Bridge: UDP receiving server ..... \n");

    pkt = palacios_alloc(MAX_PACKET_LEN);

    if (!pkt) { 
	ERROR("Unable to allocate packet in VNET UDP Server\n");
	return -1;
    }

    
    noprogress_count=0;

    while (!kthread_should_stop()) {

	// This is a NONBLOCKING receive
	// If we block here, we will never detect that this thread
	// is being signaled to stop, plus we might go uninterrupted on this core
	// blocking out access to other threads - leave this NONBLOCKING
	// unless you know what you are doing
    	len = _udp_recv(vnet_brg_s.serv_sock, &pkt_addr, pkt, MAX_PACKET_LEN, 1); 


	// If it would have blocked, we have no packet, and so
	// we will give other threads on this core a chance
	if (len==-EAGAIN || len==-EWOULDBLOCK || len==-EINTR) { 

	    // avoid rollover in the counter out of paranoia
	    if (! ((noprogress_count + 1) < noprogress_count)) { 
		noprogress_count++;
	    }
	    
	    // adaptively select yielding strategy depending on
	    // whether we are making progress
	    if (noprogress_count < VNET_NOPROGRESS_LIMIT) { 
		// Likely making progress, do fast yield so we 
		// come back immediately if there is no other action
		palacios_yield_cpu();
	    } else {
		// Likely not making progress, do potentially slow
		// yield - we won't come back for until VNET_YIELD_TIME_USEC has passed
		palacios_yield_cpu_timed(VNET_YIELD_TIME_USEC);
	    }

	    continue;
	}
	

	// Something interesting has happened, therefore progress!
	noprogress_count=0;
	    

	if(len < 0) {
	    WARNING("Receive error: Could not get packet, error %d\n", len);
	    continue;
	}

	link = _link_by_ip(pkt_addr.sin_addr.s_addr);

	if (link == NULL){
	    WARNING("VNET Server: No VNET Link matches the src IP\n");
	    vnet_brg_s.stats.pkt_drop_phy ++;
	    continue;
	}
	
	vnet_brg_s.stats.pkt_from_phy ++;
	link->stats.rx_bytes += len;
	link->stats.rx_pkts ++;

	send_to_palacios(pkt, len, link->idx);
    }

    INFO("VNET Server: UDP thread exiting\n");

    palacios_free(pkt);

    return 0;
}


static int _rx_server(void * arg) {
    
    if(vnet_brg_s.serv_proto == UDP){
    	_udp_server(NULL);
    }else if(vnet_brg_s.serv_proto == TCP) {
	//accept new connection
	//use select to receive pkt from physical network
	//or create new kthread to handle each connection?
	WARNING("VNET Server: TCP is not currently supported\n");
	return -1;
    }else {
    	WARNING ("VNET Server: Unsupported Protocol\n");
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
	WARNING("Failure to initiate VNET link hashtable\n");
	return -1;
    }
	
    if(init_vnet_serv() < 0){
	WARNING("Failure to initiate VNET server\n");
	return -1;
    }

    vnet_brg_s.serv_thread = kthread_run(_rx_server, NULL, "vnet_brgd");

    bridge_ops.input = bridge_send_pkt;
    bridge_ops.poll = NULL;
	
    if( v3_vnet_add_bridge(NULL, &bridge_ops, HOST_LNX_BRIDGE, NULL) < 0){
	WARNING("VNET LNX Bridge: Fails to register bridge to VNET core");
    }

    INFO("VNET Linux Bridge initiated\n");

    return 0;
}


void vnet_bridge_deinit(void){

    INFO("VNET LNX Bridge Deinit Started\n");

    v3_vnet_del_bridge(HOST_LNX_BRIDGE);

    //DEBUG("Stopping bridge service thread\n");

    kthread_stop(vnet_brg_s.serv_thread);

    //DEBUG("Releasing bridee service socket\n");

    vnet_brg_s.serv_sock->ops->release(vnet_brg_s.serv_sock);

    //DEBUG("Deiniting bridge links\n");

    deinit_links_list();

    //DEBUG("Freeing bridge hash tables\n");

    vnet_free_htable(vnet_brg_s.ip2link, 0, 0);

    vnet_brg_s.status = 0;

    INFO("VNET LNX Bridge Deinit Finished\n");
}


