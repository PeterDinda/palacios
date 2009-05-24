#ifndef __VTL_MODEL_H
#define __VTL_MODEL_H


#include "vtl_util.h"

#define INVALID_PKT 0
#define OUTBOUND_PKT 1
#define INBOUND_PKT 2

typedef enum tcp_state { CLOSED,
			 LISTEN,
			 SYN_RCVD,
			 SYN_SENT,
			 ESTABLISHED,
			 CLOSE_WAIT,
			 LAST_ACK,
			 FIN_WAIT1,
			 FIN_WAIT2,
			 CLOSING,
			 TIME_WAIT } tcp_state_t;


/* State Models */
/*#define ETHERNET_MODEL 0
#define IP_MODEL 1
#define TCP_MODEL 2
#define UDP_MODEL 3
*/

typedef enum model_type{ETHERNET_MODEL, 
			IP_MODEL, 
			TCP_MODEL, 
			UDP_MODEL } model_type_t;

typedef struct ethernet_host_state {
  char addr[6];

} ethernet_host_state_t;

typedef struct ethernet_model {
  ethernet_host_state_t src;
  ethernet_host_state_t dst;
  unsigned short type;
} ethernet_model_t;

typedef struct ip_host_state {
  unsigned short ip_id;
  unsigned long addr;

  unsigned char ttl;

} ip_host_state_t;

typedef struct ip_model {
  ip_host_state_t src;
  ip_host_state_t dst;

  char version;
  unsigned char proto;
  ethernet_model_t ethernet;
} ip_model_t;

typedef struct tcp_host_state {
  unsigned short port;
  unsigned long seq_num;
  unsigned long last_ack;
  unsigned short win;

  unsigned long ts;
  unsigned short mss;

  tcp_state_t state;

} tcp_host_state_t;

typedef struct tcp_model {
  tcp_host_state_t src;
  tcp_host_state_t dst;

  ip_model_t ip;
} tcp_model_t;

typedef struct udp_host_state {
  unsigned short port;
} udp_host_state_t;

typedef struct udp_model {
  udp_host_state_t src;
  udp_host_state_t dst;

  ip_model_t ip;
} udp_model_t;


typedef struct vtl_model {
  union model_u {
    ethernet_model_t ethernet_model;
    ip_model_t ip_model;
    tcp_model_t tcp_model;
    udp_model_t udp_model;
  } model;
  model_type_t type;
} vtl_model_t;



udp_model_t * new_udp_model();
tcp_model_t * new_tcp_model();
ip_model_t * new_ip_model();
ethernet_model_t * new_ethernet_model();
vtl_model_t * new_vtl_model(model_type_t type);

int initialize_ip_model(ip_model_t * model, RawEthernetPacket * pkt, int dir = OUTBOUND_PKT);
int initialize_tcp_model(tcp_model_t * model, RawEthernetPacket * pkt, int dir = OUTBOUND_PKT);
int initialize_udp_model(udp_model_t * model, RawEthernetPacket * pkt, int dir = OUTBOUND_PKT);
int initialize_model(vtl_model_t * model, RawEthernetPacket * pkt, int dir = OUTBOUND_PKT);

int sync_ip_model(ip_model_t * model, RawEthernetPacket * pkt);
int sync_tcp_model(tcp_model_t * model, RawEthernetPacket * pkt);
int sync_udp_model(udp_model_t * model, RawEthernetPacket * pkt);
int sync_model(vtl_model_t * model, RawEthernetPacket * pkt);

int is_udp_model_pkt(udp_model_t * model, RawEthernetPacket * pkt);
int is_tcp_model_pkt(tcp_model_t * model, RawEthernetPacket * pkt);
int is_ip_model_pkt(ip_model_t * model, RawEthernetPacket * pkt);
int is_ethernet_model_pkt(ethernet_model_t * model, RawEthernetPacket * pkt);
int is_model_pkt(vtl_model_t * model, RawEthernetPacket * pkt);



int create_empty_ethernet_pkt(ethernet_model_t * model, RawEthernetPacket * pkt, int dir = OUTBOUND_PKT);
int create_empty_ip_pkt(ip_model_t * model, RawEthernetPacket * pkt, int dir = OUTBOUND_PKT);
int create_empty_tcp_pkt(tcp_model_t * model, RawEthernetPacket * pkt, int dir = OUTBOUND_PKT);
int create_empty_udp_pkt(udp_model_t * model, RawEthernetPacket * pkt, int dir = OUTBOUND_PKT);
int create_empty_pkt(vtl_model_t * model, RawEthernetPacket * pkt, int dir = OUTBOUND_PKT);

// User must track tcp state changes
//tcp_state_t get_tcp_state(tcp_state_t current_state, RawEthernetPacket * pkt, int dir = OUTBOUND_PKT);


void dbg_dump_eth_model(ethernet_model_t * model);
void dbg_dump_ip_model(ip_model_t * model);
void dbg_dump_tcp_model(tcp_model_t * model);
void dbg_dump_udp_model(udp_model_t * model);
void dbg_dump_model(vtl_model_t * model);






#endif // ! __VTL_MODEL_H
