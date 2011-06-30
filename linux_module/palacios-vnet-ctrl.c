/* 
 * Palacios VNET Control Module
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

struct vnet_route_iter {
    struct v3_vnet_route route;
    uint32_t idx;

    struct list_head node;
};


struct vnet_link_iter {
    uint32_t dst_ip;
    uint16_t dst_port;
    vnet_brg_proto_t proto;
    uint32_t idx;

    struct list_head node;
};


struct vnet_ctrl_state {
    uint8_t status;

    uint32_t num_links;
    uint32_t num_routes;
	
    struct list_head route_list;
    struct list_head link_iter_list;

    spinlock_t lock;

    struct proc_dir_entry * vnet_proc_root;
};


static struct vnet_ctrl_state vnet_ctrl_s;


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


static inline struct vnet_link_iter * link_by_ip(uint32_t ip) {
    struct vnet_link_iter * link = NULL;

    list_for_each_entry(link, &(vnet_ctrl_s.link_iter_list), node) {

	if (link->dst_ip == ip) {
	    return link;
	}
    }
	
    return NULL;
}

static inline struct vnet_link_iter * link_by_idx(int idx) {
    struct vnet_link_iter * link = NULL;

    list_for_each_entry(link, &(vnet_ctrl_s.link_iter_list), node) {
	if (link->idx == idx) {
	    return link;
	}
    }
	
    return NULL;
}


static int parse_route_str(char * str, struct v3_vnet_route * route) {
    char * token = NULL;
    struct vnet_link_iter * link = NULL;

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
    printk("dst ID=(%s)\n", token);

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
	    route->dst_id = link->idx;
	}else{
	    printk("can not find dst link %s\n", token);
	    return -1;
	}

	printk("link_ip = %d, link_id = %d\n", link_ip, link->idx);	
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
	    route->src_id = link->idx;
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
    struct vnet_route_iter * route_iter = NULL;
    loff_t i = 0;

    if (*pos >= vnet_ctrl_s.num_routes) {
	return NULL;
    }

    list_for_each_entry(route_iter, &(vnet_ctrl_s.route_list), node) {
	if (i == *pos) {
	    break;
	}

	i++;
    }

    return route_iter;
}


static void * route_seq_next(struct seq_file * s, void * v, loff_t * pos) {
    struct vnet_route_iter * route_iter = NULL;

    route_iter = list_entry(((struct vnet_route_iter *)v)->node.next, struct vnet_route_iter, node);

    // Check if the list has looped
    if (&(route_iter->node) == &(vnet_ctrl_s.route_list)) {
	return NULL;
    }

    *pos += 1;

    return route_iter;
}

static void route_seq_stop(struct seq_file * s, void * v) {

    return;
}

static void * link_seq_start(struct seq_file * s, loff_t * pos) {
    struct vnet_link_iter * link_iter = NULL;
    loff_t i = 0;

    if (*pos >= vnet_ctrl_s.num_links) {
	return NULL;
    }

    list_for_each_entry(link_iter, &(vnet_ctrl_s.link_iter_list), node) {
	if (i == *pos) {
	    break;
	}

	i++;
    }

    return link_iter;
}

static int route_seq_show(struct seq_file * s, void * v) {
    struct vnet_route_iter * route_iter = v;
    struct v3_vnet_route * route = &(route_iter->route);

    seq_printf(s, "%d:\t", route_iter->idx);

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
	    struct vnet_link_iter * link = (struct vnet_link_iter *)link_by_idx(route->dst_id);
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
	    struct vnet_link_iter * link = (struct vnet_link_iter *)link_by_idx(route->src_id);
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

static void * link_seq_next(struct seq_file * s, void * v, loff_t * pos) {
    struct vnet_link_iter * link_iter = NULL;

    link_iter = list_entry(((struct vnet_link_iter *)v)->node.next, struct vnet_link_iter, node);

    // Check if the list has looped
    if (&(link_iter->node) == &(vnet_ctrl_s.link_iter_list)) {
	return NULL;
    }

    *pos += 1;

    return link_iter;
}

static void link_seq_stop(struct seq_file * s, void * v) {

    return;
}

static int link_seq_show(struct seq_file * s, void * v) {
    struct vnet_link_iter * link_iter = v;
    struct nic_statistics stats;

    vnet_brg_link_stats(link_iter->idx, &stats);

    seq_printf(s, "%d:\t%pI4\t%d\n\t\tReceived Pkts: %lld, Received Bytes %lld\n\t\tSent Pkts: %lld, Sent Bytes: %lld\n\n", 
	       link_iter->idx,
	       &link_iter->dst_ip,
	       link_iter->dst_port,
	       stats.rx_pkts,
	       stats.rx_bytes,
	       stats.tx_pkts,
	       stats.tx_bytes);

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



static int inject_route(struct vnet_route_iter * route) {
    unsigned long flags;
    
    route->idx = v3_vnet_add_route(route->route);

    spin_lock_irqsave(&(vnet_ctrl_s.lock), flags);
    list_add(&(route->node), &(vnet_ctrl_s.route_list));
    vnet_ctrl_s.num_routes ++;
    spin_unlock_irqrestore(&(vnet_ctrl_s.lock), flags);

    printk("VNET Control: One route added to VNET core\n");

    return 0;
}


static void delete_route(struct vnet_route_iter * route) {
    unsigned long flags;

    v3_vnet_del_route(route->idx);

    spin_lock_irqsave(&(vnet_ctrl_s.lock), flags);
    list_del(&(route->node));
    vnet_ctrl_s.num_routes --;
    spin_unlock_irqrestore(&(vnet_ctrl_s.lock), flags);

    printk("VNET Control: Route %d deleted from VNET\n", route->idx);

    kfree(route);
    route = NULL;
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
  *
  *
  * del route-idx
  *
  */
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

    route_buf[size] = '\0';
    printk("Route written: %s\n", route_buf);

    while ((buf_iter = strsep(&line_str, "\r\n"))) {

	token = strsep(&buf_iter, " ");
	if (!token) {
	    return -EFAULT;
	}
	
	if (strnicmp("ADD", token, strlen("ADD")) == 0) {
	    struct vnet_route_iter * new_route = NULL;
	    new_route = kmalloc(sizeof(struct vnet_route_iter), GFP_KERNEL);
	    
	    if (!new_route) {
		return -ENOMEM;
	    }
	    
	    memset(new_route, 0, sizeof(struct vnet_route_iter));
	    
	    if (parse_route_str(buf_iter, &(new_route->route)) == -1) {
		kfree(new_route);
		return -EFAULT;
	    }
	    
	    if (inject_route(new_route) != 0) {
		kfree(new_route);
		return -EFAULT;
	    }
	} else if (strnicmp("DEL", token, strlen("DEL")) == 0) {
	    char * idx_str = NULL;
	    uint32_t d_idx;
	    struct vnet_route_iter * route = NULL;

	    idx_str = strsep(&buf_iter, " ");
	    
	    if (!idx_str) {
		printk("Missing route idx in DEL Route command\n");
		return -EFAULT;
	    }

	    d_idx = simple_strtoul(idx_str, &idx_str, 10);

	    printk("VNET: deleting route %d\n", d_idx);

	    list_for_each_entry(route, &(vnet_ctrl_s.route_list), node) {
		if (route->idx == d_idx) {
		    delete_route(route);
	    	    break;
		}
    	    }
	} else {
	    printk("Invalid Route command string\n");
	}
    }

    return size;
}


static void delete_link(struct vnet_link_iter * link){
    unsigned long flags;

    vnet_brg_delete_link(link->idx);

    spin_lock_irqsave(&(vnet_ctrl_s.lock), flags);
    list_del(&(link->node));
    vnet_ctrl_s.num_links --;
    spin_unlock_irqrestore(&(vnet_ctrl_s.lock), flags);

    kfree(link);
    link = NULL;
}


static void deinit_links_list(void){
    struct vnet_link_iter * link;

    list_for_each_entry(link, &(vnet_ctrl_s.link_iter_list), node) {
     	delete_link(link);
    }
}

static void deinit_routes_list(void){
   struct vnet_route_iter * route;

   list_for_each_entry(route, &(vnet_ctrl_s.route_list), node) {
   	delete_route(route);
   }
}

/* ADD dst-ip 9000 [udp|tcp] */
/* DEL link-idx */
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
	    struct vnet_link_iter * link = NULL;
	    char * ip_str = NULL;
	    uint32_t d_ip;
	    uint16_t d_port;
	    vnet_brg_proto_t d_proto;
	    int link_idx;
	    unsigned long flags;
	    
	    ip_str = strsep(&link_iter, " ");
	    
	    if ((!ip_str) || (!link_iter)) {
		printk("Missing fields in ADD Link command\n");
		return -EFAULT;
	    }
	    
	    if (in4_pton(ip_str, strlen(ip_str), (uint8_t *)&(d_ip), '\0', NULL) != 1) {
		printk("Invalid Dst IP address (%s)\n", ip_str);
		return -EFAULT;
	    }

	    d_port = simple_strtol(link_iter, &link_iter, 10);
	    d_proto = UDP;

	    link_idx = vnet_brg_add_link(d_ip, d_port, d_proto);
	    if(link_idx < 0){
		printk("VNET Control: Failed to create link\n");
		return -EFAULT;
	    }

	    link = kmalloc(sizeof(struct vnet_link_iter), GFP_KERNEL);
	    memset(link, 0, sizeof(struct vnet_link_iter));

	    link->dst_ip = d_ip;
	    link->dst_port = d_port;
	    link->proto = d_proto;
	    link->idx = link_idx;

    	    spin_lock_irqsave(&(vnet_ctrl_s.lock), flags);
    	    list_add(&(link->node), &(vnet_ctrl_s.link_iter_list));
    	    vnet_ctrl_s.num_links ++;
    	    spin_unlock_irqrestore(&(vnet_ctrl_s.lock), flags);
	} else if (strnicmp("DEL", token, strlen("DEL")) == 0) {
	    char * idx_str = NULL;
	    uint32_t d_idx;
	    
	    idx_str = strsep(&link_iter, " ");
	    
	    if (!idx_str) {
		printk("Missing link idx in DEL Link command\n");
		return -EFAULT;
	    }

	    d_idx = simple_strtoul(idx_str, &idx_str, 10);

	    vnet_brg_delete_link(d_idx);
		
	    printk("VNET Control: One link deleted\n");		
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


static ssize_t 
debug_write(struct file * file, const char * buf, size_t size, loff_t * ppos) {
    char in_buf[32];
    char * in_iter = NULL;
    char * line_str = in_buf;
    int level = -1; 

    if (size >= 32) {
	return -EFAULT;
    }

    if (copy_from_user(in_buf, buf, size)) {
	return -EFAULT;
    }

    in_iter = strsep(&line_str, "\r\n");
    level = simple_strtol(in_iter, &in_iter, 10);

    printk("VNET Control: Set VNET Debug level to %d\n", level);

    if(level >= 0){
	net_debug = level;
    }

    return size;
}


static int debug_show(struct seq_file * file, void * v){
    seq_printf(file, "Current NET Debug Level: %d\n", net_debug);
	
    return 0;
}

static int debug_open(struct inode * inode, struct file * file) {
    return single_open(file, debug_show, NULL);
}

static struct file_operations debug_fops = {
    .owner = THIS_MODULE,
    .open = debug_open, 
    .read = seq_read,
    .write = debug_write,
    .llseek = seq_lseek,
    .release = seq_release
};

static int stat_show(struct seq_file * file, void * v){
    struct vnet_stat stats;
    struct vnet_brg_stats brg_stats;

    v3_vnet_stat(&stats);

    seq_printf(file, "VNET Core\n");
    seq_printf(file, "\tReceived Packets: %d\n", stats.rx_pkts);
    seq_printf(file, "\tReceived Bytes: %lld\n", stats.rx_bytes);
    seq_printf(file, "\tTransmitted Packets: %d\n", stats.tx_pkts);
    seq_printf(file, "\tTransmitted Bytes: %lld\n", stats.tx_bytes);

    vnet_brg_stats(&brg_stats);
   
    seq_printf(file, "\nVNET Bridge Server\n");
    seq_printf(file, "\tReceived From VMM: %lld\n", brg_stats.pkt_from_vmm);
    seq_printf(file, "\tSent To VMM: %lld\n", brg_stats.pkt_to_vmm);
    seq_printf(file, "\tDropped From VMM: %lld\n", brg_stats.pkt_drop_vmm);
    seq_printf(file, "\tReceived From Extern Network: %lld\n", brg_stats.pkt_from_phy);
    seq_printf(file, "\tSent To Extern Network: %lld\n", brg_stats.pkt_to_phy);
    seq_printf(file, "\tDropped From Extern Network: %lld\n", brg_stats.pkt_drop_phy);

    return 0;
}

static int stat_open(struct inode * inode, struct file * file) {
    return single_open(file, stat_show, NULL);
}

static struct file_operations stat_fops = {
    .owner = THIS_MODULE,
    .open = stat_open, 
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release
};


static int init_proc_files(void) {
    struct proc_dir_entry * route_entry = NULL;
    struct proc_dir_entry * link_entry = NULL;
    struct proc_dir_entry * stat_entry = NULL;
    struct proc_dir_entry * debug_entry = NULL;
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
	

    stat_entry = create_proc_entry("stats", 0, vnet_root);
    if(stat_entry == NULL) {
	remove_proc_entry("links", vnet_root);
	remove_proc_entry("routes", vnet_root);
	remove_proc_entry("vnet", NULL);
	return -1;
    }
    stat_entry->proc_fops = &stat_fops;


    debug_entry = create_proc_entry("debug", 0, vnet_root);
    if(debug_entry == NULL) {
	remove_proc_entry("links", vnet_root);
	remove_proc_entry("routes", vnet_root);
	remove_proc_entry("stats", vnet_root);
	remove_proc_entry("vnet", NULL);
	return -1;
    }
    debug_entry->proc_fops = &debug_fops;

    vnet_ctrl_s.vnet_proc_root = vnet_root;

    return 0;
}


static void destroy_proc_files(void) {
    struct proc_dir_entry * vnet_root = vnet_ctrl_s.vnet_proc_root;

    remove_proc_entry("debug", vnet_root);
    remove_proc_entry("links", vnet_root);
    remove_proc_entry("routes", vnet_root);
    remove_proc_entry("stats", vnet_root);
    remove_proc_entry("vnet", NULL);	
}


int vnet_ctrl_init(void) {
    if(vnet_ctrl_s.status != 0) {
	return -1;
    }	
    vnet_ctrl_s.status = 1;	
	
    memset(&vnet_ctrl_s, 0, sizeof(struct vnet_ctrl_state));

    INIT_LIST_HEAD(&(vnet_ctrl_s.link_iter_list));
    INIT_LIST_HEAD(&(vnet_ctrl_s.route_list));
    spin_lock_init(&(vnet_ctrl_s.lock));

    init_proc_files();
	
    printk("VNET Linux control module initiated\n");

    return 0;
}


void vnet_ctrl_deinit(void){
    destroy_proc_files();

    deinit_links_list();
    deinit_routes_list();

    vnet_ctrl_s.status = 0;
}


