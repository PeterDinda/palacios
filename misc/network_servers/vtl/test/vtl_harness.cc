#include <stdlib.h>
#include <stdio.h>
#ifdef linux
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#endif

#include "vtl_harness.h"

DEBUG_DECLARE();


#define F_NONE 0
#define F_LOCAL_ACK 1

unsigned short g_fflags;


int g_do_local_ack = 0;
/* IP Address utility functions */



/* Global Pipe Descriptors */
int vtp_in_fd, vtp_out_fd;


/* Connection List Handling */
struct VTP_CON g_vtp_cons[MAX_VTP_CONS];
int g_first_vtp;
int g_last_vtp;
int g_num_vtp_cons;

int add_vtp_con(RawEthernetPacket * pkt, unsigned long seq_num);
int find_vtp_con(RawEthernetPacket * pkt);
int find_remote_vtp_con(RawEthernetPacket * pkt);

/* Packet Handlers */
int handle_fifo_pkt(RawEthernetPacket * pkt, struct in_addr  server_addr);
int handle_tcp_pkt(RawEthernetPacket * pkt, struct in_addr  server_addr);
int handle_rem_tcp_pkt(RawEthernetPacket * pkt);
int handle_control_pkt(RawEthernetPacket * pkt, struct in_addr  server_addr);
int handle_config_pkt(RawEthernetPacket * pkt);

/* Packet functions */
int make_ack_pkt(RawEthernetPacket * pkt, int vcon_i);
int init_ack_template(RawEthernetPacket * pkt);


int main(int argc, char ** argv) {

  fd_set all_set, rset;
  int maxfd = 0;
  int conns;
  timeval timeout;
  timeval * tm_ptr;
  RawEthernetPacket pkt;
  RawEthernetPacket * recv_pkts;
  int vtp_socket;
  int i = 0;

  debug_init("/tmp/vtp.1");

  JRLDBG("Starting VTP Daemon\n");

  for (i = 0; i < MAX_VTP_CONS; i++) {
    g_vtp_cons[i].rem_seq_num = 0;
    g_vtp_cons[i].dest_ip = 0;
    g_vtp_cons[i].src_ip = 0;
    g_vtp_cons[i].src_port = 0;
    g_vtp_cons[i].dest_port = 0;
    g_vtp_cons[i].tcp_timestamp = 0;
    g_vtp_cons[i].in_use = false;
    g_vtp_cons[i].next = -1;
    g_vtp_cons[i].prev = -1;
  }

  g_last_vtp = -1;
  g_first_vtp = -1;

  g_num_vtp_cons = 0;

  vtp_in_fd = open(VTP_FIFO_RECVFILE, O_WRONLY);
  JRLDBG("Opened RECVFILE pipe\n");

  vtp_out_fd = open(VTP_FIFO_SENDFILE, O_RDONLY);
  JRLDBG("Opened SENDFILE pipe\n");


  if ((vtp_socket = vtp_init()) < 0) {
    JRLDBG("VTP Transport Layer failed to initialize\n");
    exit(-1);
  }

  FD_ZERO(&all_set);
  FD_SET(vtp_out_fd, &all_set);

  if (vtp_socket > 0) {
    FD_SET(vtp_socket, &all_set);
  
    maxfd = (vtp_out_fd > vtp_socket) ? vtp_out_fd : vtp_socket ;

    // block indefinately, because we have all the socks in the FDSET
    tm_ptr = NULL;
  } else {  
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    tm_ptr = &timeout;
  }


  while(1) {
    int n_pkts_recvd = 0;
    rset = all_set;

    conns = select(maxfd + 1, &rset, NULL, NULL, tm_ptr);
    if ((conns > 0) && (FD_ISSET(vtp_out_fd, &rset))) {
      struct in_addr server_addr;
      JRLDBG("Reception on vtp_out_fd\n");
      // we got a packet on the FIFO
      if (pkt.VtpUnserialize(vtp_out_fd, &server_addr) <= 0) {
	JRLDBG("VNET Connection has closed. We are exiting\n");
	exit(0);
      }
      handle_fifo_pkt(&pkt, server_addr);
    }

    //JRLDBG("Calling VTP Receive routine\n");
    if ((vtp_socket == 0) || ((conns > 0) && (vtp_socket > 0) && FD_ISSET(vtp_socket, &rset))) {
      if ((n_pkts_recvd = vtp_recv(&recv_pkts)) > 0) {
	int i = 0;
	struct in_addr tmp;
	JRLDBG("Receive returned %d packets\n", n_pkts_recvd);
	
	for (i = 0; i < n_pkts_recvd; i++) {

	  if (is_tcp_pkt(&(recv_pkts[i])) == 1) {
	    JRLDBG("Received a TCP packet\n");
	    if (g_do_local_ack == 1) {
	      handle_rem_tcp_pkt(&(recv_pkts[i]));
	    }
	  }

	  JRLDBG("Serializing packet %d to VNET\n", i);
	  recv_pkts[i].VtpSerialize(vtp_in_fd, &tmp);
	  usleep(50000);
	}
	
	//delete recv_pkts;
      }
    }
  }

  fclose(logfile);
  close(vtp_in_fd);
  close(vtp_out_fd);

  return(0);
}

int handle_fifo_pkt(RawEthernetPacket * pkt, struct in_addr  server_addr) {
  JRLDBG("Received a packet\n");
  //  if (strncmp(pkt->type,"et",2) == 0) {
  if ((pkt->type[0] == 'e') && (pkt->type[1] == 't')) {
    JRLDBG("Packet is Ethernet\n");
    if (is_tcp_pkt(pkt) == 0) {
      JRLDBG("Packet is a non-TCP Packet\n");
      vtp_send(pkt, server_addr);
    } else {
      JRLDBG("Packet is a TCP Packet\n");
      handle_tcp_pkt(pkt, server_addr);
    }

  } else if (strncmp(pkt->type,"lc",2) == 0) {
    JRLDBG("Packet is a Link Control Packet\n");
    handle_control_pkt(pkt, server_addr);
  } else if (strncmp(pkt->type, "cf", 2) == 0) {
    JRLDBG("Packet is a Configuration packet\n");
    handle_config_pkt(pkt);
  }
  return 0;
}


int handle_config_pkt(RawEthernetPacket * pkt) {

  return 0;
}


int handle_control_pkt(RawEthernetPacket* pkt, struct in_addr  server_addr) {
  if (strncmp(pkt->data,"con",3) == 0) {
    struct in_addr con_addr;
    unsigned short con_port = 0;
    unsigned int offset = 3;
#ifdef DEBUG
    char ip[256];
    do_binary_to_string((unsigned char*)(&server_addr),ip);
    JRLDBG("Control Message: Connect to %s\n", ip);
#endif
    memcpy(&con_addr, pkt->data + offset,sizeof(struct in_addr));
    offset += sizeof(struct in_addr);
    con_port = *((unsigned short *)(pkt->data + offset));

    vtp_connect(con_addr, con_port);
  } else if (strncmp(pkt->data, "gwc", 3) == 0) {
    struct in_addr con_addr;
    unsigned short con_port;
    unsigned int offset = 3;

    memcpy(&con_addr, pkt->data + offset, sizeof(struct in_addr));
    offset += sizeof(struct in_addr);
    con_port = *((unsigned short *)(pkt->data + offset));

    vtp_connect(con_addr, con_port);

  } else if (strncmp(pkt->data, "stop", 4) == 0) {
    exit(0);
  }

  return 0;
}


int handle_tcp_pkt(RawEthernetPacket *pkt, struct in_addr  server_addr) {

  if (g_do_local_ack == 1) {
    unsigned short ip_pkt_len = 0;
    // unsigned char ip_hdr_len = (*(pkt->data + ETH_HDR_LEN) & 0x0f) << 2;
    unsigned char ip_hdr_len = IP_HDR_LEN(pkt->data);
    unsigned short * ip_pkt_len_ptr = (unsigned short *)(pkt->data + ETH_HDR_LEN + 2); 
    ip_pkt_len = ntohs(*ip_pkt_len_ptr);
    
    JRLDBG("IP Header Length = %d(%x)\n", ip_hdr_len, *(pkt->data + ETH_HDR_LEN));
    JRLDBG("IP Packet Length = %hu\n", ip_pkt_len);
    
    if (is_syn_pkt(pkt) == 0) {
      // we don't mess with connection establishment
      int vcon_i;
      unsigned long  payload_len = 0;
      unsigned short tcp_hdr_len = 0;
      struct in_addr tmp;
      
      
      tcp_hdr_len = (*(pkt->data + ETH_HDR_LEN + ip_hdr_len + 12) & 0xf0) >> 2;
      payload_len = ip_pkt_len  - (ip_hdr_len + tcp_hdr_len);
      
      if ((payload_len == 0) && (is_ack_pkt(pkt) == 1)) {
	// we just kill empty acks. 
	//return 0;
      }
      
      vcon_i = find_remote_vtp_con(pkt);
      
      // Create ACK and send it.
      make_ack_pkt(pkt, vcon_i);
      
      g_vtp_cons[vcon_i].ack_template.VtpSerialize(vtp_in_fd, &tmp);
      
    } else {
      if(is_ack_pkt(pkt) == 1) {
	int vcon_i = find_remote_vtp_con(pkt);
	struct in_addr tmp;
	make_ack_pkt(pkt, vcon_i);
	
	g_vtp_cons[vcon_i].ack_template.VtpSerialize(vtp_in_fd, &tmp);
      }
#ifdef DEBUG 
      unsigned long * seq_num_ptr ;
      unsigned long seq_num = 0;
      unsigned long  payload_len = 0;
      unsigned short tcp_hdr_len = 0;
      unsigned long ack = 0;
      
      JRLDBG("Packet is a Syn Packet\n");
      
      seq_num_ptr = (unsigned long *)(pkt->data + ETH_HDR_LEN + ip_hdr_len + 4);
      seq_num = ntohl(*seq_num_ptr);
      JRLDBG("Sequence Number = %lu\n", seq_num);
      
      tcp_hdr_len = (*(pkt->data + ETH_HDR_LEN + ip_hdr_len + 12) & 0xf0) >> 2;
      payload_len = ip_pkt_len  - (ip_hdr_len + tcp_hdr_len);
      
      JRLDBG("TCP Header Length = %hu\n", tcp_hdr_len);
      JRLDBG("Payload Length = %lu\n", payload_len);
      
      
      
      ack = (payload_len > 0) ? (seq_num + payload_len) : (seq_num + 1);
      JRLDBG("Ack Num = %lu\n", ack);
#endif
    }

  }

  vtp_send(pkt, server_addr);

  return 0;
}

int handle_rem_tcp_pkt(RawEthernetPacket * pkt) {
  unsigned long * seq_num_ptr;
  unsigned long seq_num;
  unsigned char ip_hdr_len = IP_HDR_LEN(pkt->data);
  
  seq_num_ptr = (unsigned long *)(pkt->data + ETH_HDR_LEN + ip_hdr_len + 4);
  seq_num = ntohl(*seq_num_ptr);
  JRLDBG("Received Packet, SeqNum = %lu\n", seq_num);

  if (is_syn_pkt(pkt) == 1) {
    // syn packet
    seq_num++;
    add_vtp_con(pkt, seq_num);
    JRLDBG("Received Syn Packet, SeqNum = %lu\n", seq_num);
  } else {
    unsigned short ip_pkt_len = 0;
    unsigned short * ip_pkt_len_ptr = (unsigned short *)(pkt->data + ETH_HDR_LEN + 2); 
    unsigned long  payload_len = 0;
    unsigned short tcp_hdr_len = 0;
    int i_vcon = find_vtp_con(pkt);
    
    ip_pkt_len = ntohs(*ip_pkt_len_ptr);

    tcp_hdr_len = (*(pkt->data + ETH_HDR_LEN + ip_hdr_len + 12) & 0xf0) >> 2;

    if (tcp_hdr_len > 20) {
      unsigned long ts = get_tcp_timestamp(pkt->data + ETH_HDR_LEN + ip_hdr_len + 20, tcp_hdr_len - 20);
      JRLDBG("TCP Timestamp = %lu(%lu)\n", ts, (unsigned long)ntohl(ts));
      g_vtp_cons[i_vcon].tcp_timestamp = ts;
    }

    payload_len = ip_pkt_len  - (ip_hdr_len + tcp_hdr_len);
    seq_num += payload_len;
    JRLDBG("Received Data Packet, SeqNum = %lu\n", seq_num);
    g_vtp_cons[i_vcon].rem_seq_num = seq_num;
    JRLDBG("Remote Sequence Number (con: %d) = %lu\n", i_vcon, seq_num);

#if 0
    {
      int offset = 0;
      unsigned short tcp_cksum = 0;
      // Zero Ack Field
      *(unsigned long *)(pkt->data + ETH_HDR_LEN + ip_hdr_len + 8) = 0;
      
      // Zero Ack Flag
      offset = ETH_HDR_LEN + ip_hdr_len + 13;
      *(pkt->data + offset) &= 0xef;
      
      // Zero TCP chksum
      *(unsigned short *)(pkt->data + ETH_HDR_LEN + ip_hdr_len + 16) = 0;
      
      // Get TCP chksum
      tcp_cksum = get_tcp_checksum(pkt, ip_pkt_len - ip_hdr_len);
      
      // Set TCP chksum
      *(unsigned short *)(pkt->data + ETH_HDR_LEN + ip_hdr_len + 16) = tcp_cksum;
    }
#endif 


  }
  return 0;
}



int make_ack_pkt(RawEthernetPacket * pkt, int vcon_i) {
  unsigned long * seq_num_ptr ;
  unsigned long seq_num = 0;
  unsigned long rem_seq_num = 0;
  unsigned long  payload_len = 0;
  unsigned short tcp_hdr_len = 0;
  unsigned long ack = 0;
  unsigned long local_ts = 0;
  unsigned short tcp_cksum = 0;
  unsigned char ip_hdr_len = IP_HDR_LEN(pkt->data);
  unsigned short ip_pkt_len = *(unsigned short *)(pkt->data + ETH_HDR_LEN + 2);
  RawEthernetPacket * ack_pkt = &(g_vtp_cons[vcon_i].ack_template);
  unsigned short ack_ip_pkt_len = *(unsigned short *)(ack_pkt->data + ETH_HDR_LEN + 2);
  unsigned char ack_ip_hdr_len = IP_HDR_LEN(ack_pkt->data);

  ip_pkt_len = ntohs(ip_pkt_len);
  ack_ip_pkt_len = ntohs(ack_ip_pkt_len);

  seq_num_ptr = (unsigned long *)(pkt->data + ETH_HDR_LEN + ip_hdr_len + 4);
  seq_num = ntohl(*seq_num_ptr);
  JRLDBG("Sequence Number = %lu\n", seq_num);

  tcp_hdr_len = (*(pkt->data + ETH_HDR_LEN + ip_hdr_len + 12) & 0xf0) >> 2;

  if (is_syn_pkt(pkt) == 1) {
    ack = seq_num + 1;
  } else {    
    payload_len = ip_pkt_len  - (ip_hdr_len + tcp_hdr_len);
    
    JRLDBG("TCP Header Length = %hu\n", tcp_hdr_len);
    JRLDBG("Payload Length = %lu\n", payload_len);
    
    ack = seq_num + payload_len;
    JRLDBG("Ack Num = %lu\n", ack);
  }

  // Set IP id 
  g_vtp_cons[vcon_i].ip_id--;
  *(unsigned short *)(ack_pkt->data + ETH_HDR_LEN + 4) = htons(g_vtp_cons[vcon_i].ip_id);

  // Recompute IP checksum
  *(unsigned short *)(ack_pkt->data + ETH_HDR_LEN + 10) = 0;
  *(unsigned short *)(ack_pkt->data + ETH_HDR_LEN + 10) = get_ip_checksum(ack_pkt);

  //return 0;
  // Set Sequence Number
  rem_seq_num = htonl(g_vtp_cons[vcon_i].rem_seq_num);
  *(unsigned long *)(ack_pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 4) = rem_seq_num;
 
  // Set ACK Number
  ack = htonl(ack);
  *(unsigned long *)(ack_pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 8) = ack;

  // Set TCP Timestamp option
  local_ts = get_tcp_timestamp(pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 20, tcp_hdr_len - 20);

  /* We use this for debugging:
   * If the TCPDump trace shows timestamps with the value of '5' then they are our packets
   */
  
  *(unsigned long *)(ack_pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 24) = g_vtp_cons[vcon_i].tcp_timestamp;
  //*(unsigned long *)(ack_pkt->data + ETH_HDR_LEN + ip_hdr_len + 24) = htonl(5);


  *(unsigned long *)(ack_pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 28) = local_ts;

  // Zero TCP chksum
  *(unsigned short *)(ack_pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 16) = 0;

  // Get TCP chksum
  tcp_cksum = get_tcp_checksum(ack_pkt, ack_ip_pkt_len - ack_ip_hdr_len);

  // Set TCP chksum
  *(unsigned short *)(ack_pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 16) = tcp_cksum;

  return 0;
}





/* Connection List Handling */

int find_vtp_con(RawEthernetPacket * pkt) {
  int index = -1;
  int i = 0;
  unsigned long * src_addr;
  unsigned long * dest_addr;
  unsigned short * src_port;
  unsigned short * dest_port;
  unsigned char ip_hdr_len = IP_HDR_LEN(pkt->data);

  src_addr = (unsigned long *)(pkt->data + ETH_HDR_LEN + 12);
  dest_addr = (unsigned long *)(pkt->data + ETH_HDR_LEN + 16);
  src_port = (unsigned short *)(pkt->data + ETH_HDR_LEN + ip_hdr_len);
  dest_port = (unsigned short *)(pkt->data + ETH_HDR_LEN + ip_hdr_len + 2);

  //  for (i = 0; i < MAX_CONS; i++) {
  FOREACH_VTP_CON(i,g_vtp_cons) {
    if ((g_vtp_cons[i].dest_ip == *dest_addr) && (g_vtp_cons[i].src_ip == *src_addr) &&
	(g_vtp_cons[i].dest_port = *dest_port) && (g_vtp_cons[i].src_port == *src_port)) {
      index = i;
      break;
    }
  }
  return index;
}


/* An received packet has the header fields reversed wrt src/dest
 * So we have to be able to index remote packets as well
 */
int find_remote_vtp_con(RawEthernetPacket * pkt) {
  int index = -1;
  int i = 0;
  unsigned long * src_addr;
  unsigned long * dest_addr;
  unsigned short * src_port;
  unsigned short * dest_port;
  unsigned char ip_hdr_len = IP_HDR_LEN(pkt->data);

  src_addr = (unsigned long *)(pkt->data + ETH_HDR_LEN + 12);
  dest_addr = (unsigned long *)(pkt->data + ETH_HDR_LEN + 16);
  src_port = (unsigned short *)(pkt->data + ETH_HDR_LEN + ip_hdr_len);
  dest_port = (unsigned short *)(pkt->data + ETH_HDR_LEN + ip_hdr_len + 2);

  //  for (i = 0; i < MAX_CONS; i++) {
  FOREACH_VTP_CON(i,g_vtp_cons) {
    if ((g_vtp_cons[i].src_ip == *dest_addr) && (g_vtp_cons[i].dest_ip == *src_addr) &&
	(g_vtp_cons[i].src_port = *dest_port) && (g_vtp_cons[i].dest_port == *src_port)) {
      index = i;
      break;
    }
  }
  return index;
}


int add_vtp_con(RawEthernetPacket * pkt, unsigned long seq_num) {
  int i;
  unsigned long * src_addr;
  unsigned long * dest_addr;
  unsigned short * src_port;
  unsigned short * dest_port;
  unsigned char ip_hdr_len = IP_HDR_LEN(pkt->data);
  unsigned short tcp_hdr_len = (*(pkt->data + ETH_HDR_LEN + ip_hdr_len + 12) & 0xf0) >> 2;

  src_addr = (unsigned long *)(pkt->data + ETH_HDR_LEN + 12);
  dest_addr = (unsigned long *)(pkt->data + ETH_HDR_LEN + 16);
  src_port = (unsigned short *)(pkt->data + ETH_HDR_LEN + ip_hdr_len);
  dest_port = (unsigned short *)(pkt->data + ETH_HDR_LEN + ip_hdr_len + 2);
  
  for (i = 0; i < MAX_VTP_CONS; i++) {
    if (!(g_vtp_cons[i].in_use)) {
      JRLDBG("Adding connection in slot %d\n", i);
      g_vtp_cons[i].rem_seq_num = seq_num;
      
      // ADD PACKET CONNECTION INFO
      g_vtp_cons[i].dest_ip = *dest_addr;
      g_vtp_cons[i].src_ip = *src_addr;
      g_vtp_cons[i].src_port = *src_port;
      g_vtp_cons[i].dest_port = *dest_port;
      g_vtp_cons[i].ack_template = *pkt;
      g_vtp_cons[i].ip_id = ntohs(*(unsigned short *)(pkt->data + ETH_HDR_LEN + 4));
	
      init_ack_template(&(g_vtp_cons[i].ack_template));

      if (tcp_hdr_len > 20) {
	unsigned long ts = get_tcp_timestamp(pkt->data + ETH_HDR_LEN + ip_hdr_len + 20, tcp_hdr_len - 20);
	JRLDBG("TCP Timestamp = %lu(%lu)\n", ts, (unsigned long)ntohl(ts));
	g_vtp_cons[i].tcp_timestamp = ts;
      }
      
      g_vtp_cons[i].in_use = true;
      
      if (g_first_vtp == -1)
	g_first_vtp = i;

      g_vtp_cons[i].prev = g_last_vtp;
      g_vtp_cons[i].next = -1;

      if (g_last_vtp != -1) {
	g_vtp_cons[g_last_vtp].next = i;
      }
      
      g_last_vtp = i;

      g_num_vtp_cons++;
      return 0;
    }
  }
  return -1;
}


int init_ack_template(RawEthernetPacket * pkt) {
  // We assume here that the ethernet and ip headers are ok, except for ip pkt length
  // TCP is mostly right because its pulled off of a syn packet
  // we need to zero the data, and reset the syn flag.

  unsigned short IP_PACKET_LEN = 52;
  unsigned short TCP_HEADER_LEN = 32;
  unsigned char ip_hdr_len = IP_HDR_LEN(pkt->data);
  unsigned short ip_pkt_len = 0;
  unsigned short tcp_hdr_len = 0;
  unsigned short payload_len = 0;
  unsigned short * ip_pkt_len_ptr = (unsigned short *)(pkt->data + ETH_HDR_LEN + 2); 
  unsigned int offset = 0;
  unsigned short ip_chksum = 0;

  JRLDBG("--> Initializing ACK Template <--\n");

  ip_pkt_len = ntohs(*ip_pkt_len_ptr);
  JRLDBG("ip_pkt_len = %hu\n", ip_pkt_len);
  
  tcp_hdr_len = (*(pkt->data + ETH_HDR_LEN + ip_hdr_len + 12) & 0xf0) >> 2;
  payload_len = ip_pkt_len  - (ip_hdr_len + tcp_hdr_len);
  JRLDBG("tcp_hdr_len = %hu\n", tcp_hdr_len);
  JRLDBG("payload_len = %hu\n", payload_len);

  // set only the ack flags
  offset = ETH_HDR_LEN + ip_hdr_len + 13;
  *(pkt->data + offset)  |= 0x10;
  *(pkt->data + offset)  &= 0xd0;

  // set up tcp options
  offset = ETH_HDR_LEN + ip_hdr_len + 20;
  *(pkt->data + offset) = 0x01;
  offset++;
  *(pkt->data + offset) = 0x01;
  offset++;
  *(pkt->data + offset) = 0x08;
  offset++;
  *(pkt->data + offset) = 0x0a;

  // Set Header Lengths
  // IP HEADER = 20 (same)
  // IP PACKET LEN = 52
  // TCP Header len = 32
  
  ip_pkt_len = htons(IP_PACKET_LEN);
  memcpy(pkt->data + ETH_HDR_LEN + 2, &ip_pkt_len, 2);
  tcp_hdr_len = (TCP_HEADER_LEN << 2);
  *(pkt->data + ETH_HDR_LEN + ip_hdr_len + 12) &= 0x0f;
  *(pkt->data + ETH_HDR_LEN + ip_hdr_len + 12) |= tcp_hdr_len;

  JRLDBG("Setting TEMPLATE TCPLEN = %2x\n", *(pkt->data + ETH_HDR_LEN +ip_hdr_len + 12));

  // Set IP Header chksum
  *(unsigned short *)(pkt->data + ETH_HDR_LEN + 10) = 0;
  ip_chksum = get_ip_checksum(pkt);
  *(unsigned short *)(pkt->data + ETH_HDR_LEN + 10) = ip_chksum;


  // Set RawEthernetPacket size
  pkt->set_size(IP_PACKET_LEN + ETH_HDR_LEN);
  pkt->set_type("et");

  JRLDBG("--> ACK Template Initialized <--\n");


  return 0;
}

