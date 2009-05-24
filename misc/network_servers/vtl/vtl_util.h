#ifndef __VTL_UTIL_H
#define __VTL_UTIL_H 1


#include <stdlib.h>
#include <stdio.h>

#ifdef linux
#include <sys/socket.h>
#include <sys/types.h>
#elif WIN32

#endif 

#include "raw_ethernet_packet.h"
#include "debug.h"
#include "socks.h"



typedef struct ip_hdr {
  unsigned char hdr_len:4, version:4;
  unsigned char tos;
  unsigned short total_len;
  unsigned short id;
  unsigned char flags:3;
  unsigned short offset:13;
  unsigned char ttl;
  unsigned char proto;
  unsigned short cksum;
  unsigned int src_addr;
  unsigned int dst_addr;
} ip_hdr_t;


struct tcpheader {
 unsigned short int th_sport;
 unsigned short int th_dport;
 unsigned int th_seq;
 unsigned int th_ack;
 unsigned char th_x2:4, th_off:4;
 unsigned char th_flags;
 unsigned short int th_win;
 unsigned short int th_sum;
 unsigned short int th_urp;
};

struct udpheader {
 unsigned short int uh_sport;
 unsigned short int uh_dport;
 unsigned short int uh_len;
 unsigned short int uh_check;
}; 

struct icmpheader {
 unsigned char icmp_type;
 unsigned char icmp_code;
 unsigned short int icmp_cksum;
 /* The following data structures are ICMP type specific */
 unsigned short int icmp_id;
 unsigned short int icmp_seq;
};

/* ETHERNET MACROS */
#define ETH_HDR_LEN 14
#define MAC_BCAST {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
#define ETH_IP 0x0008
#define ETH_ARP 0x0608
#define ETH_RARP 0x3508

#define MAC_LEN 6

#define ETH_DST_OFFSET 0
#define ETH_SRC_OFFSET 6
#define ETH_TYPE_OFFSET 12
#define ETH_DATA_OFFSET 14

#define ETH_DST(pkt) (pkt + ETH_DST_OFFSET)
#define ETH_SRC(pkt) (pkt + ETH_SRC_OFFSET)
#define ETH_TYPE(pkt) (pkt + ETH_TYPE_OFFSET)
#define ETH_DATA(pkt) (pkt + ETH_DATA_OFFSET)

#define SET_ETH_DST(pkt, dst) memcpy(ETH_DST(pkt), dst, 6)
#define SET_ETH_SRC(pkt, src) memcpy(ETH_SRC(pkt), src, 6)
#define SET_ETH_TYPE(pkt, type) (*(unsigned short *)(ETH_TYPE(pkt)) = type)
#define GET_ETH_TYPE(pkt) (*(unsigned short *)ETH_TYPE(pkt))
#define GET_ETH_DST(pkt, dst) memcpy(dst, ETH_DST(pkt), 6)
#define GET_ETH_SRC(pkt, src) memcpy(src, ETH_SRC(pkt), 6)

/* ARP MACROS */

#define ARP_HW_TYPE_OFFSET 0
#define ARP_PROTO_TYPE_OFFSET 2
#define ARP_HW_LEN_OFFSET 4
#define ARP_PROTO_LEN_OFFSET 5
#define ARP_OP_OFFSET 6
#define ARP_ADDR_OFFSET 8

#define ARP_REQUEST 0x0001
#define ARP_REPLY 0x0002
#define RARP_REQUEST 0x0003
#define RARP_REPLY 0x0004

#define ARP_HDR(pkt) (pkt + ETH_HDR_LEN)

#define ARP_HW_TYPE(pkt) (ARP_HDR(pkt) + ARP_HW_TYPE_OFFSET)
#define ARP_PROTO_TYPE(pkt) (ARP_HDR(pkt) + ARP_PROTO_TYPE_OFFSET)
#define ARP_HW_LEN(pkt) (ARP_HDR(pkt) + ARP_HW_LEN_OFFSET)
#define ARP_PROTO_LEN(pkt) (ARP_HDR(pkt) + ARP_PROTO_LEN_OFFSET)
#define ARP_OP(pkt) (ARP_HDR(pkt) + ARP_OP_OFFSET)
#define ARP_SRC_HW(pkt) (ARP_HDR(pkt) + ARP_ADDR_OFFSET)
#define ARP_SRC_PROTO(pkt) (ARP_HDR(pkt) + ARP_ADDR_OFFSET + GET_ARP_HW_LEN(pkt))
#define ARP_DST_HW(pkt) (ARP_HDR(pkt) + ARP_ADDR_OFFSET + GET_ARP_HW_LEN(pkt) + GET_ARP_PROTO_LEN(pkt))
#define ARP_DST_PROTO(pkt) (ARP_HDR(pkt) +  ARP_ADDR_OFFSET + (GET_ARP_HW_LEN(pkt) * 2) + GET_ARP_PROTO_LEN(pkt)) 

#define ARP_DST_MAC(pkt) ARP_DST_HW(pkt)
#define ARP_DST_IP(pkt) ARP_DST_PROTO(pkt)
#define ARP_SRC_MAC(pkt) ARP_SRC_HW(pkt)
#define ARP_SRC_IP(pkt) ARP_SRC_PROTO(pkt)

#define GET_ARP_HW_TYPE(pkt) ntohs(*(unsigned short *)ARP_HW_TYPE(pkt))
#define GET_ARP_PROTO_TYPE(pkt) ntohs(*(unsigned short *)ARP_PROTO_TYPE(pkt))
#define GET_ARP_HW_LEN(pkt) (*(char *)ARP_HW_LEN(pkt))
#define GET_ARP_PROTO_LEN(pkt) (*(char *)ARP_PROTO_LEN(pkt))
#define GET_ARP_OP(pkt) ntohs(*(unsigned short *)ARP_OP(pkt))
#define GET_ARP_SRC_HW(pkt, src) memcpy(src, ARP_SRC_HW(pkt), GET_ARP_HW_LEN(pkt))
#define GET_ARP_SRC_PROTO(pkt, src) memcpy(src, ARP_SRC_PROTO(pkt), GET_ARP_PROTO_LEN(pkt))
#define GET_ARP_DST_HW(pkt, dst) memcpy(dst, ARP_DST_HW(pkt), GET_ARP_HW_LEN(pkt))
#define GET_ARP_DST_PROTO(pkt, dst) memcpy(dst, ARP_DST_PROTO(pkt), GET_ARP_PROTO_LEN(pkt))

#define GET_ARP_SRC_MAC(pkt, src) GET_ARP_SRC_HW(pkt, src)
#define GET_ARP_DST_MAC(pkt, dst) GET_ARP_DST_HW(pkt, dst)
#define GET_ARP_SRC_IP(pkt) ntohl(*(unsigned long *)ARP_SRC_IP(pkt))
#define GET_ARP_DST_IP(pkt) ntohl(*(unsigned long *)ARP_DST_IP(pkt))

#define SET_ARP_HW_TYPE(pkt, type) (*(unsigned short *)ARP_HW_TYPE(pkt) = htons(type))
#define SET_ARP_PROTO_TYPE(pkt, type) (*(unsigned short *)ARP_PROTO_TYPE(pkt) = htons(type))
#define SET_ARP_HW_LEN(pkt, len) (*(char *)ARP_HW_LEN(pkt) = len)
#define SET_ARP_PROTO_LEN(pkt, len) (*(char *)ARP_PROTO_LEN(pkt) = len)
#define SET_ARP_OP(pkt, op)  (*(unsigned short *)ARP_OP(pkt) = htons(op))
#define SET_ARP_SRC_HW(pkt, src) memcpy(ARP_SRC_HW(pkt), src, GET_ARP_HW_LEN(pkt))
#define SET_ARP_SRC_PROTO(pkt, src) memcpy(ARP_SRC_PROTO(pkt), src, GET_ARP_PROTO_LEN(pkt))
#define SET_ARP_DST_HW(pkt, dst) memcpy(ARP_DST_HW(pkt), dst, GET_ARP_HW_LEN(pkt))
#define SET_ARP_DST_PROTO(pkt, dst) memcpy(ARP_DST_PROTO(pkt), dst, GET_ARP_PROTO_LEN(pkt))

#define SET_ARP_SRC_MAC(pkt, src) SET_ARP_SRC_HW(pkt, src)
#define SET_ARP_DST_MAC(pkt, dst) SET_ARP_DST_HW(pkt, dst)
#define SET_ARP_SRC_IP(pkt, src) (*(unsigned long *)ARP_SRC_IP(pkt) = htonl(src))
#define SET_ARP_DST_IP(pkt, dst) (*(unsigned long *)ARP_DST_IP(pkt) = htonl(dst))


/* IP MACROS */
#define IP_SVC_TYPE_OFFSET 1
#define IP_TOTAL_LEN_OFFSET 2
#define IP_ID_OFFSET 4
#define IP_FLAGS_OFFSET 6
#define IP_FRAG_OFFSET 6
#define IP_TTL_OFFSET 8
#define IP_PROTO_OFFSET 9
#define IP_CKSUM_OFFSET 10
#define IP_SRC_OFFSET 12
#define IP_DST_OFFSET 16


// These can be fleshed out: 
// http://www.iana.org/assignments/protocol-numbers
#define IP_ICMP 0x01
#define IP_TCP 0x06
#define IP_UDP 0x11


#define IP_HDR(pkt) (pkt + ETH_HDR_LEN)

#define IP_HDR_LEN(pkt) (IP_HDR(pkt));
#define IP_SVC_TYPE(pkt) (IP_HDR(pkt) + IP_SVC_TYPE_OFFSET)
#define IP_DSCP(pkt) IP_TOS(pkt)
#define IP_TOTAL_LEN(pkt) (IP_HDR(pkt) + IP_TOTAL_LEN_OFFSET)
#define IP_ID(pkt) (IP_HDR(pkt) + IP_ID_OFFSET)
#define IP_FLAGS(pkt) (IP_HDR(pkt) + IP_FLAGS_OFFSET)
#define IP_FRAG(pkt) (IP_HDR(pkt) + IP_FRAG_OFFSET)
#define IP_TTOL(pkt) (IP_HDR(pkt) + IP_TTL_OFFSET)
#define IP_PROTO(pkt) (IP_HDR(pkt) + IP_PROTO_OFFSET)
#define IP_CKSUM(pkt) (IP_HDR(pkt) + IP_CKSUM_OFFSET)
#define IP_SRC(pkt) (IP_HDR(pkt) + IP_SRC_OFFSET)
#define IP_DST(pkt) (IP_HDR(pkt) + IP_DST_OFFSET)
#define IP_DATA(pkt) (IP_HDR(pkt) + GET_IP_HDR_LEN(pkt))


#define GET_IP_VERSION(pkt) (((*(char *)IP_HDR(pkt)) & 0xf0) >> 4)
#define GET_IP_HDR_LEN(pkt) ((*((char *)IP_HDR(pkt)) & 0x0f) << 2)
#define GET_IP_SVC_TYPE(pkt) (*(char *)IP_SVC_TYPE(pkt))
#define GET_IP_DSCP(pkt) (*(char *)IP_DSCP(pkt))
#define GET_IP_TOTAL_LEN(pkt) ntohs(*(unsigned short *)IP_TOTAL_LEN(pkt))
#define GET_IP_ID(pkt) ntohs(*(unsigned short *)IP_ID(pkt))
#define GET_IP_FLAGS(pkt) (*(char *)IP_FLAGS(pkt) & 0xe0)
#define GET_IP_FRAG(pkt) ntohs(*(unsigned short *)IP_FRAG(pkt) & htons(0x1fff))
#define GET_IP_TTL(pkt) (*(char *)IP_TTOL(pkt))
#define GET_IP_PROTO(pkt) (*(char *)IP_PROTO(pkt))
#define GET_IP_CKSUM(pkt) (*(unsigned short *)IP_CKSUM(pkt))
#define GET_IP_SRC(pkt) ntohl(*(unsigned long *)IP_SRC(pkt))
#define GET_IP_DST(pkt) ntohl(*(unsigned long *)IP_DST(pkt))



void inline SET_IP_VERSION(char * pkt, char version) {
  *(char *)IP_HDR(pkt) &= 0x0f;
  *(char *)IP_HDR(pkt) |= ((version & 0x0f) << 4);
}

void inline SET_IP_HDR_LEN(char * pkt, char len) {
  *(char *)IP_HDR(pkt) &= 0xf0;
  *(char *)IP_HDR(pkt) |= ((len >> 2) & 0x0f);
}

#define SET_IP_SVC_TYPE(pkt, tos) (*(char *)IP_SVC_TYPE(pkt) = tos)
#define SET_IP_DSCP(pkt, dscp) (*(char *)IP_DSCP(pkt) = dscp)
#define SET_IP_TOTAL_LEN(pkt, len) (*(unsigned short *)IP_TOTAL_LEN(pkt) = htons(len))
#define SET_IP_ID(pkt, id) (*(unsigned short *)IP_ID(pkt) = htons(id))

void inline SET_IP_FLAGS(char * pkt, char flags) {
  *(char *)IP_FLAGS(pkt) &= 0x1f;
  *(char *)IP_FLAGS(pkt) |= (flags & 0xe0);
}

void inline SET_IP_FRAG(char * pkt, unsigned short frag) {
  *(unsigned short *)IP_FRAG(pkt) &= htons(0xe000);
  *(unsigned short *)IP_FRAG(pkt) |= htons(frag & 0x1fff);
}

#define SET_IP_TTL(pkt, ttl) (*(char *)IP_TTOL(pkt) = ttl)
#define SET_IP_PROTO(pkt, proto) (*(char *)IP_PROTO(pkt) = proto)
#define SET_IP_CKSUM(pkt, cksum) (*(unsigned short *)IP_CKSUM(pkt) = cksum)
#define SET_IP_SRC(pkt, src) (*(unsigned long *)IP_SRC(pkt) = htonl(src))
#define SET_IP_DST(pkt, dst) (*(unsigned long *)IP_DST(pkt) = htonl(dst))

unsigned short compute_ip_checksum(RawEthernetPacket * pkt);

/* TCP MACROS */
#define TCP_SRC_PORT_OFFSET 0
#define TCP_DST_PORT_OFFSET 2
#define TCP_SEQ_NUM_OFFSET 4
#define TCP_ACK_NUM_OFFSET 8
#define TCP_HDR_LEN_OFFSET 12
#define TCP_RSVD_OFFSET 12
#define TCP_FLAGS_OFFSET 13
#define TCP_WIN_OFFSET 14
#define TCP_CKSUM_OFFSET 16
#define TCP_URG_PTR_OFFSET 18
#define TCP_OPTS_OFFSET 20


#define TCP_HDR(pkt) (pkt + ETH_HDR_LEN + GET_IP_HDR_LEN(pkt))
#define TCP_SRC_PORT(pkt) (TCP_HDR(pkt) + TCP_SRC_PORT_OFFSET)
#define TCP_DST_PORT(pkt) (TCP_HDR(pkt) + TCP_DST_PORT_OFFSET)
#define TCP_SEQ_NUM(pkt) (TCP_HDR(pkt) + TCP_SEQ_NUM_OFFSET)
#define TCP_ACK_NUM(pkt) (TCP_HDR(pkt) + TCP_ACK_NUM_OFFSET)
#define TCP_HDR_LEN(pkt) (TCP_HDR(pkt) + TCP_HDR_LEN_OFFSET)
#define TCP_RSVD(pkt) (TCP_HDR(pkt) + TCP_RSVD_OFFSET)
#define TCP_FLAGS(pkt) (TCP_HDR(pkt) + TCP_FLAGS_OFFSET)
#define TCP_WIN(pkt) (TCP_HDR(pkt) + TCP_WIN_OFFSET)
#define TCP_CKSUM(pkt) (TCP_HDR(pkt) + TCP_CKSUM_OFFSET)
#define TCP_URG_PTR(pkt) (TCP_HDR(pkt) + TCP_URG_PTR_OFFSET)
#define TCP_OPTS(pkt) (TCP_HDR(pkt) + TCP_OPTS_OFFSET)
#define TCP_DATA(pkt) (TCP_HDR(pkt) + GET_TCP_HDR_LEN(pkt))

#define GET_TCP_SRC_PORT(pkt) ntohs(*(unsigned short *)TCP_SRC_PORT(pkt))
#define GET_TCP_DST_PORT(pkt) ntohs(*(unsigned short *)TCP_DST_PORT(pkt))
#define GET_TCP_SEQ_NUM(pkt) ntohl(*(unsigned long *)TCP_SEQ_NUM(pkt))
#define GET_TCP_ACK_NUM(pkt) ntohl(*(unsigned long *)TCP_ACK_NUM(pkt))
#define GET_TCP_HDR_LEN(pkt) (((*(char *)TCP_HDR_LEN(pkt)) & 0xf0) >> 2)
#define GET_TCP_RSVD(pkt) (((*(unsigned short *)TCP_RSVD(pkt)) & htons(0x0fc0)) >> 6)
#define GET_TCP_FLAGS(pkt) ((*(char *)TCP_FLAGS(pkt)) & 0x3f)
#define GET_TCP_EFLAGS(pkt) (*(char *)TCP_FLAGS(pkt))
#define GET_TCP_WIN(pkt) ntohs(*(unsigned short *)TCP_WIN(pkt))
#define GET_TCP_CKSUM(pkt) (*(unsigned short *)TCP_CKSUM(pkt))
#define GET_TCP_URG_PTR(pkt) ntohs(*(unsigned short *)TCP_URG_PTR(pkt))

#define GET_TCP_DATA_LEN(pkt) (GET_IP_TOTAL_LEN(pkt) - (GET_IP_HDR_LEN(pkt) + GET_TCP_HDR_LEN(pkt)))
#define GET_TCP_TOTAL_LEN(pkt) (GET_IP_TOTAL_LEN(pkt) - GET_IP_HDR_LEN(pkt))
#define GET_TCP_OPTS_LEN(pkt) (GET_TCP_HDR_LEN(pkt) - 20)

#define SET_TCP_SRC_PORT(pkt, port) ((*(unsigned short *)TCP_SRC_PORT(pkt)) = htons(port))
#define SET_TCP_DST_PORT(pkt, port) ((*(unsigned short *)TCP_DST_PORT(pkt)) = htons(port))
#define SET_TCP_SEQ_NUM(pkt, num) ((*(unsigned long *)TCP_SEQ_NUM(pkt)) = htonl(num))
#define SET_TCP_ACK_NUM(pkt, num) ((*(unsigned long *)TCP_ACK_NUM(pkt)) = htonl(num))

void inline SET_TCP_HDR_LEN(char * pkt, char len) {
  *(char *)TCP_HDR_LEN(pkt) &= 0x0f;
  *(char *)TCP_HDR_LEN(pkt) |= ((len << 2) & 0xf0);
}

void inline SET_TCP_RSVD(char * pkt, char rsvd) {
  *(unsigned short *)TCP_RSVD(pkt) &= htons(0xf03f);
  *(char *)TCP_RSVD(pkt) |= ((rsvd >> 2) & 0x0f);
  *(char *)(TCP_RSVD(pkt) + 1) |= ((rsvd << 6) & 0xc0);
}

void inline SET_TCP_FLAGS(char * pkt, char flags) {
  *(char *)TCP_FLAGS(pkt) &= 0xc0;
  *(char *)TCP_FLAGS(pkt) |= (flags & 0x3f);
}
#define SET_TCP_EFLAGS(pkt, eflags) (*(char *)TCP_FLAGS(pkt) = eflags)
#define SET_TCP_WIN(pkt, win) (*(unsigned short *)TCP_WIN(pkt) = htons(win))
#define SET_TCP_CKSUM(pkt, cksum) (*(unsigned short *)TCP_CKSUM(pkt) = cksum)
#define SET_TCP_URG_PTR(pkt, urg) (*(unsigned short *)TCP_URG_PTR(pkt) = htons(urg))


#define TCP_OPTS_MSS 0x0001
#define TCP_OPTS_TS 0x0002
#define TCP_OPTS_SACK_OK 0x0004

typedef struct tcp_opts {
  unsigned short mss ;
  char window;
  char sack_ok;
  unsigned long * sack_entries;
  unsigned long local_ts;
  unsigned long remote_ts;
} tcp_opts_t;

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20
#define TCP_ECN 0x40
#define TCP_CWR 0x80

#define SET_TCP_FIN_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) |= TCP_FIN)
#define SET_TCP_SYN_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) |= TCP_SYN)
#define SET_TCP_RST_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) |= TCP_RST)
#define SET_TCP_PSH_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) |= TCP_PSH)
#define SET_TCP_ACK_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) |= TCP_ACK)
#define SET_TCP_URG_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) |= TCP_URG)
#define SET_TCP_ECN_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) |= TCP_ECN)
#define SET_TCP_CWR_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) |= TCP_CWR)



#define UNSET_TCP_FIN_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) &= ~TCP_FIN)
#define UNSET_TCP_SYN_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) &= ~TCP_SYN)
#define UNSET_TCP_RST_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) &= ~TCP_RST)
#define UNSET_TCP_PSH_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) &= ~TCP_PSH)
#define UNSET_TCP_ACK_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) &= ~TCP_ACK)
#define UNSET_TCP_URG_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) &= ~TCP_URG)
#define UNSET_TCP_ECN_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) &= ~TCP_ECN)
#define UNSET_TCP_CWR_FLAG(pkt) (*(char *)TCP_FLAGS(pkt) &= ~TCP_CWR)

int is_syn_pkt(RawEthernetPacket * pkt);
int is_ack_pkt(RawEthernetPacket * pkt);
int is_fin_pkt(RawEthernetPacket * pkt);


unsigned long compute_next_tcp_seq_num(RawEthernetPacket * pkt);
unsigned short compute_tcp_checksum(RawEthernetPacket * pkt);

/* UDP MACROS */
#define UDP_SRC_PORT_OFFSET 0
#define UDP_DST_PORT_OFFSET 2
#define UDP_LEN_OFFSET 4
#define UDP_CKSUM_OFFSET 6
#define UDP_DATA_OFFSET 8

#define UDP_HDR(pkt) (pkt + ETH_HDR_LEN + GET_IP_HDR_LEN(pkt))
#define UDP_SRC_PORT(pkt) (UDP_HDR(pkt) + UDP_SRC_PORT_OFFSET)
#define UDP_DST_PORT(pkt) (UDP_HDR(pkt) + UDP_DST_PORT_OFFSET)
#define UDP_LEN(pkt) (UDP_HDR(pkt) + UDP_LEN_OFFSET)
#define UDP_CKSUM(pkt) (UDP_HDR(pkt) + UDP_CKSUM_OFFSET)
#define UDP_DATA(pkt) (UDP_HDR(pkt) + UDP_DATA_OFFSET)

#define GET_UDP_SRC_PORT(pkt) ntohs(*(unsigned short *)UDP_SRC_PORT(pkt))
#define GET_UDP_DST_PORT(pkt) ntohs(*(unsigned short *)UDP_DST_PORT(pkt))
#define GET_UDP_LEN(pkt) ntohs(*(unsigned short *)UDP_LEN(pkt))
#define GET_UDP_CKSUM(pkt) ntohs(*(unsigned short *)UDP_CKSUM(pkt))


#define SET_UDP_SRC_PORT(pkt, src) (*(unsigned short *)UDP_SRC_PORT(pkt) = htons(src))
#define SET_UDP_DST_PORT(pkt, dst) (*(unsigned short *)UDP_DST_PORT(pkt) = htons(dst))
#define SET_UDP_LEN(pkt, len) (*(unsigned short *)UDP_LEN(pkt) = htons(len))
#define SET_UDP_CKSUM(pkt, cksum) (*(unsigned short *)UDP_CKSUM(pkt) = cksum)

unsigned short compute_udp_checksum(RawEthernetPacket * pkt);

/* DNS MACROS */

#define DNS_PORT 53


void dbg_print_pkt_info(RawEthernetPacket * pkt);
void dbg_print_pkt(RawEthernetPacket * pkt);
void dbg_print_buf(unsigned char * buf, unsigned int len);



/* Packet Field Utility Functions */
int is_tcp_pkt(RawEthernetPacket * pkt);
int is_arp_pkt(RawEthernetPacket * pkt);
int is_udp_pkt(RawEthernetPacket * pkt);
int is_ip_pkt(RawEthernetPacket * pkt);

/* UDP Packet queries */
int is_dns_pkt(RawEthernetPacket * pkt);

/* TCP Packet queries */

int parse_tcp_options(tcp_opts_t * options, RawEthernetPacket * pkt);
int set_tcp_options(tcp_opts_t * options, unsigned long opt_flags, RawEthernetPacket * pkt);

int compute_pkt_size(RawEthernetPacket * pkt);

/* ARP Packet queries */
int is_arp_bcast_pkt(RawEthernetPacket * pkt);

/* ARP functions */





void swap_eth_addrs(RawEthernetPacket * pkt);
void swap_ip_addrs(RawEthernetPacket * pkt);
void swap_ports(RawEthernetPacket * pkt);

int pkt_has_timestamp(RawEthernetPacket * pkt);

char * get_eth_protocol(unsigned short protocol);
char * get_ip_protocol(unsigned char protocol);

unsigned long get_tcp_timestamp(char *opts, int len);
unsigned short OnesComplementSum(unsigned short *buf, int len);
unsigned short get_tcp_checksum(RawEthernetPacket * pkt, unsigned short tcp_len);
unsigned short get_ip_checksum(RawEthernetPacket * pkt);
unsigned short get_udp_checksum(RawEthernetPacket * pkt, unsigned short udp_len);

int get_mss(RawEthernetPacket * pkt);


void set_tcp_timestamp(char * ts_opt, unsigned long local_ts, unsigned long remote_ts);




#endif 
