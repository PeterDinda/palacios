#ifndef __IF_H
#define __IF_H 1

#include "util.h"
#include "debug.h"
#include "raw_ethernet_packet.h"



#ifdef linux
#include <libnet.h>
#define LIBNET_ERRORBUF_SIZE 256
#elif defined(WIN32)
#define WPCAP
#endif

#include <pcap.h>

#ifdef WIN32
#include <Packet32.h>
#endif


typedef struct iface {
  string *name;

  pcap_t * pcap_interface;

  char mode;

#ifdef linux
  int pcap_fd;

  libnet_t * net_interface;
#elif defined(WIN32)
  HANDLE pcap_event;
#endif

} iface_t;

#define IF_PACKET 1
#define IF_BREAK 2
#define IF_CONT 3


#define IF_RD 0x1
#define IF_WR 0x2
#define IF_RW 0x3


iface_t * if_connect(string if_name, char mode = IF_RW);
int if_setup_filter(iface_t * iface, string bpf_str);

#ifdef linux
int if_get_fd(iface_t * iface);
#elif WIN32
HANDLE if_get_event(iface_t * iface);
#endif

int if_loop(iface_t * iface, RawEthernetPacket * pkt);
void pkt_handler(u_char * pkt, const struct pcap_pkthdr * pkt_header, const u_char * pkt_data);

int if_write_pkt(iface_t * iface, RawEthernetPacket * pkt);
int if_read_pkt(iface_t * iface, RawEthernetPacket * pkt);


#endif // !__IF_H
