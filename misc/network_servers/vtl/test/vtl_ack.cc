#include "vtl_ack.h"


int make_ack_pkt(tcp_model_t * model, RawEthernetPacket * data_pkt, RawEthernetPacket * ack_pkt) {
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


}
