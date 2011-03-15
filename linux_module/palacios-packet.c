/* 
 * VM Raw Packet 
 * (c) Lei Xia, 2010
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
 
#include <palacios/vmm_packet.h>
#include <palacios/vmm_host_events.h>
#include <palacios/vmm_vnet.h>

#include "palacios.h"
#include "palacios-packet.h"

//#define DEBUG_PALACIOS_PACKET

static struct socket * raw_sock;

static int packet_inited = 0;

static int init_raw_socket (const char * eth_dev){
    int err;
    struct sockaddr_ll sock_addr;
    struct ifreq if_req;
    int dev_idx;

    err = sock_create(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL), &raw_sock); 
    if (err < 0) {
	printk(KERN_WARNING "Could not create a PF_PACKET Socket, err %d\n", err);
	return -1;
    }

    if(eth_dev == NULL){
	return 0;
    }

    memset(&if_req, 0, sizeof(if_req));
    strncpy(if_req.ifr_name, eth_dev, sizeof(if_req.ifr_name));

    err = raw_sock->ops->ioctl(raw_sock, SIOCGIFINDEX, (long)&if_req);
    if(err < 0){
	printk(KERN_WARNING "Palacios Packet: Unable to get index for device %s, error %d\n", if_req.ifr_name, err);
	dev_idx = 2; /* default "eth0" */
    }
    else{
	dev_idx = if_req.ifr_ifindex;
    }

    printk(KERN_INFO "Palacios Packet: bind to device index: %d\n", dev_idx);

    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sll_family = PF_PACKET;
    sock_addr.sll_protocol = htons(ETH_P_ALL);
    sock_addr.sll_ifindex = dev_idx;

    err = raw_sock->ops->bind(raw_sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
    if (err < 0){
	printk(KERN_WARNING "Error binding raw packet to device %s, %d\n", eth_dev, err);
	return -1;
    }

    printk(KERN_INFO "Bind palacios raw packet interface to device %s\n", eth_dev);

    return 0;
}


static int
palacios_packet_send(const char * pkt, unsigned int len, void * private_data) {
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int size = 0;

#ifdef DEBUG_PALACIOS_PACKET
    {
    	printk("Palacios Packet: send pkt to NIC (size: %d)\n", 
			len);
	//print_hex_dump(NULL, "pkt_data: ", 0, 20, 20, pkt, len, 0);
    }
#endif
	

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
    size = sock_sendmsg(raw_sock, &msg, len);
    set_fs(oldfs);

    return size;
}


static struct v3_packet_hooks palacios_packet_hooks = {
    .send	= palacios_packet_send,
};


static int 
recv_pkt(char * pkt, int len) {
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int size = 0;
    
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


void
send_raw_packet_to_palacios(char * pkt,
			       int len,
			       struct v3_vm_info * vm) {
    struct v3_packet_event event;

    event.pkt = kmalloc(len, GFP_KERNEL);
    memcpy(event.pkt, pkt, len);
    event.size = len;
	
    v3_deliver_packet_event(vm, &event);
}

static int packet_server(void * arg) {
    char pkt[ETHERNET_PACKET_LEN];
    int size;

    printk("Palacios Raw Packet Bridge: Staring receiving server\n");

    while (!kthread_should_stop()) {
	size = recv_pkt(pkt, ETHERNET_PACKET_LEN);
	if (size < 0) {
	    printk(KERN_WARNING "Palacios Packet Socket receive error\n");
	    break;
	}

#ifdef DEBUG_PALACIOS_PACKET
    {
    	printk("Palacios Packet: receive pkt from NIC (size: %d)\n", 
			size);
	//print_hex_dump(NULL, "pkt_data: ", 0, 20, 20, pkt, size, 0);
    }
#endif

	send_raw_packet_to_palacios(pkt, size, NULL);
    }

    return 0;
}


int palacios_init_packet(const char * eth_dev) {

    if(packet_inited == 0){
	packet_inited = 1;
	init_raw_socket(eth_dev);
	V3_Init_Packet(&palacios_packet_hooks);

	kthread_run(packet_server, NULL, "raw-packet-server");
    }
	
    return 0;
}

