#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <fcntl.h>

#include "vtl.h"
#include "vtl_harness.h"

DEBUG_DECLARE();



/* Connection List Handling */
struct VTL_CON g_vtl_cons[MAX_VTL_CONS];
int g_first_vtl;
int g_last_vtl;
int g_num_vtl_cons;

int add_vtl_con(RawEthernetPacket * pkt);
int find_vtl_con(RawEthernetPacket * pkt);

/* Packet Handlers */
int handle_local_tcp_pkt(RawEthernetPacket * pkt, iface_t * dev);
int handle_remote_tcp_pkt(RawEthernetPacket * pkt);

/* Packet functions */
int make_ack_pkt(RawEthernetPacket * pkt, int vcon_i);
int init_ack_template(RawEthernetPacket * pkt);


unsigned short ip_id_ctr = 1;

int main(int argc, char ** argv) {
  RawEthernetPacket pkt;
  RawEthernetPacket ack_pkt;
  int i = 0;
  iface_t * dev;
  unsigned long src_addr;

  debug_init("./vtl.log");

  JRLDBG("Starting VTP Daemon\n");

  for (i = 0; i < MAX_VTL_CONS; i++) {
    g_vtl_cons[i].con_model.type = TCP_MODEL;
    g_vtl_cons[i].in_use = false;
    g_vtl_cons[i].next = -1;
    g_vtl_cons[i].prev = -1;
  }

  g_last_vtl = -1;
  g_first_vtl = -1;

  g_num_vtl_cons = 0;

  src_addr = ToIPAddress(argv[2]);
  dev = if_connect(argv[1]);

  while (if_read_pkt(dev, &pkt) != -1) {



    if (is_tcp_pkt(&pkt)) {
      if (GET_IP_SRC(pkt.data) == src_addr) {
	handle_local_tcp_pkt(&pkt, dev);
      } else if (GET_IP_DST(pkt.data) == src_addr) {
	if (GET_IP_ID(pkt.data) == ip_id_ctr -1) {
	  continue;
	}
	handle_remote_tcp_pkt(&pkt);
	printf("Remote tcp packet\n");
      }
    }
  }


  fclose(logfile);


  return(0);
}


int handle_local_tcp_pkt(RawEthernetPacket * pkt, iface_t * dev) {
  printf("local tcp pkt\n");
  RawEthernetPacket ack_pkt;

  int index = find_vtl_con(pkt);

  if (index != -1) {
    // packet in the system
    sync_model(&(g_vtl_cons[index].con_model), pkt);


    // dbg_dump_model(&(g_vtl_cons[index].con_model));
    if (GET_TCP_DATA_LEN(pkt->data) > 0) {
      create_empty_pkt(&(g_vtl_cons[index].con_model), &ack_pkt, INBOUND_PKT);
      dbg_print_pkt_info(&ack_pkt);
      if_write_pkt(dev, &ack_pkt);
      
    }
  } else {
    if (is_syn_pkt(pkt)) {
      int index = -1;

      index = add_vtl_con(pkt);
      printf("Connection added at %d\n", index);

      
    }
  }

  return 0;

}

int handle_remote_tcp_pkt(RawEthernetPacket * pkt) {
  int index;

  index = find_vtl_con(pkt);

  if (index != -1) {
    sync_model(&(g_vtl_cons[index].con_model), pkt);
    g_vtl_cons[index].con_model.model.ip_model.dst.ip_id = ip_id_ctr++;
  }

  return 0;
}



int find_vtl_con(RawEthernetPacket * pkt) {
  int index = -1;
  int i = 0;


  FOREACH_VTL_CON(i,g_vtl_cons) {
    if (is_model_pkt(&(g_vtl_cons[i].con_model), pkt)) {
      index = i;
      break;
    }
  }
  return index;
}



int add_vtl_con(RawEthernetPacket * pkt) {
  int i;
  
  for (i = 0; i < MAX_VTL_CONS; i++) {
    if (!(g_vtl_cons[i].in_use)) {
      JRLDBG("Adding connection in slot %d\n", i);

      initialize_model(&(g_vtl_cons[i].con_model), pkt);
      g_vtl_cons[i].in_use = true;
      
      dbg_dump_model(&(g_vtl_cons[i].con_model));

      if (g_first_vtl == -1)
	g_first_vtl = i;

      g_vtl_cons[i].prev = g_last_vtl;
      g_vtl_cons[i].next = -1;

      if (g_last_vtl != -1) {
	g_vtl_cons[g_last_vtl].next = i;
      }
      
      g_last_vtl = i;

      g_num_vtl_cons++;
      return 0;
    }
  }
  return -1;
}



/*


int handle_tcp_pkt(RawEthernetPacket *pkt) {


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
      
      g_vtl_cons[vcon_i].ack_template.VtpSerialize(vtp_in_fd, &tmp);
      
    } else {
      if(is_ack_pkt(pkt) == 1) {
	int vcon_i = find_remote_vtp_con(pkt);
	struct in_addr tmp;
	make_ack_pkt(pkt, vcon_i);
	
	g_vtl_cons[vcon_i].ack_template.VtpSerialize(vtp_in_fd, &tmp);
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
      g_vtl_cons[i_vcon].tcp_timestamp = ts;
    }

    payload_len = ip_pkt_len  - (ip_hdr_len + tcp_hdr_len);
    seq_num += payload_len;
    JRLDBG("Received Data Packet, SeqNum = %lu\n", seq_num);
    g_vtl_cons[i_vcon].rem_seq_num = seq_num;
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
  RawEthernetPacket * ack_pkt = &(g_vtl_cons[vcon_i].ack_template);
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
  g_vtl_cons[vcon_i].ip_id--;
  *(unsigned short *)(ack_pkt->data + ETH_HDR_LEN + 4) = htons(g_vtl_cons[vcon_i].ip_id);

  // Recompute IP checksum
  *(unsigned short *)(ack_pkt->data + ETH_HDR_LEN + 10) = 0;
  *(unsigned short *)(ack_pkt->data + ETH_HDR_LEN + 10) = get_ip_checksum(ack_pkt);

  //return 0;
  // Set Sequence Number
  rem_seq_num = htonl(g_vtl_cons[vcon_i].rem_seq_num);
  *(unsigned long *)(ack_pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 4) = rem_seq_num;
 
  // Set ACK Number
  ack = htonl(ack);
  *(unsigned long *)(ack_pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 8) = ack;

  // Set TCP Timestamp option
  local_ts = get_tcp_timestamp(pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 20, tcp_hdr_len - 20);

 //  We use this for debugging:
  //  If the TCPDump trace shows timestamps with the value of '5' then they are our packets
  // 
  
  *(unsigned long *)(ack_pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 24) = g_vtl_cons[vcon_i].tcp_timestamp;
//  *(unsigned long *)(ack_pkt->data + ETH_HDR_LEN + ip_hdr_len + 24) = htonl(5);


  *(unsigned long *)(ack_pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 28) = local_ts;

  // Zero TCP chksum
  *(unsigned short *)(ack_pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 16) = 0;

  // Get TCP chksum
  tcp_cksum = get_tcp_checksum(ack_pkt, ack_ip_pkt_len - ack_ip_hdr_len);

  // Set TCP chksum
  *(unsigned short *)(ack_pkt->data + ETH_HDR_LEN + ack_ip_hdr_len + 16) = tcp_cksum;

  return 0;
}





// Connection List Handling //



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

*/
