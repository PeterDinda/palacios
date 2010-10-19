/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Lei Xia <lxia@northwestern.edu> 
 * Copyright (c) 2009, Yuan Tang <ytang@northwestern.edu>  
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu>
 *	   Yuan Tang <ytang@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
 
#include <palacios/vmm_vnet.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_lock.h>
#include <palacios/vmm_queue.h>
#include <palacios/vmm_sprintf.h>

#ifndef CONFIG_DEBUG_VNET
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif



struct eth_hdr {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t type; // indicates layer 3 protocol type
} __attribute__((packed));





struct vnet_dev {

    uint8_t mac_addr[6];
    struct v3_vm_info * vm;
    
    int (*input)(struct v3_vm_info * vm, struct v3_vnet_pkt * pkt, void * private_data);
    void * private_data;
    
    int dev_id;
    struct list_head node;
} __attribute__((packed));


#define BRIDGE_BUF_SIZE 512
struct bridge_pkts_buf {
    int start, end;
    int num; 
    v3_lock_t lock;
    struct v3_vnet_pkt pkts[BRIDGE_BUF_SIZE];
    uint8_t datas[ETHERNET_PACKET_LEN * BRIDGE_BUF_SIZE];
};

struct vnet_brg_dev {
    struct v3_vm_info * vm;
    
    int (*input)(struct v3_vm_info * vm, struct v3_vnet_pkt pkt[], uint16_t pkt_num, void * private_data);
    void (*xcall_input)(void * data);
    int (*polling_pkt)(struct v3_vm_info * vm,  void * private_data);

    int disabled;
	
    uint16_t max_delayed_pkts;
    long max_latency; //in cycles
    void * private_data;
} __attribute__((packed));





struct vnet_route_info {
    struct v3_vnet_route route_def;

    struct vnet_dev * dst_dev;
    struct vnet_dev * src_dev;

    struct list_head node;
    struct list_head match_node; // used for route matching
};


struct route_list {
    uint8_t hash_buf[VNET_HASH_SIZE];

    uint32_t num_routes;
    struct vnet_route_info * routes[0];
} __attribute__((packed));



static struct {
    struct list_head routes;
    struct list_head devs;
    
    int num_routes;
    int num_devs;

    struct vnet_brg_dev * bridge;

    v3_lock_t lock;

    struct hashtable * route_cache;

    struct bridge_pkts_buf in_buf;  //incoming packets buffer
} vnet_state;




#ifdef CONFIG_DEBUG_VNET
static inline void mac_to_string(uint8_t mac[6], char * buf) {
    snprintf(buf, 100, "%d:%d:%d:%d:%d:%d", 
	     mac[0], mac[1], mac[2],
	     mac[3], mac[4], mac[5]);
}

static void print_route(struct vnet_route_info * route){
    char str[50];

    memset(str, 0, 50);

    mac_to_string(route->route_def.src_mac, str);
    PrintDebug("Src Mac (%s),  src_qual (%d)\n", 
			str, route->route_def.src_mac_qual);

    mac_to_string(route->route_def.dst_mac, str);
    PrintDebug("Dst Mac (%s),  dst_qual (%d)\n", 
			str, route->route_def.dst_mac_qual);

    PrintDebug("Src dev id (%d), src type (%d)", 
			route->route_def.src_id, 
			route->route_def.src_type);

    PrintDebug("Dst dev id (%d), dst type (%d)\n", 
			route->route_def.dst_id, 
			route->route_def.dst_type);

    if (route->route_def.dst_type == LINK_INTERFACE) {
    	PrintDebug("dst_dev (%p), dst_dev_id (%d), dst_dev_input (%p), dst_dev_data (%p)\n",
		   route->dst_dev,
		   route->dst_dev->dev_id,
		   route->dst_dev->input,
		   route->dst_dev->private_data);
    }
}

static void dump_routes() {
	struct vnet_route_info * route = NULL;
	int i = 0;

	PrintDebug("\n========Dump routes starts ============\n");

	list_for_each_entry(route, &(vnet_state.routes), node) {
		PrintDebug("\nroute %d:\n", i++);
		print_route(route);
	}

	PrintDebug("\n========Dump routes end ============\n");
}

#endif


/* 
 * A VNET packet is a packed struct with the hashed fields grouped together.
 * This means we can generate the hash from an offset into the pkt struct
 */
static inline uint_t hash_fn(addr_t hdr_ptr) {    
    uint8_t * hdr_buf = (uint8_t *)hdr_ptr;

    return v3_hash_buffer(hdr_buf, VNET_HASH_SIZE);
}

static inline int hash_eq(addr_t key1, addr_t key2) {	
    return (memcmp((uint8_t *)key1, (uint8_t *)key2, VNET_HASH_SIZE) == 0);
}

static int add_route_to_cache(const struct v3_vnet_pkt * pkt, struct route_list * routes) {
    memcpy(routes->hash_buf, pkt->hash_buf, VNET_HASH_SIZE);    

    if (v3_htable_insert(vnet_state.route_cache, (addr_t)routes->hash_buf, (addr_t)routes) == 0) {
	PrintError("Vnet: Failed to insert new route entry to the cache\n");
	return -1;
    }
    
    return 0;
}

static int clear_hash_cache() {

    v3_free_htable(vnet_state.route_cache, 1, 1);
    vnet_state.route_cache = v3_create_htable(0, &hash_fn, &hash_eq);

    return 0;
}

static int look_into_cache(const struct v3_vnet_pkt * pkt, struct route_list ** routes) {
    
    *routes = (struct route_list *)v3_htable_search(vnet_state.route_cache, (addr_t)(pkt->hash_buf));
   
    return 0;
}


static struct vnet_dev * find_dev_by_id(int idx) {
    struct vnet_dev * dev = NULL; 
    
    list_for_each_entry(dev, &(vnet_state.devs), node) {
	int dev_id = dev->dev_id;

	if (dev_id == idx) {
	    return dev;
	}
    }

    return NULL;
}

static struct vnet_dev * find_dev_by_mac(char mac[6]) {
    struct vnet_dev * dev = NULL; 
    
    list_for_each_entry(dev, &(vnet_state.devs), node) {
	if (memcmp(dev->mac_addr, mac, 6) == 0) {
	    return dev;
	}
    }

    return NULL;
}

int get_device_id_by_mac(char mac[6]) {
    struct vnet_dev * dev = find_dev_by_mac(mac);
    
    if (dev == NULL) {
	return -1;
    }
    
    return dev->dev_id;
}


int v3_vnet_add_route(struct v3_vnet_route route) {
    struct vnet_route_info * new_route = NULL;
    uint32_t flags = 0; 

    new_route = (struct vnet_route_info *)V3_Malloc(sizeof(struct vnet_route_info));
    memset(new_route, 0, sizeof(struct vnet_route_info));

    PrintDebug("Vnet: vnet_add_route_entry: dst_id: %d, dst_type: %d\n",
			route.dst_id, route.dst_type);	
    
    memcpy(new_route->route_def.src_mac, route.src_mac, 6);
    memcpy(new_route->route_def.dst_mac, route.dst_mac, 6);
    new_route->route_def.src_mac_qual = route.src_mac_qual;
    new_route->route_def.dst_mac_qual = route.dst_mac_qual;
    new_route->route_def.dst_id = route.dst_id;
    new_route->route_def.dst_type = route.dst_type;
    new_route->route_def.src_id = route.src_id;
    new_route->route_def.src_type = route.src_type;

    if (new_route->route_def.dst_type == LINK_INTERFACE) {
	new_route->dst_dev = find_dev_by_id(new_route->route_def.dst_id);
	PrintDebug("Vnet: Add route, get device: dev_id %d, input : %p, private_data %p\n",
			new_route->dst_dev->dev_id, new_route->dst_dev->input, new_route->dst_dev->private_data);
    }

    if (new_route->route_def.src_type == LINK_INTERFACE) {
	new_route->src_dev = find_dev_by_id(new_route->route_def.src_id);
    }

    flags = v3_lock_irqsave(vnet_state.lock);

    list_add(&(new_route->node), &(vnet_state.routes));
    clear_hash_cache();

    v3_unlock_irqrestore(vnet_state.lock, flags);
   

#ifdef CONFIG_DEBUG_VNET
    dump_routes();
#endif

    return 0;
}



// At the end allocate a route_list
// This list will be inserted into the cache so we don't need to free it
static struct route_list * match_route(const struct v3_vnet_pkt * pkt) {
    struct vnet_route_info * route = NULL; 
    struct route_list * matches = NULL;
    int num_matches = 0;
    int max_rank = 0;
    struct list_head match_list;
    struct eth_hdr * hdr = (struct eth_hdr *)(pkt->data);
    uint8_t src_type = pkt->src_type;
    uint32_t src_link = pkt->src_id;

#ifdef CONFIG_DEBUG_VNET
    {
	char dst_str[100];
	char src_str[100];

	mac_to_string(hdr->src_mac, src_str);  
	mac_to_string(hdr->dst_mac, dst_str);
	PrintDebug("Vnet: match_route. pkt: SRC(%s), DEST(%s)\n", src_str, dst_str);
    }
#endif

    INIT_LIST_HEAD(&match_list);
    
#define UPDATE_MATCHES(rank) do {				\
	if (max_rank < (rank)) {				\
	    max_rank = (rank);					\
	    INIT_LIST_HEAD(&match_list);			\
	    							\
	    list_add(&(route->match_node), &match_list);	\
	    num_matches = 1;					\
	} else if (max_rank == (rank)) {			\
	    list_add(&(route->match_node), &match_list);	\
	    num_matches++;					\
	}							\
    } while (0)
    

    list_for_each_entry(route, &(vnet_state.routes), node) {
	struct v3_vnet_route * route_def = &(route->route_def);

	// CHECK SOURCE TYPE HERE
	if ( (route_def->src_type != LINK_ANY) && 
	     ( (route_def->src_type != src_type) || 
	       ( (route_def->src_id != src_link) &&
		 (route_def->src_id != (uint32_t)-1)))) {
	    continue;
	}


	if ((route_def->dst_mac_qual == MAC_ANY) &&
	    (route_def->src_mac_qual == MAC_ANY)) {      
	    UPDATE_MATCHES(3);
	}
	
	if (memcmp(route_def->src_mac, hdr->src_mac, 6) == 0) {
	    if (route_def->src_mac_qual != MAC_NOT) {
		if (route_def->dst_mac_qual == MAC_ANY) {
		    UPDATE_MATCHES(6);
		} else if (route_def->dst_mac_qual != MAC_NOT &&
			   memcmp(route_def->dst_mac, hdr->dst_mac, 6) == 0) {
		    UPDATE_MATCHES(8);
		}
	    }
	}
	    
	if (memcmp(route_def->dst_mac, hdr->dst_mac, 6) == 0) {
	    if (route_def->dst_mac_qual != MAC_NOT) {
		if (route_def->src_mac_qual == MAC_ANY) {
		    UPDATE_MATCHES(6);
		} else if ((route_def->src_mac_qual != MAC_NOT) && 
			   (memcmp(route_def->src_mac, hdr->src_mac, 6) == 0)) {
		    UPDATE_MATCHES(8);
		}
	    }
	}
	    
	if ((route_def->dst_mac_qual == MAC_NOT) &&
	    (memcmp(route_def->dst_mac, hdr->dst_mac, 6) != 0)) {
	    if (route_def->src_mac_qual == MAC_ANY) {
		UPDATE_MATCHES(5);
	    } else if ((route_def->src_mac_qual != MAC_NOT) && 
		       (memcmp(route_def->src_mac, hdr->src_mac, 6) == 0)) {     
		UPDATE_MATCHES(7);
	    }
	}
	
	if ((route_def->src_mac_qual == MAC_NOT) &&
	    (memcmp(route_def->src_mac, hdr->src_mac, 6) != 0)) {
	    if (route_def->dst_mac_qual == MAC_ANY) {
		UPDATE_MATCHES(5);
	    } else if ((route_def->dst_mac_qual != MAC_NOT) &&
		       (memcmp(route_def->dst_mac, hdr->dst_mac, 6) == 0)) {
		UPDATE_MATCHES(7);
	    }
	}
	
	// Default route
	if ( (memcmp(route_def->src_mac, hdr->src_mac, 6) == 0) &&
	     (route_def->dst_mac_qual == MAC_NONE)) {
	    UPDATE_MATCHES(4);
	}
    }

    PrintDebug("Vnet: match_route: Matches=%d\n", num_matches);

    if (num_matches == 0) {
	return NULL;
    }

    matches = (struct route_list *)V3_Malloc(sizeof(struct route_list) + 
				(sizeof(struct vnet_route_info *) * num_matches));

    matches->num_routes = num_matches;

    {
	int i = 0;
	list_for_each_entry(route, &match_list, match_node) {
	    matches->routes[i++] = route;
	}
    }

    return matches;
}

#if 0
static int flush_bridge_pkts(struct vnet_brg_dev *bridge){
    uint32_t flags;
    int num;
    int start;
    int send;
    struct v3_vnet_bridge_input_args args;
    int cpu_id = bridge->vm->cores[0].cpu_id;
    int current_core = V3_Get_CPU();
	
    if (bridge == NULL) {
	PrintDebug("VNET: No bridge to sent data to links\n");
	return -1;
    }

    flags = v3_lock_irqsave(bridge->recv_buf.lock);
		
    num = bridge->recv_buf.num;
    start = bridge->recv_buf.start;

    bridge->recv_buf.num -= num;
    bridge->recv_buf.start += num;
    bridge->recv_buf.start %= BRIDGE_BUF_SIZE;
	
    v3_unlock_irqrestore(bridge->recv_buf.lock, flags);


    if (bridge->disabled) {
	PrintDebug("VNET: In flush bridge pkts: Bridge is disabled\n");
	return -1;
    }

    if (num <= 2 && num > 0) {
	PrintDebug("VNET: In flush bridge pkts: %d\n", num);
    }

    if (num > 0) {
	PrintDebug("VNET: In flush bridge pkts to bridge, cur_cpu %d, brige_core: %d\n", current_core, cpu_id);
	if (current_core == cpu_id) {
	    if ((start + num) < BRIDGE_BUF_SIZE) { 
	    	bridge->input(bridge->vm, &(bridge->recv_buf.pkts[start]), num, bridge->private_data);
	    } else {
		bridge->input(bridge->vm, &(bridge->recv_buf.pkts[start]), (BRIDGE_BUF_SIZE - start), bridge->private_data);				
	    	send = num - (BRIDGE_BUF_SIZE - start);
		bridge->input(bridge->vm, &(bridge->recv_buf.pkts[0]), send, bridge->private_data);
	    }	
	} else {
	    args.vm = bridge->vm;
	    args.private_data = bridge->private_data;
	
	    if ((start + num) < BRIDGE_BUF_SIZE) {
	    	args.pkt_num = num;
	    	args.vnet_pkts = &(bridge->recv_buf.pkts[start]);
		V3_Call_On_CPU(cpu_id, bridge->xcall_input, (void *)&args);
	    } else {
	    	args.pkt_num = BRIDGE_BUF_SIZE - start;
	    	args.vnet_pkts = &(bridge->recv_buf.pkts[start]);
	    	V3_Call_On_CPU(cpu_id, bridge->xcall_input, (void *)&args);
				
	    	send = num - (BRIDGE_BUF_SIZE - start);
	    	args.pkt_num = send;
	    	args.vnet_pkts = &(bridge->recv_buf.pkts[0]);			
	    	V3_Call_On_CPU(cpu_id, bridge->xcall_input, (void *)&args);
	    }
	}
	
	PrintDebug("VNET: flush bridge pkts %d\n", num);
    }
			
    return 0;
}
#endif

static int send_to_bridge(struct v3_vnet_pkt * pkt){
    struct vnet_brg_dev * bridge = vnet_state.bridge;

    if (bridge == NULL) {
	PrintDebug("VNET: No bridge to sent data to links\n");
	return -1;
    }

    if (bridge->max_delayed_pkts <= 1) {

	if (bridge->disabled) {
	    PrintDebug("VNET: Bridge diabled\n");
	    return -1;
	}

	bridge->input(bridge->vm, pkt, 1, bridge->private_data);

	PrintDebug("VNET: sent one packet to the bridge\n");
    }


    return 0;
}

int v3_vnet_send_pkt(struct v3_vnet_pkt * pkt, void * private_data) {
    struct route_list * matched_routes = NULL;
    uint32_t flags = 0;
    int i = 0;
    
#ifdef CONFIG_DEBUG_VNET
    {
	struct eth_hdr * hdr = (struct eth_hdr *)(pkt->header);
	char dest_str[100];
	char src_str[100];
	int cpu = V3_Get_CPU();
	
	mac_to_string(hdr->src_mac, src_str);  
	mac_to_string(hdr->dst_mac, dest_str);
	PrintDebug("Vnet: on cpu %d, HandleDataOverLink. SRC(%s), DEST(%s), pkt size: %d\n", cpu, src_str, dest_str, pkt->size);
   }
#endif

    flags = v3_lock_irqsave(vnet_state.lock);

    look_into_cache(pkt, &matched_routes);
	
    if (matched_routes == NULL) {  
	PrintDebug("Vnet: send pkt Looking into routing table\n");
	
	matched_routes = match_route(pkt);
	
      	if (matched_routes) {
	    add_route_to_cache(pkt, matched_routes);
	} else {
	    PrintDebug("Could not find route for packet... discards packet\n");
	    v3_unlock_irqrestore(vnet_state.lock, flags);
	    return -1;
	}
    }

    v3_unlock_irqrestore(vnet_state.lock, flags);

    PrintDebug("Vnet: send pkt route matches %d\n", matched_routes->num_routes);

    for (i = 0; i < matched_routes->num_routes; i++) {
	 struct vnet_route_info * route = matched_routes->routes[i];
	
        if (route->route_def.dst_type == LINK_EDGE) {			
            pkt->dst_type = LINK_EDGE;
            pkt->dst_id = route->route_def.dst_id;

            if (send_to_bridge(pkt) == -1) {
                PrintDebug("VNET: Packet not sent properly to bridge\n");
                continue;
	     }
            
        } else if (route->route_def.dst_type == LINK_INTERFACE) {
            if (route->dst_dev->input(route->dst_dev->vm, pkt, route->dst_dev->private_data) == -1) {
                PrintDebug("VNET: Packet not sent properly\n");
                continue;
	     }
        } else {
            PrintDebug("Vnet: Wrong Edge type\n");
            continue;
        }

        PrintDebug("Vnet: v3_vnet_send_pkt: Forward packet according to Route %d\n", i);
    }
    
    return 0;
}

void v3_vnet_send_pkt_xcall(void * data) {
    struct v3_vnet_pkt * pkt = (struct v3_vnet_pkt *)data;
    v3_vnet_send_pkt(pkt, NULL);
}


void v3_vnet_polling() {
    uint32_t flags = 0;
    int num = 0;
    int start = 0;
    struct v3_vnet_pkt * buf = NULL;

    PrintDebug("In vnet pollling: cpu %d\n", V3_Get_CPU());

    flags = v3_lock_irqsave(vnet_state.in_buf.lock);
		
    num = vnet_state.in_buf.num;
    start = vnet_state.in_buf.start;

    PrintDebug("VNET: polling pkts %d\n", num);

    while (num > 0) {
	buf = &(vnet_state.in_buf.pkts[vnet_state.in_buf.start]);

	v3_vnet_send_pkt(buf, NULL);

	vnet_state.in_buf.num--;
    	vnet_state.in_buf.start++;
	vnet_state.in_buf.start %= BRIDGE_BUF_SIZE;
	num--;
    }

    v3_unlock_irqrestore(vnet_state.in_buf.lock, flags);

    return;
}


int v3_vnet_rx(uint8_t * buf, uint16_t size, uint16_t src_id, uint8_t src_type) {
    uint32_t flags = 0;
    int end = 0;
    struct v3_vnet_pkt * pkt = NULL;
   
    flags = v3_lock_irqsave(vnet_state.in_buf.lock);
	    
    end = vnet_state.in_buf.end;
    pkt = &(vnet_state.in_buf.pkts[end]);

    if (vnet_state.in_buf.num > BRIDGE_BUF_SIZE){
 	PrintDebug("VNET: bridge rx: buffer full\n");
	v3_unlock_irqrestore(vnet_state.in_buf.lock, flags);
	return 0;
    }

    vnet_state.in_buf.num++;
    vnet_state.in_buf.end++;
    vnet_state.in_buf.end %= BRIDGE_BUF_SIZE;

    pkt->size = size;
    pkt->src_id = src_id;
    pkt->src_type = src_type;
    memcpy(pkt->header, buf, ETHERNET_HEADER_LEN);
    memcpy(pkt->data, buf, size);

	
    v3_unlock_irqrestore(vnet_state.in_buf.lock, flags);

    return 0;
}
	

int v3_vnet_add_dev(struct v3_vm_info * vm, uint8_t mac[6], 
		    int (*netif_input)(struct v3_vm_info * vm, struct v3_vnet_pkt * pkt, void * private_data), 
		    void * priv_data){
    struct vnet_dev * new_dev = NULL;
    uint32_t flags = 0;

    new_dev = (struct vnet_dev *)V3_Malloc(sizeof(struct vnet_dev)); 

    if (new_dev == NULL) {
	PrintError("VNET: Malloc fails\n");
	return -1;
    }
   
    memcpy(new_dev->mac_addr, mac, 6);
    new_dev->input = netif_input;
    new_dev->private_data = priv_data;
    new_dev->vm = vm;
    new_dev->dev_id = 0;	

    flags = v3_lock_irqsave(vnet_state.lock);

    if (!find_dev_by_mac(mac)) {
	list_add(&(new_dev->node), &(vnet_state.devs));
	new_dev->dev_id = ++vnet_state.num_devs;
    }

    v3_unlock_irqrestore(vnet_state.lock, flags);

    // if the device was found previosly the id should still be 0
    if (new_dev->dev_id == 0) {
	PrintError("Device Alrady exists\n");
	return -1;
    }

    PrintDebug("Vnet: Add Device: dev_id %d, input : %p, private_data %p\n",
	       new_dev->dev_id, new_dev->input, new_dev->private_data);

    return new_dev->dev_id;
}


void v3_vnet_heartbeat(struct guest_info *core){
    //static long last_time, cur_time;

    if (vnet_state.bridge == NULL) {
	return;
    }
/*	
    if(vnet_state.bridge->max_delayed_pkts > 1){
	if(V3_Get_CPU() != vnet_state.bridge->vm->cores[0].cpu_id){
	    rdtscll(cur_time);
    	}

    	if ((cur_time - last_time) >= vnet_state.bridge->max_latency) {
	    last_time = cur_time;
	    flush_bridge_pkts(vnet_state.bridge);
    	}
    }
*/
    vnet_state.bridge->polling_pkt(vnet_state.bridge->vm, vnet_state.bridge->private_data);
}

int v3_vnet_add_bridge(struct v3_vm_info * vm,
		       int (*input)(struct v3_vm_info * vm, struct v3_vnet_pkt pkt[], uint16_t pkt_num, void * private_data),
		       void (*xcall_input)(void * data),
		       int (*poll_pkt)(struct v3_vm_info * vm, void * private_data),
		       uint16_t max_delayed_pkts,
		       long max_latency,
		       void * priv_data) {

    uint32_t flags = 0;
    int bridge_free = 0;
    struct vnet_brg_dev * tmp_bridge = NULL;    
    
    flags = v3_lock_irqsave(vnet_state.lock);

    if (vnet_state.bridge == NULL) {
	bridge_free = 1;
	vnet_state.bridge = (void *)1;
    }

    v3_unlock_irqrestore(vnet_state.lock, flags);

    if (bridge_free == 0) {
	PrintError("Bridge already set\n");
	return -1;
    }

    tmp_bridge = (struct vnet_brg_dev *)V3_Malloc(sizeof(struct vnet_brg_dev));

    if (tmp_bridge == NULL) {
	PrintError("Malloc Fails\n");
	vnet_state.bridge = NULL;
	return -1;
    }
    
    tmp_bridge->vm = vm;
    tmp_bridge->input = input;
    tmp_bridge->xcall_input = xcall_input;
    tmp_bridge->polling_pkt = poll_pkt;
    tmp_bridge->private_data = priv_data;
    tmp_bridge->disabled = 0;

/*
    //initial receving buffer
    tmp_bridge->recv_buf.start = 0;
    tmp_bridge->recv_buf.end = 0;
    tmp_bridge->recv_buf.num = 0;
    if(v3_lock_init(&(tmp_bridge->recv_buf.lock)) == -1){
	PrintError("VNET: add bridge, error to initiate recv buf lock\n");
    }
    int i;
    for(i = 0; i<BRIDGE_BUF_SIZE; i++){
	tmp_bridge->recv_buf.pkts[i].data = &(tmp_bridge->recv_buf.datas[i*ETHERNET_PACKET_LEN]);
    }

*/
    
    tmp_bridge->max_delayed_pkts = (max_delayed_pkts < BRIDGE_BUF_SIZE) ? max_delayed_pkts : BRIDGE_BUF_SIZE;
    tmp_bridge->max_latency = max_latency;
	
    // make this atomic to avoid possible race conditions
    flags = v3_lock_irqsave(vnet_state.lock);
    vnet_state.bridge = tmp_bridge;
    v3_unlock_irqrestore(vnet_state.lock, flags);

    return 0;
}


int v3_vnet_disable_bridge() {
    uint32_t flags = 0; 
    
    flags = v3_lock_irqsave(vnet_state.lock);

    if (vnet_state.bridge != NULL) {
	vnet_state.bridge->disabled = 1;
    }

    v3_unlock_irqrestore(vnet_state.lock, flags);

    return 0;
}


int v3_vnet_enable_bridge() {
    uint32_t flags = 0;
    
    flags = v3_lock_irqsave(vnet_state.lock);

    if (vnet_state.bridge != NULL) {
	vnet_state.bridge->disabled = 0;
    }

    v3_unlock_irqrestore(vnet_state.lock, flags);

    return 0;
}



int V3_init_vnet() {
    int i = 0;

    memset(&vnet_state, 0, sizeof(vnet_state));
	
    INIT_LIST_HEAD(&(vnet_state.routes));
    INIT_LIST_HEAD(&(vnet_state.devs));

    vnet_state.num_devs = 0;
    vnet_state.num_routes = 0;

    PrintDebug("VNET: Links and Routes tables initiated\n");

    if (v3_lock_init(&(vnet_state.lock)) == -1){
        PrintError("VNET: Failure to init lock for routes table\n");
    }

    PrintDebug("VNET: Locks initiated\n");
    
    //initial incoming pkt buffer
    vnet_state.in_buf.start = 0;
    vnet_state.in_buf.end = 0;
    vnet_state.in_buf.num = 0;

    if (v3_lock_init(&(vnet_state.in_buf.lock)) == -1){
	PrintError("VNET: add bridge, error to initiate send buf lock\n");
    }

    for (i = 0; i < BRIDGE_BUF_SIZE; i++){
	vnet_state.in_buf.pkts[i].data = &(vnet_state.in_buf.datas[i * ETHERNET_PACKET_LEN]);
    }

    PrintDebug("VNET: Receiving buffer initiated\n");

    vnet_state.route_cache = v3_create_htable(0, &hash_fn, &hash_eq);

    if (vnet_state.route_cache == NULL) {
        PrintError("Vnet: Route Cache Init Fails\n");
        return -1;
    }

    PrintDebug("VNET: initiated\n");

    return 0;
}
