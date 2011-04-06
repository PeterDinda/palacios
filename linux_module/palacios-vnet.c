/* 
   Palacios VNET interface
   (c) Lei Xia, 2010
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
#include <linux/string.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <asm/msr.h>

#include <palacios/vmm_vnet.h>
#include "palacios-vnet.h"

//#define DEBUG_VNET_BRIGE

#define VNET_UDP_PORT 9000

struct palacios_vnet_route {
    struct v3_vnet_route route;

    int route_idx;

    struct list_head node;
};


struct vnet_link {
    uint32_t dst_ip;
    uint16_t dst_port;
    
    struct socket * sock;
    struct sockaddr_in sock_addr;

    int link_idx;

    struct list_head node;
};

struct palacios_vnet_state {
    uint32_t num_routes;
    uint32_t num_links; 

    struct list_head route_list;
    struct list_head link_list;

    struct socket * serv_sock;
    struct sockaddr_in serv_addr;

    /* The thread recving pkts from sockets. */
    struct task_struct * serv_thread;
    spinlock_t lock;

    unsigned long pkt_sent, pkt_recv, pkt_drop, pkt_udp_recv, pkt_udp_send;
};


static struct palacios_vnet_state vnet_state;


struct vnet_link * link_by_ip(uint32_t ip) {
    struct vnet_link * link = NULL;

    list_for_each_entry(link, &(vnet_state.link_list), node) {

	if (link->dst_ip == ip) {
	    return link;
	}
    }

    return NULL;
}

struct vnet_link * link_by_idx(int idx) {
    struct vnet_link * link = NULL;

    list_for_each_entry(link, &(vnet_state.link_list), node) {

	if (link->link_idx == idx) {
	    return link;
	}
    }
    return NULL;
}

struct palacios_vnet_route * route_by_idx(int idx) {
    struct palacios_vnet_route * route = NULL;

    list_for_each_entry(route, &(vnet_state.route_list), node) {

	if (route->route_idx == idx) {
	    return route;
	}
    }

    return NULL;
}


static int parse_mac_str(char * str, uint8_t * qual, uint8_t * mac) {
    char * token;

    printk("Parsing MAC (%s)\n", str);
	
    *qual = MAC_NOSET;
    if(strnicmp("any", str, strlen(str)) == 0){
	*qual = MAC_ANY;
	return 0;
    }else if(strnicmp("none", str, strlen(str)) == 0){
       *qual = MAC_NONE;
	return 0;
    }else{
    	if (strstr(str, "-")) {
	    token = strsep(&str, "-");

	    if (strnicmp("not", token, strlen("not")) == 0) {
	    	*qual = MAC_NOT;
	    } else {
	    	printk("Invalid MAC String token (%s)\n", token);
	    	return -1;
	    }
    	}

    	if (strstr(str, ":")) {
	    int i = 0;

	    if(*qual == MAC_NOSET){
	   	*qual = MAC_ADDR;
	    }

	    for (i = 0; i < 6; i++) {
	    	token = strsep(&str, ":");
	    	if (!token) {
		    printk("Invalid MAC String token (%s)\n", token);
		    return -1;
   		}
	    	mac[i] = simple_strtol(token, &token, 16);
	    }
           printk("MAC: %2x:%2x:%2x:%2x:%2x:%2x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		
    	}else {
	    printk("Invalid MAC String token (%s)\n", token);
	    return -1;
	}
    		
    }

    return 0;
}


static int str2mac(char * str, uint8_t * mac){
    int i = 0;
    char *hex = NULL;
	
    for (i = 0; i < ETH_ALEN; i++) {		
	hex = strsep(&str, ":");
	if (!hex) {
	    printk("Invalid MAC String token (%s)\n", str);
	    return -1;
	}
	mac[i] = simple_strtol(hex, &hex, 16);
    }
	
    return 0;
}


/* Format:
  * add src-MAC dst-MAC dst-TYPE [dst-ID] src-TYPE [src-ID]
  *
  * src-MAC = dst-MAC = not-MAC|any|none|MAC 
  * dst-TYPE = edge|interface 
  * src-TYPE = edge|interface|any
  * dst-ID = src-ID = IP|MAC
  * MAC=xx:xx:xx:xx:xx:xx
  * IP = xxx.xxx.xxx.xxx
  */
static int parse_route_str(char * str, struct v3_vnet_route * route) {
    char * token = NULL;
    struct vnet_link *link = NULL;

    // src MAC
    token = strsep(&str, " ");
    if (!token) {
	return -1;
    }
    parse_mac_str(token, &(route->src_mac_qual), route->src_mac);

    // dst MAC
    token = strsep(&str, " ");
    if (!token) {
	return -1;
    }
    parse_mac_str(token, &(route->dst_mac_qual), route->dst_mac);

    // dst LINK type
    token = strsep(&str, " ");
    if (!token) {
	return -1;
    }
    printk("dst type =(%s)\n", token);
    
    if (strnicmp("interface", token, strlen("interface")) == 0) {
	route->dst_type = LINK_INTERFACE;
    } else if (strnicmp("edge", token, strlen("edge")) == 0) {
	route->dst_type = LINK_EDGE;
    } else {
	printk("Invalid Destination Link Type (%s)\n", token);
	return -1;
    }

    // dst link
    token = strsep(&str, " ");
    if (!token) {
	return -1;
    }
    printk("dst link ID=(%s)\n", token);

    // Figure out link here
    if (route->dst_type == LINK_EDGE) {
	uint32_t link_ip;

	// Figure out Link Here
	if (in4_pton(token, strlen(token), (uint8_t *)&(link_ip), '\0', NULL) != 1) {
	    printk("Invalid Dst IP address (%s)\n", token);
	    return -EFAULT;
	}

	link = link_by_ip(link_ip);
	if (link != NULL){
	    route->dst_id = link->link_idx;
	}else{
	    printk("can not find dst link %s\n", token);
	    return -1;
	}

	printk("link_ip = %d, link_id = %d\n", link_ip, link->link_idx);	
    } else if (route->dst_type == LINK_INTERFACE) {
	uint8_t mac[ETH_ALEN];
	
       if(str2mac(token, mac) == -1){
	   printk("wrong MAC format (%s)\n", token);
	   return -1;
       }
	   
	route->dst_id = v3_vnet_find_dev(mac);
	if (route->dst_id == -1){
	    printk("can not find dst device %s\n", token);
	    return -1;
	}		
    } else {
	printk("Unsupported dst link type\n");
	return -1;
    }

    route->src_id = -1;
    route->src_type = -1;

    // src LINK
    token = strsep(&str, " ");

    printk("SRC type = %s\n", token);

    if (!token) {
	return -1;
    }

    if (strnicmp("interface", token, strlen("interface")) == 0) {
	route->src_type = LINK_INTERFACE;
    } else if (strnicmp("edge", token, strlen("edge")) == 0) {
	route->src_type = LINK_EDGE;
    } else if (strnicmp("any", token, strlen("any")) == 0) {
	route->src_type = LINK_ANY;
    } else {
	printk("Invalid Src link type (%s)\n", token);
	return -1;
    }


    if (route->src_type == LINK_ANY) {
	route->src_id = -1;
    } else if (route->src_type == LINK_EDGE) {
	uint32_t src_ip;
	token = strsep(&str, " ");

	if (!token) {
	    return -1;
	}

	// Figure out Link Here
	if (in4_pton(token, strlen(token), (uint8_t *)&(src_ip), '\0', NULL) != 1) {
	    printk("Invalid SRC IP address (%s)\n", token);
	    return -EFAULT;
	}

	link = link_by_ip(src_ip);
	if (link != NULL){
	    route->src_id = link->link_idx;
	}else{
	    printk("can not find src link %s\n", token);
	    return -1;
	}
    } else if(route->src_type == LINK_INTERFACE){
       uint8_t mac[ETH_ALEN];
	
       if(str2mac(token, mac) == -1){
	   printk("wrong MAC format (%s)\n", token);
	   return -1;
       }
	   
	route->src_id = v3_vnet_find_dev(mac);
	if (route->src_id == -1){
	    printk("can not find dst device %s\n", token);
	    return -1;
	}		
    } else {
	printk("Invalid link type\n");
	return -1;
    }

    return 0;
}




static void * route_seq_start(struct seq_file * s, loff_t * pos) {
    struct palacios_vnet_route * route_iter = NULL;
    loff_t i = 0;


    if (*pos >= vnet_state.num_routes) {
	return NULL;
    }

    list_for_each_entry(route_iter, &(vnet_state.route_list), node) {

	if (i == *pos) {
	    break;
	}

	i++;
    }

    return route_iter;
}


static void * link_seq_start(struct seq_file * s, loff_t * pos) {
    struct vnet_link * link_iter = NULL;
    loff_t i = 0;

    if (*pos >= vnet_state.num_links) {
	return NULL;
    }

    list_for_each_entry(link_iter, &(vnet_state.link_list), node) {

	if (i == *pos) {
	    break;
	}

	i++;
    }

    return link_iter;
}



static void * route_seq_next(struct seq_file * s, void * v, loff_t * pos) {
    struct palacios_vnet_route * route_iter = NULL;

    route_iter = list_entry(((struct palacios_vnet_route *)v)->node.next, struct palacios_vnet_route, node);

    // Check if the list has looped
    if (&(route_iter->node) == &(vnet_state.route_list)) {
	return NULL;
    }

    *pos += 1;

    return route_iter;
}


static void * link_seq_next(struct seq_file * s, void * v, loff_t * pos) {
    struct vnet_link * link_iter = NULL;

 
    link_iter = list_entry(((struct vnet_link *)v)->node.next, struct vnet_link, node);

    // Check if the list has looped
    if (&(link_iter->node) == &(vnet_state.link_list)) {
	return NULL;
    }

    *pos += 1;

    return link_iter;
}


static void route_seq_stop(struct seq_file * s, void * v) {
    printk("route_seq_stop\n");

    return;
}


static void link_seq_stop(struct seq_file * s, void * v) {
    printk("link_seq_stop\n");

    return;
}

static int route_seq_show(struct seq_file * s, void * v) {
    struct palacios_vnet_route * route_iter = v;
    struct v3_vnet_route * route = &(route_iter->route);

    seq_printf(s, "%d:\t", route_iter->route_idx);

    seq_printf(s, "\nSrc:\t");
    switch (route->src_mac_qual) {
	case MAC_ANY:
	    seq_printf(s, "any ");
	    break;
	case MAC_NONE:
	    seq_printf(s, "none ");
	    break;
	case MAC_NOT:
	    seq_printf(s, "not-%2x:%2x:%2x:%2x:%2x:%2x ", 
		       route->src_mac[0], route->src_mac[1], route->src_mac[2],
		       route->src_mac[3], route->src_mac[4], route->src_mac[5]);
	    break;
	default:
	    seq_printf(s, "%x:%x:%x:%x:%x:%x ", 
		       route->src_mac[0], route->src_mac[1], route->src_mac[2],
		       route->src_mac[3], route->src_mac[4], route->src_mac[5]);
	    break;
    }

    seq_printf(s, "\nDst:\t");
    switch (route->dst_mac_qual) {
	case MAC_ANY:
	    seq_printf(s, "any ");
	    break;
	case MAC_NONE:
	    seq_printf(s, "none ");
	    break;
	case MAC_NOT:
	    seq_printf(s, "not-%x:%x:%x:%x:%x:%x ", 
		       route->src_mac[0], route->src_mac[1], route->src_mac[2],
		       route->src_mac[3], route->src_mac[4], route->src_mac[5]);
	    break;
	default:
	    seq_printf(s, "%x:%x:%x:%x:%x:%x ", 
		       route->src_mac[0], route->src_mac[1], route->src_mac[2],
		       route->src_mac[3], route->src_mac[4], route->src_mac[5]);
	    break;
    }

    seq_printf(s, "\nDst-Type:\t");
    switch (route->dst_type) {
	case LINK_EDGE: {
	    struct vnet_link * link = (struct vnet_link *)link_by_idx(route->dst_id);
	    seq_printf(s, "EDGE %pI4", &link->dst_ip);
	    break;
	}
	case LINK_INTERFACE: {
	    seq_printf(s, "INTERFACE ");
	    seq_printf(s, "%d ", route->dst_id);
	    break;
	}
	default:
	    seq_printf(s, "Invalid Dst Link Type (%d) ", route->dst_type);
	    break;
    }

    seq_printf(s, "\nSrc-Type:\t");
    switch (route->src_type) {
	case LINK_EDGE: {
	    struct vnet_link * link = (struct vnet_link *)link_by_idx(route->src_id);
	    seq_printf(s, "EDGE %pI4", &link->dst_ip);
	    break;
	}
	case LINK_INTERFACE: {
	    seq_printf(s, "INTERFACE %d", route->src_id);
	    break;
	}
	case LINK_ANY:
	    seq_printf(s, "ANY");
	    break;
	default:
	    seq_printf(s, "Invalid Src Link Type (%d) ", route->src_type);
	    break;
    }


    seq_printf(s, "\n");

    return 0;
}


static int link_seq_show(struct seq_file * s, void * v) {
    struct vnet_link * link_iter = v;

    seq_printf(s, "%d:\t%pI4\t%d\n", 
	       link_iter->link_idx,
	       &link_iter->dst_ip,
	       link_iter->dst_port);

    return 0;
}


static struct seq_operations route_seq_ops = {
    .start = route_seq_start, 
    .next = route_seq_next,
    .stop = route_seq_stop,
    .show = route_seq_show
};


static struct seq_operations link_seq_ops = {
    .start = link_seq_start,
    .next = link_seq_next,
    .stop = link_seq_stop,
    .show = link_seq_show
};


static int route_open(struct inode * inode, struct file * file) {
    return seq_open(file, &route_seq_ops);
}


static int link_open(struct inode * inode, struct file * file) {
    return seq_open(file, &link_seq_ops);
}

static int inject_route(struct palacios_vnet_route * route) {
    unsigned long flags;

    v3_vnet_add_route(route->route);

    spin_lock_irqsave(&(vnet_state.lock), flags);
    list_add(&(route->node), &(vnet_state.route_list));
    route->route_idx = vnet_state.num_routes++;
    spin_unlock_irqrestore(&(vnet_state.lock), flags);

    printk("Palacios-vnet: One route added to VNET core\n");

    return 0;
}

static ssize_t 
route_write(struct file * file, 
	    const char * buf, 
	    size_t size, 
	    loff_t * ppos) {
    char route_buf[256];
    char * buf_iter = NULL;
    char * line_str = route_buf;
    char * token = NULL;

    if (size >= 256) {
	return -EFAULT;
    }

    if (copy_from_user(route_buf, buf, size)) {
	return -EFAULT;
    }

    printk("Route written: %s\n", route_buf);

    while ((buf_iter = strsep(&line_str, "\r\n"))) {

	token = strsep(&buf_iter, " ");
	if (!token) {
	    return -EFAULT;
	}
	
	if (strnicmp("ADD", token, strlen("ADD")) == 0) {
	    struct palacios_vnet_route * new_route = NULL;
	    new_route = kmalloc(sizeof(struct palacios_vnet_route), GFP_KERNEL);
	    
	    if (!new_route) {
		return -ENOMEM;
	    }
	    
	    memset(new_route, 0, sizeof(struct palacios_vnet_route));
	    
	    if (parse_route_str(buf_iter, &(new_route->route)) == -1) {
		kfree(new_route);
		return -EFAULT;
	    }
	    
	    if (inject_route(new_route) != 0) {
		return -EFAULT;
	    }
	} else if (strnicmp("DEL", token, strlen("DEL")) == 0) {
	    printk("I should delete the route here\n");
	} else {
	    printk("Invalid Route command string\n");
	}
    }

    return size;
}


static int create_link(struct vnet_link * link) {
    int err;
    unsigned long flags;

    if ( (err = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &link->sock)) < 0) {
	printk("Could not create socket\n");
	return -1;
    }

    memset(&link->sock_addr, 0, sizeof(struct sockaddr));

    link->sock_addr.sin_family = AF_INET;
    link->sock_addr.sin_addr.s_addr = link->dst_ip;
    link->sock_addr.sin_port = htons(link->dst_port);

    if ((err = link->sock->ops->connect(link->sock, (struct sockaddr *)&(link->sock_addr), sizeof(struct sockaddr), 0) < 0)) {
	printk("Could not connect to remote host\n");
	return -1;
    }

    // We use the file pointer because we are in the kernel
    // This is only used to assigned File Descriptors for user space, so it is available here
    // link->sock->file = link;

    spin_lock_irqsave(&(vnet_state.lock), flags);
    list_add(&(link->node), &(vnet_state.link_list));
    link->link_idx = vnet_state.num_links++;
    spin_unlock_irqrestore(&(vnet_state.lock), flags);

    printk("VNET Bridge: Link created, ip %d, port: %d, idx: %d, link: %p\n", 
	   link->dst_ip, 
	   link->dst_port, 
	   link->link_idx, 
	   link);

    return 0;
}


/* ADD dst-ip 9000 */
static ssize_t 
link_write(struct file * file, const char * buf, size_t size, loff_t * ppos) {
    char link_buf[256];
    char * link_iter = NULL;
    char * line_str = link_buf;
    char * token = NULL;

    if (size >= 256) {
	return -EFAULT;
    }

    if (copy_from_user(link_buf, buf, size)) {
	return -EFAULT;
    }

    while ((link_iter = strsep(&line_str, "\r\n"))) {
	printk("Link written: %s\n", link_buf);
	
	token = strsep(&link_iter, " ");
	
	if (!token) {
	    return -EFAULT;
	}
	
	if (strnicmp("ADD", token, strlen("ADD")) == 0) {
	    struct vnet_link * new_link = NULL;
	    char * ip_str = NULL;
	    uint32_t ip;
	    
	    ip_str = strsep(&link_iter, " ");
	    
	    if ((!ip_str) || (!link_iter)) {
		printk("Missing fields in ADD Link command\n");
		return -EFAULT;
	    }
	    
	    if (in4_pton(ip_str, strlen(ip_str), (uint8_t *)&(ip), '\0', NULL) != 1) {
		printk("Invalid Dst IP address (%s)\n", ip_str);
		return -EFAULT;
	    }

	    new_link = kmalloc(sizeof(struct vnet_link), GFP_KERNEL);

	    if (!new_link) {
		return -ENOMEM;
	    }

	    memset(new_link, 0, sizeof(struct vnet_link));

	    new_link->dst_ip = ip;
	    new_link->dst_port = simple_strtol(link_iter, &link_iter, 10);
	    
	    if (create_link(new_link) != 0) {
		printk("Could not create link\n");
		kfree(new_link);
		return -EFAULT;
	    }

	} else if (strnicmp("DEL", token, strlen("DEL")) == 0) {
	    printk("Link deletion not supported\n");
	} else {
	    printk("Invalid Link command string\n");
	}
    }

    return size;
}


static struct file_operations route_fops = {
    .owner = THIS_MODULE,
    .open = route_open, 
    .read = seq_read,
    .write = route_write,
    .llseek = seq_lseek,
    .release = seq_release
};


static struct file_operations link_fops = {
    .owner = THIS_MODULE,
    .open = link_open, 
    .read = seq_read,
    .write = link_write,
    .llseek = seq_lseek,
    .release = seq_release
};


static int init_proc_files(void) {
    struct proc_dir_entry * route_entry = NULL;
    struct proc_dir_entry * link_entry = NULL;
    struct proc_dir_entry * vnet_root = NULL;


    vnet_root = proc_mkdir("vnet", NULL);
    if (vnet_root == NULL) {
	return -1;
    }

    route_entry = create_proc_entry("routes", 0, vnet_root);

    if (route_entry == NULL) {
	remove_proc_entry("vnet", NULL);
	return -1;
    }

    route_entry->proc_fops = &route_fops;


    link_entry = create_proc_entry("links", 0, vnet_root);

    if (link_entry == NULL) {
	remove_proc_entry("routes", vnet_root);
	remove_proc_entry("vnet", NULL);
	return -1;
    }

    link_entry->proc_fops = &link_fops;

    return 0;

}



static int 
udp_send(struct socket * sock, 
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
udp_recv(struct socket * sock, 
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

//send packets from Network to VNET core
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

#ifdef CONFIG_PALACIOS_VNET_DEBUG
    {
    	printk("VNET Lnx Bridge: send pkt to VNET core (size: %d, src_id: %d, src_type: %d)\n", 
			pkt.size,  pkt.src_id, pkt.src_type);

	print_hex_dump(NULL, "pkt_data: ", 0, 20, 20, pkt.data, pkt.size, 0);
    }
#endif

    return v3_vnet_send_pkt(&pkt, NULL);;
}


//send packet from VNET core to Network
static int 
bridge_send_pkt(struct v3_vm_info * vm, 
		struct v3_vnet_pkt * pkt, 
		void * private_data) {
    struct vnet_link * link;

    #ifdef CONFIG_PALACIOS_VNET_DEBUG
    	   {
    	    	printk("VNET Lnx Host Bridge: packet received from VNET Core ... len: %d, pkt size: %d, link: %d\n",
    	    		len,
			pkt->size,
			pkt->dst_id);

    	    	print_hex_dump(NULL, "pkt_data: ", 0, 20, 20, pkt->data, pkt->size, 0);
    	   }
    #endif

    vnet_state.pkt_recv ++;

    link = link_by_idx(pkt->dst_id);
    if (link != NULL) {
 	udp_send(link->sock, &(link->sock_addr), pkt->data, pkt->size);
	vnet_state.pkt_udp_send ++;
    } else {
	printk("VNET Bridge Linux Host: wrong dst link, idx: %d, discards the packet\n", pkt->dst_id);
	vnet_state.pkt_drop ++;
    }

    return 0;
}


static void 
poll_pkt(struct v3_vm_info * vm, 
	 void * private_data) {


}



static int init_vnet_serv(void) {
     
    if (sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &vnet_state.serv_sock) < 0) {
	printk("Could not create socket\n");
	return -1;
    }

    memset(&vnet_state.serv_addr, 0, sizeof(struct sockaddr));

    vnet_state.serv_addr.sin_family = AF_INET;
    vnet_state.serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    vnet_state.serv_addr.sin_port = htons(VNET_UDP_PORT);

    if (vnet_state.serv_sock->ops->bind(vnet_state.serv_sock, (struct sockaddr *)&(vnet_state.serv_addr), sizeof(struct sockaddr)) < 0) {
	printk("Could not bind VNET server socket to port %d\n", VNET_UDP_PORT);
	return -1;
    }

    printk("VNET server bind to port: %d\n", VNET_UDP_PORT);

    return 0;
}

static int vnet_server(void * arg) {
    unsigned char pkt[ETHERNET_PACKET_LEN];
    struct sockaddr_in pkt_addr;
    struct vnet_link *link = NULL;
    int len;
    int link_id;

    printk("Palacios VNET Bridge: UDP receiving server ..... \n");

    while (!kthread_should_stop()) {
	
	len = udp_recv(vnet_state.serv_sock, &pkt_addr, pkt, ETHERNET_PACKET_LEN); 
	if(len < 0) {
	    printk("Receive error: Could not get packet, error %d\n", len);
	    continue;
	}

	link = link_by_ip(ntohl(pkt_addr.sin_addr.s_addr));
	if (link != NULL){
	    link_id= link->link_idx;
	}
	else { 
	    link_id= 0;
	}
	
	vnet_state.pkt_udp_recv ++;

	send_to_palacios(pkt, len, link_id);
    }

    return 0;
}


int  palacios_init_vnet(void) {
    struct v3_vnet_bridge_ops bridge_ops;
	
    memset(&vnet_state, 0, sizeof(struct palacios_vnet_state));

    INIT_LIST_HEAD(&(vnet_state.link_list));
    INIT_LIST_HEAD(&(vnet_state.route_list));
    spin_lock_init(&(vnet_state.lock));

    init_proc_files();
    if(init_vnet_serv() < 0){
	printk("Failure to initiate VNET server\n");
	return -1;
    }

    vnet_state.serv_thread = kthread_run(vnet_server, NULL, "vnet-server");

    //kthread_run(profiling, NULL, "Profiling");

    bridge_ops.input = bridge_send_pkt;
    bridge_ops.poll = poll_pkt;
	
    v3_vnet_add_bridge(NULL, &bridge_ops, HOST_LNX_BRIDGE, NULL);

    printk("Palacios VNET Linux Bridge initiated\n");

    return 0;
}

