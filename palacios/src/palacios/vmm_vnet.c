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
#include <palacios/vmm_hypercall.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_lock.h>

#ifndef CONFIG_DEBUG_VNET
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define ETHERNET_HEADER_LEN 14
#define ETHERNET_DATA_MIN   46
#define ETHERNET_DATA_MAX   1500
#define ETHERNET_PACKET_LEN (ETHERNET_HEADER_LEN + ETHERNET_DATA_MAX)


typedef enum {TCP_TYPE, UDP_TYPE, NONE_TYPE} prot_type_t;

#define VNET_INITAB_HCALL 0xca00

struct eth_header {
    uchar_t dest[6];
    uchar_t src[6];
    uint16_t type; // indicates layer 3 protocol type
}__attribute__((packed));

struct ip_header {
    uint8_t version: 4;
    uint8_t hdr_len: 4;
    uchar_t tos;
    uint16_t total_len;
    uint16_t id;
    uint8_t flags:     3;
    uint16_t offset: 13;
    uchar_t ttl;
    uchar_t proto;
    uint16_t cksum;
    uint32_t src_addr;
    uint32_t dst_addr;
}__attribute__((packed));

struct udp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t csum;//set to zero, disable the xsum
}__attribute__((packed));

struct udp_link_header {
    struct eth_header eth_hdr;
    struct ip_header ip_hdr;
    struct udp_header udp_hdr;
}__attribute__((packed));


struct ethernet_pkt {
    uint32_t size; //size of data field
    uint16_t type;
    uint8_t use_header;
    struct udp_link_header ext_hdr;
    char data[ETHERNET_PACKET_LEN];
}__attribute__((packed));


#define DEVICE_NAME_LEN 20
struct vnet_if_device {
    char name[DEVICE_NAME_LEN];
    uchar_t mac_addr[6];
    struct v3_vm_info *vm;
    
    int (*input)(struct v3_vm_info *vm, uchar_t *data, uint32_t len, void *private_data);
    
    void *private_data;
}__attribute__((packed));

struct vnet_if_link {
    prot_type_t pro_type; //transport layer protocal type of this link
    unsigned long dest_ip;
    uint16_t dest_port;

    struct udp_link_header vnet_header; //header applied to the packet in/out from this link

    int (*input)(uchar_t *data, uint32_t len, void *private_data);
    
    void *private_data;
}__attribute__((packed));


struct link_entry {
    link_type_t type;
  
    union {
	struct vnet_if_device *dst_dev;
	struct vnet_if_link *dst_link;
    } __attribute__((packed));

    int use;
}__attribute__((packed));

//routing table entry
struct routing_entry {
    char src_mac[6];
    char dest_mac[6];

    mac_type_t src_mac_qual;
    mac_type_t dest_mac_qual;

    int link_idx; //link[dest] is the link to be used to send pkt
    link_type_t link_type; //EDGE|INTERFACE|ANY
 
    int src_link_idx;
    link_type_t src_type; //EDGE|INTERFACE|ANY
};


// 14 (ethernet frame) + 20 bytes
struct in_pkt_header {
    uchar_t ethernetdest[6];
    uchar_t ethernetsrc[6];
    uchar_t ethernettype[2]; //layer 3 protocol type
    char ip[20];
}__attribute__((packed));


#define MAX_LINKS 10
#define MAX_ROUTES 10
#define HASH_KEY_LEN 16
#define MIN_CACHE_SIZE 100
#define HASH_KEY_SIZE 16

struct link_table {
    struct link_entry * links[MAX_LINKS];
    uint16_t size;
    v3_lock_t lock;
}__attribute__((packed));

struct routing_table {
    struct routing_entry * routes[MAX_ROUTES];
    uint16_t size;
    v3_lock_t lock;
}__attribute__((packed));

typedef char * route_hashkey_t;

struct route_cache_entry {
    int num_matched_routes;
    int * matches; 
};

struct vnet_state_t {
    struct link_table g_links;
    struct routing_table g_routes;
    struct gen_queue * g_inpkt_q;
    struct hashtable * g_route_cache;
    v3_lock_t cache_lock;
};

static struct vnet_state_t g_vnet_state;//global state for vnet

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

static void ethernet_packet_init(struct ethernet_pkt *pt, 
                                                              uchar_t *data, 
                                                              const size_t size) {
    pt->size = size;
    pt->use_header = 0;
    memset(&pt->ext_hdr, 0, sizeof(struct udp_link_header));
    memcpy(pt->data, data, size);
}

static inline uint_t hash_from_key_fn(addr_t hashkey) {    
    return v3_hash_buffer((uint8_t *)hashkey, HASH_KEY_LEN);
}

static inline int hash_key_equal(addr_t key1, addr_t key2) {
    return (memcmp((uint8_t *)key1, (uint8_t *)key2, HASH_KEY_LEN) == 0);
}

static int init_route_cache(struct vnet_state_t *vnet_state) {	
    vnet_state->g_route_cache = v3_create_htable(MIN_CACHE_SIZE, 
                                                                      &hash_from_key_fn, 
                                                                      &hash_key_equal);

    if (vnet_state->g_route_cache == NULL) {
        PrintError("Vnet: Route Cache Init Fails\n");
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

static int add_route_to_cache(route_hashkey_t hashkey, 
							int num_matched_r, 
							int * matches) {
    struct route_cache_entry * new_entry = NULL;
    struct vnet_state_t *vnet_state = &g_vnet_state;
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
    
    if (v3_htable_insert(vnet_state->g_route_cache, (addr_t)hashkey, (addr_t)new_entry) == 0) {
	PrintError("Vnet: Failed to insert new route entry to the cache\n");
	V3_Free(new_entry->matches);
	V3_Free(new_entry);
    }
    
    return 0;
}

static int clear_hash_cache() {
    struct vnet_state_t *vnet_state = &g_vnet_state;

    v3_free_htable(vnet_state->g_route_cache, 1, 1);		
    vnet_state->g_route_cache = v3_create_htable(MIN_CACHE_SIZE, 
                                                                        hash_from_key_fn, 
                                                                        hash_key_equal);
    
    if (vnet_state->g_route_cache == NULL) {
        PrintError("Vnet: Route Cache Create Failurely\n");
        return -1;
    }

    return 0;
}

static int look_into_cache(route_hashkey_t hashkey, int * matches) {
    struct route_cache_entry * found = NULL;
    int n_matches = -1;
    int i = 0;
    struct vnet_state_t *vnet_state = &g_vnet_state;
    
    found = (struct route_cache_entry *)v3_htable_search(vnet_state->g_route_cache, 
                                                                               (addr_t)hashkey);
   
    if (found != NULL) {
        n_matches = found->num_matched_routes;

        for (i = 0; i < n_matches; i++) {
            matches[i] = found->matches[i];
	}
    }

    return n_matches;
}


#ifdef CONFIG_DEBUG_VNET

static void print_packet(uchar_t *pkt, int size) {
    PrintDebug("Vnet: data_packet: size: %d\n", size);
    v3_hexdump(pkt, size, NULL, 0);
}

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
    snprintf(buf, 20, "%02x:%02x:%02x:%02x:%02x:%02x", 
	     mac[0], mac[1], mac[2],
	     mac[3], mac[4], mac[5]);
}


static void print_route(struct routing_entry *route){
    char dest_str[18];
    char src_str[18];

    mac_to_string(route->src_mac, src_str);  
    mac_to_string(route->dest_mac, dest_str);

    PrintDebug("SRC(%s), DEST(%s), src_mac_qual(%d), dst_mac_qual(%d)\n", 
                 src_str, 
                 dest_str, 
                 route->src_mac_qual, 
                 route->dest_mac_qual);
    PrintDebug("Src_Link(%d), src_type(%d), dst_link(%d), dst_type(%d)\n\n", 
                 route->src_link_idx, 
                 route->src_type, 
                 route->link_idx, 
                 route->link_type);
}
	

static void dump_routes(struct routing_entry **route_tables) {
    int i;

    PrintDebug("\nVnet: route table dump start =====\n");

    for(i = 0; i < MAX_ROUTES; i++) {
        if (route_tables[i] != NULL){
	     print_route(route_tables[i]);
        }
    }

    PrintDebug("\nVnet: route table dump end =====\n");
}

#endif

static int __add_link_entry(struct link_entry * link) {
    int idx;
    struct vnet_state_t *vnet_state = &g_vnet_state;

    v3_lock(vnet_state->g_links.lock);
    for (idx = 0; idx < MAX_LINKS; idx++) {
	if (vnet_state->g_links.links[idx] == NULL) {
 	    vnet_state->g_links.links[idx] = link;
	    vnet_state->g_links.size++;
	    break;
	}
    }
    v3_unlock(vnet_state->g_links.lock);

    if (idx == MAX_LINKS) {
    	PrintDebug("VNET: No available Link entry for new link\n");
    	return -1;
    }

    return idx;
}

static int __add_route_entry(struct routing_entry * route) {
    int idx;
    struct vnet_state_t *vnet_state = &g_vnet_state;

    v3_lock(vnet_state->g_routes.lock);
    for (idx = 0; idx < MAX_ROUTES; idx++) {
	if (vnet_state->g_routes.routes[idx] == NULL) {
 	    vnet_state->g_routes.routes[idx] = route;
	    vnet_state->g_routes.size++;
          break;
	}
    }
    v3_unlock(vnet_state->g_routes.lock);

    if(idx == MAX_LINKS){
        PrintDebug("VNET: No available route entry for new route\n");
        return -1;
    }

#ifdef CONFIG_DEBUG_VNET
    dump_routes(vnet_state->g_routes.routes);
#endif

    return idx;
}

int v3_vnet_add_route(struct v3_vnet_route *route){
    struct routing_entry * new_route = (struct routing_entry *)V3_Malloc(sizeof(struct routing_entry));
    int idx = -1;

    PrintDebug("Vnet: vnet_add_route_entry\n");
	
    memset(new_route, 0, sizeof(struct routing_entry));
    if ((route->src_mac_qual != MAC_ANY)) {
        memcpy(new_route->src_mac, route->src_mac, 6);
    }
	    
    if ((route->dest_mac_qual != MAC_ANY)) {
        memcpy(new_route->dest_mac, route->dest_mac, 6);
    }
	    
    new_route->src_mac_qual = route->src_mac_qual;
    new_route->dest_mac_qual = route->dest_mac_qual;
    new_route->link_idx= route->link_idx;
    new_route->link_type = route->link_type;
    new_route->src_link_idx = route->src_link_idx;
    new_route->src_type = route->src_type;

    if ((idx = __add_route_entry(new_route)) == -1) {
	PrintError("Could not add route entry\n");
        return -1;
    }
    
    clear_hash_cache();

    return idx;
}

static int match_route(uint8_t * src_mac, 
		       uint8_t * dst_mac, 
		       link_type_t src_type, 
		       int src_index, 
		       int * matches) {
    struct routing_entry *route = NULL; 
    struct vnet_state_t *vnet_state = &g_vnet_state;
    int matched_routes[MAX_ROUTES];
    int num_matches = 0;
    int i;

#ifdef CONFIG_DEBUG_VNET
    char dest_str[18];
    char src_str[18];

    mac_to_string(src_mac, src_str);  
    mac_to_string(dst_mac, dest_str);
    PrintDebug("Vnet: match_route. pkt: SRC(%s), DEST(%s)\n", src_str, dest_str);
#endif

    for(i = 0; i < MAX_ROUTES; i++) {
        if (vnet_state->g_routes.routes[i] != NULL){
	     route = vnet_state->g_routes.routes[i];

            if(src_type == LINK_ANY && src_index == -1) {
	         if ((route->dest_mac_qual == MAC_ANY) &&
		       (route->src_mac_qual == MAC_ANY)) {      
                    matched_routes[num_matches] = i;
                    num_matches++;
	         }
	
                if (memcmp((void *)&route->src_mac, (void *)src_mac, 6) == 0) {
	             if (route->src_mac_qual !=  MAC_NOT) {
		          if (route->dest_mac_qual == MAC_ANY) {
		              matched_routes[num_matches] = i;
		              num_matches++;
		          } else if (route->dest_mac_qual != MAC_NOT &&
		                      memcmp((void *)&route->dest_mac, (void *)dst_mac, 6) == 0) {
                            matched_routes[num_matches] = i;
                            num_matches++;
                        }
                    }
                }

                if (memcmp((void *)&route->dest_mac, (void *)dst_mac, 6) == 0) {
                    if (route->dest_mac_qual != MAC_NOT) {
                        if (route->src_mac_qual == MAC_ANY) {
                            matched_routes[num_matches] = i;
                            num_matches++;
                        } else if ((route->src_mac_qual != MAC_NOT) && 
                                       (memcmp((void *)&route->src_mac, (void *)src_mac, 6) == 0)) {
                            matched_routes[num_matches] = i;
                            num_matches++;
                        }
                     }
                }

                if ((route->dest_mac_qual == MAC_NOT) &&
		       (memcmp((void *)&route->dest_mac, (void *)dst_mac, 6) != 0)) {
                    if (route->src_mac_qual == MAC_ANY) {
                        matched_routes[num_matches] = i;	    
                        num_matches++;    
                    } else if ((route->src_mac_qual != MAC_NOT) && 
                                   (memcmp((void *)&route->src_mac, (void *)src_mac, 6) == 0)) {     
                        matched_routes[num_matches] = i;     
                        num_matches++;
		      }
                }

                if ((route->src_mac_qual == MAC_NOT) &&
		       (memcmp((void *)&route->src_mac, (void *)src_mac, 6) != 0)) {
                    if (route->dest_mac_qual == MAC_ANY) {
                        matched_routes[num_matches] = i;   
                        num_matches++;
                    } else if ((route->dest_mac_qual != MAC_NOT) &&
                                   (memcmp((void *)&route->dest_mac, (void *)dst_mac, 6) == 0)) {
                        matched_routes[num_matches] = i;
                        num_matches++;
                    }
                }
            }//end if src_type == Link_any
        }	
    }//end for

    PrintDebug("Vnet: match_route: Matches=%d\n", num_matches);
	
    for (i = 0; i < num_matches; i++) {
        matches[i] = matched_routes[i];
    }
    
    return num_matches;
}

static int handle_one_pkt(struct ethernet_pkt *pkt) {
    int src_link_index = -1;	//the value of src_link_index of udp always is 0
    char src_mac[6];
    char dst_mac[6];
    int matches[MAX_ROUTES];
    int num_matched_routes = 0;
    struct in_pkt_header header;
    char hash_key[HASH_KEY_SIZE];
    struct vnet_state_t *vnet_state = &g_vnet_state;
    int i;

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

    make_hash_key(hash_key, src_mac, dst_mac, LINK_EDGE, src_link_index); 
    num_matched_routes = look_into_cache((route_hashkey_t)hash_key, matches);
    
    if (num_matched_routes == -1) {  
        num_matched_routes = match_route(src_mac, dst_mac, LINK_ANY, src_link_index, matches);	
        if (num_matched_routes > 0) {
	     add_route_to_cache(hash_key, num_matched_routes,matches);      
	 }
    }
    
    PrintDebug("Vnet: HandleDataOverLink: Matches=%d\n", num_matched_routes);

    if (num_matched_routes == 0) {
        return -1;
    }
    
    for (i = 0; i < num_matched_routes; i++) {//send packet to all destinations
        int route_index = -1;
        int link_index = -1;
        int pkt_len = 0;
        struct link_entry * link = NULL;

        route_index = matches[i];
        link_index = vnet_state->g_routes.routes[route_index]->link_idx;

        if ((link_index < 0) || (link_index > MAX_LINKS) || 
	     (vnet_state->g_links.links[link_index] == NULL)) {
	     continue;
	 }
	
        link = vnet_state->g_links.links[link_index];
        pkt_len = pkt->size;
        if (link->type == LINK_EDGE) {

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
                    PrintDebug("VNET: Packet not sent properly to link: %d\n", link_index);
                    continue;
                }
	     }else {
                PrintDebug("VNET: Link protocol type not support\n");
                continue;
	     }
        } else if (link->type == LINK_INTERFACE) {
            if ((link->dst_dev->input(link->dst_dev->vm, pkt->data, pkt_len, link->dst_dev->private_data)) != pkt_len) {
                PrintDebug("VNET: Packet not sent properly to link: %d\n", link_index);
                continue;
	     }
        } else {
            PrintDebug("Vnet: Wrong Edge type of link: %d\n", link_index);
            continue;
        }

        PrintDebug("Vnet: HandleDataOverLink: Forward packet according to Route entry %d to link %d\n", route_index, link_index);
    }
    
    return 0;
}

static int send_ethernet_pkt(uchar_t *data, int len, void *private_data) {
    struct ethernet_pkt *pkt;
    struct vnet_state_t *vnet_state = &g_vnet_state;

    pkt = (struct ethernet_pkt *)V3_Malloc(sizeof(struct ethernet_pkt));
    if(pkt == NULL){
        PrintError("VNET: Memory allocate fails\n");
        return -1;
    }

    memset(pkt, 0, sizeof(struct ethernet_pkt));
    ethernet_packet_init(pkt, data, len);
    v3_enqueue(vnet_state->g_inpkt_q, (addr_t)pkt);
  
#ifdef CONFIG_DEBUG_VNET
    PrintDebug("VNET: send_pkt: transmitting packet: (size:%d)\n", (int)pkt->size);
    print_packet((char *)data, len);
#endif

    return 0;
}

int v3_vnet_send_rawpkt(uchar_t * buf, 
                                          int len, 
                                          void *private_data) {
    PrintDebug("VNET: In v3_vnet_send_rawpkt: pkt length %d\n", len);
    
    return send_ethernet_pkt(buf, len, private_data);
}

//sending the packet from Dom0, should remove the link header
int v3_vnet_send_udppkt(uchar_t * buf, 
                                           int len, 
                                           void *private_data) {
    uint_t hdr_len = sizeof(struct udp_link_header);
	
    PrintDebug("VNET: In v3_vnet_send_udppkt: pkt length %d\n", len);
   
    return send_ethernet_pkt((uchar_t *)(buf+hdr_len), len - hdr_len, private_data);
}

static int search_device(char * device_name) {
    struct vnet_state_t *vnet_state = &g_vnet_state;
    int i;

    for (i = 0; i < MAX_LINKS; i++) {
        if ((vnet_state->g_links.links[i] != NULL) && (vnet_state->g_links.links[i]->type == LINK_INTERFACE)) {
	    if (strcmp(device_name, vnet_state->g_links.links[i]->dst_dev->name) == 0) {
		return i;
	    }
        }
    }

    return -1;
}

int v3_vnet_add_node(struct v3_vm_info *info, 
	           char * dev_name, 
	           uchar_t mac[6], 
		    int (*netif_input)(struct v3_vm_info * vm, uchar_t * pkt, uint_t size, void * private_data), 
		    void * priv_data){
    struct vnet_if_device * if_dev;

    int idx = search_device(dev_name);
    if (idx != -1) {
	PrintDebug("VNET: register device: Already has device with the name %s\n", dev_name);
	return -1;
    }
    
    if_dev = (struct vnet_if_device *)V3_Malloc(sizeof(struct vnet_if_device)); 
    if (if_dev == NULL) {
	PrintError("VNET: Malloc fails\n");
	return -1;
    }
    
    strcpy(if_dev->name, dev_name);
    strncpy(if_dev->mac_addr, mac, 6);
    if_dev->input = netif_input;
    if_dev->private_data = priv_data;
    if_dev->vm = info;

    struct link_entry * link = (struct link_entry *)V3_Malloc(sizeof(struct link_entry));
    link->type = LINK_INTERFACE;
    link->dst_dev = if_dev;

    idx = __add_link_entry(link);

    if (idx < 0) return -1;

    return idx;
}

int v3_vnet_pkt_process() {
    struct ethernet_pkt * pkt;
    struct vnet_state_t *vnet_state = &g_vnet_state;

    while ((pkt = (struct ethernet_pkt *)v3_dequeue(vnet_state->g_inpkt_q))!= NULL) {
        if (handle_one_pkt(pkt) != -1) {
            PrintDebug("VNET: vnet_check: handle one packet! pt length %d, pt type %d\n", (int)pkt->size, (int)pkt->type);  
        } else {
            PrintDebug("VNET: vnet_check: Fail to forward one packet, discard it!\n"); 
        }
	
        V3_Free(pkt); // be careful here
    }
    
    return 0;
}

static void vnet_state_init(struct vnet_state_t *vnet_state) {
    int i;

    /*initial links table */
    for (i = 0; i < MAX_LINKS; i++) {
        vnet_state->g_links.links[i] = NULL;
    }
    vnet_state->g_links.size = 0;
    if(v3_lock_init(&(vnet_state->g_links.lock)) == -1){
        PrintError("VNET: Failure to init lock for links table\n");
    }
    PrintDebug("VNET: Links table initiated\n");

    /*initial routes table */
    for (i = 0; i < MAX_ROUTES; i++) {
        vnet_state->g_routes.routes[i] = NULL;
    }
    vnet_state->g_routes.size = 0;
    if(v3_lock_init(&(vnet_state->g_routes.lock)) == -1){
        PrintError("VNET: Failure to init lock for routes table\n");
    }
    PrintDebug("VNET: Routes table initiated\n");

    /*initial pkt receiving queue */
    vnet_state->g_inpkt_q = v3_create_queue();
    v3_init_queue(vnet_state->g_inpkt_q);
    PrintDebug("VNET: Receiving queue initiated\n");

    /*initial routing cache */
    init_route_cache(vnet_state);
}


int v3_init_vnet() {
    vnet_state_init(&g_vnet_state);
	
    PrintDebug("VNET Initialized\n");

    return 0;
}

