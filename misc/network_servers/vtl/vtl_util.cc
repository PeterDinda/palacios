#include "vtl_util.h"
#include <assert.h>


void dbg_print_pkt_info(RawEthernetPacket * pkt) {
  unsigned short src_port;
  unsigned short dest_port;
  string dest_str;
  string src_str;

  unsigned long seq_num = GET_TCP_SEQ_NUM(pkt->data);
  unsigned long ack_num = GET_TCP_ACK_NUM(pkt->data);

  src_port = GET_TCP_SRC_PORT(pkt->data);
  dest_port = GET_TCP_DST_PORT(pkt->data);

  dest_str = ip_to_string(GET_IP_DST(pkt->data));
  src_str = ip_to_string(GET_IP_SRC(pkt->data));

  vtl_debug("Packet: %s:%d-%s:%d seq: %lu, ack: %lu\n", src_str.c_str(), ntohs(src_port), dest_str.c_str(), ntohs(dest_port), 
	 seq_num, ack_num);
	 
  return;

}

void dbg_print_pkt(RawEthernetPacket * pkt) {
  unsigned int x; 
  int i;
  char pkt_line[128];
  unsigned int pkt_size = pkt->get_size() - 1;

  vtl_debug("Packet Dump: (pkt_size=%lu) \n", pkt->get_size());

  for (x = 0; x < pkt_size;) {
    sprintf(pkt_line, "\t%.4x:  ", x);

    for (i = 0; i < 16; i += 2) {
      if (pkt_size < x + i) {
	break;
      } 

      if (pkt_size == x + i) {
	sprintf(pkt_line, "%s%.2x ", pkt_line, *(unsigned char *)(pkt->data + i + x));
      } else {

	sprintf(pkt_line, "%s%.4x  ", pkt_line, ntohs(*(unsigned short *)(pkt->data + i + x)));
      }
    }

    vtl_debug("%s\n", pkt_line);

    x += 16;
  }
}

void dbg_print_buf(unsigned char * buf, unsigned int len) {
  unsigned int x; 
  int i;
  char pkt_line[128];

  vtl_debug("Buf Dump: (len=%d) \n", len);

  for (x = 0; x < len-1;) {
    sprintf(pkt_line, "\t%.4x:  ", x);

    for (i = 0; i < 16; i += 2) {
      if ((len - 1) < x + i) {
	break;
      } 

      if (len == x + i + 1) {
	sprintf(pkt_line, "%s%.2x ", pkt_line, *(unsigned char *)(buf + i + x));
      } else {

	sprintf(pkt_line, "%s%.4x  ", pkt_line, ntohs(*(unsigned short *)(buf + i + x)));
      }
    }

    vtl_debug("%s\n", pkt_line);

    x += 16;
  }

}
/*
  void do_binary_to_ipaddress(unsigned char* ip,IPADDRESS& ipaddress)
  {
  ipaddress.a1=ip[0];
  ipaddress.a2=ip[1];
  ipaddress.a3=ip[2];
  ipaddress.a4=ip[3];
  }
  
  void do_ipaddress_to_string(IPADDRESS ipaddress,char* buffer)
  {
  sprintf(buffer,"%d.%d.%d.%d",ipaddress.a1,ipaddress.a2,ipaddress.a3,ipaddress.a4);
  }
  
  void do_binary_to_string(unsigned char* ip,char* buffer)
  {
  IPADDRESS ipaddress;
  do_binary_to_ipaddress(ip,ipaddress);
  do_ipaddress_to_string(ipaddress,buffer);
  }
*/

int get_mss(RawEthernetPacket * pkt) {
  unsigned long ip_hdr_len = GET_IP_HDR_LEN(pkt->data);
  unsigned short tcp_hdr_len = (*(pkt->data + ETH_HDR_LEN + ip_hdr_len + 12) & 0xf0) >> 2;
  int offset = 0;
  int len = tcp_hdr_len - 20;
  unsigned short mss;

  char * opts = (pkt->data + ETH_HDR_LEN + ip_hdr_len + 20);

  if (len <= 0) {
    return -1;
  }

  while (offset < len) {
    if (*(opts + offset) == 0x00) {
      break;
    } else if (*(opts + offset) == 0x01) {
      offset++;
    } else if (*(opts + offset) == 0x02) {
      mss = (*(unsigned short *)(opts + offset + 2));
      offset += *(opts + offset + 1);
      return (int)ntohs(mss);
    } else {
      offset += *(opts + offset + 1);
    }
  }
  return -1;
}

int parse_tcp_options(tcp_opts_t * options, RawEthernetPacket * pkt) {
  assert((options != NULL) && (pkt != NULL));
  
  memset(options, 0, sizeof(options));

  int offset = 0;
  char * opts = TCP_OPTS(pkt->data);
  int opt_len = GET_TCP_OPTS_LEN(pkt->data);
  int field_len = 0;
  if (opt_len == 0) {
    // no options
    return -1;
  }


  while (offset < opt_len) {
    if (*(opts + offset) == 0x00) {
      break;
    } else if (*(opts + offset) == 0x01) {
      offset++;
    } else if (*(opts + offset) == 0x02) {
      options->mss = ntohs(*(unsigned short *)(opts + offset + 2));
      offset += *(opts + offset + 1);
    } else if (*(opts + offset) == 0x03) {
      options->window = *(unsigned char *)(opts + offset + 2);
      offset += *(opts + offset + 1);
    } else if (*(opts + offset) == 0x04) {
      // SACK OK
      options->sack_ok = 1;
      offset += 2;
    } else if (*(opts + offset) == 0x05) {
      field_len = *(opts + offset + 1);
      options->sack_entries = (unsigned long *)malloc(field_len - 2);
      memcpy(options->sack_entries, opts + offset + 2, field_len - 2);
      offset += field_len;
    } else if (*(opts + offset) == 0x08) {
      offset += 2;
      options->local_ts = *(unsigned long *)(opts + offset);
      offset += 4;
      options->remote_ts = *(unsigned long *)(opts + offset);
      offset += 4;
    } else {
      // default handler to skip what we don't look for
      offset += *(opts + offset + 1);
    }
  }
  return 0;
}

int set_tcp_options(tcp_opts_t * options, unsigned long opt_flags, RawEthernetPacket * pkt) {
  char * pkt_opts = TCP_DATA(pkt->data);
  int offset = 0;
 

  if (opt_flags & TCP_OPTS_MSS) {
    *(pkt_opts + offset) = 0x02;
    *(pkt_opts + offset + 1) = 0x04;
    *(unsigned short *)(pkt_opts + offset + 2) = ntohs(options->mss);
    offset += 4;
  }


  *(pkt_opts + offset) = 0x00;
  offset++;

  SET_IP_TOTAL_LEN(pkt->data, GET_IP_TOTAL_LEN(pkt->data) + offset);
  compute_ip_checksum(pkt);
  SET_TCP_HDR_LEN(pkt->data, GET_TCP_HDR_LEN(pkt->data) + offset);
  compute_tcp_checksum(pkt);
  pkt->set_size(pkt->get_size() + offset);

  return 0;
}

unsigned long get_tcp_timestamp(char *opts, int len) {
  int offset = 0;
  unsigned long timestamp = 0;
  unsigned long * ts_ptr; 

  while (offset < len) {
    if (*(opts + offset) == 0x00) {
      break;
    } else if (*(opts + offset) == 0x01) {
      offset++;
    } else if (*(opts + offset) == 0x08) {
      offset += 2;
      ts_ptr = (unsigned long *)(opts + offset);
      timestamp = (*ts_ptr);
      break;
    } else if (*(opts + offset) == 0x02) {
      offset += *(opts + offset + 1);
    } else if (*(opts + offset) == 0x03) {
      offset += *(opts + offset + 1);
    } else if (*(opts + offset) == 0x04) {
      // SACK OK
      offset += 2;
    } else if (*(opts + offset) == 0x05) {
      offset += *(opts + offset + 1);
    } else {
      offset += *(opts + offset + 1);
      //vtl_debug("Could not find timestamp\n");
      //break;
    }
  }
  return timestamp;
}

void set_tcp_timestamp(char * ts_opt, unsigned long local_ts, unsigned long remote_ts) {
  int offset = 0;

  //  *(ts_opt + offset) = 0x01;
  //offset++;
  //*(ts_opt + offset) = 0x01;
  //offset++;
  *(ts_opt + offset) = 0x08;
  offset++;
  *(ts_opt + offset) = 0x0a;
  offset++;
  
  *(unsigned long *)(ts_opt + offset) = local_ts;
  offset += sizeof(unsigned long);

  *(unsigned long *)(ts_opt + offset) = remote_ts;
  
  return;
}



int pkt_has_timestamp(RawEthernetPacket * pkt) {
  unsigned short ip_hdr_len = GET_IP_HDR_LEN(pkt->data);
  unsigned short tcp_hdr_len = (*(pkt->data + ETH_HDR_LEN + ip_hdr_len + 12) & 0xf0) >> 2;
  int offset = 0;
  int len = tcp_hdr_len - 20;

  char * opts = (pkt->data + ETH_HDR_LEN + ip_hdr_len + 20);

  if (len <= 0) {
    return -1;
  }
		       
  

  while (offset < len) {
    if (*(opts + offset) == 0x00) {
      break;
    } else if (*(opts + offset) == 0x01) {
      offset++;
    } else if (*(opts + offset) == 0x08) {
      return offset;
    } else {
      offset += *(opts + offset + 1);
      //vtl_debug("Could not find timestamp\n");
      //break;
    }
  }
  return -1;

}

int compute_pkt_size(RawEthernetPacket * pkt) {
  if (is_ip_pkt(pkt)) {
    return ETH_HDR_LEN + GET_IP_TOTAL_LEN(pkt->data);
  }

  return -1;
}




int is_arp_bcast_pkt(RawEthernetPacket * pkt) {
  char broadcast[MAC_LEN] = MAC_BCAST;

  if (memcmp(ETH_DST(pkt->data), broadcast, MAC_LEN) == 0) {
    return 1;
  } 

  return 0;
}



void swap_eth_addrs(RawEthernetPacket * pkt) {
  char mac_addr[MAC_LEN];
 
  memcpy(mac_addr, ETH_DST(pkt->data), MAC_LEN);

  // copy the source mac to dest mac
  memcpy(ETH_DST(pkt->data), ETH_SRC(pkt->data), MAC_LEN);
  
  // set the dest mac to our taken address
  memcpy(ETH_SRC(pkt->data), mac_addr, MAC_LEN);
}

void swap_ip_addrs(RawEthernetPacket * pkt) {
  unsigned long src_ip;
  unsigned long dst_ip;

  src_ip = GET_IP_SRC(pkt->data);
  dst_ip = GET_IP_DST(pkt->data);

  SET_IP_SRC(pkt->data, dst_ip);
  SET_IP_DST(pkt->data, src_ip);
}

void swap_ports(RawEthernetPacket * pkt) {
  unsigned short src_port;
  unsigned short dst_port;

  src_port = GET_TCP_SRC_PORT(pkt->data);
  dst_port = GET_TCP_DST_PORT(pkt->data);

  SET_TCP_SRC_PORT(pkt->data, dst_port);
  SET_TCP_DST_PORT(pkt->data, src_port);
}


int is_syn_pkt(RawEthernetPacket * pkt) {
  char flags = GET_TCP_FLAGS(pkt->data);

  if ((flags & TCP_SYN) == TCP_SYN) {
    return 1;
  }

  return 0;
}

int is_fin_pkt(RawEthernetPacket * pkt) {
  char flags = GET_TCP_FLAGS(pkt->data);


  if ((flags & TCP_FIN) == TCP_FIN) 
    return 1;
  return 0;
}


int is_ack_pkt(RawEthernetPacket * pkt) {
  char flags = GET_TCP_FLAGS(pkt->data);

   if ((flags & TCP_ACK) == TCP_ACK)
     return 1;
   return 0;
 }

int is_dns_pkt(RawEthernetPacket * pkt) {
  // Right now we just look at the destination port address
  // there is probably a better way though....
  if (GET_UDP_DST_PORT(pkt->data) == DNS_PORT) {
    return 1;
  }
  return 0;
}

int is_tcp_pkt(RawEthernetPacket * pkt) {
  //int eth_hdr_len = 14;
  if (is_ip_pkt(pkt)) {
    // IP packet
    if (GET_IP_PROTO(pkt->data) == IP_TCP) {
      // TCP packet
      return 1;
    }
  }
  return 0;
}

int is_udp_pkt(RawEthernetPacket * pkt) {
  if (is_ip_pkt(pkt)) {
    if (GET_IP_PROTO(pkt->data) == IP_UDP) {
      return 1;
    }
  }
  return 0;
}

int is_arp_pkt(RawEthernetPacket * pkt) {
  if (GET_ETH_TYPE(pkt->data) == ETH_ARP) {
    return 1;
  } 
  return 0;
}

inline int is_ip_pkt(RawEthernetPacket * pkt) {
  if (GET_ETH_TYPE(pkt->data) == ETH_IP) {
    return 1;
  }
  return 0;
}



unsigned long compute_next_tcp_seq_num(RawEthernetPacket * pkt) {
  if (is_syn_pkt(pkt)) {
    return GET_TCP_SEQ_NUM(pkt->data) + 1;
  } else {
    return GET_TCP_SEQ_NUM(pkt->data) + GET_TCP_DATA_LEN(pkt->data);
  }

  return 0;
}


unsigned short compute_ip_checksum(RawEthernetPacket * pkt) {
  unsigned short ip_cksum;
  SET_IP_CKSUM(pkt->data, 0);
  ip_cksum =  get_ip_checksum(pkt);
  SET_IP_CKSUM(pkt->data, ip_cksum);
  return ip_cksum;
}

unsigned short compute_tcp_checksum(RawEthernetPacket * pkt) {
  unsigned short tcp_cksum;
  SET_TCP_CKSUM(pkt->data, 0);
  tcp_cksum = get_tcp_checksum(pkt, GET_TCP_TOTAL_LEN(pkt->data));
  SET_TCP_CKSUM(pkt->data, tcp_cksum);
  return tcp_cksum;
}

unsigned short compute_udp_checksum(RawEthernetPacket * pkt) {
  unsigned short udp_cksum;
  SET_UDP_CKSUM(pkt->data, 0);
  udp_cksum = get_udp_checksum(pkt, GET_UDP_LEN(pkt->data));

  // Funky optional checksum... See the RFC
  if (udp_cksum == 0) {
    udp_cksum = 0xffff;
  }
  SET_UDP_CKSUM(pkt->data, udp_cksum);
  return udp_cksum;
}

char * get_eth_protocol(unsigned short protocol) {
  if (protocol == ETH_IP) {
    return "IP";
  } else if (protocol == ETH_ARP) {
    return "ARP";
  } else if (protocol == ETH_RARP) {
    return "RARP";
  } else {
    return "Unknown";
  }
}

char* get_ip_protocol(unsigned char protocol) {
  if(protocol == IP_ICMP) {
    return "ICMP";
  } else if(protocol == IP_TCP) {
    return "TCP";
  } else if(protocol == IP_UDP) {
    return "UDP";
  } else if(protocol == 121) {
    return "SMP";
  } else {
    return "Unknown";
  }
}


unsigned short get_tcp_checksum(RawEthernetPacket * pkt, unsigned short tcp_len) {
  unsigned short buf[1600];
  unsigned long  src_addr;
  unsigned long  dest_addr;
  unsigned short len;
  unsigned short proto;

  len = tcp_len;
  len += (len % 2) ? 1 : 0;
  
  src_addr = htonl(GET_IP_SRC(pkt->data));
  dest_addr = htonl(GET_IP_DST(pkt->data));
  proto = GET_IP_PROTO(pkt->data);

  *((unsigned long *)(buf)) = src_addr;
  *((unsigned long *)(buf + 2)) = dest_addr;
  
  buf[4]=htons(proto);
  buf[5]=htons(tcp_len);
  // return 0;

  //  memcpy(buf + 6, (pkt->data + ETH_HDR_LEN + GET_IP_HDR_LEN(pkt->data)), tcp_len);
  memcpy(buf + 6, TCP_HDR(pkt->data), tcp_len);
  if (tcp_len % 2) {
    vtl_debug("Odd tcp_len: %hu\n", tcp_len);
    *(((char*)buf) + 2 * 6 + tcp_len) = 0;
  }

  return htons(~(OnesComplementSum(buf, len/2+6)));
}

unsigned short get_udp_checksum(RawEthernetPacket * pkt, unsigned short udp_len) {
  unsigned short buf[1600];
  unsigned long src_addr;
  unsigned long dest_addr;
  unsigned short len;
  unsigned short proto;

  len = udp_len;
  len += (len % 2) ? 1 : 0;

  
  src_addr = GET_IP_SRC(pkt->data);
  dest_addr = GET_IP_DST(pkt->data);
  proto = GET_IP_PROTO(pkt->data);

  *((unsigned long *)(buf)) = htonl(src_addr);
  *((unsigned long *)(buf + 2)) = htonl(dest_addr);
  
  buf[4]=htons(proto);
  buf[5]=htons(udp_len);
  // return 0;

  //  memcpy(buf + 6, (pkt->data + ETH_HDR_LEN + GET_IP_HDR_LEN(pkt->data)), udp_len);
  memcpy(buf + 6, UDP_HDR(pkt->data), udp_len);
  if (udp_len % 2) {
    vtl_debug("Odd udp_len: %hu\n", udp_len);
    *(((char*)buf) + 2 * 6 + udp_len) = 0;
  }

  return htons(~(OnesComplementSum(buf, len/2+6)));
}

unsigned short get_ip_checksum(RawEthernetPacket * pkt) {
  unsigned short buf[10];
  memset(buf, 0, 10);
  memcpy((char*)buf, IP_HDR(pkt->data), 20);
  return htons(~(OnesComplementSum(buf, 10)));
}

unsigned short OnesComplementSum(unsigned short *buf, int len) {
  unsigned long sum, sum2, sum3;
  unsigned short realsum;
  int i;

  sum=0;
  for (i=0;i<len;i++) {
    sum+=ntohs(buf[i]);
  }
  // assume there is no carry out, so now...

  sum2 = (sum&0x0000ffff) + ((sum&0xffff0000)>>16);

  sum3 = (sum2&0x0000ffff) +((sum2&0xffff0000)>>16);

  realsum=sum3;

  return realsum;
}  



