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
 *		  Yuan Tang <ytang@northwestern.edu>
 *		  Jack Lange <jarusl@cs.northwestern.edu> 
 *		  Peter Dinda <pdinda@northwestern.edu
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
 
#include <devices/vnet.h>

struct raw_ethernet_pkt {
  int  size;
  int type; // vm or link type:  INTERFACE|EDGE
  char  data[ETHERNET_PACKET_LEN];
};

char *vnet_version = "0.9";
static int vnet_server = 0;
static bool use_tcp = false;

static uint_t vnet_udp_port = 22;

#define MAX_LINKS 1
#define MAX_ROUTES 1
#define MAX_DEVICES 16


struct topology g_links[MAX_LINKS];
int g_num_links; //The current number of links
int g_first_link;
int g_last_link;

struct routing g_routes[MAX_ROUTES];
int g_num_routes; //The current number of routes
int g_first_route;
int g_last_route;

struct device_list g_devices[MAX_DEVICES];
int g_num_devices;
int g_first_device;
int g_last_device;

SOCK g_udp_sockfd;
static struct gen_queue * g_inpkt_q;//packet receiving queue


static void print_packet(char *pkt, int size)
{
      int i;
	   
	PrintDebug("Vnet: print_data_packet: size: %d\n", size);
  	for (i = 0; i < size; i ++)
  		PrintDebug("%x ", pkt[i]);
  	PrintDebug("\n");
}

#if 0
static void print_packet_addr(char *pkt)
{
       int i;
	   
	PrintDebug("Vnet: print_packet_destination_addr: ");
  	for (i = 8; i < 14; i ++)
  		PrintDebug("%x ", pkt[i]);
  	PrintDebug("\n");
	
	PrintDebug("Vnet: print_packet_source_addr: ");
  	for (i = 14; i < 20; i ++)
  		PrintDebug("%x ", pkt[i]);
  	PrintDebug("\n");
}

static void print_device_addr(char *ethaddr)
{
      int i;
	   
	PrintDebug("Vnet: print_device_addr: ");
  	for (i = 0; i < 6; i ++)
  		PrintDebug("%x ", ethaddr[i]);
  	PrintDebug("\n");
} 
#endif

//network connection functions 

#if 0
static int CreateAndSetupTcpSocket(const int bufsize, const bool nodelay, const bool nonblocking)
{
  int mysocket;

  // create socket for connections
  if ((mysocket = V3_Create_TCP_Socket()) < 0) {
    return -1;
  }

  return mysocket;
}

static int BindSocketwPort(const int mysocket, const int myport)
{
   if (V3_Bind_Socket(mysocket, myport) < 0) {
    return -1;
  }
  
  return 0;
}  

static int ListenSocket(const int mysocket, const int maxc)
{
  return V3_Listen_Socket(mysocket, maxc);
}

static int ConnectToHost(const int mysocket, const int hostip, const int port)
{
  return V3_Connect_To_IP(mysocket, hostip, port);
}


static void close(int mysocket)
{
  V3_Close_Socket(mysocket);
}

static int raw_ethernet_pkt_sendUdp(struct raw_ethernet_pkt *pt, int sock_fd, int ip, short port)
{
    int size;

    PrintDebug("Vnet: sending by UDP socket %d  ip: %x,  port: %d\n", sock_fd, ip, port);
  
    if ((size = V3_SendTo_IP(sock_fd, ip, port, pt->data, pt->size)) != pt->size) {
        PrintError("Vnet: sending by UDP Exception, %x\n", size);
        return -1;
    }
 
    return 0;
}

#endif

static void raw_ethernet_packet_init(struct raw_ethernet_pkt *pt, const char *data, const size_t size)
{
  pt->size = size;
  memcpy(pt->data, data, size);
}

#define in_range(c, lo, up)  ((char)c >= lo && (char)c <= up)
//#define islower(c)           in_range(c, 'a', 'z')


#define HASH_KEY_SIZE 16
#define MIN_CACHE_SIZE 100

/* Hash key format:
 * 0-5:     src_eth_addr
 * 6-11:    dest_eth_addr
 * 12:      src type
 * 13-16:   src index
 */
typedef char * route_hashkey_t;

struct route_cache_entry  // This is the hash value, Format: 0: num_matched_routes, 1...n: matches[] -- TY
{
    int num_matched_routes;
    int *matches; 
};

struct hashtable *g_route_cache; //Header of the route cache

static uint_t hash_from_key_fn(addr_t hashkey)
{
    uint_t hash = 0;
    uint_t temp = 0; 
    int i;

    char *key =(char *)hashkey;

    for (i = 0; i < HASH_KEY_SIZE; i++) {
        hash = (hash << 4) + *(key + i) + i;
        if ((temp = (hash & 0xF0000000))) {
	      hash ^= (temp >> 24);
        }
        hash &= ~temp;
    }
    PrintDebug("Hash Value: %lu\n", (unsigned long)hash);
	
    return hash;
}

static int hash_key_equal(addr_t left, addr_t right)
{
    int i;
    char *key1, *key2;
 
    key1 = (char *)left;
    key2 = (char *)right;
	
    for(i = 0; i < HASH_KEY_SIZE; i++) {
        if (key1[i] != key2[i]) {
	      PrintDebug("HASHes not equal\n");

	      return -1;
        }
    }
    return 0;
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

static void make_hash_key(route_hashkey_t hashkey, char src_addr[6], char dest_addr[6], char src_type, int src_index) {
 	int j;

  	for(j = 0; j < 6; j++) {
    		hashkey[j] = src_addr[j];
    		hashkey[j + 6] = dest_addr[j] + 1;
  	}
  	hashkey[12] = src_type;
  	*(int *)(hashkey + 12) = src_index;
}

static int add_route_to_cache(route_hashkey_t hashkey, int num_matched_r, int *matches)
{
      struct route_cache_entry *new_entry = (struct route_cache_entry *)V3_Malloc(sizeof(struct route_cache_entry));

	if (new_entry == NULL){
		PrintError("Vnet: Malloc fails\n");
		return -1;
	}

	new_entry->num_matched_routes = num_matched_r;
	int i;

	new_entry->matches = (int *)V3_Malloc(num_matched_r*sizeof(int)); // TODO: Here need to consider where to release the memory when clear cache;

	if (new_entry->matches == NULL){
		PrintError("Vnet: Malloc fails\n");
		return -1;
	}
 
      for(i = 0; i < num_matched_r; i++) {
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
    int i;
    struct route_cache_entry *found;

    found = (struct route_cache_entry *)v3_htable_search(g_route_cache, (addr_t)hashkey);
   
    if (found != NULL) {
        n_matches = found->num_matched_routes;
        for (i = 0; i < n_matches; i++)
            matches[i] = found->matches[i];
    }

    return n_matches;
}


static inline char vnet_toupper(char c)
{
	if (islower(c))
		c -= 'a'-'A';
	return c;
}

static inline char hexnybbletonybble(const char hexnybble) {
  char x = vnet_toupper(hexnybble);
  if ((x >= '0') && (x <= '9')) {
    return x - '0';
  } else {
    return 10 + (x - 'A');
  }
}

static inline void hexbytetobyte(const char hexbyte[2], char *byte) {
  *byte = ((hexnybbletonybble(hexbyte[0]) << 4) + 
	   (hexnybbletonybble(hexbyte[1]) & 0xf));
}

static inline char nybbletohexnybble(const char nybble) {
  return (nybble >= 10) ? (nybble - 10 + 'A') : (nybble + '0');
}

static inline void bytetohexbyte(const char byte, char hexbyte[2]) {
  hexbyte[0] = nybbletohexnybble((byte >> 4) & 0xf);
  hexbyte[1] = nybbletohexnybble(byte & 0xf);
}

static inline void string_to_mac(const char * str, char mac[6]) {
  int k;
  for(k = 0; k < 6; k++) {
    hexbytetobyte(&(str[(2 * k) + k]), mac + k);
  }
}

static inline void mac_to_string(char address[6], char * buf) {
  int i;
  for (i = 0; i < 6; i++) {
    bytetohexbyte(address[i], &(buf[3 * i]));
    buf[(3 * i) + 2] = ':';
  }
  buf[17] = 0;
}

/*
static void ip_to_string(ulong_t addr, char * buf) {
  uint32_t addr_st;
  char * tmp_str;

  addr_st = v3_htonl(addr);
  tmp_str = v3_inet_ntoa(addr_st);

  memcpy(buf, tmp_str, strlen(tmp_str));
}
*/

int find_link_by_fd(SOCK sock) {
  int i;

  FOREACH_LINK(i, g_links, g_first_link) {
      if (g_links[i].link_sock == sock) {
          return i;
      }
  }

  return -1;
}

int vnet_add_link_entry(unsigned long dest, int type, int data_port,  SOCK fd) {
  int i;

  for(i = 0; i < MAX_LINKS; i++) {
      if (g_links[i].use == 0) {
          g_links[i].dest = dest;
          g_links[i].type = type;
          g_links[i].link_sock = fd;
          g_links[i].remote_port = data_port;
          g_links[i].use = 1;
          if (g_first_link == -1) 
	        g_first_link = i;

          g_links[i].prev = g_last_link;
          g_links[i].next = -1;
      
          if (g_last_link != -1) {
	        g_links[g_last_link].next = i;
          }

          g_last_link = i;

          g_num_links++;

	    return i;
    }
  }
  
  return -1;
}


// TODO: What is this used for?
int add_sock(struct sock_list *socks, int  len, int *first_sock, int *last_sock, SOCK fd) {
  int i;

  for (i = 0; i < len; i++) {
      if (socks[i].sock == -1) {
          socks[i].sock = fd;

      if (*first_sock == -1) 
	    *first_sock = i;

      socks[i].prev = *last_sock;
      socks[i].next = -1;

      if (*last_sock != -1) 
	    socks[*last_sock].next = i;

      *last_sock = i;

      return i;
    }
  }
  return -1;
}


int vnet_add_route_entry(char src_mac[6], char dest_mac[6], int src_mac_qual, int dest_mac_qual, int dest, int type, int src, int src_type) {
  int i;

  for(i = 0; i < MAX_ROUTES; i++) {
      if (g_routes[i].use == 0) {
     
          if ((src_mac_qual != ANY_TYPE) && (src_mac_qual != NONE_TYPE)) {
	        memcpy(g_routes[i].src_mac, src_mac, 6);
          } else {
	        memset(g_routes[i].src_mac, 0, 6);
          }
      
          if ((dest_mac_qual != ANY_TYPE) && (dest_mac_qual != NONE_TYPE)) {
	        memcpy(g_routes[i].dest_mac, dest_mac, 6);
          } else {
	        memset(g_routes[i].dest_mac, 0, 6);
          }

           g_routes[i].src_mac_qual = src_mac_qual;
           g_routes[i].dest_mac_qual = dest_mac_qual;
           g_routes[i].dest = dest;
           g_routes[i].type = type;
           g_routes[i].src = src;
           g_routes[i].src_type = src_type;
           g_routes[i].use = 1;

           if (g_first_route == -1) 
	         g_first_route = i;

           g_routes[i].prev = g_last_route;
           g_routes[i].next = -1;

           if (g_last_route != -1) {
	        g_routes[g_last_route].next = i;
           }

           g_last_route = i;

           g_num_routes++;

	     return i;
    }
  }

  clear_hash_cache();
  
  return -1;
}


static int find_link_entry(unsigned long dest, int type) 
{
  int i;

  FOREACH_LINK(i, g_links, g_first_link) {
      if ((g_links[i].dest == dest) && 
	    ((type == -1) || (g_links[i].type == type)) ) {
          return i;
      }
  } 

  return -1;
}

static int delete_link_entry(int index) 
{
  int next_i;
  int prev_i;

  if (g_links[index].use == 0) {
    return -1;
  }

  g_links[index].dest = 0;
  g_links[index].type = 0;
  g_links[index].link_sock = -1;
  g_links[index].use = 0;

  prev_i = g_links[index].prev;
  next_i = g_links[index].next;

  if (prev_i != -1)
    g_links[prev_i].next = g_links[index].next;

  if (next_i != -1) 
    g_links[next_i].prev = g_links[index].prev;

  if (g_first_link == index)
    g_first_link = g_links[index].next;

  if (g_last_link == index) 
    g_last_link = g_links[index].prev;

  g_links[index].next = -1;
  g_links[index].prev = -1;

  g_num_links--;

  return 0;
}

int vnet_delete_link_entry_by_addr(unsigned long dest, int type) 
{
  int index = find_link_entry(dest, type);
  
  if (index == -1) {
       return -1;
  }

  return delete_link_entry(index);
}


static int find_route_entry(char src_mac[6], 
							char dest_mac[6], 
							int src_mac_qual, 
							int dest_mac_qual, 
							int dest, 
							int type, 
							int src, 
							int src_type) 
{
  int i;
  char temp_src_mac[6];
  char temp_dest_mac[6];
  
  if ((src_mac_qual != ANY_TYPE) && (src_mac_qual != NONE_TYPE)) {
      memcpy(temp_src_mac, src_mac, 6);
  } else {
      memset(temp_src_mac, 0, 6);
  }
  
  if ((dest_mac_qual != ANY_TYPE) && (dest_mac_qual != NONE_TYPE)) {
      memcpy(temp_dest_mac, dest_mac, 6);
  } else {
      memset(temp_dest_mac, 0, 6);
  }

  FOREACH_LINK(i, g_routes, g_first_route) {
      if ((memcmp(temp_src_mac, g_routes[i].src_mac, 6) == 0) && 
	    (memcmp(temp_dest_mac, g_routes[i].dest_mac, 6) == 0) &&
	    (g_routes[i].src_mac_qual == src_mac_qual) &&
	    (g_routes[i].dest_mac_qual == dest_mac_qual)  &&
	    ((type == -1) || 
	    ((type == g_routes[i].type) && (g_routes[i].dest == dest))) &&
	    ((src_type == -1) || 
	    ((src_type == g_routes[i].src_type) && (g_routes[i].src == src))) ) 
      {
          return i;
      }
  } 

  return -1;
}

static int delete_route_entry(int index) 
{
  
  int next_i;
  int prev_i;

  memset(g_routes[index].src_mac, 0, 6);
  memset(g_routes[index].dest_mac, 0, 6);

  g_routes[index].dest = 0;
  g_routes[index].src = 0;
  g_routes[index].src_mac_qual = 0;
  g_routes[index].dest_mac_qual = 0;
  g_routes[index].type = -1;
  g_routes[index].src_type = -1;
  g_routes[index].use = 0;

  prev_i = g_routes[index].prev;
  next_i = g_routes[index].next;

  if (prev_i != -1)
    g_routes[prev_i].next = g_routes[index].next;

  if (next_i != -1) 
    g_routes[next_i].prev = g_routes[index].prev;

  if (g_first_route == index)
    g_first_route = g_routes[index].next;

  if (g_last_route == index) 
    g_last_route = g_routes[index].prev;

  g_routes[index].next = -1;
  g_routes[index].prev = -1;

  g_num_routes--;

  clear_hash_cache();

  return 0;
}

int vnet_delete_route_entry_by_addr(char src_mac[6], 
										char dest_mac[6], 
										int src_mac_qual, 
										int dest_mac_qual, 
										int dest, 
										int type, 
										int src, 
										int src_type) 
{
  int index = find_route_entry(src_mac, dest_mac, src_mac_qual, 
  							dest_mac_qual, dest, type, src, src_type);
  
  if (index == -1) {
      return -1;
  }

  delete_route_entry(index);

  return 0;
}

int delete_sock(struct sock_list * socks, int *first_sock, int *last_sock, SOCK fd) {
  int i;
  int prev_i;
  int next_i;

  
  FOREACH_SOCK(i, socks, (*first_sock)) {
    if (socks[i].sock == fd) {
      V3_Close_Socket(socks[i].sock);
      socks[i].sock = -1;

      prev_i = socks[i].prev;
      next_i = socks[i].next;

      if (prev_i != -1)
	socks[prev_i].next = socks[i].next;
      
      if (next_i != -1) 
	socks[next_i].prev = socks[i].prev;
      
      if (*first_sock == i)
	*first_sock = socks[i].next;
      
      if (*last_sock == i) 
	*last_sock = socks[i].prev;

      socks[i].next = -1;
      socks[i].prev = -1;

      return 0;
    }
  }
  return -1;
}

//setup the topology of the testing network
static void store_topologies(SOCK fd)
{
	int i;
	int src_mac_qual = ANY_TYPE;
	int dest_mac_qual = ANY_TYPE;
	uint_t dest;
#ifndef VNET_SERVER
	dest = (0 | 172 << 24 | 23 << 16 | 1 ); //this is in_addr.s_addr
	PrintDebug("VNET: store_topologies. NO VNET_SERVER, dest = %x\n", dest);
#elif
	dest = (0 | 172 << 24 | 23 << 16 | 2 ); //this is in_addr.s_addr
	PrintDebug("VNET: store_topologies. VNET_SERVER, dest = %x\n", dest);
#endif

	int type = UDP_TYPE;
	int src = 0;
	int src_type= ANY_SRC_TYPE;
	int data_port = 22;
	
	//store link table
      for(i = 0; i < MAX_LINKS; i++) {
          if (g_links[i].use == 0) {
               g_links[i].dest = (int)dest;
               g_links[i].type = type;
               g_links[i].link_sock = fd;
               g_links[i].remote_port = data_port;
               g_links[i].use = 1;

               if (g_first_link == -1) 
	             g_first_link = i;

               g_links[i].prev = g_last_link;
               g_links[i].next = -1;
      
               if (g_last_link != -1) {
	             g_links[g_last_link].next = i;
               }

               g_last_link = i;

               g_num_links++;
               PrintDebug("VNET: store_topologies. new link: socket: %d, remote %x:[%d]\n", g_links[i].link_sock, (uint_t)g_links[i].dest, g_links[i].remote_port);
         }
      }

  
	//store route table

	type = EDGE_TYPE;
	dest =0;

	for(i = 0; i < MAX_ROUTES; i++) {
    		if (g_routes[i].use == 0) {
                 if ((src_mac_qual != ANY_TYPE) && (src_mac_qual != NONE_TYPE)) {
	//		    memcpy(g_routes[i].src_mac, src_mac, 6);
     	 	    } else {
			memset(g_routes[i].src_mac, 0, 6);
      		    }
      
      		    if ((dest_mac_qual != ANY_TYPE) && (dest_mac_qual != NONE_TYPE)) {
	//		    memcpy(g_routes[i].dest_mac, dest_mac, 6);
      		    } else {
			   memset(g_routes[i].dest_mac, 0, 6);
      		    }

	      g_routes[i].src_mac_qual = src_mac_qual;
	      g_routes[i].dest_mac_qual = dest_mac_qual;
	      g_routes[i].dest = (int)dest;
	      g_routes[i].type = type;
	      g_routes[i].src = src;
	      g_routes[i].src_type = src_type;
	      
	      g_routes[i].use = 1;

	      if (g_first_route == -1) 
			g_first_route = i;

	      g_routes[i].prev = g_last_route;
	      g_routes[i].next = -1;

	      if (g_last_route != -1) {
		g_routes[g_last_route].next = i;
	      }

	      g_last_route = i;

	      g_num_routes++;

		PrintDebug("VNET: store_topologies. new route: src_mac: %s, dest_mac: %s, dest: %d\n", g_routes[i].src_mac, g_routes[i].dest_mac, dest);

	}
  }
}

static int match_route(char *src_mac, char *dst_mac, int src_type, int src_index, int *matches) 
{ 
    int values[MAX_ROUTES];
    int matched_routes[MAX_ROUTES];

    int num_matches = 0;
    int i;
    int max = 0;
    int no = 0;
    int exact_match = 0;

    FOREACH_ROUTE(i, g_routes, g_first_route) {
	    if ((g_routes[i].src_type != ANY_SRC_TYPE) &&
	         ((g_routes[i].src_type != src_type) ||
	         ((g_routes[i].src != src_index) &&
	         (g_routes[i].src != -1))))
	    {
	      PrintDebug("Vnet: MatchRoute: Source route is on and does not match\n");
	      continue;
	    }

	    if ( (g_routes[i].dest_mac_qual == ANY_TYPE) &&
	     (g_routes[i].src_mac_qual == ANY_TYPE) ) 
	    {      
	       matched_routes[num_matches] = i;
	       values[num_matches] = 3;
	       num_matches++;
	    }
	    
	    if (memcmp((void *)&g_routes[i].src_mac, (void *)src_mac, 6) == 0) 
	    {
	       if (g_routes[i].src_mac_qual !=  NOT_TYPE) {
	         if (g_routes[i].dest_mac_qual == ANY_TYPE) {
	       	matched_routes[num_matches] = i;
	       	values[num_matches] = 6;
	      
	       	num_matches++;
	    	   }else if (memcmp((void *)&g_routes[i].dest_mac, (void *)dst_mac, 6) == 0) {
	        	if (g_routes[i].dest_mac_qual != NOT_TYPE) {   
		        	matched_routes[num_matches] = i;
		        	values[num_matches] = 8;    
		        	exact_match = 1;
		        	num_matches++;
	        	}
	    	   }
	       }
	    }
	    
	    if (memcmp((void *)&g_routes[i].dest_mac, (void *)dst_mac, 6) == 0) 
	    {
	      	if (g_routes[i].dest_mac_qual != NOT_TYPE) {
	    		if (g_routes[i].src_mac_qual == ANY_TYPE) {
			      matched_routes[num_matches] = i;
			      values[num_matches] = 6;

			      num_matches++;
	    		} else if (memcmp((void *)&g_routes[i].src_mac, (void *)src_mac, 6) == 0) {
	      			if (g_routes[i].src_mac_qual != NOT_TYPE) {
	        			if (exact_match == 0) {
				          matched_routes[num_matches] = i;
				          values[num_matches] = 8;
				          num_matches++;
	        			}
	      			}
	    		}
	      }
	    }
	    
	    if ((g_routes[i].dest_mac_qual == NOT_TYPE) &&
	     	  (memcmp((void *)&g_routes[i].dest_mac, (void *)dst_mac, 6) != 0)) 
	    {
	      	if (g_routes[i].src_mac_qual == ANY_TYPE) {
		    matched_routes[num_matches] = i;
		    values[num_matches] = 5;		    
		    num_matches++;    
	       } else if (memcmp((void *)&g_routes[i].src_mac, (void *)src_mac, 6) == 0) {
	    		if (g_routes[i].src_mac_qual != NOT_TYPE) {      
			      matched_routes[num_matches] = i;
			      values[num_matches] = 7;		      
			      num_matches++;
	    		}
	      	}
	    }
	    
	    if ((g_routes[i].src_mac_qual == NOT_TYPE) &&
	         (memcmp((void *)&g_routes[i].src_mac, (void *)src_mac, 6) != 0)) 
	    {
	      	if (g_routes[i].dest_mac_qual == ANY_TYPE) {
		    matched_routes[num_matches] = i;
		    values[num_matches] = 5;	    
		    num_matches++;
	      	} else if (memcmp((void *)&g_routes[i].dest_mac, (void *)dst_mac, 6) == 0) {
	   		 if (g_routes[i].dest_mac_qual != NOT_TYPE) { 
			      matched_routes[num_matches] = i;
			      values[num_matches] = 7;
			      num_matches++;
	    		 }
	      }
	    }
  }//end FOREACH_ROUTE

  FOREACH_ROUTE(i, g_routes, g_first_route) {
    	if ((memcmp((void *)&g_routes[i].src_mac, (void *)src_mac, 6) == 0) &&
     		(g_routes[i].dest_mac_qual == NONE_TYPE) &&
     		((g_routes[i].src_type == ANY_SRC_TYPE) ||
       	((g_routes[i].src_type == src_type) &&
        	((g_routes[i].src == src_index) ||
           	(g_routes[i].src == -1))))) {
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

static inline int if_write_pkt(struct vnet_if_device *iface, struct raw_ethernet_pkt *pkt)
{
   return iface->input((uchar_t *)pkt->data, pkt->size);
}

static int handle_one_pkt(struct raw_ethernet_pkt *pkt)
{
    int src_link_index = 0;	//the value of src_link_index of udp always is 0
    int i;
    char src_mac[6];
    char dst_mac[6];

    int matches[g_num_routes];
    int num_matched_routes = 0;

    struct HEADERS headers;
  
    // get the ethernet and ip headers from the packet
    memcpy((void *)&headers, (void *)pkt->data, sizeof(headers));

    int j;
    for(j = 0;j < 6; j++) {
       src_mac[j] = headers.ethernetsrc[j];
       dst_mac[j] = headers.ethernetdest[j];
    }


#ifdef DEBUG
    char dest_str[18];
    char src_str[18];

    mac_to_string(src_mac, src_str);  
    mac_to_string(dst_mac, dest_str);

    PrintDebug("Vnet: HandleDataOverLink. SRC(%s), DEST(%s)\n", src_str, dest_str);
#endif

    char hash_key[HASH_KEY_SIZE];
    make_hash_key(hash_key, src_mac, dst_mac, EDGE_TYPE, src_link_index);

    num_matched_routes = look_into_cache((route_hashkey_t)hash_key, matches);

    if (num_matched_routes == -1) {//no match
        num_matched_routes = match_route(src_mac, dst_mac, pkt->type, src_link_index, matches);

	 if (num_matched_routes > 0)
	     add_route_to_cache(hash_key, num_matched_routes,matches);      
    }

    PrintDebug("Vnet: HandleDataOverLink: Matches=%d\n", num_matched_routes);

    for (i = 0; i < num_matched_routes; i++) {
        int route_index = -1;
        int link_index = -1;
        int dev_index = -1;

        route_index = matches[i];

        PrintDebug("Vnet: HandleDataOverLink: Forward packet from link according to Route entry %d\n", route_index);

        if (g_routes[route_index].type == EDGE_TYPE) {
            link_index = g_routes[route_index].dest;

            if(g_links[link_index].type == UDP_TYPE) {
                int size;

                if ((size = V3_SendTo_IP(g_links[link_index].link_sock,  g_links[link_index].dest,  g_links[link_index].remote_port, pkt->data, pkt->size)) != pkt->size)  {
                    PrintError("Vnet: sending by UDP Exception, %x\n", size);
                    return -1;
                }

                PrintDebug("Vnet: HandleDataOverLink: Serializing UDP Packet to link_sock [%d], dest [%x], remote_port [%d], size [%d]\n", g_links[link_index].link_sock, (uint_t)g_links[link_index].dest,  g_links[link_index].remote_port, (int)pkt->size);

            }else if (g_links[link_index].type == TCP_TYPE) {
 
            }
        } else if (g_routes[route_index].type == INTERFACE_TYPE) {
            dev_index = g_routes[route_index].dest;
      
            PrintDebug("Writing Packet to device=%s\n", g_devices[dev_index].device->name);

            if (if_write_pkt(g_devices[dev_index].device, pkt) == -1) {
    	          PrintDebug("Can't write output packet to link\n");
                return -1;
            }
        } else {
            PrintDebug("Vnet: Wrong Edge type\n");
        }
    }

     return 0;
}

static int send_ethernet_pkt(char *buf, int length)
{
	struct raw_ethernet_pkt *pt;

	pt = (struct raw_ethernet_pkt *)V3_Malloc(sizeof(struct raw_ethernet_pkt));
	raw_ethernet_packet_init(pt, buf, length);  //====here we copy sending data once 

	PrintDebug("VNET: vm_send_pkt: transmitting packet: (size:%d)\n", (int)pt->size);
	print_packet((char *)buf, length);
	
	v3_enqueue(g_inpkt_q, (addr_t)pt);
	return 0;
	
}

int V3_Send_pkt(uchar_t *buf, int length)
{
	PrintDebug("VNET: In V3_Send_pkt: pkt length %d\n", length);

	return send_ethernet_pkt((char *)buf, length);
}

static int add_device_to_table(struct vnet_if_device*device, int type) {
  int i;

  for (i = 0; i < MAX_DEVICES; i++) {
      if (g_devices[i].use == 0) {
          g_devices[i].type = type;
          g_devices[i].use = 1;

          if (g_first_device == -1) 
	        g_first_device = i;

          g_devices[i].prev = g_last_device;
          g_devices[i].next = -1;

          if (g_last_device != -1) 
	        g_devices[g_last_device].next = i;

          g_last_device = i;
          g_num_devices++;

	    return i;
      }
  }
  
  return -1;
}

static int search_device(char *device_name)
{
    int i;

    for (i=0; i<MAX_DEVICES; i++)
        if (g_devices[i].use == 1){
		if (!strcmp(device_name, g_devices[i].device->name))
			return i;
        }

    return -1;
}

static struct vnet_if_device* delete_device_from_table(int index) 
{
    int next_i;
    int prev_i;
    struct vnet_if_device *device = NULL;

    if (g_devices[index].use == 0) 
	  return NULL;

    g_devices[index].use = 0;

    prev_i = g_devices[index].prev;
    next_i = g_devices[index].next;

    if (prev_i != -1)
        g_devices[prev_i].next = g_devices[index].next;

    if (next_i != -1) 
        g_devices[next_i].prev = g_devices[index].prev;

    if (g_first_device == index)
        g_first_device = g_devices[index].next;

    if (g_last_device == index) 
        g_last_device = g_devices[index].prev;

    g_devices[index].next = -1;
    g_devices[index].prev = -1;

    device = g_devices[index].device;
    g_devices[index].device = NULL;

    g_num_devices--;

    return device;
}


int vnet_register_device(char *dev_name, int (*netif_input)(uchar_t * pkt, uint_t size), void *data)
{
	struct vnet_if_device *dev;

	dev = (struct vnet_if_device *)V3_Malloc(sizeof(struct vnet_if_device));

	if(dev == NULL){
		PrintError("VNET: Malloc fails\n");
		return -1;
	}

	strncpy(dev->name, dev_name, (strlen(dev_name) < 50)?strlen(dev_name):50);
	dev->input = netif_input;
	dev->data = data;

	if (add_device_to_table(dev, GENERAL_NIC) == -1)
		return -1;

	return 0;
}

int vnet_unregister_device(char *dev_name)
{
    int i;

    i=search_device(dev_name);
    if (i == -1)
        return -1;

    struct vnet_if_device *device = delete_device_from_table(i);
    if (device == NULL)
	  return -1;

    V3_Free(device);

    return 0;
}

int V3_Register_pkt_event(int (*netif_input)(uchar_t * pkt, uint_t size))
{
	return vnet_register_device("NE2000", netif_input, NULL);

}
int vnet_pkt_process()
{
	struct raw_ethernet_pkt *pt;
	int i;

	PrintDebug("VNET: In vnet_check\n");
	
	while ((pt = (struct raw_ethernet_pkt *)v3_dequeue(g_inpkt_q)) != NULL){

		PrintDebug("VNET: In vnet_check: pt length %d, pt type %d\n", (int)pt->size, (int)pt->type);

		for (i = 0; i <  (int)pt->size; i++)
			PrintDebug("%x ", pt->data[i]);
		PrintDebug("\n");
	
              if(handle_one_pkt(pt)) {
			PrintDebug("VNET: vnet_check: handle one packet!\n");  
		}

		V3_Free(pt); //be careful here
	}

	return 0;
}

static int process_tcpdata()
{
	return 0;
}

static int process_udpdata()
{

  struct raw_ethernet_pkt *pt;

  unsigned long dest = 0;
  unsigned short remote_port = 0;
  SOCK link_sock = g_udp_sockfd;
  int length = sizeof(struct raw_ethernet_pkt) - 2*sizeof(int);   //minus the "size" and "type" 

  //run in a loop to get packets from outside network, adding them to the incoming packet queue
  while (1) {
  	pt = (struct raw_ethernet_pkt *)V3_Malloc(sizeof(struct raw_ethernet_pkt));
  	if (pt == NULL){
  		PrintError("Vnet: process_udp: Malloc fails\n");
		continue;
  	}

	PrintDebug("Vnet: route_thread: socket: %d. ready to receive from ip %x, port [%d] or from VMs\n", link_sock, (uint_t)dest, remote_port);
  	pt->size = V3_RecvFrom_IP( link_sock, dest, remote_port, pt->data, length);
	PrintDebug("Vnet: route_thread: socket: %d receive from ip %x, port [%d]\n", link_sock, (uint_t)dest, remote_port);

  	if (pt->size <= 0){
  		PrintDebug("Vnet: process_udp: receiving packet from UDP fails\n");
  		V3_Free(pt);
		return -1;
  	}

       PrintDebug("Vnet: process_udp: get packet\n");
  	print_packet(pt->data, pt->size);
  	
  	v3_enqueue(g_inpkt_q, (addr_t)pt);

	//V3_Yield();
  }
}

static int indata_handler( )
{
      if (use_tcp)
	   process_tcpdata();
      else
      	   process_udpdata( );	  

      return 0;   
}

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

static void init_link_table()
{
    int i;
    for (i = 0; i < MAX_LINKS; i++) {
        g_links[i].use = 0;
        g_links[i].next = -1;
        g_links[i].prev = -1;
    }
	
    g_first_link = -1;
    g_last_link = -1;
    g_num_links = 0;
}

static void init_device_table()
{
    int i;
    for (i = 0; i < MAX_DEVICES; i++) {
        g_devices[i].use = 0;
        g_devices[i].next = -1;
        g_devices[i].prev = -1;
    }
	
    g_first_device = -1;
    g_last_device = -1;
    g_num_devices = 0;
}

static void init_route_table()
{	
    int i;
    for (i = 0; i < MAX_ROUTES; i++) {
        g_routes[i].use = 0;
        g_routes[i].next = -1;
        g_routes[i].prev = -1;
    }
	
     g_first_route = -1;
     g_last_route = -1;
     g_num_routes = 0;
}

static void init_tables()
{
    init_link_table();
    init_device_table();
    init_route_table();
    init_route_cache();
}

static void init_pkt_queue()
{
    PrintDebug("VNET Init package receiving queue\n");

    g_inpkt_q = v3_create_queue();
    v3_init_queue(g_inpkt_q);
}

void vnet_init()
{	
	vnet_server = 0;
	#ifdef VNET_SERVER
		vnet_server =1;
	#endif

	PrintDebug("VNET Init: Vnet input queue successful.\n");

	init_tables();
	init_pkt_queue();
 
	start_recv_data();
}


