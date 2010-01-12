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
 * Copyright (c) 2009, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2009, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu>
 *	   Yuan Tang <ytang@northwestern.edu>
 *	   Jack Lange <jarusl@cs.northwestern.edu> 
 *	   Peter Dinda <pdinda@northwestern.edu
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
 
#include <palacios/vmm_vnet.h>
#include <palacios/vmm_hypercall.h>
#include <palacios/vm_guest_mem.h>

#ifndef CONFIG_DEBUG_VNET
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


struct ethernet_pkt {
    uint32_t size; //size of data
    uint16_t type;
    struct udp_link_header ext_hdr; //possible externel header to applied to data before sent
    char data[ETHERNET_PACKET_LEN];
};


// 14 (ethernet frame) + 20 bytes
struct in_pkt_header {
    char ethernetdest[6];
    char ethernetsrc[6];
    unsigned char ethernettype[2]; //layer 3 protocol type
    char ip[20];
};

#define VNET_INITAB_HCALL 0xca00  // inital hypercall id

#define MAX_LINKS 10
#define MAX_ROUTES 10
#define HASH_KEY_LEN 16
#define MIN_CACHE_SIZE 100
static const uint_t hash_key_size = 16;

struct link_table {
    struct link_entry * links[MAX_LINKS];
    uint16_t size;
};

struct routing_table {
    struct routing_entry * routes[MAX_ROUTES];
    uint16_t size;
};

static struct link_table g_links;
static struct routing_table g_routes;
static struct gen_queue * g_inpkt_q;

/* Hash key format:
 * 0-5:     src_eth_addr
 * 6-11:    dest_eth_addr
 * 12:      src type
 * 13-16:   src index
 */
typedef char * route_hashkey_t;

// This is the hash value, Format: 0: num_matched_routes, 1...n: matches[] -- TY
struct route_cache_entry {
    int num_matched_routes;
    int * matches; 
};

// the route cache
static struct hashtable * g_route_cache; 


static void print_packet(uchar_t *pkt, int size) {
    PrintDebug("Vnet: print_data_packet: size: %d\n", size);
    v3_hexdump(pkt, size, NULL, 0);
}

#if 0
static void print_packet_addr(char * pkt) {
    PrintDebug("Vnet: print_packet_destination_addr: ");
    v3_hexdump(pkt + 8, 6, NULL, 0);
    
    PrintDebug("Vnet: print_packet_source_addr: ");
    v3_hexdump(pkt + 14, 6, NULL, 0);
}

static void print_device_addr(char * ethaddr) {
    PrintDebug("Vnet: print_device_addr: ");
    v3_hexdump(ethaddr, 6, NULL, 0);
} 
#endif

static uint16_t ip_xsum(struct ip_header *ip_hdr, int hdr_len){
    long sum = 0;

    uint16_t *p = (uint16_t*) ip_hdr;
    while(hdr_len > 1){
        sum += *(p++);
        if(sum & 0x80000000)
            sum = (sum & 0xFFFF) + (sum >> 16);
            hdr_len -= 2;
        }
 
    if(hdr_len) 
        sum += (uint16_t) *(uchar_t *)p;
          
    while(sum>>16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)~sum;
}

static inline void ethernet_packet_init(struct ethernet_pkt *pt, uchar_t *data, const size_t size) {
    pt->size = size;
    memcpy(pt->data, data, size);
}

static uint_t hash_from_key_fn(addr_t hashkey) {    
    uint8_t * key = (uint8_t *)hashkey;
    return v3_hash_buffer(key, HASH_KEY_LEN);
}

static int hash_key_equal(addr_t key1, addr_t key2) {
    uint8_t * buf1 = (uint8_t *)key1;
    uint8_t * buf2 = (uint8_t *)key2;
    return (memcmp(buf1, buf2, HASH_KEY_LEN) == 0);
}

static int init_route_cache() {
    g_route_cache = v3_create_htable(MIN_CACHE_SIZE, &hash_from_key_fn, &hash_key_equal);

    if (g_route_cache == NULL) {
        PrintError("Vnet: Route Cache Initiate Failurely\n");
        return -1;
    }

    return 0;
}

static void make_hash_key(route_hashkey_t hashkey,
			  char src_addr[6],
			  char dest_addr[6],
			  char src_type,
			  int src_index) {
    int j;

    for (j = 0; j < 6; j++) {
	hashkey[j] = src_addr[j];
	hashkey[j + 6] = dest_addr[j] + 1;
    }

    hashkey[12] = src_type;

    *(int *)(hashkey + 12) = src_index;
}

static int add_route_to_cache(route_hashkey_t hashkey, int num_matched_r, int * matches) {
    struct route_cache_entry * new_entry = NULL;
    int i;
    
    new_entry = (struct route_cache_entry *)V3_Malloc(sizeof(struct route_cache_entry));
    if (new_entry == NULL) {
	PrintError("Vnet: Malloc fails\n");
	return -1;
    }
    
    new_entry->num_matched_routes = num_matched_r;

    new_entry->matches = (int *)V3_Malloc(num_matched_r * sizeof(int));
    
    if (new_entry->matches == NULL) {
	PrintError("Vnet: Malloc fails\n");
	return -1;
    }
    
    for (i = 0; i < num_matched_r; i++) {
	new_entry->matches[i] = matches[i];
    }
    
    // here, when v3_htable_insert return 0, it means insert fails
    if (v3_htable_insert(g_route_cache, (addr_t)hashkey, (addr_t)new_entry) == 0) {
	PrintError("Vnet: Insert new route entry to cache failed\n");
	V3_Free(new_entry->matches);
	V3_Free(new_entry);
    }
    
    return 0;
}

static int clear_hash_cache() {
    v3_free_htable(g_route_cache, 1, 1);
		
    g_route_cache = v3_create_htable(MIN_CACHE_SIZE, hash_from_key_fn, hash_key_equal);
    
    if (g_route_cache == NULL) {
        PrintError("Vnet: Route Cache Create Failurely\n");
        return -1;
    }

    return 0;
}

static int look_into_cache(route_hashkey_t hashkey, int * matches) {
    struct route_cache_entry * found = NULL;
    int n_matches = -1;
    int i = 0;
    
    found = (struct route_cache_entry *)v3_htable_search(g_route_cache, (addr_t)hashkey);
   
    if (found != NULL) {
        n_matches = found->num_matched_routes;

        for (i = 0; i < n_matches; i++) {
            matches[i] = found->matches[i];
	}
    }

    return n_matches;
}

#ifdef CONFIG_DEBUG_VNET
static inline uint8_t hex_nybble_to_nybble(const uint8_t hexnybble) {
    uint8_t x = toupper(hexnybble);

    if (isdigit(x)) {
	return x - '0';
    } else {
	return 10 + (x - 'A');
    }
}

static inline uint8_t hex_byte_to_byte(const uint8_t hexbyte[2]) {
    return ((hex_nybble_to_nybble(hexbyte[0]) << 4) + 
	    (hex_nybble_to_nybble(hexbyte[1]) & 0xf));
}

static inline void string_to_mac(const char *str, uint8_t mac[6]) {
    int k;

    for (k = 0; k < 6; k++) {
	mac[k] = hex_byte_to_byte(&(str[(2 * k) + k]));
    }
}

static inline void mac_to_string(char mac[6], char * buf) {
    snprintf(buf, 20, "%x:%x:%x:%x:%x:%x", 
	     mac[0], mac[1], mac[2],
	     mac[3], mac[4], mac[5]);
}
#endif

static int __add_link_entry(struct link_entry * link) {
    int idx;
    
    for (idx = 0; idx < MAX_LINKS; idx++) {
	if (g_links.links[idx] == NULL) {
 	    g_links.links[idx] = link;
	    g_links.size++;
	    
	    return idx;
	}
    }

    PrintError("No available Link entry\n");
    return -1;
}

static int __add_route_entry(struct routing_entry * route) {
    int idx;
    
    for (idx = 0; idx < MAX_ROUTES; idx++) {
	if (g_routes.routes[idx] == NULL) {
 	    g_routes.routes[idx] = route;
	    g_routes.size++;
	    
	    return idx;
	}
    }

    PrintError("No available route entry\n");
    return -1;
}


static int vnet_add_route_entry(char src_mac[6],
				char dest_mac[6],
				int src_mac_qual,
				int dest_mac_qual,
				int link_idx,
				link_type_t link_type,
				int src,
				link_type_t src_type) {
    struct routing_entry * new_route = (struct routing_entry *)V3_Malloc(sizeof(struct routing_entry));
    int idx = -1;

    memset(new_route, 0, sizeof(struct routing_entry));

    if ((src_mac_qual != MAC_ANY) && (src_mac_qual != MAC_NONE)) {
        memcpy(new_route->src_mac, src_mac, 6);
    }
	    
    if ((dest_mac_qual != MAC_ANY) && (dest_mac_qual != MAC_NONE)) {
        memcpy(new_route->dest_mac, dest_mac, 6);
    }
	    
    new_route->src_mac_qual = src_mac_qual;
    new_route->dest_mac_qual = dest_mac_qual;
    new_route->link_idx= link_idx;
    new_route->link_type = link_type;
    new_route->src_link_idx = src;
    new_route->src_type = src_type;

    if ((idx = __add_route_entry(new_route)) == -1) {
	PrintError("Could not add route entry\n");
        return -1;
    }
    
    clear_hash_cache();
    
    return idx;
}

static void * __delete_link_entry(int index) {
    struct link_entry * link = NULL;
    void * ret = NULL;
    link_type_t type;
  
    if ((index >= MAX_LINKS) || (g_links.links[index] == NULL)) {
	return NULL;
    }

    link = g_links.links[index];
    type = g_links.links[index]->type;

    if (type == LINK_INTERFACE) {
    	ret = (void *)g_links.links[index]->dst_dev;
    } else if (type == LINK_EDGE) {
	ret = (void *)g_links.links[index]->dst_link;
    }

    g_links.links[index] = NULL;
    g_links.size--;

    V3_Free(link);
	
    return ret;
}

static int find_route_entry(char src_mac[6], 
			    char dest_mac[6], 
			    int src_mac_qual, 
			    int dest_mac_qual, 
			    int link_idx, 
			    link_type_t link_type, 
			    int src, 
			    link_type_t src_type) {
    int i;
    char temp_src_mac[6];
    char temp_dest_mac[6];
  
    if ((src_mac_qual != MAC_ANY) && (src_mac_qual != MAC_NONE)) {
	memcpy(temp_src_mac, src_mac, 6);
    } else {
	memset(temp_src_mac, 0, 6);
    }
    
    if ((dest_mac_qual != MAC_ANY) && (dest_mac_qual != MAC_NONE)) {
	memcpy(temp_dest_mac, dest_mac, 6);
    } else {
	memset(temp_dest_mac, 0, 6);
    }
    
    for (i = 0; i < MAX_ROUTES; i++) {
	if (g_routes.routes[i] != NULL) {
	    if ((memcmp(temp_src_mac, g_routes.routes[i]->src_mac, 6) == 0) && 
		(memcmp(temp_dest_mac, g_routes.routes[i]->dest_mac, 6) == 0) &&
		(g_routes.routes[i]->src_mac_qual == src_mac_qual) &&
		(g_routes.routes[i]->dest_mac_qual == dest_mac_qual)  &&
		( (link_type == LINK_ANY) || 
		  ((link_type == g_routes.routes[i]->link_type) && (g_routes.routes[i]->link_idx == link_idx))) &&
		( (src_type == LINK_ANY) || 
		  ((src_type == g_routes.routes[i]->src_type) && (g_routes.routes[i]->src_link_idx == src)))) {
	        return i;
	    }
        } 
     }
    
    return -1;
}

static int __delete_route_entry(int index) {
    struct routing_entry * route;

    if ((index >= MAX_ROUTES) || (g_routes.routes[index] == NULL)) {
	PrintDebug("VNET: wrong index in delete route entry %d\n", index);
	return -1;
    }

    route = g_routes.routes[index];
    g_routes.routes[index] = NULL;
    g_routes.size--;

    V3_Free(route);

    clear_hash_cache();
    
    return 0;
}

static int vnet_delete_route_entry_by_addr(char src_mac[6], 
					   char dest_mac[6], 
					   int src_mac_qual, 
					   int dest_mac_qual, 
					   int link_idx, 
					   link_type_t type, 
					   int src, 
					   link_type_t src_type) {
    int index = find_route_entry(src_mac, dest_mac, src_mac_qual, 
				 dest_mac_qual, link_idx, type, src, src_type);
    
    if (index == -1) {
	PrintDebug("VNET: wrong in delete route entry %d\n", index);
	return -1;
    }
    
    return __delete_route_entry(index);
}

static int match_route(uint8_t * src_mac, 
		       uint8_t * dst_mac, 
		       link_type_t src_type, 
		       int src_index, 
		       int * matches) { 
    int values[MAX_ROUTES];
    int matched_routes[MAX_ROUTES];
    
    int num_matches = 0;
    int i;
    int max = 0;
    int no = 0;
    int exact_match = 0;

    for(i = 0; i < MAX_ROUTES; i++) {
	if (g_routes.routes[i] != NULL){
	    if ( (g_routes.routes[i]->src_type != LINK_ANY) &&
		 ( (g_routes.routes[i]->src_type != src_type) ||
		   ( (g_routes.routes[i]->src_link_idx != src_index) &&
		     (g_routes.routes[i]->src_link_idx != -1)))) {
	        PrintDebug("Vnet: MatchRoute: Source route is on and does not match\n");
	        continue;
	    }
	
	    if ( (g_routes.routes[i]->dest_mac_qual == MAC_ANY) &&
		 (g_routes.routes[i]->src_mac_qual == MAC_ANY)) {      

		matched_routes[num_matches] = i;
		values[num_matches] = 3;
		num_matches++;
	    }
	
	    if (memcmp((void *)&g_routes.routes[i]->src_mac, (void *)src_mac, 6) == 0) {
	        if (g_routes.routes[i]->src_mac_qual !=  MAC_NOT) {
		    if (g_routes.routes[i]->dest_mac_qual == MAC_ANY) {
		        matched_routes[num_matches] = i;
		        values[num_matches] = 6;
			
		        num_matches++;
		    } else if (memcmp((void *)&g_routes.routes[i]->dest_mac, (void *)dst_mac, 6) == 0) {
		        if (g_routes.routes[i]->dest_mac_qual != MAC_NOT) {   
			    matched_routes[num_matches] = i;
			    values[num_matches] = 8;    
			    exact_match = 1;
			    num_matches++;
		        }
		    }
	        }
	    }
	
	    if (memcmp((void *)&g_routes.routes[i]->dest_mac, (void *)dst_mac, 6) == 0) {
	        if (g_routes.routes[i]->dest_mac_qual != MAC_NOT) {
		    if (g_routes.routes[i]->src_mac_qual == MAC_ANY) {
		        matched_routes[num_matches] = i;
		        values[num_matches] = 6;
			
		        num_matches++;
		    } else if (memcmp((void *)&g_routes.routes[i]->src_mac, (void *)src_mac, 6) == 0) {
		        if (g_routes.routes[i]->src_mac_qual != MAC_NOT) {
			    if (exact_match == 0) {
			        matched_routes[num_matches] = i;
			        values[num_matches] = 8;
			        num_matches++;
			    }
		        }
		    }
	        }
	    }
	
	    if ( (g_routes.routes[i]->dest_mac_qual == MAC_NOT) &&
		 (memcmp((void *)&g_routes.routes[i]->dest_mac, (void *)dst_mac, 6) != 0)) {
	        if (g_routes.routes[i]->src_mac_qual == MAC_ANY) {
		    matched_routes[num_matches] = i;
		    values[num_matches] = 5;		    
		    num_matches++;    
	        } else if (memcmp((void *)&g_routes.routes[i]->src_mac, (void *)src_mac, 6) == 0) {
		    if (g_routes.routes[i]->src_mac_qual != MAC_NOT) {      
			matched_routes[num_matches] = i;
			values[num_matches] = 7;		      
			num_matches++;
		    }
	        }
	    }
	    
	    if ( (g_routes.routes[i]->src_mac_qual == MAC_NOT) &&
		 (memcmp((void *)&g_routes.routes[i]->src_mac, (void *)src_mac, 6) != 0)) {
		if (g_routes.routes[i]->dest_mac_qual == MAC_ANY) {
		    matched_routes[num_matches] = i;
		    values[num_matches] = 5;	    
		    num_matches++;
		} else if (memcmp((void *)&g_routes.routes[i]->dest_mac, (void *)dst_mac, 6) == 0) {
		    if (g_routes.routes[i]->dest_mac_qual != MAC_NOT) { 
			matched_routes[num_matches] = i;
			values[num_matches] = 7;
			num_matches++;
		    }
		}
	    }
       }
    } // end for
    
    for (i = 0; i < MAX_ROUTES; i++) {
    	if ( (memcmp((void *)&g_routes.routes[i]->src_mac, (void *)src_mac, 6) == 0) &&
	     (g_routes.routes[i]->dest_mac_qual == MAC_NONE) &&
	     ( (g_routes.routes[i]->src_type == LINK_ANY) ||
	       ( (g_routes.routes[i]->src_type == src_type) &&
		 ( (g_routes.routes[i]->src_link_idx == src_index) ||
		   (g_routes.routes[i]->src_link_idx == -1))))) {
	    matched_routes[num_matches] = i;
	    values[num_matches] = 4;
	    PrintDebug("Vnet: MatchRoute: We matched a default route (%d)\n", i);
	    num_matches++;
    	}
    }
    
    //If many rules have been matched, we choose one which has the highest value rating
    if (num_matches == 0) {
    	return 0;
    }
    
    for (i = 0; i < num_matches; i++) {
    	if (values[i] > max) {
	    no = 0;
	    max = values[i];
	    matches[no] = matched_routes[i];
	    no++;
    	} else if (values[i] == max) {
	    matches[no] = matched_routes[i];
	    no++;
    	}
    }
    
    return no;
}

static int handle_one_pkt(struct ethernet_pkt *pkt) {
    int src_link_index = -1;	//the value of src_link_index of udp always is 0
    int i;
    char src_mac[6];
    char dst_mac[6];

    int matches[MAX_ROUTES];
    int num_matched_routes = 0;

    struct in_pkt_header header;

    char hash_key[hash_key_size];
  
    // get the ethernet and ip headers from the packet
    memcpy((void *)&header, (void *)pkt->data, sizeof(header));
    memcpy(src_mac, header.ethernetsrc, 6);
    memcpy(dst_mac, header.ethernetdest, 6);

#ifdef CONFIG_DEBUG_VNET
    char dest_str[18];
    char src_str[18];
    
    mac_to_string(src_mac, src_str);  
    mac_to_string(dst_mac, dest_str);
    
    PrintDebug("Vnet: HandleDataOverLink. SRC(%s), DEST(%s)\n", src_str, dest_str);
#endif

    // link_edge -> pt->type???
    make_hash_key(hash_key, src_mac, dst_mac, LINK_EDGE, src_link_index); 
    
    num_matched_routes = look_into_cache((route_hashkey_t)hash_key, matches);
    
    if (num_matched_routes == -1) {  
    // no match in the cache
        num_matched_routes = match_route(src_mac, dst_mac, LINK_ANY, src_link_index, matches);
	
        if (num_matched_routes > 0) {
	     add_route_to_cache(hash_key, num_matched_routes,matches);      
	 }
    }
    
    PrintDebug("Vnet: HandleDataOverLink: Matches=%d\n", num_matched_routes);
    
    for (i = 0; i < num_matched_routes; i++) {//send packet to all destinations
        int route_index = -1;
        int link_index = -1;
        int pkt_len = 0;
        struct link_entry * link = NULL;

        route_index = matches[i];
	
        PrintDebug("Vnet: HandleDataOverLink: Forward packet from link according to Route entry %d\n", route_index);
	
        link_index = g_routes.routes[route_index]->link_idx;

        if ((link_index < 0) || 
	     (link_index > MAX_LINKS) || 
	     (g_links.links[link_index] == NULL)) {
	     continue;
	 }
	
        link = g_links.links[link_index];
        pkt_len = pkt->size;
        if (g_routes.routes[route_index]->link_type == LINK_EDGE) {

	      //apply the header in the beginning of the packet to be sent
            if (link->dst_link->pro_type == UDP_TYPE) {
                struct udp_link_header *hdr = &(link->dst_link->vnet_header);
                struct ip_header *ip = &hdr->ip_hdr;
                struct udp_header *udp = &hdr->udp_hdr;
		   udp->len = pkt_len + sizeof(struct udp_header);
		   ip->total_len = pkt_len + sizeof(struct udp_header) + sizeof(struct ip_header);
		   ip->cksum = ip_xsum(ip, sizeof(struct ip_header));
		
                int hdr_size = sizeof(struct udp_link_header);
                memcpy(&pkt->ext_hdr, hdr, hdr_size);

                pkt_len += hdr_size;
                if ((link->dst_link->input((uchar_t *)&pkt->ext_hdr, pkt_len, link->dst_link->private_data)) != pkt_len) {
                    PrintDebug("VNET: Packet not sent properly\n");
                    return -1;
                }
	     }else {
                PrintDebug("VNET: Link protocol type not support\n");
                return -1;
	     }
        } else if (g_routes.routes[route_index]->link_type == LINK_INTERFACE) {
            if ((link->dst_dev->input(pkt->data, pkt_len, link->dst_dev->private_data)) != pkt_len) {
		   PrintDebug("VNET: Packet not sent properly\n");
		   return -1;
	     }
        } else {
            PrintDebug("Vnet: Wrong Edge type\n");
            return -1;
        }
    }
    
    return 0;
}

static int send_ethernet_pkt(uchar_t *data, int len) {
    struct ethernet_pkt *pkt;

    pkt = (struct ethernet_pkt *)V3_Malloc(sizeof(struct ethernet_pkt));
    memset(pkt, 0, sizeof(struct ethernet_pkt));

    if(pkt == NULL){
        PrintError("VNET: Memory allocate fails\n");
        return -1;
    }
	
    ethernet_packet_init(pkt, data, len);  //====here we copy sending data once 
	
    PrintDebug("VNET: vm_send_pkt: transmitting packet: (size:%d)\n", (int)pkt->size);
    print_packet((char *)data, len);
    
    v3_enqueue(g_inpkt_q, (addr_t)pkt);

    return 0;
}

//send raw ethernet packet
int v3_vnet_send_rawpkt(uchar_t * buf, int len, void *private_data) {
    PrintDebug("VNET: In V3_Send_pkt: pkt length %d\n", len);
    
    return send_ethernet_pkt(buf, len);
}

//sending the packet from Dom0, should remove the link header
int v3_vnet_send_udppkt(uchar_t * buf, int len, void *private_data) {
    PrintDebug("VNET: In V3_Send_pkt: pkt length %d\n", len);

    uint_t hdr_len = sizeof(struct udp_link_header);
   
    return send_ethernet_pkt((uchar_t *)(buf+hdr_len), len - hdr_len);
}

static int search_device(char * device_name) {
    int i;

    for (i = 0; i < MAX_LINKS; i++) {
        if ((g_links.links[i] != NULL) && (g_links.links[i]->type == LINK_INTERFACE)) {
	    if (strcmp(device_name, g_links.links[i]->dst_dev->name) == 0) {
		return i;
	    }
        }
    }

    return -1;
}

int vnet_register_device(struct vm_device * vdev, 
			 char * dev_name, 
			 uchar_t mac[6], 
			 int (*netif_input)(uchar_t * pkt, uint_t size, void * private_data), 
			 void * data) {
    struct vnet_if_device * if_dev;

    int idx = search_device(dev_name);

    if (idx != -1) {
	PrintDebug("VNET: register device: Already has device with the name %s\n", dev_name);
	return -1;
    }
    
    if_dev = (struct vnet_if_device *)(sizeof(struct vnet_if_device));
    
    if (if_dev == NULL) {
	PrintError("VNET: Malloc fails\n");
	return -1;
    }
    
    strcpy(if_dev->name, dev_name);
    strncpy(if_dev->mac_addr, mac, 6);
    if_dev->dev = vdev;
    if_dev->input = netif_input;
    if_dev->private_data = data;

    struct link_entry * link = (struct link_entry *)(sizeof(struct link_entry));

    link->type = LINK_INTERFACE;
    link->dst_dev = if_dev;

    idx = __add_link_entry(link);

    PrintDebug("VNET: Add device %s to link table, idx %d", dev_name, idx);
    
    return idx;
}

#if 0
static int vnet_unregister_device(char * dev_name) {
    int idx;

    idx = search_device(dev_name);
    
    if (idx == -1) {
	PrintDebug("VNET: No device with name %s found\n", dev_name);
        return -1;
    }

    struct vnet_if_device * device = (struct vnet_if_device *)__delete_link_entry(idx);
    if (device == NULL) {
	PrintError("VNET: Device %s not in the link table %d, something may be wrong in link table\n", dev_name, idx);
	return -1;
    }

    V3_Free(device);

    return idx;
}

#endif

int v3_vnet_pkt_process() {
    struct ethernet_pkt * pkt;

    PrintDebug("VNET: In vnet_check\n");
	
    while ((pkt = (struct ethernet_pkt *)v3_dequeue(g_inpkt_q)) != NULL) {
	PrintDebug("VNET: In vnet_check: pt length %d, pt type %d\n", (int)pkt->size, (int)pkt->type);
	v3_hexdump(pkt->data, pkt->size, NULL, 0);
	
	if (handle_one_pkt(pkt)) {
	    PrintDebug("VNET: vnet_check: handle one packet!\n");  
	} else {
	    PrintError("VNET: vnet_check: fail to forward one packet, discard it!\n"); 
	}
	
	V3_Free(pkt); // be careful here
    }
    
    return 0;
}


static void init_empty_link_table() {
    int i;

    for (i = 0; i < MAX_LINKS; i++) {
        g_links.links[i] = NULL;
    }

    g_links.size = 0;
}

static void init_empty_route_table() {	
    int i;

    for (i = 0; i < MAX_ROUTES; i++) {
        g_routes.routes[i] = NULL;
    }

    g_links.size = 0;
}

static void init_tables() {
    init_empty_link_table();
    init_empty_route_table();
    init_route_cache();
}

static void init_pkt_queue() {
    PrintDebug("VNET Init package receiving queue\n");

    g_inpkt_q = v3_create_queue();
    v3_init_queue(g_inpkt_q);
}

static void free_link_mem(struct link_entry *link){
    V3_Free(link->dst_dev);
    V3_Free(link);
}

// TODO:
static int addto_routing_link_tables(struct routing_entry *route_tab, 
							uint16_t num_routes,
							struct link_entry *link_tab,
							uint16_t num_links){
    struct routing_entry *route, *new_route;
    struct link_entry *link, *new_link;
    int new_idx;
    int i;
    int link_idxs[MAX_LINKS];

    //add all of the links first, record their new indexs
    for (i = 0; i < num_links; i++) {
	link_idxs[i] = -1;
	link = &link_tab[i];
	new_link = (struct link_entry *)V3_Malloc(sizeof(struct link_entry));

	if (new_link == NULL){
	    PrintError("VNET: Memory allocate error\n");
	    return -1;
	}

	new_link->type = link->type;
	
	//TODO: how to set the input parameters here
	if (link->type == LINK_EDGE){
      	    struct vnet_if_device *ifdev = (struct vnet_if_device *)V3_Malloc(sizeof(struct vnet_if_device));

	    if (ifdev == NULL){
		PrintError("VNET: Memory allocate fails\n");
		return -1;
	    }
	    
	    memcpy(ifdev->name, link->dst_dev->name, DEVICE_NAME_LEN);

	    // TODO:
	    //ifdev->mac_addr
	    //ifdev->input
	    //ifdev->private_data

	    new_link->dst_dev = ifdev;    
	}else if (link->type == LINK_INTERFACE){
	    struct vnet_if_link *iflink = (struct vnet_if_link *)V3_Malloc(sizeof(struct vnet_if_link));

	    if (iflink == NULL){
		PrintError("VNET: Memory allocate fails\n");
		return -1;
	    }
	    iflink->pro_type = link->dst_link->pro_type;
	    iflink->dest_ip = link->dst_link->dest_ip;
	    iflink->dest_port = link->dst_link->dest_port;
	    memcpy(&iflink->vnet_header, &link->dst_link->vnet_header, sizeof(struct udp_link_header));

	    // TODO:
	    //iflink->input = 
	    //iflink->private_data = 
	    
	    new_link->dst_link = iflink;
	}else{
	    PrintDebug("VNET: invalid link type\n");
	    V3_Free(new_link);
	    continue;
	}
	
	new_idx = __add_link_entry(new_link);
	if (new_idx < 0) {
	    PrintError("VNET: Adding link fails\n");
	    free_link_mem(new_link);
	    continue;
	}	
	link_idxs[i] = new_idx;
    }

    //add all of routes, replace with new link indexs
    for (i = 0; i < num_routes; i++) {
	route = &route_tab[i];
       if (route->link_idx < 0 || route->link_idx >= num_links || 
	    ((route->src_link_idx != -1) && 
	      (route->src_link_idx < 0 || route->src_link_idx >= num_links))){
	    PrintError("VNET: There is error in the intial tables data from guest\n");
	    continue;
	}

	new_route = (struct routing_entry *)V3_Malloc(sizeof(struct routing_entry));

	if (new_route == NULL){
	    PrintError("VNET: Memory allocate fails\n");
	    return -1;
	}
	memcpy(new_route, route, sizeof(struct routing_entry));
	
	new_route->link_idx = link_idxs[new_route->link_idx];
		
	if (route->src_link_idx != -1)
	    new_route->src_link_idx = link_idxs[new_route->src_link_idx];

	if((__add_route_entry(new_route)) == -1){
	    PrintDebug("VNET: Adding route fails");
	    V3_Free(new_route);
	}
	new_route = NULL;	
    }


    return 0;
}

struct table_init_info {
    addr_t routing_table_start;
    uint16_t routing_table_size;
    addr_t link_table_start;
    uint16_t link_table_size;
};

//add the guest specified routes and links to the tables
static int handle_init_tables_hcall(struct guest_info * info, uint_t hcall_id, void * priv_data) {
    addr_t guest_addr = (addr_t)info->vm_regs.rcx;
    addr_t info_addr, route_addr, link_addr;
    struct table_init_info *init_info;
    struct link_entry *link_array;
    struct routing_entry *route_array;   

    if (guest_va_to_host_va(info, guest_addr, &info_addr) == -1) {
	PrintError("VNET: Could not translate guest address\n");
	return -1;
    }

    init_info = (struct table_init_info *)info_addr;

    if (guest_va_to_host_va(info, init_info->routing_table_start, &route_addr) == -1) {
	PrintError("VNET: Could not translate guest address\n");
	return -1;
    }
    route_array = (struct routing_entry *)route_addr;

    if (guest_va_to_host_va(info, init_info->link_table_start, &link_addr) == -1) {
	PrintError("VNET: Could not translate guest address\n");
	return -1;
    }	
    link_array = (struct link_entry *)link_addr;

    addto_routing_link_tables(route_array, init_info->routing_table_size, link_array, init_info->link_table_size);
    
    return 0;
}


void v3_vnet_init(struct guest_info * vm) {
    init_tables();
    init_pkt_queue();
	
    v3_register_hypercall(vm, VNET_INITAB_HCALL, handle_init_tables_hcall, NULL);

    PrintDebug("VNET Initialized\n");
}


