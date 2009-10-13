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
 * Copyright (c) 2009, Peter Dinda <pdinda@northwestern.edu
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu>
 *            Yuan Tang <ytang@northwestern.edu>
 *		  Jack Lange <jarusl@cs.northwestern.edu> 
 *		  Peter Dinda <pdinda@northwestern.edu
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <devices/vnet.h>

#define MAX_ADDRESS 10
typedef enum {LOCAL,REMOTE} ctype;

struct handler{
  int    fd;
  int local_address;
  short  local_port;
  struct vnet_device *local_device;
  int remote_address;
  short  remote_port;
  struct vnet_device *remote_device;
  struct ethAddr addresses[MAX_ADDRESS];
  ctype local_config;
  ctype remote_config; 
};


typedef struct {
  int  size;
  char  data[ETHERNET_PACKET_LEN];
  int index;
}RawEthernetPacket;


#define NUM_DEVICES 1
#define NUM_HANDLERS 1

static struct vnet_device *available_devices[NUM_DEVICES];

struct handler *active_handlers[NUM_HANDLERS];

static int bind_address = 0;
static short  bind_port = 22;
//static char *vnet_version = "0.9";
static int vnet_server = 0;
static bool use_tcp = false;



static void print_packet(char *pkt, int size)
{
      int i;
	   
	PrintDebug("Vnet: packet: size: %d\n", size);
  	for (i = 0; i < size; i ++)
  		PrintDebug("%x ", pkt[i]);
  	PrintDebug("\n");
}


#if !(ROUTE)
static struct vnet_device *get_device(RawEthernetPacket *pt)
{
	return available_devices[0];
}
#endif

static struct handler * get_handler()
{
	
	return active_handlers[0];
}

static inline bool add_handler(struct handler *hd)
{
       int num = 0;
	   
	while (active_handlers[num] != NULL) num ++;

	if (num >= NUM_HANDLERS)
		return false;

	active_handlers[num] = hd;

	return true;
}

static inline bool add_device(struct vnet_device *dev)
{
       int num = 0;
	   
	while (available_devices[num] != NULL) num ++;

	if (num >= NUM_DEVICES)
		return false;

	available_devices[num] = dev;

	return true;
}


int CreateAndSetupTcpSocket(const int bufsize, const bool nodelay, const bool nonblocking)
{
  int mysocket;

  // create socket for connections
  if ((mysocket = V3_Create_TCP_Socket()) < 0) {
    return -1;
  }

  return mysocket;
}

int BindSocketwPort(const int mysocket, const int myport)
{
   if (V3_Bind_Socket(mysocket, myport) < 0) {
    return -1;
  }
  
  return 0;
}  

int ListenSocket(const int mysocket, const int maxc)
{
  return V3_Listen_Socket(mysocket, maxc);
}

int ConnectToHost(const int mysocket, const int hostip, const int port)
{
  return V3_Connect_To_IP(mysocket, hostip, port);
}

/*
void close(int mysocket)
{
  V3_Close_Socket(mysocket);
}
*/

#if 0 //May be needed later
int SendTo(int const mysocket, 
           const unsigned ip, const int port, 
           const char *buf, const int len, const bool sendall)
{
  int left;
  int sent;

  if (!sendall) {
    return V3_SendTo_IP(mysocket, ip, port, buf, len);
  } else {
    left = len;

    while (left>0) {
      sent = V3_SendTo_IP(mysocket, ip, port, buf, len);
      if (sent < 0) {	
          return -1;
      } else {
        left-=sent;
      }
    }
    return len;
  }
}


int ReceiveFrom(const int mysocket, 
                const unsigned ip, const int port, 
                char *buf, const int len,
                const bool recvall)
{
    int left;	
    int received;
  
    if (!recvall) {
       return V3_RecvFrom_IP(mysocket, ip, port, buf, len);
    } else {
       left = len;	

       while (left > 0) {
     		received = V3_RecvFrom_IP(mysocket, ip, port, buf, len);
      		if (received < 0) {
          		return -1;
      		} else if (received == 0) {
          		break;
      		} else {	
        		left -= received;
      		}
    	}
	
    	return len-left;
    }
}


int Send(int socketfd, const char *buf, const int len, const bool sendall)
{
  int left;
  int sent;

  if (!sendall) {
      return V3_Send(socketfd, buf, len);
  } else  {
      left = len;
	 
      while (left>0) {
          sent = V3_Send(socketfd, &(buf[len-left]), left);

          if (sent < 0) {
              return -1;
          } else if (sent == 0) {
              break;
          } else {
              left -= sent;
          }
      }
	  
      return len-left;
  }
}

int GetLine(int fd, char *buf, int size)
{
  int rc;
  int received = 0;
  char c;
  
  while (1) { 
    rc = Receive(fd, &c, 1, true);
    
    if (rc < 0) { 
      return rc;
    } 
	
    if (rc == 0 || c == '\n') {
      return received;
    }
	
    buf[received] = c;
    received += rc;

    if (received >= size)
	break;
  }

  return received;
}


int PutLine(int mysocket, char *buf, int size)
{
  buf[size] = '\n';
  return (Send(mysocket, buf, size+1, true) - 1);
}
#endif


static int readall(const int fd, char *buf, const int len, const int oneshot, const int awaitblock)
{
  int rc;
  int left;

  left = len;
  
  while (left > 0) {
    rc = V3_Recv(fd, &(buf[len-left]), left);
	
    if (oneshot) { 
      return rc;
    }
	
    if (rc <= 0) { 
      return rc;
    } else {
      left -= rc;
    }
  }
  
  return len;
}

static int writeall(const int fd, const char *buf, const int len, const int oneshot, const int awaitblock)
{
  int rc;
  int left;

  left = len;
  
  while (left > 0) {
    rc = V3_Send(fd, &(buf[len-left]), left);

    if (oneshot) { 
      return rc;
    }
	
    if (rc <= 0) { 
      return rc;
    } else {
      left -= rc;
    }
  }
  return len;
}

static void RawEthernetPacketInit(RawEthernetPacket *pt, const char *data, const size_t size)
{
  pt->size = size;
  memcpy(pt->data, data, size);
}

static bool RawEthernetPacketSerialize(RawEthernetPacket *pt, const int fd)
{
  if (writeall(fd,(char*)&(pt->size),sizeof(pt->size),0,1)!=sizeof(pt->size)) 
    {
      PrintError("Vnet: Serialization Exception\n");
      return false;
    }
  if (writeall(fd,pt->data,pt->size,0,1)!=(int)(pt->size))
    {
      PrintError("Vnet: Serialization Exception\n");
      return false;
    }

  return true;
}

static bool RawEthernetPacketUnserialize(RawEthernetPacket *pt, const int fd)
{
  if (readall(fd,(char*)&(pt->size),sizeof(pt->size),0,1) != sizeof(pt->size))
    {
      PrintError("Vnet: Unserialization Exception\n");
      return false;
    }
  if (readall(fd,pt->data,pt->size,0,1) != pt->size)
    {
      PrintError("Vnet: Unserialization Exception\n");
      return false;
    }

  return true;
}

static bool RawEthernetPacketSendUdp(RawEthernetPacket *pt, int sock_fd, int ip, short port)
{
  int size;

  PrintDebug("Vnet: sending by UDP socket %d  ip: %x,  port: %d\n", sock_fd, ip, port);
  
  if ((size = V3_SendTo_IP(sock_fd, ip, port, pt->data, pt->size)) != pt->size) 
    {
      PrintError("Vnet: sending by UDP Exception, %x\n", size);
      return false;
    }
 
  return true;
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

/*
static void print_device_addr(char *ethaddr)
{
      int i;
	   
	PrintDebug("Vnet: print_device_addr: ");
  	for (i = 0; i < 6; i ++)
  		PrintDebug("%x ", ethaddr[i]);
  	PrintDebug("\n");
}
*/
#endif


#if ROUTE  //new routing accrding to VNET-VTL, no hash --TY

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

#define in_range(c, lo, up)  ((char)c >= lo && (char)c <= up)
#define islower(c)           in_range(c, 'a', 'z')

//------------------hash function begin-------------------

#define HASH_KEY_SIZE 16
/* Hash key format:
 * 0-5:     src_eth_addr
 * 6-11:    dest_eth_addr
 * 12:      src type
 * 13-16:   src index
 */
 struct hash_key
{
	char *key_from_addr;
};

struct hash_value  // This is the hash value, defined as a dynamic array. Format: 0: num_matched_routes, 1...n: matches[] -- TY
{
	int num_matched_routes;
	int *matches; 
};
/*
    static int insert_some (struct hashtable * htable, struct hash_key *key, struct hash_value *value) { 
	return hashtable_insert(htable, (addr_t)key, (addr_t)value);	
    }
*/

DEFINE_HASHTABLE_INSERT(insert_some, struct hash_key *, struct hash_value *);
DEFINE_HASHTABLE_SEARCH(search_some, struct hash_key *, struct hash_value);
//DEFINE_HASHTABLE_REMOVE(remove_some, struct hash_key *, struct hash_value, 1);
//DEFINE_HASHTABLE_ITERATOR_SEARCH(search_itr_some, struct key);

struct hash_key *cache_key;   //maybe these three variables should be defined as local variable. -- TY
struct hash_value *cache_entry;
struct hashtable *hash_route;

/*
malloc_key_value()
{
cache_key = (hash_key *)V3_Malloc(sizeof(hash_key));
addr_t key = *cache_key;
}
*/
 // This is the hash algorithm used for UNIX ELF object files
inline size_t hash_from_key(const char * arg) 
{
    size_t hash = 0;
    size_t temp = 0;

    int i;
    for(i = 0; i < HASH_KEY_SIZE; i++) {
      hash = (hash << 4) + *(arg + i) + i;
      if ((temp = (hash & 0xF0000000))) {
	hash ^= (temp >> 24);
      }
      hash &= ~temp;
    }
    PrintDebug("Hash Value: %lu\n", (unsigned long)hash);
    return hash;
}

inline bool hash_key_equal(const char * left, const char * right)
{
      int i;
      for(i = 0; i < HASH_KEY_SIZE; i++) {
      if (left[i] != right[i]) {
	PrintDebug("HASHes not equal\n");
	return false;
      }
    }
    return true;
}

void make_hash_key(char * hash_key, char src_addr[6], char dest_addr[6], char src_type, int src_index) {
  int j;

  for(j = 0; j < 6; j++) {
    hash_key[j] = src_addr[j];
    hash_key[j + 6] = dest_addr[j] + 1;
  }

  hash_key[12] = src_type;

  *(int *)(hash_key + 12) = src_index;
}

int AddMatchedRoutesToCache(int num_matched_r, int * matches)
{
	cache_entry->num_matched_routes = num_matched_r;
	int i;
	for(i = 0; i < num_matched_r; i++) {
		cache_entry->matches = (int *)V3_Malloc(sizeof(int));
		*(cache_entry->matches) = matches[i];
	}
	if (!insert_some(hash_route, cache_key, cache_entry)) return -1; /*oom*/
	return 0;
}

void clear_hash_cache() 
{
    hashtable_destroy(hash_route, 1, 1);  //maybe there are some problems.
/*    	 hashtable_destroy(hash_route, 1, 1);
   	 hash_route = NULL;
    	 V3_Free(cache_key);
    	 V3_Free(cache_entry);
	 cache_key = NULL;
	 cache_entry = NULL;
*/
}

//int LookIntoCache(char compare_address[HASH_KEY_SIZE], int * do_we_analyze, int * matches) {
int LookIntoCache(char compare_address[HASH_KEY_SIZE], int * matches) 
{
  int n_matches = 0;
  int i;
  struct hash_value *found;

  cache_key->key_from_addr = compare_address;
  if (NULL != (found = search_some(hash_route, cache_key))) {
    n_matches = found->num_matched_routes;
    for (i = 0; i < found->num_matched_routes; i++) {
      //*(matches+i) = *(found->matches+i);
      matches[i] = found->matches[i];
    }
    return n_matches;
  } else {
    PrintDebug("VNET: LookIntoCache, hash key not found\n");
    return -1;
  }
}


//------------------hash function end-------------------

char vnet_toupper(char c)
{
	if (islower(c))
		c -= 'a'-'A';
	return c;
}

char hexnybbletonybble(const char hexnybble) {
  char x = vnet_toupper(hexnybble);
  if ((x >= '0') && (x <= '9')) {
    return x - '0';
  } else {
    return 10 + (x - 'A');
  }
}

void hexbytetobyte(const char hexbyte[2], char *byte) {
  *byte = ((hexnybbletonybble(hexbyte[0]) << 4) + 
	   (hexnybbletonybble(hexbyte[1]) & 0xf));
}

char nybbletohexnybble(const char nybble) {
  return (nybble >= 10) ? (nybble - 10 + 'A') : (nybble + '0');
}

void bytetohexbyte(const char byte, char hexbyte[2]) {
  hexbyte[0] = nybbletohexnybble((byte >> 4) & 0xf);
  hexbyte[1] = nybbletohexnybble(byte & 0xf);
}

void string_to_mac(const char * str, char mac[6]) {
  int k;
  for(k = 0; k < 6; k++) {
    hexbytetobyte(&(str[(2 * k) + k]), mac + k);
  }
}

void mac_to_string(char address[6], char * buf) {
  int i;
  for (i = 0; i < 6; i++) {
    bytetohexbyte(address[i], &(buf[3 * i]));
    buf[(3 * i) + 2] = ':';
  }
  buf[17] = 0;
}
/*
void ip_to_string(ulong_t addr, char * buf) {
  struct in_addr addr_st;
  char * tmp_str;

  addr_st.s_addr = htonl(addr);
  tmp_str = inet_ntoa(addr_st);

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

int if_write_pkt(iface_t *iface, RawEthernetPacket *pkt)
{
   iface->input((uchar_t *)pkt->data, pkt->size);
   return 0;
}



//int add_link_entry(unsigned long dest, int type, int link_class, int data_port, int authenticated, SOCK fd) {
int add_link_entry(unsigned long dest, int type, int data_port,  SOCK fd) {
  int i;

  for(i = 0; i < MAX_LINKS; i++) {
    if (g_links[i].use == 0) {
      g_links[i].dest = dest;
 //     g_links[i].authenticated = authenticated;
      g_links[i].type = type;
      g_links[i].link_sock = fd;
      g_links[i].remote_port = data_port;
      g_links[i].use = 1;
 //     g_links[i].link_class = link_class;

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




int add_sock(struct sock_list * socks, int  len, int * first_sock, int * last_sock, SOCK fd) {
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




int add_route_entry(char src_mac[6], char dest_mac[6], int src_mac_qual, int dest_mac_qual, int dest, int type, int src, int src_type) {
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
  return -1;
}


/*
 * This returns an integer, if the interger is negative then AddLink failed, else it was
 * successful and returns the socket descriptor
 */

#if 0
int AddLink(string input, SOCK * link_sock, string * ret_str) {
  string output;
  string hello, password, version, command;
  string remote_address, type, remote_password;
  int ctrl_port = 0;
  int data_port = 0;
  char buf[128];
  
  struct hostent * remote_he;
  struct in_addr dest_addr;
  unsigned long dest;

  SOCK data_sock;
  con_t ctrl_con;
  int link_index;
  int ret = CTRL_DO_NOTHING;

  struct sockaddr_in local_address;
  int local_port;

  *link_sock = 0;

  {
	  istringstream is(input,istringstream::in);
    is >> hello >> password >> version >> command >> remote_address >> ctrl_port >> data_port >> remote_password >> type;
  }


  if ((remote_address.empty()) || (remote_password.empty()) ||  (type.empty()) || (ctrl_port == 0) || (data_port == 0)) {
    *ret_str = "Invalid Addlink command";
    return CTRL_ERROR;
  }

  if (get_tunnel_type(type) == -1) {
    *ret_str = "Invalid link type";
    return CTRL_ERROR;
  }

  if ((remote_he = gethostbyname(remote_address.c_str())) == NULL) {
    *ret_str = "Could not lookup address of " + remote_address;
    return CTRL_ERROR;
  }

  dest_addr = *((struct in_addr *)remote_he->h_addr);
  dest = dest_addr.s_addr;
  
  link_index = find_link_entry(dest, get_tunnel_type(type));

  if (link_index != -1) {
    JRLDBG("Link already exists\n");
    *ret_str = "Link already Exists";
    return CTRL_ERROR;
  }


  if(type == TCP_STR) {
    struct sockaddr_in serv_addr;

    if ((data_sock = CreateAndSetupTcpSocket()) < 0) {
      cerr << "the client descriptor could not be opened" << endl;
      *ret_str = "Could not establish link";
      return CTRL_ERROR;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(data_port);
    serv_addr.sin_addr = dest_addr;
    memset(&(serv_addr.sin_zero), 0, 8);
    
    JRLDBG("Connecting TCP Data link\n");
    /*we establish a connection with the server */
    if (connect(data_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
      cerr << "the connection could not be established : " << endl;
      *ret_str = "Could not establish link";
      return CTRL_ERROR;
    }


    local_port = data_port;

    *ret_str = "Successfully added a TCP link to " + remote_address;
    ret = CTRL_ADD_TCP_SOCK;

  } else if(type == UDP_STR) {
    
    if (GetLocalSockAddress(g_udp_sockfd, (struct sockaddr *)&local_address) == -1) {
      JRLDBG("Could not get local address\n");
      return CTRL_ERROR;
    }
    local_port = ntohs(local_address.sin_port);

    data_sock = g_udp_sockfd;
    *ret_str = "Successfully added a virtual UDP link to " + remote_address;

  } 

  *link_sock = data_sock;

  if (create_ctrl_connection(&ctrl_con, remote_address, ctrl_port) == -1) {
    JRLDBG("Could not establish control connection\n");
    *ret_str = "Could not register new link";
    if (type == TCP_STR) {
      CLOSE_SSL(use_ssl_data, ssl);
      CLOSE(*link_sock);
    }
    return CTRL_ERROR;
  }

  JRLDBG("ctrl session established\n");

  snprintf(buf, 128, "%s %d", "myself", local_port);
  output = "REGISTERLINK ";
  output = output + buf + " " + type;

  if (send_ctrl_msg(ctrl_con, output, remote_password) == -1) {
    JRLDBG("Could not send the control message\n");
    *ret_str = "Could not register new link";
    if (type == TCP_STR) {
      CLOSE_SSL(use_ssl_data, ssl);
      CLOSE(*link_sock);
    }
    return CTRL_ERROR;
  }

  JRLDBG("Ctrl message sent\n");
  if (close_ctrl_connection(ctrl_con, remote_password) == -1) {
    JRLDBG("Could not close control connection\n");
  }

  link_index = add_link_entry(dest, get_tunnel_type(type), LINK, data_port, 1, data_sock);
  
  if (link_index == -1) {
    *ret_str = "Could not add link entry";
    if (type == TCP_STR) {
      CLOSE_SSL(use_ssl_data, ssl);
      CLOSE(*link_sock);
    }
    return CTRL_ERROR;
  }

  JRLDBG("Addlink Created link: FD=%d\n", g_links[link_index].link_sock);

  return ret;
}



int AddRoute(string input, string * ret_str) {
  int src_mac_qual;
  int dest_mac_qual;

  int dest;
  int src;

  int route_index;

  int route_type;
  int src_route_type;

  char src_mac[6];
  char dest_mac[6];

  string hello, version, password, command;
  string src_str, dest_str, src_mac_str, dest_mac_str, src_type, type;
  

  {
    istringstream is(input,istringstream::in);
    is >> hello >> password >> version >> command >> src_mac_str >> dest_mac_str >>  type >> dest_str >> src_type >> src_str;
  }

  if ((src_mac_str.empty()) || (dest_mac_str.empty()) || 
      (dest_str.empty()) || (type.empty())) {
    *ret_str = "Invalid AddRoute command";
    return CTRL_ERROR;
  }


  if (get_route_type(type) == -1) {
    *ret_str = "Invalid route type";
    return CTRL_ERROR;
  }


  if (!(src_type.empty()) && (get_route_src_type(src_type) == -1)) {
    *ret_str = "Invalid route source type";
    return CTRL_ERROR;
  }


  //We will first do some parsing to obtain correct the qualifier for source and destination address
  if (src_mac_str.find(":",0) == string::npos) {
    src_mac_qual = get_qual_type(src_mac_str);

    if (src_mac_qual == -1) {
      *ret_str = "Invalid Qual Type";
      return CTRL_ERROR;
    }

    memset(src_mac, 0, 6);
  } else {
    if(src_mac_str.substr(0,3) == NOT) {
      src_mac_qual = NOT_TYPE;
      src_mac_str = src_mac_str.substr(4);
    } else {
      src_mac_qual = EMPTY_TYPE;
    }
    
    src_mac_str = src_mac_str.substr(0,2) + src_mac_str.substr(3,2) + src_mac_str.substr(6,2) + src_mac_str.substr(9,2) + src_mac_str.substr(12,2) + src_mac_str.substr(15,2);
  
    ConvertHexEthernetAddressToBinary(src_mac_str.c_str(), src_mac);
  }
  

  if (dest_mac_str.find (":",0) == string::npos) {
    dest_mac_qual = get_qual_type(dest_mac_str);

    if (dest_mac_qual == -1) {
	  ret_str->assign("Invalid Qual Type");
      return CTRL_ERROR;
    }

    memset(dest_mac, 0, 6);
  } else {
    if(dest_mac_str.substr(0,3) == NOT) {
      dest_mac_qual = NOT_TYPE;
      dest_mac_str = dest_mac_str.substr(4);
    } else {
      dest_mac_qual = EMPTY_TYPE;
    }
    
    dest_mac_str = dest_mac_str.substr(0,2) + dest_mac_str.substr(3,2) + dest_mac_str.substr(6,2) + dest_mac_str.substr(9,2) + dest_mac_str.substr(12,2) + dest_mac_str.substr(15,2);
    
    ConvertHexEthernetAddressToBinary(dest_mac_str.c_str(), dest_mac);  
  }
  
  if (type == INTERFACE) {
    int dest_dev;
    
    dest_dev = get_device(dest_str);

    if (dest_dev == -1) {
	  ret_str->assign("Could not find Destination Interface");
      return CTRL_ERROR;
    }

    dest = dest_dev;
    
    route_type = INTERFACE_TYPE;
  } else if (type == EDGE) {
    hostent * remote_he;

    if ((remote_he = gethostbyname(dest_str.c_str())) == NULL) {
      ret_str->assign("Could not lookup address for " + dest_str);
      return CTRL_ERROR;
    }

    dest = find_link_entry((*((struct in_addr *)remote_he->h_addr)).s_addr, -1);
    if (dest == -1) {
      JRLDBG("AddRoute: Invalid Destination: %s\n", inet_ntoa((*((struct in_addr *)remote_he->h_addr))));
      ret_str->assign("Addroute requested for invalid destination");
      return CTRL_ERROR;
    }
     
    route_type = EDGE_TYPE;
  } 


  if (src_type.empty() || (src_type == ANY_SRC)) {
    src = -1;
    src_route_type = ANY_SRC_TYPE;
  } else if (src_type == INTERFACE) {

    if (src_str.empty()) {
		ret_str->assign("Missing Source INTERFACE");
      return CTRL_ERROR;
    }

    if (src_str == ANY_SRC) {
      src = -1;
    } else {
      int src_dev;
      src_dev = get_device(src_str);
      
      if (src_dev == -1) {
		  ret_str->assign("Could not find Source Interface");
	return CTRL_ERROR;
      }
      
      src = src_dev;
    }
    src_route_type = INTERFACE_TYPE;

  } else if (src_type == EDGE) {

    if (src_str == ANY_SRC) {
      src = -1;
    } else {
      hostent * src_he;

      if (src_str.empty()) {
		  ret_str->assign("Missing Source EDGE Address");
	return CTRL_ERROR;
      }
      
      if ((src_he = gethostbyname(src_str.c_str())) == NULL) {
		  ret_str->assign("Could not lookup address for " + src_str);
	return CTRL_ERROR;
      }

      src = find_link_entry((*((struct in_addr *)src_he->h_addr)).s_addr, -1);
  
      if (src == -1) {
	JRLDBG("AddRoute: Invalid Destination: %s\n", inet_ntoa((*((struct in_addr *)src_he->h_addr))));
		ret_str->assign("Addroute requested for invalid destination");
	return CTRL_ERROR;
      }
    }

    src_route_type = EDGE_TYPE;

  }


  JRLDBG("Route Def: %d:%d:%d:%d:%d:%d  %d:%d:%d:%d:%d:%d src_qual=%d, dest_qual=%d, dest=%d, type=%d src=%d, src_type=%d\n",  src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5], dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5], src_mac_qual, dest_mac_qual, dest, route_type, src, src_route_type);

  route_index = find_route_entry(src_mac, dest_mac, src_mac_qual, dest_mac_qual, dest, route_type, src, src_route_type);

  if (route_index != -1) {
    JRLDBG("Route Already Exists, Index: %d\n", route_index);
	ret_str->assign("Route alredy Exists");
    return CTRL_ERROR;
  }

  route_index = add_route_entry(src_mac, dest_mac, src_mac_qual, dest_mac_qual, dest, get_route_type(type), src, src_route_type);
    
  if (route_index >= 0) {
	  ret_str->assign("Successfully added the route for " +src_mac_str+ " and "+dest_mac_str);
  } else {
	  ret_str->assign("Could not add route for " + src_mac_str + " and " + dest_mac_str);
    return CTRL_ERROR;
  }

  return route_index;  
}

#endif

int add_device_to_table(char *device_name, int type) {
  int i;

  for (i = 0; i < MAX_DEVICES; i++) {
    if (g_devices[i].use == 0) {
 //     g_devices[i].device_name = device_name;
      strcpy(g_devices[i].device_name, device_name);
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


int find_link_entry(unsigned long dest, int type) {
  int i;

  //  PringDebug("Src: %lu, Dst: %lu\n", src, dest);

  FOREACH_LINK(i, g_links, g_first_link) {
    if ( (g_links[i].dest == dest) && 
	 ((type == -1) || (g_links[i].type == type)) ) {
      return i;
    }
  } 

  return -1;
}

int delete_link_entry(int index) {
  int next_i;
  int prev_i;

  if (g_links[index].use == 0) {
    return -1;
  }

  g_links[index].dest = 0;
//  g_links[index].authenticated = 0;
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

int delete_link_entry_by_addr(unsigned long dest, int type) {
  int index = find_link_entry(dest, type);
  
  if (index == -1) {
    return -1;
  }

  return delete_link_entry(index);
}

#if 0
int delete_device(string device_name) {
  int index = get_device(device_name);
  
  if (index == -1) {
    return -1;
  }

  delete_device(index);

  return 0;
}

#endif

int delete_device(int index) {
  int next_i;
  int prev_i;

  g_devices[index].device_name = "";
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

  g_num_devices--;

  return 0;
}

int find_route_entry(char src_mac[6], char dest_mac[6], int src_mac_qual, int dest_mac_qual, int dest, int type, int src, int src_type) {
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
    if ( (memcmp(temp_src_mac, g_routes[i].src_mac, 6) == 0) && 
	 (memcmp(temp_dest_mac, g_routes[i].dest_mac, 6) == 0) &&
	 (g_routes[i].src_mac_qual == src_mac_qual) &&
	 (g_routes[i].dest_mac_qual == dest_mac_qual)  &&
	 ( (type == -1) || 
	   ((type == g_routes[i].type) && (g_routes[i].dest == dest))) &&
	 ( (src_type == -1) || 
	   ((src_type == g_routes[i].src_type) && (g_routes[i].src == src))) ) {
      return i;
    }
  } 

  return -1;
}

int delete_route_entry(int index) {
  
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

  return 0;
}

int delete_route_entry_by_addr(char src_mac[6], char dest_mac[6], int src_mac_qual, int dest_mac_qual, int dest, int type, int src, int src_type) {
  int index = find_route_entry(src_mac, dest_mac, src_mac_qual, dest_mac_qual, dest, type, src, src_type);
  
  if (index == -1) {
    return -1;
  }

  delete_route_entry(index);

  return 0;
}


#if 0
int DeleteLink(string input, SOCK * link_sock, string * ret_str) {
  string output;
  string hello,password, version, command;
  string remote_address, type, remote_password;
  int ctrl_port = 0;

  struct hostent * remote_he;
  unsigned long dest_addr;

  int link_index;
  int ret = CTRL_DO_NOTHING;

  con_t ctrl_con;
  struct sockaddr_in local_address;
  char buf[128];

  *link_sock = 0;

  {
    istringstream is(input,istringstream::in);
    is >> hello >> password >> version >> command >> remote_address >> ctrl_port >> remote_password >> type ;
  }

  if ((remote_address.empty()) || (remote_password.empty()) || 
      (type.empty()) || (ctrl_port == 0)) {
    *ret_str = "Invalid Deletelink command";
    return CTRL_ERROR;
  }

  if (get_tunnel_type(type) == -1) {
    *ret_str = "Invalid link type";
    return CTRL_ERROR;
  }

  if ((remote_he = gethostbyname(remote_address.c_str())) == NULL) {
    *ret_str = "could not lookup address of " + remote_address;
    return CTRL_ERROR;
  }

  dest_addr = (*((struct in_addr *)remote_he->h_addr)).s_addr; 

  JRLDBG("Deleting  Dst: %lu\n", dest_addr);

  link_index = find_link_entry(dest_addr, get_tunnel_type(type));


  if (link_index == -1) {
 
    JRLDBG("Link Does Not Exist\n");
    *ret_str = "Link Does not exist";
    return CTRL_ERROR;
  }

  if (type == TCP_STR) {
    *link_sock = g_links[link_index].link_sock;
    *ret_str = "Successfully deleted a TCP link to " + remote_address;
    ret = CTRL_DELETE_TCP_SOCK;

  } else if (type == UDP_STR) {
    *ret_str = "Successfully deleted a virtual UDP link to " + remote_address;
  } else if (type == VTP_STR) {
    *ret_str = "Successfully deleted a VTP link to " + remote_address;
    // TODO: Send delete control message
  }



  if (create_ctrl_connection(&ctrl_con, remote_address, ctrl_port) == -1) {
    JRLDBG("Could not establish control connection\n");
    *ret_str = "Could not deregister link";
    CLOSE(*link_sock);
    return CTRL_ERROR;
  }


  /*
    if (GetLocalSockAddress(ctrl_con.sock, (struct sockaddr *)&local_address) == -1) {
    JRLDBG("Could not get local address\n");
    *ret_str = "Could not deregister link";
    CLOSE(*link_sock);
    return CTRL_ERROR;
    }
    
    snprintf(buf, 128, "%s", inet_ntoa(local_address.sin_addr));
  */
  output = "DEREGISTERLINK ";
  output = output + "myself" + " " + type;

  if (send_ctrl_msg(ctrl_con, output, remote_password) == -1) {
    // Could not register on the remote host, 
    CLOSE(*link_sock);
    *ret_str = "Could not deregister link on remote host";
    return CTRL_ERROR;
  }

  if (close_ctrl_connection(ctrl_con, remote_password) == -1) {
    JRLDBG("Could not close control connection\n");
  }

  if (close_link(link_index) == -1) {
    *ret_str = "Could not delete link";
    return CTRL_ERROR;
  }

  return ret;
}


int DeleteRoute(string input, string * ret_str) {
  int src_mac_qual;
  int dest_mac_qual;

  int dest;
  int src;

  int ret = 0;
  char src_mac[6];
  char dest_mac[6];

  int route_type;
  int src_route_type;

  string hello, version, password, command;
  string src_str, dest_str, src_mac_str, dest_mac_str, type, src_type;
  
  {
    istringstream is(input,istringstream::in);
    is >> hello >> password >> version >> command >> src_mac_str >> dest_mac_str >> type >> dest_str >> src_type >> src_str ;
  }

  if ((src_mac_str.empty()) || (dest_mac_str.empty()) || 
      (dest_str.empty()) || (type.empty())) {
		  ret_str->assign("Invalid Deleteroute command");
    return CTRL_ERROR;
  }


  if (get_route_type(type) == -1) {
	  ret_str->assign("Invalid route type");
    return CTRL_ERROR;
  }

  if (!(src_type.empty()) && (get_route_src_type(src_type) == -1)) {
    ret_str->assign("Invalid route source type");
    return CTRL_ERROR;
  }

  //We will first do some parsing to obtain correct the qualifier for source and destination address
  if (src_mac_str.find(":",0) == string::npos) {
    src_mac_qual = get_qual_type(src_mac_str);

    if (src_mac_qual == -1) {
      ret_str->assign("Invalid qual type");
      return CTRL_ERROR;
    }

    memset(src_mac, 0, 6);
  } else {
    if(src_mac_str.substr(0,3) == NOT) {
      src_mac_qual = NOT_TYPE;
      src_mac_str = src_mac_str.substr(4);
    } else {
      src_mac_qual = EMPTY_TYPE;
    }
    
    src_mac_str = src_mac_str.substr(0,2) + src_mac_str.substr(3,2) + src_mac_str.substr(6,2) + src_mac_str.substr(9,2) + src_mac_str.substr(12,2) + src_mac_str.substr(15,2);
  
    ConvertHexEthernetAddressToBinary(src_mac_str.c_str(), src_mac);
  }
  

  if (dest_mac_str.find (":",0) == string::npos) {
    dest_mac_qual = get_qual_type(dest_mac_str);

    if (dest_mac_qual == -1) {
		ret_str->assign("Invalid Qual Type");
      return CTRL_ERROR;
    }

    memset(dest_mac, 0, 6);
  } else {
    if(dest_mac_str.substr(0,3) == NOT) {
      dest_mac_qual = NOT_TYPE;
      dest_mac_str = dest_mac_str.substr(4);
    } else {
      dest_mac_qual = EMPTY_TYPE;
    }
    
    dest_mac_str = dest_mac_str.substr(0,2) + dest_mac_str.substr(3,2) + dest_mac_str.substr(6,2) + dest_mac_str.substr(9,2) + dest_mac_str.substr(12,2) + dest_mac_str.substr(15,2);
    
    ConvertHexEthernetAddressToBinary(dest_mac_str.c_str(), dest_mac);  
  }
  
  if (type == INTERFACE) {
    int dest_dev;

    dest_dev = get_device(dest_str);

    if (dest_dev == -1) {
		ret_str->assign("Could not find Destination Interface");
      return CTRL_ERROR;
    }

    dest = dest_dev;

    route_type = INTERFACE_TYPE;
  } else if (type == EDGE) {
    hostent * remote_he;

    if ((remote_he = gethostbyname(dest_str.c_str())) == NULL) {
		ret_str->assign("Could not lookup address for " + dest_str);
      return CTRL_ERROR;
    }

    dest = find_link_entry((*((struct in_addr *)remote_he->h_addr)).s_addr, -1);
    if (dest == -1) {
		ret_str->assign("Route requested for invalid destination");
      return CTRL_ERROR;
    }

    route_type = EDGE_TYPE;
  }


  if (src_type.empty() || (src_type == ANY_SRC)) {
    src = -1;
    src_route_type = ANY_SRC_TYPE;
  } else if (src_type == INTERFACE) {

    if (src_str.empty()) {
		ret_str->assign("Missing Source INTERFACE");
      return CTRL_ERROR;
    }
    if (src_str == ANY_SRC) {
      src = -1;
    } else {
      int src_dev;
      src_dev = get_device(src_str);
      
      if (src_dev == -1) {
		  ret_str->assign("Could not find Source Interface");
	return CTRL_ERROR;
      }
      
      src = src_dev;
    }
    src_route_type = INTERFACE_TYPE;

  } else if (src_type == EDGE) {

    if (src_str.empty()) {
		ret_str->assign("Missing Source EDGE Address");
      return CTRL_ERROR;
    }

    if (src_str == ANY_SRC) {
      src = -1;
    } else {
      hostent * src_he;
    
      if ((src_he = gethostbyname(src_str.c_str())) == NULL) {
		  ret_str->assign("Could not lookup address for " + src_str);
	return CTRL_ERROR;
      }
      
      src = find_link_entry((*((struct in_addr *)src_he->h_addr)).s_addr, -1);

      if (src == -1) {
	JRLDBG("DeleteRoute: Invalid Destination: %s\n", inet_ntoa((*((struct in_addr *)src_he->h_addr))));
		ret_str->assign("DeleteToute requested for invalid destination");
	return CTRL_ERROR;
      }
    }
    src_route_type = EDGE_TYPE;
  }

  ret = delete_route_entry(src_mac, dest_mac, src_mac_qual, dest_mac_qual, dest, route_type, src, src_route_type);
  
  if (ret == 0) {
	  ret_str->assign("Successfully deleted the route for " +src_mac_str+ " and "+dest_mac_str);
  } else {
	  ret_str->assign("Could not delete the route");
    return CTRL_ERROR;
  }
  
  return ret;
}
#endif

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

/*
//During testing, route table, link table, etc. are writen in program, isteadjing of AddRoute(), AddLink()...
void Create_topologies()
{
	//create link table
	unsigned long dest; //  dest_addr = *((struct in_addr *)remote_he->h_addr);  dest = dest_addr.s_addr;
	add_link_entry(dest, TCP_STR, int data_port, SOCK fd)  //the parameter need modify

	//create device table

	add_device_to_table(eth0)

	//create route table
	//example(VnetClientJava):FORWARD endeavor.cs.northwestern.edu EDGE not-00:0C:29:0F:4E:AA 00:0C:29:0F:4E:AA virtuoso-22.cs.northwestern.edu ANY ANY	
	//format(VnetClientJava): String routeStr = "FORWARD " + host.getHostname() + " " + dstType +  " " +  srcMacStr + " " + dstMacStr + " " + dst + " " + srcType + " " + src; 
	char src_mac[6]; //={};  //00:0C:29:0F:4E:AA
  	char dest_mac[6];	//00:0C:29:0F:4E:AA, empty type is only a mac adress.
	add_route_entry(src_mac, dest_mac, NOT_TYPE, EMPTY_TYPE, , EDGE, ANY, ANY); //decide "dest" after creade device table or link table.
}

*/


//int data_port, is udp/tcp port.  SOCK fd comes from socket() function.. so this function is called after socket().
void store_topologies(int data_port,  SOCK fd)
{
	int i;
	int src_mac_qual = ANY_TYPE;
	int dest_mac_qual = ANY_TYPE;
	int dest =0; //this is in_addr.s_addr
	int type = UDP_TYPE;
	int src = 0;
	int src_type= ANY_SRC_TYPE;
	
	//store link table
  for(i = 0; i < 1; i++) {
    if (g_links[i].use == 0) {
      g_links[i].dest = dest;
 //     g_links[i].authenticated = authenticated;
      g_links[i].type = type;
      g_links[i].link_sock = fd;
      g_links[i].remote_port = data_port;
      g_links[i].use = 1;
 //     g_links[i].link_class = link_class;

      if (g_first_link == -1) 
	g_first_link = i;

      g_links[i].prev = g_last_link;
      g_links[i].next = -1;
      
      if (g_last_link != -1) {
	g_links[g_last_link].next = i;
      }

      g_last_link = i;

      g_num_links++;
    }
  }
	//store route table

	type = EDGE_TYPE;
	dest =0;

	for(i = 0; i < 1; i++) {
    		if (g_routes[i].use == 0) {
     
     		 if ((src_mac_qual != ANY_TYPE) && (src_mac_qual != NONE_TYPE)) {
	//		memcpy(g_routes[i].src_mac, src_mac, 6);
     	 	} else {
			memset(g_routes[i].src_mac, 0, 6);
      		}
      
      		if ((dest_mac_qual != ANY_TYPE) && (dest_mac_qual != NONE_TYPE)) {
	//		memcpy(g_routes[i].dest_mac, dest_mac, 6);
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

	}
  }
}

//int MatchRoute(char * src_mac, char * dst_mac, int src_type, int src_index, int * do_we_analyze, int * matches) {
int MatchRoute(char * src_mac, char * dst_mac, int src_type, int src_index, int * matches) { 

  int values[g_num_routes];
  int matched_routes[g_num_routes];

  int num_matches = 0;
  int i;
  int max = 0;
  int no = 0;
  int exact_match = 0;



   FOREACH_ROUTE(i, g_routes, g_first_route) {
    if ( (g_routes[i].src_type != ANY_SRC_TYPE) &&
     ( (g_routes[i].src_type != src_type) ||
       ( (g_routes[i].src != src_index) &&
         (g_routes[i].src != -1) ) ) ) {
      PrintDebug("Source route is on and does not match\n");
      continue;
    }

    if ( (g_routes[i].dest_mac_qual == ANY_TYPE) &&
     (g_routes[i].src_mac_qual == ANY_TYPE) ) {
      
      matched_routes[num_matches] = i;
      values[num_matches] = 3;

      num_matches++;
    }
    
    if (memcmp((void *)&g_routes[i].src_mac, (void *)src_mac, 6) == 0) {
      if (g_routes[i].src_mac_qual !=  NOT_TYPE) {
    if (g_routes[i].dest_mac_qual == ANY_TYPE) {
      
      matched_routes[num_matches] = i;
      values[num_matches] = 6;
      
      num_matches++;
    } else if (memcmp((void *)&g_routes[i].dest_mac, (void *)dst_mac, 6) == 0) {
      if (g_routes[i].dest_mac_qual != NOT_TYPE) {
        
        matched_routes[num_matches] = i;
        values[num_matches] = 8;
        
        exact_match = 1;

        num_matches++;
      }
    }
      }
    }
    
    if (memcmp((void *)&g_routes[i].dest_mac, (void *)dst_mac, 6) == 0) {
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
    
    if ( (g_routes[i].dest_mac_qual == NOT_TYPE) &&
     (memcmp((void *)&g_routes[i].dest_mac, (void *)dst_mac, 6) != 0)) {
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
    
    if ( (g_routes[i].src_mac_qual == NOT_TYPE) &&
     (memcmp((void *)&g_routes[i].src_mac, (void *)src_mac, 6) != 0) ) {
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
  }



  FOREACH_ROUTE(i, g_routes, g_first_route) {
    if ( (memcmp((void *)&g_routes[i].src_mac, (void *)src_mac, 6) == 0) &&
     (g_routes[i].dest_mac_qual == NONE_TYPE) &&
     ( (g_routes[i].src_type == ANY_SRC_TYPE) ||
       ( (g_routes[i].src_type == src_type) &&
         ( (g_routes[i].src == src_index) ||
           (g_routes[i].src == -1) )))) {

      matched_routes[num_matches] = i;
      values[num_matches] = 4;
      PrintDebug("We matched a default route (%d)\n", i);
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

bool HandleDataOverLink(RawEthernetPacket *pkt, int src_link_index)
{

  char src_mac[6];
  char dst_mac[6];
//  int do_we_analyze;

  int matches[g_num_routes];

  int num_matched_routes = 0;
  int i;
  struct HEADERS headers;
//  char hash_key[HASH_KEY_SIZE];

  // get the ethernet and ip headers from the packet
  memcpy((void *)&headers, (void *)pkt->data, sizeof(headers));
  int j;
  for(j = 0;j < 6; j++) {
    src_mac[j] = headers.ethernetsrc[j];
    dst_mac[j] = headers.ethernetdest[j];
  }
 

//#ifdef DEBUG
  char dest_str[18];
  char src_str[18];

  mac_to_string(src_mac, src_str);  
  mac_to_string(dst_mac, dest_str);

  PrintDebug("SRC(%s), DEST(%s)\n", src_str, dest_str);
//#endif

//  make_hash_key(hash_key, src_mac, dst_mac, EDGE_TYPE, src_link_index);


  //  num_matched_routes = MatchRoute(src_mac, dst_mac, EDGE_TYPE, src_link_index, &do_we_analyze, matches);
  num_matched_routes = MatchRoute(src_mac, dst_mac, EDGE_TYPE, src_link_index, matches);

  PrintDebug("Matches=%d\n", num_matched_routes);

  for (i = 0; i < num_matched_routes; i++) {
    int route_index = -1;
    int link_index = -1;
    int dev_index = -1;

    route_index = matches[i];

#ifdef DEBUG
    {
  //    char *route_str;
      PrintDebug("Forward packet from link according to Route entry %d\n", route_index);
 //     route_to_str(route_index, &route_str);
   //   PrintDebug("Route Rule: %s\n", route_str.c_str());
    }
#endif

    if (g_routes[route_index].type == EDGE_TYPE) {
     
      link_index = g_routes[route_index].dest;
            
   //   PrintDebug("Destination Host: %s\n", ip_to_string(htonl(g_links[link_index].dest)));

      if(g_links[link_index].type == UDP_TYPE) {
   
	  int size;
    	  PrintDebug("Serializing UDP Packet from LINK, fd=%d\n", g_links[link_index].link_sock);
 
  	  if ((size = V3_SendTo_IP(g_links[link_index].link_sock,  g_links[link_index].dest,  g_links[link_index].remote_port, pkt->data, pkt->size)) != pkt->size)  {
    	    PrintError("Vnet: sending by UDP Exception, %x\n", size);
      	    return false;
         }

      }  else if (g_links[link_index].type == TCP_TYPE) {
 //   PrintDebug("Serializing TCP Packet from link to LINk %s\n", ip_to_string(htonl(g_links[link_index].dest)));
    		RawEthernetPacketSerialize(pkt, g_links[link_index].link_sock);
      }
    } else if (g_routes[route_index].type == INTERFACE_TYPE) {
      dev_index = g_routes[route_index].dest;
      
      PrintDebug("Writing Packet to device=%s\n", g_devices[dev_index].device_name);

      if (if_write_pkt(g_devices[dev_index].vnet_device, pkt) == -1) {
    	  PrintDebug("Can't write output packet to link\n");
	  return false;
      }
    }
  }
  return true;
}




#endif  //Route


static void hanlder_ProcessTcp(struct handler *hd)
{
  RawEthernetPacket *pt;
  
  pt = (RawEthernetPacket *)V3_Malloc(sizeof(RawEthernetPacket));
  if (pt == NULL){
  	PrintError("Vnet: Malloc fails\n");
	return;
  }

  if (RawEthernetPacketUnserialize(pt, hd->fd) == false){
  	PrintError("Vnet: Hanlder:ProcessTcp: receiving packet from TCP fails\n");
  	return;
  }

  PrintDebug("Vnet: In Hanlder.ProcessTcp: get packet\n");
  print_packet(pt->data, pt->size);
#ifdef ROUTE
/*  int  *link_index ;
  link_index = (int *)V3_Malloc(sizeof(int));
  if (link_index == NULL){
  	PrintError("Vnet: Malloc fails\n");
	return;
  }

  *link_index = find_link_by_fd(hd->fd);
*/
  int link_index;
//  int *link_index_ptr;
  link_index = find_link_by_fd(hd->fd);
//  link_index_ptr = &link_index;
//  v3_enqueue(src_link_index_q, (addr_t)link_index_ptr);
  pt->index = link_index;
#endif
  v3_enqueue(vnet_inpkt_q, (addr_t)pt);
}

static void RunUdp(struct handler *hd)
{
  RawEthernetPacket *pt;

  while (1) {
  	pt = (RawEthernetPacket *)V3_Malloc(sizeof(RawEthernetPacket));
  	if (pt == NULL){
  		PrintError("Vnet: Malloc fails\n");
		return;
  	}

	PrintDebug("Vnet: In Hanlder.RunUdp: socket: %d receive from ip %x, port %d\n", hd->fd, hd->remote_address, hd->remote_port);

  	pt->size = V3_RecvFrom_IP(hd->fd, hd->remote_address, hd->remote_port, pt->data, ETHERNET_PACKET_LEN);

  	if (pt->size <= 0){
  		PrintDebug("Vnet: Hanlder:ProcessUdp: receiving packet from UDP fails\n");
  		V3_Free(pt);
		return;
  	}

       PrintDebug("Vnet: In Hanlder.RunUdp: get packet\n");
  	print_packet(pt->data, pt->size);
  	

  	v3_enqueue(vnet_inpkt_q, (addr_t)pt);
	
  }
}


static void RunTcp(struct handler *hd)
{
      struct v3_sock_set fds;
      int rc;
  
      while (1) {
	 V3_SOCK_ZERO(&fds);
	 V3_SOCK_SET(hd->fd, &fds);
	 rc = V3_Select_Socket(&fds, NULL, NULL, NULL);

	 PrintDebug("Vnet: In Hanlder.Run: Return from Select(): rc : %d\n", rc);
		
        if (rc < 0) {
		PrintError("Vnet: In Hanlder.Run: Return from Select() Error: rc : %d\n", rc);
         	return;
        } else if (rc == 0) {
          // huh? didn't ask for timeouts so just repeat
            	continue;
        } else {
            	hanlder_ProcessTcp(hd);
        } 
      }
}

static int vnet_handle(struct handler *hd)
{
      if (use_tcp)
	  	RunTcp(hd);
      else
      		RunUdp(hd);
	  
      return 0;
}

static bool handler_tx_packet(struct handler *hd, RawEthernetPacket *pt)
{

  if (pt == NULL) {
    PrintError("VNET Hanlder: sending a NULL packet\n");
	
    return false;
  }

  PrintDebug("VNET Hanlder: socket: %d local %x:[%d], remote %x:[%d]\n", hd->fd, hd->local_address, hd->local_port, hd->remote_address, hd->remote_port);

  if (use_tcp)
  	return RawEthernetPacketSerialize(pt, hd->fd);
  else 
  	return RawEthernetPacketSendUdp(pt, hd->fd, hd->remote_address, hd->remote_port);
}


int V3_Send_pkt(uchar_t *buf, int length)
{
	PrintDebug("VNET: In V3_Send_pkt: pkt length %d\n", length);

	print_packet((char *)buf, length);

	return vnet_send_pkt((char *)buf, length);
}

int V3_Register_pkt_event(int (*netif_input)(uchar_t * pkt, uint_t size))
{
	return vnet_register_pkt_event("NE2000", netif_input, NULL);
}

int vnet_send_pkt(char *buf, int length)
{
	struct handler * hd;
	RawEthernetPacket pt;

	hd = get_handler();

	if (hd == NULL){
		return -1;
	}

	PrintDebug("Vnet: vnet_send_pkt: get handler: %p\n", hd);

	RawEthernetPacketInit(&pt, buf, length);  //====here we copy sending data once 

	if (!handler_tx_packet(hd, &pt))
		return -1;

       V3_Yield();

	return 0;
	
}

int vnet_register_pkt_event(char *dev_name, int (*netif_input)(uchar_t * pkt, uint_t size), void *data)
{
	struct vnet_device *dev;

	dev = (struct vnet_device *)V3_Malloc(sizeof(struct vnet_device));

	if(dev == NULL){
		PrintError("VNET: Malloc fails\n");
		return -1;
	}

	strncpy(dev->name, dev_name, (strlen(dev_name) < 50)?strlen(dev_name):50);

	dev->input = netif_input;
	dev->data = data;

	if (!add_device(dev))
		return -1;

	return 0;
}

int vnet_check()
{
//	struct vnet_device *(*dev)[]; -- YT
//	struct vnet_device *dev;
	RawEthernetPacket *pt;
	int link_index;
//	int i;

	//PrintDebug("VNET: In vnet_check\n");

	V3_Yield();
	
//	while (((pt = (RawEthernetPacket *)v3_dequeue(vnet_inpkt_q)) != NULL)&&((link_index = (int *)v3_dequeue(src_link_index_q)) != NULL)){
	while ((pt = (RawEthernetPacket *)v3_dequeue(vnet_inpkt_q)) != NULL){
#ifdef ROUTE
/*		dev = get_device(pt);
		//while (dev[device_num] != NULL) {	//multi-get_device -- YT
		if (dev == NULL){
			PrintDebug("VNET: In vnet_check: pkt length %d, no destination device, pkt discarded\n", (int)pt->size);
			V3_Free(pt);
			continue;
		}
		
		dev->input((uchar_t *)pt->data, pt->size);

		PrintDebug("VNET: In vnet_check: pkt length %d, device: %s\n", (int)pt->size, dev->name);

		for (i = 0; i <  (int)pt->size; i++)
			PrintDebug("%x ", pt->data[i]);
		PrintDebug("\n");
*/
		link_index = pt->index;
		if(HandleDataOverLink(pt, link_index)) {
			PrintDebug("VNET: vnet_check: Receive and Send a packet!\n");  //--YT
		}
#endif
//		V3_Free(link_index);
		V3_Free(pt); //be careful here
	}

	return 0;
}

static int handler_thread(void *arg)
{
    struct handler *hd = (struct handler *)arg;

    return vnet_handle(hd);
}

static int vnet_setup_handler(int con_fd, ctype local_config, int remote_addr, int remote_port)
{
     struct handler *h;

     h = (struct handler *)V3_Malloc(sizeof(struct handler));
	 
     if (local_config != LOCAL && local_config != REMOTE){ 
           PrintError("VNET: bad local config\n");
           return -1;
     }	
     h->local_config = local_config;
     
     h->local_address = bind_address;
     h->local_port = bind_port;

     h->remote_address = remote_addr;
     h->remote_port = remote_port;

 
     if (h->local_config == LOCAL) {
	     if ((h->fd = V3_Create_TCP_Socket() < 0)){
	           PrintError("VNET: can't create socket\n");
	           return -1;
	     }
	  
	     if (V3_Connect_To_IP(h->fd, h->remote_address, h->remote_port) < 0){ 
	           V3_Close_Socket(h->fd);
	           PrintError("VNET: can't connect to remote VNET server\n");
	           return -1;
	     }

	     PrintDebug("VNET: ConnectToHost done\n");
     } else if (local_config == REMOTE) {
     	     if (con_fd < 0) {
		PrintError("VNET: Invalid socket discriptor\n");
		return -1;
     	     }
	     h->fd = con_fd;
     } else {
     	     return -1;
     }

     add_handler(h);
		 
     // Now run protocol to bootstrap the remote VNET server
     PrintDebug("VNET: socket connection done socket: %d\n", h->fd);

     V3_CREATE_THREAD(&handler_thread, (void *)h, "VNET_HANDLER");

     return 0;
}

static int vnet_setup_tcp()
{
  uint_t remote_ip;
  uint_t remote_port;
  int accept_socket;
  int connection_socket;
  uint_t server_ip =  (0 | 172 << 24 | 23 << 16 | 1 );
  uint_t server_port = 22;
  	

  if (vnet_server) {
	if ((accept_socket = V3_Create_TCP_Socket() < 0)){
	      PrintError("VNET: Can't setup socket\n");
	      return -1;
	}

	if (bind_address == 0) {
	      if (V3_Bind_Socket(accept_socket, bind_port) < 0){ 
	          PrintError("VNET: Can't bind socket\n");
	          return -1;
	      }
	} else {
	      if (0) {//BindSocketwAdx(accept_socket, bind_address, bind_port) < 0){
	          PrintError("VNET: Can't bind socket\n");
	          return -1;
	      }
	}

	if (V3_Listen_Socket(accept_socket, 1) < 0) {
	      PrintError("VNET: Can't listen socket\n");
	      return -1;
	}
	  
	// create new connection for each handler remotely
	// this will stuck the initiation of vnet
	// TODO: put this in a separate kernel thread
	do{
	      PrintDebug("VNET: In Accepting Socket\n");
		
	      connection_socket = V3_Accept_Socket(accept_socket, &remote_ip, &remote_port);

	      PrintDebug("VNET: return from Accepting Socket %d\n", connection_socket);
	     
	      if (connection_socket < 0){
	             PrintError("VNET: Accept failed");
	             return -1;
	      }

	      //At this point our TCP connection is setup and running
	      vnet_setup_handler(connection_socket, REMOTE, 0, 0);
	}while (0);
  }else { //vnet_host
	PrintDebug("VNET: In host status\n");
  
 	//At this point our TCP connection is not setup
      vnet_setup_handler(-1, LOCAL, server_ip, server_port);
  }

  return 0;
}


static int vnet_setup_udp()
{
  uint_t client_ip = (0 | 172 << 24 | 23 << 16 | 2 );
  uint_t client_port = 22;
  int socket;
  uint_t server_ip =  (0 | 172 << 24 | 23 << 16 | 1 );
  uint_t server_port = 22;
  struct handler *hd;

  hd = (struct handler *)V3_Malloc(sizeof(struct handler));

  if ((socket = V3_Create_UDP_Socket()) < 0){
	      PrintError("VNET: Can't setup udp socket\n");
	      return -1; 
  }


  if (vnet_server) {// vnet_proxy
  	PrintDebug("VNET: In proxy status local ip: %x, %d\n", server_ip, server_port);
	
       if (V3_Bind_Socket(socket, server_port) < 0){ 
	          PrintError("VNET: Can't bind socket\n");
	          return -1;
	}

      hd->local_address = server_ip;
      hd->local_port = server_port;
      hd->remote_address = client_ip;
      hd->remote_port = client_port;
      hd->fd = socket;
      hd->remote_config = REMOTE;	
  }else { //vnet_host
	PrintDebug("VNET: In host status local ip: %x\n", client_ip);

	if (V3_Bind_Socket(socket, client_port) < 0){ 
	          PrintError("VNET: Can't bind socket\n");
	          return -1;
	}
	
      hd->local_address = client_ip;
      hd->local_port = client_port;
      hd->remote_address = server_ip;
      hd->remote_port = server_port;
      hd->fd = socket;
      hd->remote_config = LOCAL;
  }

  PrintDebug("VNET: vnet_setup_udp new handler: socket: %d local %x:[%d], remote %x:[%d]\n", hd->fd, hd->local_address, hd->local_port, hd->remote_address, hd->remote_port);

  add_handler(hd);
		 
  PrintDebug("VNET: add handler: %d\n", hd->fd);

  V3_CREATE_THREAD(&handler_thread, (void *)hd, "VNET_HANDLER");

  return 0;
}


void vnet_init()
{	
	int i=0;

	#ifdef VNET_SERVER
		vnet_server = 1;
	#endif

	extern struct v3_socket_hooks * sock_hooks;
	PrintDebug("In VMM_SOCK: %p\n", sock_hooks);

	PrintDebug("VNET Init: VNET_SERVER: %d\n", vnet_server);

	vnet_inpkt_q = v3_create_queue();
	v3_init_queue(vnet_inpkt_q);

	//queue for src_link_index -- YT
#ifdef ROUTE
//	src_link_index_q = v3_create_queue();
//	v3_init_queue(src_link_index_q);
#endif

	for (i = 0; i < NUM_DEVICES; i++)
		available_devices[i] = NULL;

	for (i = 0; i < NUM_HANDLERS; i++)
		active_handlers[i] = NULL;

	if (use_tcp)
		vnet_setup_tcp();
	else 
		vnet_setup_udp();

      //not continue on the guest
	//while(1){}
}

#if 0
	
static void test_send(int sock, int time)
{
    char *buf = "\001\002\003\004\005\006\007\008\009\010\011\012\013\014\015\n";
    int i, j;
    int num;

    i = time;
    while(i-- > 0){
	num = V3_Send_pkt((uchar_t *)buf, strlen(buf));

	PrintDebug("VNET: In test Send: sent size %d\n", num);
	
	for (j = 0; i < strlen(buf); j++)
	 	PrintDebug("%x ", buf[j]);
	PrintDebug("\n");
    }
}
  
 #endif  

