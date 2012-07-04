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
 * Copyright (c) 2009, Yuan Tang <ytang@northwestern.edu>  
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved
 *
 * Author: Lei Xia <lxia@northwestern.edu>
 *	   Yuan Tang <ytang@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
 
#include <vnet/vnet.h>
#include <vnet/vnet_hashtable.h>
#include <vnet/vnet_host.h>
#include <vnet/vnet_vmm.h>

#include <palacios/vmm_queue.h>

#ifndef V3_CONFIG_DEBUG_VNET
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define VNET_YIELD_USEC 1000

int net_debug = 0;

struct eth_hdr {
    uint8_t dst_mac[ETH_ALEN];
    uint8_t src_mac[ETH_ALEN];
    uint16_t type; /* indicates layer 3 protocol type */
} __attribute__((packed));


struct vnet_dev {
    int dev_id;
    uint8_t mac_addr[ETH_ALEN];
    struct v3_vm_info * vm;
    struct v3_vnet_dev_ops dev_ops;

    int poll;

#define VNET_MAX_QUOTE 64
    int quote;
	
    void * private_data;

    struct list_head node;
} __attribute__((packed));


struct vnet_brg_dev {
    struct v3_vm_info * vm;
    struct v3_vnet_bridge_ops brg_ops;

    uint8_t type;

    void * private_data;
} __attribute__((packed));



struct vnet_route_info {
    struct v3_vnet_route route_def;

    struct vnet_dev * dst_dev;
    struct vnet_dev * src_dev;

    uint32_t idx;

    struct list_head node;
    struct list_head match_node; // used for route matching
};


struct route_list {
    uint8_t hash_buf[VNET_HASH_SIZE];

    uint32_t num_routes;
    struct vnet_route_info * routes[0];
} __attribute__((packed));


struct queue_entry{
    uint8_t use;
    struct v3_vnet_pkt pkt;
    uint8_t * data;
    uint32_t size_alloc;
};


static struct {
    struct list_head routes;
    struct list_head devs;

    uint8_t status; 
   
    uint32_t num_routes;
    uint32_t route_idx;
    uint32_t num_devs;
    uint32_t dev_idx;

    struct vnet_brg_dev * bridge;

    vnet_lock_t lock;
    struct vnet_stat stats;

   /* device queue that are waiting to be polled */
    struct v3_queue * poll_devs;

    struct vnet_thread * pkt_flush_thread;

    struct hashtable * route_cache;
} vnet_state;
	

#ifdef V3_CONFIG_DEBUG_VNET
static inline void mac2str(uint8_t * mac, char * buf) {
    snprintf(buf, 100, "%2x:%2x:%2x:%2x:%2x:%2x", 
	     mac[0], mac[1], mac[2],
	     mac[3], mac[4], mac[5]);
}

static void print_route(struct v3_vnet_route * route){
    char str[50];

    mac2str(route->src_mac, str);
    PrintDebug("Src Mac (%s),  src_qual (%d)\n", 
	       str, route->src_mac_qual);
    mac2str(route->dst_mac, str);
    PrintDebug("Dst Mac (%s),  dst_qual (%d)\n", 
	       str, route->dst_mac_qual);
    PrintDebug("Src dev id (%d), src type (%d)", 
	       route->src_id, 
	       route->src_type);
    PrintDebug("Dst dev id (%d), dst type (%d)\n", 
	       route->dst_id, 
	       route->dst_type);
}

static void dump_routes(){
    struct vnet_route_info *route;

    PrintDebug("\n========Dump routes starts ============\n");
    list_for_each_entry(route, &(vnet_state.routes), node) {
    	PrintDebug("\nroute %d:\n", route->idx);
		
	print_route(&(route->route_def));
	if (route->route_def.dst_type == LINK_INTERFACE) {
	    PrintDebug("dst_dev (%p), dst_dev_id (%d), dst_dev_ops(%p), dst_dev_data (%p)\n",
	       	route->dst_dev,
	       	route->dst_dev->dev_id,
	       	(void *)&(route->dst_dev->dev_ops),
	       	route->dst_dev->private_data);
	}
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

    return vnet_hash_buffer(hdr_buf, VNET_HASH_SIZE);
}

static inline int hash_eq(addr_t key1, addr_t key2) {	
    return (memcmp((uint8_t *)key1, (uint8_t *)key2, VNET_HASH_SIZE) == 0);
}

static int add_route_to_cache(const struct v3_vnet_pkt * pkt, struct route_list * routes) {
    memcpy(routes->hash_buf, pkt->hash_buf, VNET_HASH_SIZE);    

    if (vnet_htable_insert(vnet_state.route_cache, (addr_t)routes->hash_buf, (addr_t)routes) == 0) {
	PrintError("VNET/P Core: Failed to insert new route entry to the cache\n");
	return -1;
    }
    
    return 0;
}

static int clear_hash_cache() {
    vnet_free_htable(vnet_state.route_cache, 1, 1);
    vnet_state.route_cache = vnet_create_htable(0, &hash_fn, &hash_eq);

    return 0;
}

static int look_into_cache(const struct v3_vnet_pkt * pkt, 
			   struct route_list ** routes) {
    *routes = (struct route_list *)vnet_htable_search(vnet_state.route_cache, (addr_t)(pkt->hash_buf));
   
    return 0;
}


static struct vnet_dev * dev_by_id(int idx) {
    struct vnet_dev * dev = NULL; 

    list_for_each_entry(dev, &(vnet_state.devs), node) {
	if (dev->dev_id == idx) {
	    return dev;
	}
    }

    return NULL;
}

static struct vnet_dev * dev_by_mac(uint8_t * mac) {
    struct vnet_dev * dev = NULL; 
    
    list_for_each_entry(dev, &(vnet_state.devs), node) {
	if (!compare_ethaddr(dev->mac_addr, mac)){
	    return dev;
	}
    }

    return NULL;
}


int v3_vnet_find_dev(uint8_t  * mac) {
    struct vnet_dev * dev = NULL;

    dev = dev_by_mac(mac);

    if(dev != NULL) {
	return dev->dev_id;
    }

    return -1;
}


int v3_vnet_add_route(struct v3_vnet_route route) {
    struct vnet_route_info * new_route = NULL;
    vnet_intr_flags_t flags; 

    new_route = (struct vnet_route_info *)Vnet_Malloc(sizeof(struct vnet_route_info));

    if (!new_route) {
	PrintError("Cannot allocate new route\n");
	return -1;
    }

    memset(new_route, 0, sizeof(struct vnet_route_info));

#ifdef V3_CONFIG_DEBUG_VNET
    PrintDebug("VNET/P Core: add_route_entry:\n");
    print_route(&route);
#endif
    
    memcpy(new_route->route_def.src_mac, route.src_mac, ETH_ALEN);
    memcpy(new_route->route_def.dst_mac, route.dst_mac, ETH_ALEN);
    new_route->route_def.src_mac_qual = route.src_mac_qual;
    new_route->route_def.dst_mac_qual = route.dst_mac_qual;
    new_route->route_def.dst_type = route.dst_type;
    new_route->route_def.src_type = route.src_type;
    new_route->route_def.src_id = route.src_id;
    new_route->route_def.dst_id = route.dst_id;

    if (new_route->route_def.dst_type == LINK_INTERFACE) {
	new_route->dst_dev = dev_by_id(new_route->route_def.dst_id);
    }

    if (new_route->route_def.src_type == LINK_INTERFACE) {
	new_route->src_dev = dev_by_id(new_route->route_def.src_id);
    }


    flags = vnet_lock_irqsave(vnet_state.lock);

    list_add(&(new_route->node), &(vnet_state.routes));
    new_route->idx = ++ vnet_state.route_idx;
    vnet_state.num_routes ++;
	
    vnet_unlock_irqrestore(vnet_state.lock, flags);

    clear_hash_cache();

#ifdef V3_CONFIG_DEBUG_VNET
    dump_routes();
#endif

    return new_route->idx;
}


void v3_vnet_del_route(uint32_t route_idx){
    struct vnet_route_info * route = NULL;
    vnet_intr_flags_t flags; 

    flags = vnet_lock_irqsave(vnet_state.lock);

    list_for_each_entry(route, &(vnet_state.routes), node) {
	Vnet_Print(0, "v3_vnet_del_route, route idx: %d\n", route->idx);
	if(route->idx == route_idx){
	    list_del(&(route->node));
	    Vnet_Free(route);
	    break;    
	}
    }

    vnet_unlock_irqrestore(vnet_state.lock, flags);
    clear_hash_cache();

#ifdef V3_CONFIG_DEBUG_VNET
    dump_routes();
#endif	
}


/* delete all route entries with specfied src or dst device id */ 
static void inline del_routes_by_dev(int dev_id){
    struct vnet_route_info * route, *tmp_route;
    vnet_intr_flags_t flags; 

    flags = vnet_lock_irqsave(vnet_state.lock);

    list_for_each_entry_safe(route, tmp_route, &(vnet_state.routes), node) {
	if((route->route_def.dst_type == LINK_INTERFACE &&
	     route->route_def.dst_id == dev_id) ||
	     (route->route_def.src_type == LINK_INTERFACE &&
	      route->route_def.src_id == dev_id)){
	      
	    list_del(&(route->node));
	    list_del(&(route->match_node));
	    Vnet_Free(route);    
	}
    }

    vnet_unlock_irqrestore(vnet_state.lock, flags);
}




/* At the end allocate a route_list
 * This list will be inserted into the cache so we don't need to free it
 */
static struct route_list * match_route(const struct v3_vnet_pkt * pkt) {
    struct vnet_route_info * route = NULL; 
    struct route_list * matches = NULL;
    int num_matches = 0;
    int max_rank = 0;
    struct list_head match_list;
    struct eth_hdr * hdr = (struct eth_hdr *)(pkt->data);
    //  uint8_t src_type = pkt->src_type;
    //  uint32_t src_link = pkt->src_id;

#ifdef V3_CONFIG_DEBUG_VNET
    {
	char dst_str[100];
	char src_str[100];

	mac2str(hdr->src_mac, src_str);  
	mac2str(hdr->dst_mac, dst_str);
	PrintDebug("VNET/P Core: match_route. pkt: SRC(%s), DEST(%s)\n", src_str, dst_str);
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

/*
	// CHECK SOURCE TYPE HERE
	if ( (route_def->src_type != LINK_ANY) && 
	     ( (route_def->src_type != src_type) || 
	       ( (route_def->src_id != src_link) &&
		 (route_def->src_id != -1)))) {
	    continue;
	}
*/

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

    PrintDebug("VNET/P Core: match_route: Matches=%d\n", num_matches);

    if (num_matches <= 0) {
	return NULL;
    }

    matches = (struct route_list *)Vnet_Malloc(sizeof(struct route_list) + 
					       (sizeof(struct vnet_route_info *) * num_matches));


    if (!matches) {
	PrintError("VNET/P Core: Unable to allocate matches\n");
	return NULL;
    }

    matches->num_routes = num_matches;

    {
	int i = 0;
	list_for_each_entry(route, &match_list, match_node) {
	    matches->routes[i++] = route;
	}
    }

    return matches;
}

int v3_vnet_query_header(uint8_t src_mac[6], 
			 uint8_t dest_mac[6],
			 int     recv,         // 0 = send, 1=recv
			 struct v3_vnet_header *header)
{
    struct route_list *routes;
    struct vnet_route_info *r;
    struct v3_vnet_pkt p;

    p.size=14;
    p.data=p.header;
    memcpy(p.header,dest_mac,6);
    memcpy(p.header+6,src_mac,6);
    memset(p.header+12,0,2);

    p.src_type = LINK_EDGE;
    p.src_id = 0;

    memcpy(header->src_mac,src_mac,6);
    memcpy(header->dst_mac,dest_mac,6);

    
    look_into_cache(&p,&routes);

    if (!routes) { 
	routes = match_route(&p);
	if (!routes) { 
	    PrintError("Cannot match route\n");
	    header->header_type=VNET_HEADER_NOMATCH;
	    header->header_len=0;
	    return -1;
	} else {
	    add_route_to_cache(&p,routes);
	}
    }
    
    if (routes->num_routes<1) { 
	PrintError("Less than one route\n");
	header->header_type=VNET_HEADER_NOMATCH;
	header->header_len=0;
	return -1;
    }

    if (routes->num_routes>1) { 
	PrintError("More than one route, building header for the first one only\n");
    }

    r=routes->routes[0];

    switch (r->route_def.dst_type) {
	case LINK_EDGE: {
	    // switch based on the link type
	    // for mac-in-udp, we would want to generate a mac, ip, and udp header
	    // direct transmission

	    // for now we will say we have no encapsulation
	    //
	    header->header_type=VNET_HEADER_NONE;
	    header->header_len=0;
	    header->src_mac_qual=r->route_def.src_mac_qual;
	    header->dst_mac_qual=r->route_def.dst_mac_qual;
	    
	}
	    
	    return 0;
	    break;
	    

	case LINK_INTERFACE:
	    // direct transmission
	    // let's guess that it goes to the same interface...
	    header->header_type=VNET_HEADER_NONE;
	    header->header_len=0;
	    header->src_mac_qual=r->route_def.src_mac_qual;
	    header->dst_mac_qual=r->route_def.dst_mac_qual;

	    return 0;
	    break;

	default:
	    PrintError("Unknown destination type\n");
	    return -1;
	    break;

    }
    
}




int v3_vnet_send_pkt(struct v3_vnet_pkt * pkt, void * private_data) {
    struct route_list * matched_routes = NULL;
    vnet_intr_flags_t flags;
    int i;

    int cpu = V3_Get_CPU();

    Vnet_Print(2, "VNET/P Core: cpu %d: pkt (size %d, src_id:%d, src_type: %d, dst_id: %d, dst_type: %d)\n",
	       cpu, pkt->size, pkt->src_id, 
	       pkt->src_type, pkt->dst_id, pkt->dst_type);

    if(net_debug >= 4){
	v3_hexdump(pkt->data, pkt->size, NULL, 0);
    }

    flags = vnet_lock_irqsave(vnet_state.lock);

    vnet_state.stats.rx_bytes += pkt->size;
    vnet_state.stats.rx_pkts++;

    look_into_cache(pkt, &matched_routes);

    if (matched_routes == NULL) {  
	PrintDebug("VNET/P Core: sending pkt - matching route\n");
	
	matched_routes = match_route(pkt);
	
      	if (matched_routes) {
	    add_route_to_cache(pkt, matched_routes);
	} else {
	    PrintDebug("VNET/P Core: Could not find route for packet... discarding packet\n");
	    vnet_unlock_irqrestore(vnet_state.lock, flags);
	    return 0; /* do we return -1 here?*/
	}
    }

    vnet_unlock_irqrestore(vnet_state.lock, flags);

    PrintDebug("VNET/P Core: send pkt route matches %d\n", matched_routes->num_routes);

    for (i = 0; i < matched_routes->num_routes; i++) {
	struct vnet_route_info * route = matched_routes->routes[i];
	
	if (route->route_def.dst_type == LINK_EDGE) {
	    struct vnet_brg_dev * bridge = vnet_state.bridge;
	    pkt->dst_type = LINK_EDGE;
	    pkt->dst_id = route->route_def.dst_id;

    	    if (bridge == NULL) {
	        Vnet_Print(2, "VNET/P Core: No active bridge to sent data to\n");
		continue;
    	    }

    	    if(bridge->brg_ops.input(bridge->vm, pkt, bridge->private_data) < 0){
                Vnet_Print(2, "VNET/P Core: Packet not sent properly to bridge\n");
                continue;
	    }         
	    vnet_state.stats.tx_bytes += pkt->size;
	    vnet_state.stats.tx_pkts ++;
        } else if (route->route_def.dst_type == LINK_INTERFACE) {
            if (route->dst_dev == NULL){
	 	  Vnet_Print(2, "VNET/P Core: No active device to sent data to\n");
	        continue;
            }

	    if(route->dst_dev->dev_ops.input(route->dst_dev->vm, pkt, route->dst_dev->private_data) < 0) {
                Vnet_Print(2, "VNET/P Core: Packet not sent properly\n");
                continue;
	    }
	    vnet_state.stats.tx_bytes += pkt->size;
	    vnet_state.stats.tx_pkts ++;
        } else {
            Vnet_Print(0, "VNET/P Core: Wrong dst type\n");
        }
    }
    
    return 0;
}


int v3_vnet_add_dev(struct v3_vm_info * vm, uint8_t * mac, 
		    struct v3_vnet_dev_ops * ops, int quote, int poll_state,
		    void * priv_data){
    struct vnet_dev * new_dev = NULL;
    vnet_intr_flags_t flags;

    new_dev = (struct vnet_dev *)Vnet_Malloc(sizeof(struct vnet_dev)); 

    if (new_dev == NULL) {
	Vnet_Print(0, "VNET/P Core: Unable to allocate a new device\n");
	return -1;
    }
   
    memcpy(new_dev->mac_addr, mac, 6);
    new_dev->dev_ops.input = ops->input;
    new_dev->dev_ops.poll = ops->poll;
    new_dev->private_data = priv_data;
    new_dev->vm = vm;
    new_dev->dev_id = 0;
    new_dev->quote = quote<VNET_MAX_QUOTE ? quote : VNET_MAX_QUOTE;
    new_dev->poll = poll_state;

    flags = vnet_lock_irqsave(vnet_state.lock);

    if (dev_by_mac(mac) == NULL) {
	list_add(&(new_dev->node), &(vnet_state.devs));
	new_dev->dev_id = ++ vnet_state.dev_idx;
	vnet_state.num_devs ++;

	if(new_dev->poll) {
	    v3_enqueue(vnet_state.poll_devs, (addr_t)new_dev);
	}
    } else {
	PrintError("VNET/P: Device with the same MAC has already been added\n");
    }

    vnet_unlock_irqrestore(vnet_state.lock, flags);

    /* if the device was found previosly the id should still be 0 */
    if (new_dev->dev_id == 0) {
	Vnet_Print(0, "VNET/P Core: Device Already exists\n");
	return -1;
    }

    PrintDebug("VNET/P Core: Add Device: dev_id %d\n", new_dev->dev_id);

    return new_dev->dev_id;
}


int v3_vnet_del_dev(int dev_id){
    struct vnet_dev * dev = NULL;
    vnet_intr_flags_t flags;

    flags = vnet_lock_irqsave(vnet_state.lock);
	
    dev = dev_by_id(dev_id);
    if (dev != NULL){
    	list_del(&(dev->node));
	//del_routes_by_dev(dev_id);
	vnet_state.num_devs --;
    }
	
    vnet_unlock_irqrestore(vnet_state.lock, flags);

    Vnet_Free(dev);

    PrintDebug("VNET/P Core: Removed Device: dev_id %d\n", dev_id);

    return 0;
}


int v3_vnet_stat(struct vnet_stat * stats){
    stats->rx_bytes = vnet_state.stats.rx_bytes;
    stats->rx_pkts = vnet_state.stats.rx_pkts;
    stats->tx_bytes = vnet_state.stats.tx_bytes;
    stats->tx_pkts = vnet_state.stats.tx_pkts;

    return 0;
}

static void deinit_devices_list(){
    struct vnet_dev * dev, * tmp; 

    list_for_each_entry_safe(dev, tmp, &(vnet_state.devs), node) {
	list_del(&(dev->node));
	Vnet_Free(dev);
    }
}

static void deinit_routes_list(){
    struct vnet_route_info * route, * tmp; 

    list_for_each_entry_safe(route, tmp, &(vnet_state.routes), node) {
	list_del(&(route->node));
	list_del(&(route->match_node));
	Vnet_Free(route);
    }
}

int v3_vnet_add_bridge(struct v3_vm_info * vm,
		       struct v3_vnet_bridge_ops * ops,
		       uint8_t type,
		       void * priv_data) {
    vnet_intr_flags_t flags;
    int bridge_free = 0;
    struct vnet_brg_dev * tmp_bridge = NULL;    
    
    flags = vnet_lock_irqsave(vnet_state.lock);
    if (vnet_state.bridge == NULL) {
	bridge_free = 1;
	vnet_state.bridge = (void *)1;
    }
    vnet_unlock_irqrestore(vnet_state.lock, flags);

    if (bridge_free == 0) {
	PrintError("VNET/P Core: Bridge already set\n");
	return -1;
    }

    tmp_bridge = (struct vnet_brg_dev *)Vnet_Malloc(sizeof(struct vnet_brg_dev));

    if (tmp_bridge == NULL) {
	PrintError("VNET/P Core: Unable to allocate new bridge\n");
	vnet_state.bridge = NULL;
	return -1;
    }
    
    tmp_bridge->vm = vm;
    tmp_bridge->brg_ops.input = ops->input;
    tmp_bridge->brg_ops.poll = ops->poll;
    tmp_bridge->private_data = priv_data;
    tmp_bridge->type = type;
	
    /* make this atomic to avoid possible race conditions */
    flags = vnet_lock_irqsave(vnet_state.lock);
    vnet_state.bridge = tmp_bridge;
    vnet_unlock_irqrestore(vnet_state.lock, flags);

    return 0;
}


void v3_vnet_del_bridge(uint8_t type) {
    vnet_intr_flags_t flags;
    struct vnet_brg_dev * tmp_bridge = NULL;    
    
    flags = vnet_lock_irqsave(vnet_state.lock);
	
    if (vnet_state.bridge != NULL && vnet_state.bridge->type == type) {
	tmp_bridge = vnet_state.bridge;
	vnet_state.bridge = NULL;
    }
	
    vnet_unlock_irqrestore(vnet_state.lock, flags);

    if (tmp_bridge) {
	Vnet_Free(tmp_bridge);
    }
}


/* can be instanieoued to multiple threads
  * that runs on multiple cores 
  * or it could be running on a dedicated side core
  */
static int vnet_tx_flush(void * args){
    struct vnet_dev * dev = NULL;
    int more;
    int rc;

    Vnet_Print(0, "VNET/P Polling Thread Starting ....\n");

    // since there are multiple instances of this thread, and only
    // one queue of pollable devices, our model here will be to synchronize
    // on that queue, removing devices as we go, and keeping them
    // then putting them back on the queue when we are done
    // in this way, multiple instances of this function will never
    // be polling the same device at the same time

    struct v3_queue * tq = v3_create_queue();

    if (!tq) { 
	PrintError("VNET/P polling thread cannot allocate queue\n");
	return -1;
    }


    while (!vnet_thread_should_stop()) {

	more=0; // will indicate if any device has more work for us to do

	while ((dev = (struct vnet_dev *)v3_dequeue(vnet_state.poll_devs))) { 
	    // we are handling this device
	    v3_enqueue(tq,(addr_t)dev);
	    
	    if (dev->poll && dev->dev_ops.poll) {
		// The device's poll function MUST NOT BLOCK
		rc = dev->dev_ops.poll(dev->vm, dev->quote, dev->private_data);

		if (rc<0) { 
		    Vnet_Print(0, "VNET/P: poll from device %p error (ignoring) !\n", dev);
		} else {
		    more |= rc;  
		}
	    }
	}
	
	while ((dev = (struct vnet_dev *)v3_dequeue(tq))) { 
	    // now someone else can handle it
	    v3_enqueue(vnet_state.poll_devs, (addr_t)dev); 
	}

	// Yield regardless of whether we handled any devices - need
	// to allow other threads to run
	if (more) { 
	    // we have more to do, so we want to get back asap
	    V3_Yield();
	} else {
	    // put ourselves briefly to sleep if we we don't have more
	    V3_Yield_Timed(VNET_YIELD_USEC);
	}

    }

    Vnet_Free(tq);
    
    Vnet_Print(0, "VNET/P Polling Thread Done.\n");

    return 0;
}

int v3_init_vnet() {
    memset(&vnet_state, 0, sizeof(vnet_state));
	
    INIT_LIST_HEAD(&(vnet_state.routes));
    INIT_LIST_HEAD(&(vnet_state.devs));

    vnet_state.num_devs = 0;
    vnet_state.num_routes = 0;

    if (vnet_lock_init(&(vnet_state.lock)) == -1){
        PrintError("VNET/P: Fails to initiate lock\n");
    }

    vnet_state.route_cache = vnet_create_htable(0, &hash_fn, &hash_eq);
    if (vnet_state.route_cache == NULL) {
        PrintError("VNET/P: Fails to initiate route cache\n");
        return -1;
    }

    vnet_state.poll_devs = v3_create_queue();

    vnet_state.pkt_flush_thread = vnet_start_thread(vnet_tx_flush, NULL, "vnetd-1");

    PrintDebug("VNET/P is initiated\n");

    return 0;
}


void v3_deinit_vnet() {

    v3_deinit_queue(vnet_state.poll_devs);
    Vnet_Free(vnet_state.poll_devs);

    PrintDebug("Stopping flush thread\n");
    // This will pause until the flush thread is gone
    vnet_thread_stop(vnet_state.pkt_flush_thread);
    // At this point there should be no lock-holder

    Vnet_Free(vnet_state.poll_devs);


    PrintDebug("Deiniting Device List\n");
    // close any devices we have open
    deinit_devices_list();  
    
    PrintDebug("Deiniting Route List\n");
    // remove any routes we have
    deinit_routes_list();

    PrintDebug("Freeing hash table\n");
    // remove the hash table
    vnet_free_htable(vnet_state.route_cache, 1, 1);

    
    PrintDebug("Removing Bridge\n");
    // remove bridge if it was added
    if (vnet_state.bridge) { 
	Vnet_Free(vnet_state.bridge);
    }

    PrintDebug("Deleting lock\n");
    // eliminate the lock
    vnet_lock_deinit(&(vnet_state.lock));

}


