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
  uint32_t size;
  uint16_t type;
  char data[ETHERNET_PACKET_LEN];
};


// 14 (ethernet frame) + 20 bytes
struct in_pkt_header {
    char ethernetdest[6];
    char ethernetsrc[6];
    unsigned char ethernettype[2]; // indicates layer 3 protocol type
    char ip[20];
};

#define VNET_INITAB_HCALL 0xca00  // inital hypercall id

#define MAX_LINKS 10
#define MAX_ROUTES 10
#define HASH_KEY_LEN 16
#define MIN_CACHE_SIZE 100
static const uint_t hash_key_size = 16;

struct link_table {
    struct link_entry *links[MAX_LINKS];
    uint16_t size;
};

struct routing_table {
    struct routing_entry *routes[MAX_ROUTES];
    uint16_t size;
};

static struct link_table g_links;
static struct routing_table g_routes;
static struct gen_queue *g_inpkt_q;

/* Hash key format:
 * 0-5:     src_eth_addr
 * 6-11:    dest_eth_addr
 * 12:      src type
 * 13-16:   src index
 */
typedef char *route_hashkey_t;

// This is the hash value, Format: 0: num_matched_routes, 1...n: matches[] -- TY
struct route_cache_entry {
    int num_matched_routes;
    int * matches; 
};

//the route cache
static struct hashtable *g_route_cache; 


static void print_packet(char *pkt, int size) {
    PrintDebug("Vnet: print_data_packet: size: %d\n", size);
    v3_hexdump(pkt, size, NULL, 0);
}

#if 0
static void print_packet_addr(char *pkt) {
    PrintDebug("Vnet: print_packet_destination_addr: ");
    v3_hexdump(pkt + 8, 6, NULL, 0);
    
    PrintDebug("Vnet: print_packet_source_addr: ");
    v3_hexdump(pkt + 14, 6, NULL, 0);
}

static void print_device_addr(char *ethaddr) {
    PrintDebug("Vnet: print_device_addr: ");
    v3_hexdump(ethaddr, 6, NULL, 0);
} 
#endif


//network connection functions 
static inline void ethernet_packet_init(struct ethernet_pkt *pt, const char *data, const size_t size) 
{
    pt->size = size;
    memcpy(pt->data, data, size);
}

static uint_t hash_from_key_fn(addr_t hashkey) 
{    
    uint8_t * key = (uint8_t *)hashkey;
    return v3_hash_buffer(key, HASH_KEY_LEN);
}

static int hash_key_equal(addr_t key1, addr_t key2) 
{
    uint8_t * buf1 = (uint8_t *)key1;
    uint8_t * buf2 = (uint8_t *)key2;
    return (memcmp(buf1, buf2, HASH_KEY_LEN) == 0);
}

static int init_route_cache() 
{
    g_route_cache = v3_create_htable(MIN_CACHE_SIZE, &hash_from_key_fn, &hash_key_equal);

    if (g_route_cache == NULL){
        PrintError("Vnet: Route Cache Initiate Failurely\n");
        return -1;
    }

    return 0;
}

static void make_hash_key(route_hashkey_t hashkey,
						      char src_addr[6],
						      char dest_addr[6],
						      char src_type,
						      int src_index) 
{
    int j;

    for (j = 0; j < 6; j++) {
	hashkey[j] = src_addr[j];
	hashkey[j + 6] = dest_addr[j] + 1;
    }

    hashkey[12] = src_type;

    *(int *)(hashkey + 12) = src_index;
}

static int add_route_to_cache(route_hashkey_t hashkey, int num_matched_r, int *matches) 
{
    struct route_cache_entry * new_entry = NULL;
    int i;
    
    new_entry = (struct route_cache_entry *)V3_Malloc(sizeof(struct route_cache_entry));
    if (new_entry == NULL){
	PrintError("Vnet: Malloc fails\n");
	return -1;
    }
    
    new_entry->num_matched_routes = num_matched_r;

    new_entry->matches = (int *)V3_Malloc(num_matched_r * sizeof(int));
    
    if (new_entry->matches == NULL){
	PrintError("Vnet: Malloc fails\n");
	return -1;
    }
    
    for (i = 0; i < num_matched_r; i++) {
	new_entry->matches[i] = matches[i];
    }
    
    //here, when v3_htable_insert return 0, it means insert fails
    if (v3_htable_insert(g_route_cache, (addr_t)hashkey, (addr_t)new_entry) == 0){
	PrintError("Vnet: Insert new route entry to cache failed\n");
	V3_Free(new_entry->matches);
	V3_Free(new_entry);
    }
    
    return 0;
}

static int clear_hash_cache() 
{
    v3_free_htable(g_route_cache, 1, 1);
		
    g_route_cache = v3_create_htable(MIN_CACHE_SIZE, hash_from_key_fn, hash_key_equal);
    
    if (g_route_cache == NULL){
        PrintError("Vnet: Route Cache Create Failurely\n");
        return -1;
    }

    return 0;
}

static int look_into_cache(route_hashkey_t hashkey, int *matches) 
{
    int n_matches = -1;
    int i = 0;
    struct route_cache_entry * found = NULL;
    
    found = (struct route_cache_entry *)v3_htable_search(g_route_cache, (addr_t)hashkey);
   
    if (found != NULL) {
        n_matches = found->num_matched_routes;

        for (i = 0; i < n_matches; i++) {
            matches[i] = found->matches[i];
	}
    }

    return n_matches;
}

static inline uint8_t hex_nybble_to_nybble(const uint8_t hexnybble) 
{
    uint8_t x = toupper(hexnybble);

    if (isdigit(x)) {
	return x - '0';
    } else {
	return 10 + (x - 'A');
    }
}

static inline uint8_t hex_byte_to_byte(const uint8_t hexbyte[2]) 
{
    return ((hex_nybble_to_nybble(hexbyte[0]) << 4) + 
	    (hex_nybble_to_nybble(hexbyte[1]) & 0xf));
}

static inline void string_to_mac(const char *str, uint8_t mac[6]) 
{
    int k;

    for (k = 0; k < 6; k++) {
	mac[k] = hex_byte_to_byte(&(str[(2 * k) + k]));
    }
}

static inline void mac_to_string(char mac[6], char * buf) 
{
    snprintf(buf, 20, "%x:%x:%x:%x:%x:%x", 
	     mac[0], mac[1], mac[2],
	     mac[3], mac[4], mac[5]);
}

static int add_link_entry(struct link_entry *link)
{ 
    int idx;
    
    for (idx = 0; idx < MAX_LINKS; idx++) {
	if (g_links.links[idx] == NULL) {
 	    g_links.links[idx] = link;
	    g_links.size ++;
	    
	    return idx;
	}
    }
    
    return -1;
}

static int add_route_entry(struct routing_entry *route)
{ 
    int idx;
    
    for (idx = 0; idx < MAX_ROUTES; idx++) {
	if (g_routes.routes[idx] == NULL) {
 	    g_routes.routes[idx] = route;
	    g_routes.size ++;
	    
	    return idx;
	}
    }
    
    return -1;
}


int vnet_add_route_entry(char src_mac[6],
							char dest_mac[6],
							int src_mac_qual,
							int dest_mac_qual,
							int link_idx,
							link_type_t link_type,
							int src,
							link_type_t src_type)
{
    struct routing_entry *new_route = (struct routing_entry *)V3_Malloc(sizeof(struct routing_entry));

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

    int idx = -1;
    if ((idx = add_route_entry(new_route)) == -1)
        return -1;
    
    clear_hash_cache();
    
    return idx;
}

static void * delete_link_entry(int index) 
{
    struct link_entry *link = NULL;
    void *ret = NULL;
    link_type_t type;
  
    if (index >= MAX_LINKS || g_links.links[index] == NULL) {
	return NULL;
    }

    link = g_links.links[index];
    type = g_links.links[index]->type;

    if (type == LINK_INTERFACE)	
    	ret = (void *)g_links.links[index]->dst_dev;
    else if (type == LINK_EDGE)
	ret = (void *)g_links.links[index]->dst_link;

    g_links.links[index] = NULL;
    g_links.size --;

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
			    link_type_t src_type) 
{
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
    
    for (i = 0; i<MAX_ROUTES; i++) {
	if (g_routes.routes[i] != NULL) {
	    if ((memcmp(temp_src_mac, g_routes.routes[i]->src_mac, 6) == 0) && 
	          (memcmp(temp_dest_mac, g_routes.routes[i]->dest_mac, 6) == 0) &&
	          (g_routes.routes[i]->src_mac_qual == src_mac_qual) &&
	          (g_routes.routes[i]->dest_mac_qual == dest_mac_qual)  &&
	          ((link_type == LINK_ANY) || 
	          ((link_type == g_routes.routes[i]->link_type) && (g_routes.routes[i]->link_idx == link_idx))) &&
	          ((src_type == LINK_ANY) || 
	          ((src_type == g_routes.routes[i]->src_type) && (g_routes.routes[i]->src_link_idx == src)))) {
	        return i;
	    }
        } 
     }
    
    return -1;
}

static int delete_route_entry(int index) 
{
    struct routing_entry *route;

    if (index >= MAX_ROUTES || g_routes.routes[index] == NULL)
		return -1;

    route = g_routes.routes[index];
    g_routes.routes[index] = NULL;
    g_routes.size --;

    V3_Free(route);

    clear_hash_cache();
    
    return 0;
}

int vnet_delete_route_entry_by_addr(char src_mac[6], 
				    char dest_mac[6], 
				    int src_mac_qual, 
				    int dest_mac_qual, 
				    int link_idx, 
				    link_type_t type, 
				    int src, 
				    link_type_t src_type) 
{
    int index = find_route_entry(src_mac, dest_mac, src_mac_qual, 
				 dest_mac_qual, link_idx, type, src, src_type);
    
    if (index == -1) {
	return -1;
    }
    
    return delete_route_entry(index);
}

static int match_route(uint8_t *src_mac, uint8_t *dst_mac, link_type_t src_type, int src_index, int *matches)
{ 
    int values[MAX_ROUTES];
    int matched_routes[MAX_ROUTES];
    
    int num_matches = 0;
    int i;
    int max = 0;
    int no = 0;
    int exact_match = 0;

    for(i = 0; i<MAX_ROUTES; i++) {
	if (g_routes.routes[i] != NULL){
	    if ((g_routes.routes[i]->src_type != LINK_ANY) &&
	         ((g_routes.routes[i]->src_type != src_type) ||
	         ((g_routes.routes[i]->src_link_idx != src_index) &&
	           (g_routes.routes[i]->src_link_idx != -1)))) {
	        PrintDebug("Vnet: MatchRoute: Source route is on and does not match\n");
	        continue;
	    }
	
	    if ((g_routes.routes[i]->dest_mac_qual == MAC_ANY) &&
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
	
	    if ((g_routes.routes[i]->dest_mac_qual == MAC_NOT) &&
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
	
	    if ((g_routes.routes[i]->src_mac_qual == MAC_NOT) &&
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
    }//end for
    
    for(i = 0; i<MAX_ROUTES; i++) {
    	if ((memcmp((void *)&g_routes.routes[i]->src_mac, (void *)src_mac, 6) == 0) &&
	     (g_routes.routes[i]->dest_mac_qual == MAC_NONE) &&
	     ((g_routes.routes[i]->src_type == LINK_ANY) ||
	     ((g_routes.routes[i]->src_type == src_type) &&
	     ((g_routes.routes[i]->src_link_idx == src_index) ||
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

#if 0
// TODO: To be removed
static int process_udpdata() 
{
    struct ethernet_pkt * pt;

    uint32_t dest = 0;
    uint16_t remote_port = 0;
    SOCK link_sock = g_udp_sockfd;
    int length = sizeof(struct ethernet_pkt) - (2 * sizeof(int));   //minus the "size" and "type" 

    //run in a loop to get packets from outside network, adding them to the incoming packet queue
    while (1) {
	pt = (struct ethernet_pkt *)V3_Malloc(sizeof(struct ethernet_pkt));

	if (pt == NULL){
	    PrintError("Vnet: process_udp: Malloc fails\n");
	    continue;
	}
	
	PrintDebug("Vnet: route_thread: socket: [%d]. ready to receive from ip [%x], port [%d] or from VMs\n", link_sock, (uint_t)dest, remote_port);
	pt->size = V3_RecvFrom_IP( link_sock, dest, remote_port, pt->data, length);
	PrintDebug("Vnet: route_thread: socket: [%d] receive from ip [%x], port [%d]\n", link_sock, (uint_t)dest, remote_port);
	
	if (pt->size <= 0) {
	    PrintDebug("Vnet: process_udp: receiving packet from UDP fails\n");
	    V3_Free(pt);
	    return -1;
	}
	
	PrintDebug("Vnet: process_udp: get packet\n");
	print_packet(pt->data, pt->size);
    }
}

// TODO: To be removed
static int indata_handler( )
{
      if (!use_tcp)
      	   process_udpdata();	  

      return 0;   
}

// TODO: To be removed
static int start_recv_data()
{
	if (use_tcp){
		
	} else {
  		SOCK udp_data_socket;
  
  		if ((udp_data_socket = V3_Create_UDP_Socket()) < 0){
	      		PrintError("VNET: Can't setup udp socket\n");
	      		return -1; 
  		}
  		PrintDebug("Vnet: vnet_setup_udp: get socket: %d\n", udp_data_socket);
		g_udp_sockfd = udp_data_socket;

  		store_topologies(udp_data_socket);

  		if (V3_Bind_Socket(udp_data_socket, vnet_udp_port) < 0){ 
	          	PrintError("VNET: Can't bind socket\n");
	          	return -1;
  		}
  		PrintDebug("VNET: vnet_setup_udp: bind socket successful\n");
	}

	V3_CREATE_THREAD(&indata_handler, NULL, "VNET_DATA_HANDLER");
	return 0;
}

static void store_test_topologies(SOCK fd) 
{
    int i;
    int src_mac_qual = MAC_ANY;
    int dest_mac_qual = MAC_ANY;
    uint_t dest;
#ifndef VNET_SERVER
    dest = (0 | 172 << 24 | 23 << 16 | 1 );
    PrintDebug("VNET: store_topologies. NOT VNET_SERVER, dest = %x\n", dest);
#else
    dest = (0 | 172 << 24 | 23 << 16 | 2 );
    PrintDebug("VNET: store_topologies. VNET_SERVER, dest = %x\n", dest);
#endif

    int type = UDP_TYPE;
    int src = 0;
    int src_type= LINK_ANY; //ANY_SRC_TYPE
    int data_port = 22;
}

#endif

static int handle_one_pkt(struct ethernet_pkt * pkt) 
{
    int src_link_index = -1;	//the value of src_link_index of udp always is 0
    int i;
    char src_mac[6];
    char dst_mac[6];

    int matches[MAX_ROUTES];
    int num_matched_routes = 0;

    struct in_pkt_header header;
  
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
    
    char hash_key[hash_key_size];
    make_hash_key(hash_key, src_mac, dst_mac, LINK_EDGE, src_link_index);//link_edge -> pt->type???
    
    num_matched_routes = look_into_cache((route_hashkey_t)hash_key, matches);
    
    if (num_matched_routes == -1) {  //no match
        num_matched_routes = match_route(src_mac, dst_mac, LINK_ANY, src_link_index, matches);
	
	 if (num_matched_routes > 0) {
	     add_route_to_cache(hash_key, num_matched_routes,matches);      
	 }
    }
    
    PrintDebug("Vnet: HandleDataOverLink: Matches=%d\n", num_matched_routes);
    
    for (i = 0; i < num_matched_routes; i++) {
        int route_index = -1;
        int link_index = -1;
	 int pkt_len = 0;
	
        route_index = matches[i];
	
        PrintDebug("Vnet: HandleDataOverLink: Forward packet from link according to Route entry %d\n", route_index);

	 link_index = g_routes.routes[route_index]->link_idx;
	 if (link_index < 0 || link_index > MAX_LINKS)
	     continue;
	 
	 struct link_entry *link = g_links.links[link_index];
	 if (link == NULL)
	     continue;
	 
	 pkt_len = pkt->size;
        if (g_routes.routes[route_index]->link_type == LINK_EDGE) {

            // TODO: apply the header in the beginning of the packet to be sent
	     if ((link->dst_link->input(pkt->data, pkt_len, NULL)) != pkt_len)
	         return -1;
        } else if (g_routes.routes[route_index]->link_type == LINK_INTERFACE) {
          
      
            if ((link->dst_link->input(pkt->data, pkt_len, NULL)) != pkt_len)
	         return -1;
        } else {
            PrintError("Vnet: Wrong Edge type\n");
	     return -1;
        }
    }

     return 0;
}

static int send_ethernet_pkt(char *buf, int length) 
{
	struct ethernet_pkt *pt;

	pt = (struct ethernet_pkt *)V3_Malloc(sizeof(struct ethernet_pkt));
	ethernet_packet_init(pt, buf, length);  //====here we copy sending data once 
	
	PrintDebug("VNET: vm_send_pkt: transmitting packet: (size:%d)\n", (int)pt->size);
	print_packet((char *)buf, length);
	
	v3_enqueue(g_inpkt_q, (addr_t)pt);

	return 0;
}

int v3_vnet_send_pkt(uchar_t *buf, int length) 
{
    PrintDebug("VNET: In V3_Send_pkt: pkt length %d\n", length);
    
    return send_ethernet_pkt((char *)buf, length);
}

static int search_device(char *device_name) 
{
    int i;

    for (i = 0; i < MAX_LINKS; i++) {
        if (g_links.links[i] != NULL && g_links.links[i]->type == LINK_INTERFACE) {
	    if (!strcmp(device_name, g_links.links[i]->dst_dev->name)) {
		return i;
	    }
        }
    }
    
    return -1;
}

int vnet_register_device(struct vm_device *vdev, 
						   char *dev_name, 
						   uchar_t mac[6], 
						   int (*netif_input)(uchar_t * pkt, uint_t size, void *private_data), 
						   void *data) 
{
    struct vnet_if_device *if_dev;

    int idx = search_device(dev_name);
    if (idx != -1)
	return -1;
    
    if_dev = (struct vnet_if_device *)V3_Malloc(sizeof(struct vnet_if_device));
    
    if (if_dev == NULL){
	PrintError("VNET: Malloc fails\n");
	return -1;
    }
    
    strcpy(if_dev->name, dev_name);
    strncpy(if_dev->mac_addr, mac, 6);
    if_dev->dev = vdev;
    if_dev->input = netif_input;
    if_dev->private_data = data;

    struct link_entry *link = (struct link_entry *)V3_Malloc(sizeof(struct link_entry));

    link->type = LINK_INTERFACE;
    link->dst_dev = if_dev;

    idx = add_link_entry(link);
    
    return idx;
}

int vnet_unregister_device(char *dev_name) 
{
    int idx;

    idx = search_device(dev_name);
    
    if (idx == -1) {
        return -1;
    }

    struct vnet_if_device *device = (struct vnet_if_device *)delete_link_entry(idx);
    if (device == NULL) {
	return -1;
    }

    V3_Free(device);

    return idx;
}

int v3_vnet_pkt_process() 
{
    struct ethernet_pkt *pt;

    PrintDebug("VNET: In vnet_check\n");
	
    while ((pt = (struct ethernet_pkt *)v3_dequeue(g_inpkt_q)) != NULL) {
	PrintDebug("VNET: In vnet_check: pt length %d, pt type %d\n", (int)pt->size, (int)pt->type);
	v3_hexdump(pt->data, pt->size, NULL, 0);
	
	if(handle_one_pkt(pt)) {
	    PrintDebug("VNET: vnet_check: handle one packet!\n");  
	}else {
	    PrintError("VNET: vnet_check: fail to forward one packet, discard it!\n"); 
	}
	
	V3_Free(pt); //be careful here
    }
    
    return 0;
}


static void init_empty_link_table() 
{
    int i;

    for (i = 0; i < MAX_LINKS; i++)
        g_links.links[i] = NULL;

	
    g_links.size = 0;
}

static void init_empty_route_table() 
{	
    int i;

    for (i = 0; i < MAX_ROUTES; i++) 
        g_routes.routes[i] = NULL;

    g_links.size = 0;
}

static void init_tables() {
    init_empty_link_table();
    init_empty_route_table();
    init_route_cache();
}

static void init_pkt_queue()
{
    PrintDebug("VNET Init package receiving queue\n");

    g_inpkt_q = v3_create_queue();
    v3_init_queue(g_inpkt_q);
}

#if 0
// TODO: 
static int init_routing_tables(struct routing_entry *route_tab, uint16_t size)
{
    //struct routing_entry *route;


    return 0;
}


// TODO: 
static int init_link_tables(struct link_entry *link_tab, uint16_t size)
{
    //struct link_entry *link;

    return 0;
}

#endif

struct table_init_info {
    addr_t routing_table_start;
    uint16_t routing_table_size;
    addr_t link_table_start;
    uint16_t link_table_size;
};

static int handle_init_tables_hcall(struct guest_info * info, uint_t hcall_id, void *priv_data) 
{
    uint8_t *buf = NULL;
    addr_t info_addr =	(addr_t)info->vm_regs.rcx;

    if (guest_va_to_host_va(info, info_addr, (addr_t *)&(buf)) == -1) {
	PrintError("Could not translate buffer address\n");
	return -1;
    }

    //struct table_init_info *init_info = (struct table_init_info *)buf;
    
    
    return 0;
}


void v3_vnet_init(struct guest_info *vm) 
{
    init_tables();
    init_pkt_queue();
	
    v3_register_hypercall(vm, VNET_INITAB_HCALL, handle_init_tables_hcall, NULL);

    //store_test_topologies(udp_data_socket); 

    PrintDebug("VNET Initied\n");
}


