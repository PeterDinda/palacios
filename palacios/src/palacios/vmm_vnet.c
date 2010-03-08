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


struct vnet_brg_dev {
    struct v3_vm_info * vm;
    
    int (*input)(struct v3_vm_info * vm, struct v3_vnet_pkt * pkt, void * private_data);
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

    struct vnet_brg_dev *bridge;

    v3_lock_t lock;

    struct gen_queue * inpkt_q;
    struct hashtable * route_cache;

} vnet_state;




#ifdef CONFIG_DEBUG_VNET
static inline void mac_to_string(char mac[6], char * buf) {
    snprintf(buf, 50, "%x:%x:%x:%x:%x:%x", 
	     mac[0], mac[1], mac[2],
	     mac[3], mac[4], mac[5]);
}

static void print_route(struct vnet_route_info *route){
    char str[50];

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

static void dump_routes(){
	struct vnet_route_info *route;

	int i = 0;
	PrintDebug("\n========Dump routes starts ============\n");
	list_for_each_entry(route, &(vnet_state.routes), node) {
		PrintDebug("\nroute %d:\n", ++i);
		
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


static int add_route_to_cache(struct v3_vnet_pkt * pkt, struct route_list * routes) {
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

static int look_into_cache(struct v3_vnet_pkt * pkt, struct route_list ** routes) {
    
    *routes = (struct route_list *)v3_htable_search(vnet_state.route_cache, (addr_t)(pkt->hash_buf));
   
    return 0;
}


static struct vnet_dev * find_dev_by_id(int idx) {
    struct vnet_dev * dev = NULL; 
    
    list_for_each_entry(dev, &(vnet_state.devs), node) {
	int dev_id = dev->dev_id;

	if (dev_id == idx)
	    return dev;
    }

    return NULL;
}

static struct vnet_dev * find_dev_by_mac(char mac[6]) {
    struct vnet_dev * dev = NULL; 
    
    list_for_each_entry(dev, &(vnet_state.devs), node) {
	if (!memcmp(dev->mac_addr, mac, 6))
	    return dev;
    }

    return NULL;
}

int get_device_id_by_mac(char mac[6]){

    struct vnet_dev *dev = find_dev_by_mac(mac);

    if (dev == NULL)
	return -1;

    return dev->dev_id;
}


int v3_vnet_add_route(struct v3_vnet_route route) {
    struct vnet_route_info * new_route = NULL;
    unsigned long flags; 

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
    v3_unlock_irqrestore(vnet_state.lock, flags);
   
    clear_hash_cache();

#ifdef CONFIG_DEBUG_VNET
    dump_routes();
#endif

    return 0;
}



// At the end allocate a route_list
// This list will be inserted into the cache so we don't need to free it
static struct route_list * match_route(struct v3_vnet_pkt * pkt) {
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
	char dst_str[50];
	char src_str[50];

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

    matches = V3_Malloc(sizeof(struct route_list) + 
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

static int handle_one_pkt(struct v3_vnet_pkt * pkt, void *private_data) {
    struct route_list * matched_routes = NULL;
    unsigned long flags;
    int i;


#ifdef CONFIG_DEBUG_VNET
   {
	struct eth_hdr * hdr = (struct eth_hdr *)(pkt->data);
	char dest_str[30];
	char src_str[30];

	mac_to_string(hdr->src_mac, src_str);  
	mac_to_string(hdr->dst_mac, dest_str);
	PrintDebug("Vnet: HandleDataOverLink. SRC(%s), DEST(%s)\n", src_str, dest_str);
   }
#endif

#ifdef CONFIG_VNET_PROFILE
    struct guest_info *core = (struct guest_info *)private_data;
    uint64_t start, end;
    rdtscll(start);
#endif

    flags = v3_lock_irqsave(vnet_state.lock);

    look_into_cache(pkt, &matched_routes);
	
    if (matched_routes == NULL) {  
	PrintDebug("Vnet: can not find route in cache, looking into routing table\n");
	
	matched_routes = match_route(pkt);
		
      	if (matched_routes) {
	    add_route_to_cache(pkt, matched_routes);
	} else {
	    PrintDebug("Could not find route for packet...\n");
	    v3_unlock_irqrestore(vnet_state.lock, flags);
	    return -1;
	}
    }

    v3_unlock_irqrestore(vnet_state.lock, flags);

#ifdef CONFIG_VNET_PROFILE
    {
    	rdtscll(end);
	core->vnet_times.time_route_lookup = end - start;
    }
#endif

    PrintDebug("Vnet: HandleOnePacket: route matches %d\n", matched_routes->num_routes);
    for (i = 0; i < matched_routes->num_routes; i++) {
	 struct vnet_route_info * route = matched_routes->routes[i];
	
        if (route->route_def.dst_type == LINK_EDGE) {
            pkt->dst_type = LINK_EDGE;
            pkt->dst_id = route->route_def.dst_id;
            if (vnet_state.bridge == NULL) {
                PrintDebug("VNET: No bridge to sent data to links\n");
                continue;
            }
            if (vnet_state.bridge->input(vnet_state.bridge->vm, pkt, vnet_state.bridge->private_data) == -1) {
                PrintDebug("VNET: Packet not sent properly\n");
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

        PrintDebug("Vnet: HandleOnePacket: Forward packet according to Route\n");
    }

#ifdef CONFIG_VNET_PROFILE
    {
    	rdtscll(start);
	core->vnet_times.time_copy_to_guest = start - end;
    }
#endif
    
    return 0;
}

int v3_vnet_send_pkt(struct v3_vnet_pkt * pkt, void *private_data) {
    PrintDebug("In Vnet Send: pkt size: %d\n", pkt->size);
		
    if (handle_one_pkt(pkt, private_data) != -1) {
        PrintDebug("VNET: send one packet! pt length %d\n", pkt->size);  
    } else {
        PrintDebug("VNET: Fail to forward one packet, discard it!\n"); 
    }

    return 0;
}

int v3_vnet_add_dev(struct v3_vm_info *vm,uint8_t mac[6], 
		    int (*netif_input)(struct v3_vm_info * vm, struct v3_vnet_pkt * pkt, void * private_data), 
		    void * priv_data){
    struct vnet_dev * new_dev = NULL;
    unsigned long flags;
    int dev_id;

    flags = v3_lock_irqsave(vnet_state.lock);
	
    new_dev = find_dev_by_mac(mac);

    if (new_dev) {
	PrintDebug("VNET: register device: Already has device with the same mac\n");
	dev_id = -1;
	goto exit;
    }
    
    new_dev = (struct vnet_dev *)V3_Malloc(sizeof(struct vnet_dev)); 

    if (new_dev == NULL) {
	PrintError("VNET: Malloc fails\n");
	dev_id = -1;
	goto exit;
    }
   
    memcpy(new_dev->mac_addr, mac, 6);
    new_dev->input = netif_input;
    new_dev->private_data = priv_data;
    new_dev->vm = vm;
	
    list_add(&(new_dev->node), &(vnet_state.devs));
    vnet_state.num_devs ++;
    new_dev->dev_id = vnet_state.num_devs;
    dev_id = new_dev->dev_id;

    PrintDebug("Vnet: Add Device: dev_id %d, input : %p, private_data %p\n",
			new_dev->dev_id, new_dev->input, new_dev->private_data);

exit:
	
    v3_unlock_irqrestore(vnet_state.lock, flags);
 
    return dev_id;
}


int v3_vnet_add_bridge(struct v3_vm_info * vm,
				int (*input)(struct v3_vm_info * vm, struct v3_vnet_pkt * pkt, void * private_data), 
		    		void * priv_data){
    unsigned long flags;
	
    flags = v3_lock_irqsave(vnet_state.lock);

    if(vnet_state.bridge != NULL){
	PrintDebug("Vnet: Replace current bridge with a new one\n");
    } else {
    	vnet_state.bridge = (struct vnet_brg_dev *)V3_Malloc(sizeof(struct vnet_brg_dev));
	if (vnet_state.bridge == NULL) {
	    PrintError("Malloc Fails\n");
	    return -1;
	}
    }

    vnet_state.bridge->vm = vm;
    vnet_state.bridge->input = input;
    vnet_state.bridge->private_data = priv_data;

    v3_unlock_irqrestore(vnet_state.lock, flags);

    return 0;
}

int V3_init_vnet() {
	
    INIT_LIST_HEAD(&(vnet_state.routes));
    INIT_LIST_HEAD(&(vnet_state.devs));

    vnet_state.num_devs = 0;
    vnet_state.num_routes = 0;

    PrintDebug("VNET: Links and Routes tables initiated\n");

    if (v3_lock_init(&(vnet_state.lock)) == -1){
        PrintError("VNET: Failure to init lock for routes table\n");
    }

    PrintDebug("VNET: Locks initiated\n");

    vnet_state.inpkt_q = v3_create_queue();
    v3_init_queue(vnet_state.inpkt_q);
    PrintDebug("VNET: Receiving queue initiated\n");

    vnet_state.route_cache = v3_create_htable(0, &hash_fn, &hash_eq);

    if (vnet_state.route_cache == NULL) {
        PrintError("Vnet: Route Cache Init Fails\n");
        return -1;
    }

    PrintDebug("VNET: initiated\n");

    return 0;
}
