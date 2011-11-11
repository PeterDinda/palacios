/* 
 * Palacios Raw Packet Interface Implementation
 * (c) Lei Xia  2010
 */
 
#include <asm/uaccess.h>
#include <linux/inet.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/if_packet.h>
#include <linux/errno.h>
#include <asm/msr.h>
 
#include <interfaces/vmm_packet.h>
#include <palacios/vmm_ethernet.h>

#include "palacios.h"
#include "util-hashtable.h"
#include "linux-exts.h"
#include "vm.h"


/* there is one for each host nic */
struct raw_interface {
    char eth_dev[126];  /* host nic name "eth0" ... */
	
    struct socket * raw_sock;
    uint8_t inited;

    struct list_head brdcast_recvers;
    struct hashtable * mac_to_recver;

    struct task_struct * recv_thread;

    struct list_head node; 
};

struct palacios_packet_state{
    spinlock_t lock;

    struct list_head open_interfaces;
};

struct palacios_packet_state packet_state;

static inline uint_t hash_fn(addr_t hdr_ptr) {    
    uint8_t * hdr_buf = (uint8_t *)hdr_ptr;

    return palacios_hash_buffer(hdr_buf, ETH_ALEN);
}

static inline int hash_eq(addr_t key1, addr_t key2) {	
    return (memcmp((uint8_t *)key1, (uint8_t *)key2, ETH_ALEN) == 0);
}


static int 
recv_pkt(struct socket * raw_sock, unsigned char * pkt, unsigned int len) {
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    unsigned int size = 0;
    
    if (raw_sock == NULL) {
	return -1;
    }

    iov.iov_base = pkt;
    iov.iov_len = len;
    
    msg.msg_flags = 0;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    size = sock_recvmsg(raw_sock, &msg, len, msg.msg_flags);
    set_fs(oldfs);
    
    return size;
}


static int 
init_socket(struct raw_interface * iface, const char * eth_dev){
    int err;
    struct sockaddr_ll sock_addr;
    struct net_device * net_dev;

    err = sock_create(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL), &(iface->raw_sock)); 
    if (err < 0) {
	printk(KERN_WARNING "Could not create a PF_PACKET Socket, err %d\n", err);
	return -1;
    }

    net_dev = dev_get_by_name(&init_net, eth_dev);
    if(net_dev == NULL) {
	printk(KERN_WARNING "Palacios Packet: Unable to get index for device %s\n", eth_dev);
	sock_release(iface->raw_sock);
	return -1;
    }

    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sll_family = PF_PACKET;
    sock_addr.sll_protocol = htons(ETH_P_ALL);
    sock_addr.sll_ifindex = net_dev->ifindex;

    err = iface->raw_sock->ops->bind(iface->raw_sock, 
				     (struct sockaddr *)&sock_addr, 
				     sizeof(sock_addr));

    if (err < 0){
	printk(KERN_WARNING "Error binding raw packet to device %s, %d\n", eth_dev, err);
	sock_release(iface->raw_sock);
	
	return -1;
    }

    printk(KERN_INFO "Bind a palacios raw packet interface to device %s, device index %d\n",
	   eth_dev, net_dev->ifindex);

    return 0;
}


static inline int 
is_multicast_ethaddr(const unsigned char * addr)
{	
    return (0x01 & addr[0]);
}

static inline int 
is_broadcast_ethaddr(const unsigned char * addr)
{
    unsigned char ret = 0xff;
    int i;
	
    for(i=0; i<ETH_ALEN; i++) {
	ret &= addr[i];
    }
	
    return (ret == 0xff);
}

static int packet_recv_thread( void * arg ) {
    unsigned char * pkt;
    int size;
    struct v3_packet * recver_state;
    struct raw_interface * iface = (struct raw_interface *)arg;

    pkt = (unsigned char *)kmalloc(ETHERNET_PACKET_LEN, GFP_KERNEL);

    printk("Palacios Raw Packet Bridge: Staring receiving on ethernet device %s\n", 
	   iface->eth_dev);

    while (!kthread_should_stop()) {
	size = recv_pkt(iface->raw_sock, pkt, ETHERNET_PACKET_LEN);
	
	if (size < 0) {
	    printk(KERN_WARNING "Palacios raw packet receive error, Server terminated\n");
	    break;
	}

	if(is_broadcast_ethaddr(pkt)) {
	    /* Broadcast */

	    list_for_each_entry(recver_state, &(iface->brdcast_recvers), node) {
		recver_state->input(recver_state, pkt, size);
	    }
	    
	} else if(is_multicast_ethaddr(pkt)) {
	    /* MultiCast */

	} else {
	    recver_state = (struct v3_packet *)palacios_htable_search(iface->mac_to_recver,
								      (addr_t)pkt);
	    if(recver_state != NULL) {
		recver_state->input(recver_state, pkt, size);
	    }
	}
    }
    
    return 0;
}


static int 
init_raw_interface(struct raw_interface * iface, const char * eth_dev){

    memcpy(iface->eth_dev, eth_dev, V3_ETHINT_NAMELEN);	
    if(init_socket(iface, eth_dev) !=0) {
	printk("packet: fails to initiate raw socket\n");
	return -1;
    }
    
    
    INIT_LIST_HEAD(&(iface->brdcast_recvers));
    iface->mac_to_recver = palacios_create_htable(0, &hash_fn, &hash_eq);
    iface->recv_thread = kthread_run(packet_recv_thread, (void *)iface, "bridge-recver");
    
    iface->inited = 1;
    
    return 0;
}

static void inline 
deinit_raw_interface(struct raw_interface * iface){
    struct v3_packet * recver_state;

    kthread_stop(iface->recv_thread);
    sock_release(iface->raw_sock);
    palacios_free_htable(iface->mac_to_recver,  0,  0);
    
    list_for_each_entry(recver_state, &(iface->brdcast_recvers), node) {
	kfree(recver_state);
    }
}


static inline struct raw_interface * 
find_interface(const char * eth_dev) {
    struct raw_interface * iface;
    
    list_for_each_entry(iface, &(packet_state.open_interfaces), node) {
	if (strncmp(iface->eth_dev, eth_dev, V3_ETHINT_NAMELEN) == 0) {
	    return iface;
	}
    }

    return NULL;
}


static int
palacios_packet_connect(struct v3_packet * packet, 
			const char * host_nic, 
			void * host_vm_data) {
    struct raw_interface * iface;
    unsigned long flags;
    
    spin_lock_irqsave(&(packet_state.lock), flags);
    iface = find_interface(host_nic);
    spin_unlock_irqrestore(&(packet_state.lock),flags);

    if(iface == NULL){
	iface = (struct raw_interface *)kmalloc(sizeof(struct raw_interface), GFP_KERNEL);
	if (!iface) { 
	    printk("Palacios Packet Interface: Fails to allocate interface\n");
	    return -1;
	}
	if(init_raw_interface(iface, host_nic) != 0) {
	    printk("Palacios Packet Interface: Fails to initiate an raw interface on device %s\n", host_nic);
	    kfree(iface);
	    return -1;
	}
	spin_lock_irqsave(&(packet_state.lock), flags);	
	list_add(&(iface->node), &(packet_state.open_interfaces));
	spin_unlock_irqrestore(&(packet_state.lock),flags);
    }
    
    packet->host_packet_data = iface;
    
    list_add(&(packet->node), &(iface->brdcast_recvers));
    palacios_htable_insert(iface->mac_to_recver, 
			   (addr_t)packet->dev_mac, 
			   (addr_t)packet);

    printk(KERN_INFO "Packet: Add Receiver MAC to ethernet device %s: %2x:%2x:%2x:%2x:%2x:%2x\n", 
	   iface->eth_dev, 
	   packet->dev_mac[0], packet->dev_mac[1], 
	   packet->dev_mac[2], packet->dev_mac[3], 
	   packet->dev_mac[4], packet->dev_mac[5]);
    
    return 0;
}

static int
palacios_packet_send(struct v3_packet * packet, 
		     unsigned char * pkt, 
		     unsigned int len) {
    struct raw_interface * iface = (struct raw_interface *)packet->host_packet_data;
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int size = 0;
	
    if(iface->inited == 0 || 
       iface->raw_sock == NULL){
	printk("Palacios Packet Interface: Send fails due to inapproriate interface\n");
	
	return -1;
    }
	
    iov.iov_base = (void *)pkt;
    iov.iov_len = (__kernel_size_t)len;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_flags = 0;

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    size = sock_sendmsg(iface->raw_sock, &msg, len);
    set_fs(oldfs);

    return size;
}


static void
palacios_packet_close(struct v3_packet * packet) {
    struct raw_interface * iface = (struct raw_interface *)packet->host_packet_data;
    
    list_del(&(packet->node));
    palacios_htable_remove(iface->mac_to_recver, (addr_t)(packet->dev_mac), 0);
    
    packet->host_packet_data = NULL;
}


static struct v3_packet_hooks palacios_packet_hooks = {
    .connect = palacios_packet_connect,
    .send = palacios_packet_send,
    .close = palacios_packet_close,
};

static int packet_init( void ) {
    V3_Init_Packet(&palacios_packet_hooks);
    
    memset(&packet_state, 0, sizeof(struct palacios_packet_state));
    spin_lock_init(&(packet_state.lock));
    INIT_LIST_HEAD(&(packet_state.open_interfaces));
    
    // REGISTER GLOBAL CONTROL to add interfaces...

    return 0;
}

static int packet_deinit( void ) {
    struct raw_interface * iface;
    
    list_for_each_entry(iface, &(packet_state.open_interfaces), node) {
	deinit_raw_interface(iface);
	kfree(iface);
    }
    
    return 0;
}

static struct linux_ext pkt_ext = {
    .name = "PACKET_INTERFACE",
    .init = packet_init,
    .deinit = packet_deinit,
    .guest_init = NULL,
    .guest_deinit = NULL
};


register_extension(&pkt_ext);
