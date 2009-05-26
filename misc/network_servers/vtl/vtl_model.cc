#include "vtl_model.h"
#include <assert.h>


/* VTL Models */





vtl_model_t * new_vtl_model(model_type_t type) {
  vtl_model_t * model = (vtl_model_t *)malloc(sizeof(vtl_model_t));

  model->type = type;

  return model;
}


int initialize_ethernet_model(ethernet_model_t * model, RawEthernetPacket * pkt, int dir = OUTBOUND_PKT) {
  assert((model != NULL) && (pkt != NULL));

  vtl_debug("initializing ethernet model\n");
  if (dir == OUTBOUND_PKT) {
    GET_ETH_DST(pkt->data, model->dst.addr);
    GET_ETH_SRC(pkt->data, model->src.addr);
  } else if (dir == INBOUND_PKT) {
    GET_ETH_DST(pkt->data, model->src.addr);
    GET_ETH_SRC(pkt->data, model->dst.addr);
  } else {
    return -1;
  }

  model->type = GET_ETH_TYPE(pkt->data);

  return 0;
}

int initialize_ip_model(ip_model_t * model, RawEthernetPacket * pkt, int dir) {
  assert((model != NULL) && (pkt != NULL));

  if (!is_ip_pkt(pkt)) {
    return -1;
  }

  if (dir == OUTBOUND_PKT) {
    model->src.addr = GET_IP_SRC(pkt->data);
    model->dst.addr = GET_IP_DST(pkt->data);
    model->dst.ip_id = 1;
    model->src.ip_id = GET_IP_ID(pkt->data);
    model->src.ttl = GET_IP_TTL(pkt->data);
    model->dst.ttl = 1;
  } else if (dir == INBOUND_PKT) {
    model->src.addr = GET_IP_DST(pkt->data);
    model->dst.addr = GET_IP_SRC(pkt->data);
    model->src.ip_id = 1;
    model->dst.ip_id = GET_IP_ID(pkt->data);
    model->src.ttl = 1;
    model->dst.ttl = GET_IP_TTL(pkt->data);
  } else {
    return -1;
  }

  model->version = GET_IP_VERSION(pkt->data);
  model->proto = GET_IP_PROTO(pkt->data);

  if (initialize_ethernet_model(&(model->ethernet), pkt, dir) == -1) {
    return -1;
  }
    
  return 0;
}

int initialize_tcp_model(tcp_model_t * model, RawEthernetPacket * pkt, int dir) {
  assert((model != NULL) && (pkt != NULL));

  tcp_opts_t options;

  if (!is_tcp_pkt(pkt)) {
    return -1;
  }

  parse_tcp_options(&options, pkt);

  if (dir == OUTBOUND_PKT) {
    model->src.port = GET_TCP_SRC_PORT(pkt->data);
    model->dst.port = GET_TCP_DST_PORT(pkt->data);
    model->src.seq_num = compute_next_tcp_seq_num(pkt);
    model->src.last_ack = GET_TCP_ACK_NUM(pkt->data);
    model->src.win = GET_TCP_WIN(pkt->data);
    model->src.mss = options.mss;
    model->src.ts = options.local_ts;
    model->dst.mss = 0;
    model->dst.ts = 0;
  } else if (dir == INBOUND_PKT) {
    model->src.port = GET_TCP_DST_PORT(pkt->data);
    model->dst.port = GET_TCP_SRC_PORT(pkt->data);
    model->dst.last_ack = GET_TCP_ACK_NUM(pkt->data);
    model->src.seq_num = 1;
    model->dst.seq_num = compute_next_tcp_seq_num(pkt);
    model->dst.win = GET_TCP_WIN(pkt->data);
    model->dst.mss = options.mss;
    model->dst.ts = options.local_ts;
    model->src.mss = 0;
    model->src.ts = 0;
  } else {
    return -1;
  }

  if (initialize_ip_model(&(model->ip), pkt, dir) == -1) {
    return -1;
  }
  
  return 0;
}

int initialize_udp_model(udp_model_t * model, RawEthernetPacket * pkt, int dir) {
  assert((model != NULL) && (pkt != NULL));
  
  if (!is_udp_pkt(pkt)) {
    return -1;
  }

  if (dir == OUTBOUND_PKT) {
    model->src.port = GET_UDP_SRC_PORT(pkt->data);
    model->dst.port = GET_UDP_DST_PORT(pkt->data);
  } else if (dir == INBOUND_PKT) {
    model->src.port = GET_UDP_DST_PORT(pkt->data);
    model->dst.port = GET_UDP_SRC_PORT(pkt->data);
  } else {
    return -1;
  }

  if (initialize_ip_model(&(model->ip), pkt, dir) == -1) {
    return -1;
  }

  return 0;
}



int initialize_model(vtl_model_t * model, RawEthernetPacket * pkt, int dir) {
  assert((model != NULL) && (pkt != NULL));

  if (model->type == TCP_MODEL) {
    return initialize_tcp_model(&(model->model.tcp_model), pkt, dir);
  } else if (model->type == UDP_MODEL) {
    return initialize_udp_model(&(model->model.udp_model), pkt, dir);
  } else if (model->type == IP_MODEL) {
  } else if (model->type == ETHERNET_MODEL) {
  }

  return -1;
}



int is_ethernet_model_pkt(ethernet_model_t * model, RawEthernetPacket * pkt) {
  assert((model != NULL) && (pkt != NULL));

  if ((memcmp(model->src.addr, ETH_SRC(pkt->data), 6) == 0) &&
      (memcmp(model->dst.addr, ETH_DST(pkt->data), 6) == 0)) {
    return OUTBOUND_PKT;
  } else if ((memcmp(model->src.addr, ETH_DST(pkt->data), 6) == 0) &&
	     (memcmp(model->dst.addr, ETH_SRC(pkt->data), 6) == 0)) {
    return INBOUND_PKT;
  }

  return INVALID_PKT;
}

int is_ip_model_pkt(ip_model_t * model, RawEthernetPacket * pkt) {
  assert((model != NULL) && (pkt != NULL));

  if (!is_ip_pkt(pkt)) {
    return INVALID_PKT;
  }
  
  if ((model->src.addr == GET_IP_SRC(pkt->data)) &&
      (model->dst.addr == GET_IP_DST(pkt->data))) {
    return OUTBOUND_PKT;
  } else if ((model->src.addr == GET_IP_DST(pkt->data)) &&
	     (model->dst.addr == GET_IP_SRC(pkt->data))) {
    return INBOUND_PKT;
  }

  return INVALID_PKT;
}


int is_tcp_model_pkt(tcp_model_t * model, RawEthernetPacket * pkt) {
  assert((model != NULL) && (pkt != NULL));
  int ip_ret;

  if (!is_tcp_pkt(pkt)) {
    return INVALID_PKT;
  }

  if ((ip_ret = is_ip_model_pkt(&(model->ip), pkt)) == INVALID_PKT) {
    return INVALID_PKT;
  }


  if (ip_ret == OUTBOUND_PKT) {
    if ((model->src.port == GET_TCP_SRC_PORT(pkt->data)) && 
	(model->dst.port == GET_TCP_DST_PORT(pkt->data))) {
      return OUTBOUND_PKT;
    } 
  } else if (ip_ret == INBOUND_PKT) {
    if ((model->src.port == GET_TCP_DST_PORT(pkt->data)) && 
	(model->dst.port == GET_TCP_SRC_PORT(pkt->data))) {
      return INBOUND_PKT;
    }
  }
  return INVALID_PKT;
}

int is_udp_model_pkt(udp_model_t * model, RawEthernetPacket * pkt) {
  assert((model != NULL) && (pkt != NULL));
  int ip_ret;

  if (!is_udp_pkt(pkt)) {
    return INVALID_PKT;
  }

  if ((ip_ret = is_ip_model_pkt(&(model->ip), pkt)) == INVALID_PKT) {
    return INVALID_PKT;
  }

  if (ip_ret == OUTBOUND_PKT) {
    if ((model->src.port == GET_UDP_SRC_PORT(pkt->data)) &&
	(model->dst.port == GET_UDP_DST_PORT(pkt->data))) {
      return OUTBOUND_PKT;
    }
  } else if (ip_ret == INBOUND_PKT) {
    if ((model->src.port == GET_UDP_DST_PORT(pkt->data)) &&
	(model->dst.port == GET_UDP_SRC_PORT(pkt->data))) {
      return INBOUND_PKT;
    }
  }

  return INBOUND_PKT;
}

int is_model_pkt(vtl_model_t * model, RawEthernetPacket * pkt) {
  assert((model != NULL) && (pkt != NULL));
  
  if (model->type == TCP_MODEL) {
    return is_tcp_model_pkt(&(model->model.tcp_model), pkt);
  }

  return INVALID_PKT;
}


int sync_ip_model(ip_model_t * model, RawEthernetPacket * pkt) {
  assert((model != NULL) && (pkt != NULL));

  int ip_ret; 
  
  ip_ret = is_ip_model_pkt(model, pkt);

  if (ip_ret == OUTBOUND_PKT) {
    model->src.ip_id = GET_IP_ID(pkt->data);
    model->src.ttl = GET_IP_TTL(pkt->data);
  } else if (ip_ret == INBOUND_PKT) {
    model->dst.ip_id = GET_IP_ID(pkt->data);
    model->dst.ttl = GET_IP_TTL(pkt->data);
  } else { 
    return -1;
  }

  return 0;
}


int sync_tcp_model(tcp_model_t * model, RawEthernetPacket * pkt) {
  assert((model != NULL) && (pkt != NULL));
  int tcp_ret;
  tcp_opts_t options;
  int has_opts = 0;

  tcp_ret = is_tcp_model_pkt(model, pkt);

  has_opts = parse_tcp_options(&options, pkt);

  if (tcp_ret == OUTBOUND_PKT) {
    model->src.seq_num = compute_next_tcp_seq_num(pkt);
    model->src.last_ack = GET_TCP_ACK_NUM(pkt->data);
    model->src.win = GET_TCP_WIN(pkt->data);
    if (has_opts == 0) {
      model->src.mss = options.mss;
      model->src.ts = options.local_ts;
    }
  } else if (tcp_ret == INBOUND_PKT) {
    model->dst.last_ack = GET_TCP_ACK_NUM(pkt->data);
    model->dst.seq_num = compute_next_tcp_seq_num(pkt);
    model->dst.win = GET_TCP_WIN(pkt->data);
    if (has_opts == 0) {
      model->dst.mss = options.mss;
      model->dst.ts = options.local_ts;
    }
  } else { 
    return -1;
  }

  if (sync_ip_model(&(model->ip), pkt) == -1) {
    return -1;
  }

  return 0;
}


int sync_udp_model(udp_model_t * model, RawEthernetPacket * pkt) {
  assert((model != NULL) && (pkt != NULL));
  int udp_ret;
  
  udp_ret = is_udp_model_pkt(model, pkt);

  if (udp_ret == INVALID_PKT) {
    return -1;
  }

  if (sync_ip_model(&(model->ip), pkt) == -1) {
    return -1;
  }
  
  return 0;
}
  
int sync_model(vtl_model_t * model, RawEthernetPacket * pkt) {

  if (model->type == TCP_MODEL) {
    return sync_tcp_model(&(model->model.tcp_model), pkt);
  } else if (model->type == IP_MODEL) {
    return sync_ip_model(&(model->model.ip_model), pkt);
  }
  return -1;
}

int create_empty_ethernet_pkt(ethernet_model_t * model, RawEthernetPacket * pkt, int dir) {
  if (dir == OUTBOUND_PKT) {
    SET_ETH_SRC(pkt->data, model->src.addr);
    SET_ETH_DST(pkt->data, model->dst.addr);
  } else if (dir == INBOUND_PKT) {    
    SET_ETH_SRC(pkt->data, model->dst.addr);
    SET_ETH_DST(pkt->data, model->src.addr);
  }
  SET_ETH_TYPE(pkt->data, model->type);
  pkt->set_size(ETH_HDR_LEN);

  return 0;
}


int create_empty_ip_pkt(ip_model_t * model, RawEthernetPacket * pkt, int dir) {
  create_empty_ethernet_pkt(&(model->ethernet), pkt, dir);

  SET_IP_VERSION(pkt->data, model->version);
  SET_IP_HDR_LEN(pkt->data, 20);
  SET_IP_SVC_TYPE(pkt->data, 0);
  SET_IP_TOTAL_LEN(pkt->data, 20); // WE ARE JUST AN EMPTY PACKET HERE
  SET_IP_FLAGS(pkt->data, 0);
  SET_IP_FRAG(pkt->data, 0);
  SET_IP_PROTO(pkt->data, model->proto);


  if (dir == OUTBOUND_PKT) {
    SET_IP_ID(pkt->data, model->src.ip_id + 1);
    SET_IP_TTL(pkt->data, model->src.ttl);    
    SET_IP_SRC(pkt->data, model->src.addr);
    SET_IP_DST(pkt->data, model->dst.addr);
  } else if (dir == INBOUND_PKT) {
    SET_IP_ID(pkt->data, model->dst.ip_id + 1);
    SET_IP_TTL(pkt->data, model->dst.ttl);
    //    SET_IP_TTL(pkt->data, 5);
    SET_IP_SRC(pkt->data, model->dst.addr);
    SET_IP_DST(pkt->data, model->src.addr);
  } 

  compute_ip_checksum(pkt);
  pkt->set_size(compute_pkt_size(pkt));

  return 0;
}

int create_empty_tcp_pkt(tcp_model_t * model, RawEthernetPacket * pkt, int dir) {
  create_empty_ip_pkt(&(model->ip), pkt, dir);

  SET_TCP_HDR_LEN(pkt->data, 20);
  SET_TCP_RSVD(pkt->data, 0);
  SET_TCP_FLAGS(pkt->data, 0);
  SET_TCP_URG_PTR(pkt->data, 0);

  if (dir == OUTBOUND_PKT) {
    SET_TCP_SRC_PORT(pkt->data, model->src.port);
    SET_TCP_DST_PORT(pkt->data, model->dst.port);
    SET_TCP_SEQ_NUM(pkt->data, model->src.seq_num);
    
    // This is kind of weird
    // We set the ack number to last ack that was sent on the actual channel
    // We want to insert packets into the connection without messing things up
    // So we don't want to ack data that hasn't necessarily been received
    // Since we're blowing away the seq_num sequence anyway, this might not matter
    //    SET_TCP_ACK_NUM(pkt->data, model->src.last_ack); 
    SET_TCP_ACK_NUM(pkt->data, model->dst.seq_num); 
    SET_TCP_ACK_FLAG(pkt->data);

    SET_TCP_WIN(pkt->data, model->src.win);

  } else if (dir == INBOUND_PKT) {
    SET_TCP_SRC_PORT(pkt->data, model->dst.port);
    SET_TCP_DST_PORT(pkt->data, model->src.port);    

    SET_TCP_SEQ_NUM(pkt->data, model->dst.seq_num);
    
    // This is kind of weird
    // We set the ack number to last ack that was sent on the actual channel
    // We want to insert packets into the connection without messing things up
    // So we don't want to ack data that hasn't necessarily been received
    // Since we're blowing away the seq_num sequence anyway, this might not matter
    //SET_TCP_ACK_NUM(pkt->data, model->dst.last_ack); 
    SET_TCP_ACK_NUM(pkt->data, model->src.seq_num); 

    SET_TCP_ACK_FLAG(pkt->data);

    SET_TCP_WIN(pkt->data, model->dst.win);
  }


  SET_IP_TOTAL_LEN(pkt->data, GET_IP_HDR_LEN(pkt->data) + GET_TCP_HDR_LEN(pkt->data));
  pkt->set_size(compute_pkt_size(pkt));

  compute_ip_checksum(pkt);

  compute_tcp_checksum(pkt);

  vtl_debug("tcp_len = %d\n", GET_TCP_HDR_LEN(pkt->data));


  // Set the ip hdr len

  return 0;
}


int create_empty_udp_pkt(udp_model_t * model, RawEthernetPacket * pkt, int dir) {
  create_empty_ip_pkt(&(model->ip), pkt, dir);

  if (dir == OUTBOUND_PKT) {
    SET_UDP_SRC_PORT(pkt->data, model->src.port);
    SET_UDP_DST_PORT(pkt->data, model->dst.port);
  } else if (dir == INBOUND_PKT) {
    SET_UDP_SRC_PORT(pkt->data, model->dst.port);
    SET_UDP_DST_PORT(pkt->data, model->src.port);
  }
  SET_UDP_LEN(pkt->data, 8);
  SET_IP_TOTAL_LEN(pkt->data, GET_IP_HDR_LEN(pkt->data) + GET_UDP_LEN(pkt->data));

  compute_ip_checksum(pkt);
  compute_udp_checksum(pkt);

  return 0;
}

int create_empty_pkt(vtl_model_t * model, RawEthernetPacket * pkt, int dir) {
  if (model->type == TCP_MODEL) {
    return create_empty_tcp_pkt(&(model->model.tcp_model), pkt, dir);
  } else if (model->type == UDP_MODEL) {
    return create_empty_udp_pkt(&(model->model.udp_model), pkt, dir);
  } else if (model->type == IP_MODEL) {
    return create_empty_ip_pkt(&(model->model.ip_model), pkt, dir);
  }
  return -1;
}




void dbg_dump_eth_model(ethernet_model_t * model) {
  char src_mac[6];
  char dst_mac[6];

  vtl_debug("ETHERNET MODEL {\n");

  vtl_debug("\tType: %s\n", get_eth_protocol(model->type));

  mac_to_string(model->src.addr, src_mac);
  vtl_debug("\tSrc Host {\n");
  vtl_debug("\t\taddr: %s\n", src_mac);
  vtl_debug("\t}\n");

  mac_to_string(model->dst.addr, dst_mac);
  vtl_debug("\tDST Host {\n");
  vtl_debug("\t\taddr: %s\n", dst_mac);
  vtl_debug("\t}\n");
  vtl_debug("}\n");
}



void dbg_dump_ip_model(ip_model_t * model) {
  dbg_dump_eth_model(&(model->ethernet));

  vtl_debug("IP MODEL {\n");
  vtl_debug("\tVersion: %d\n", model->version);
  vtl_debug("\tProtocol: %s\n", get_ip_protocol(model->proto));

  vtl_debug("\tSrc Host {\n");
  vtl_debug("\t\taddr: %s\n", ip_to_string(model->src.addr));
  vtl_debug("\t\tIP ID: %d\n", model->src.ip_id);
  vtl_debug("\t\tttl: %d\n", model->src.ttl);
  vtl_debug("\t}\n");

  vtl_debug("\tDst Host {\n");
  vtl_debug("\t\taddr: %s\n", ip_to_string(model->dst.addr));
  vtl_debug("\t\tIP ID: %d\n", model->dst.ip_id);
  vtl_debug("\t\tttl: %d\n", model->dst.ttl);
  vtl_debug("\t}\n");

  vtl_debug("}\n");
}

void dbg_dump_tcp_model(tcp_model_t * model) {
  dbg_dump_ip_model(&(model->ip));

  vtl_debug("TCP MODEL {\n");
  vtl_debug("\tSrc Host {\n");
  vtl_debug("\t\tport: %hu\n", model->src.port);
  vtl_debug("\t\tseq: %lu\n", (unsigned long)(model->src.seq_num));
  vtl_debug("\t\tlast ack: %lu\n", (unsigned long)(model->src.last_ack));
  vtl_debug("\t\tWin Size: %hu\n", model->src.win);
  vtl_debug("\t\tTimestamp: %lu\n", (unsigned long)(model->src.ts));
  vtl_debug("\t\tMSS: %hu\n", model->src.mss);
  
  vtl_debug("\t}\n");

  vtl_debug("\tDst Host {\n");
  vtl_debug("\t\tport: %hu\n", model->dst.port);
  vtl_debug("\t\tseq: %lu\n", (unsigned long)(model->dst.seq_num));
  vtl_debug("\t\tlast ack: %lu\n", (unsigned long)(model->dst.last_ack));
  vtl_debug("\t\tWin Size: %hu\n", model->dst.win);
  vtl_debug("\t\tTimestamp: %lu\n", (unsigned long)(model->dst.ts));
  vtl_debug("\t\tMSS: %hu\n", model->dst.mss);
  vtl_debug("\t}\n");


  vtl_debug("}\n");

}



void dbg_dump_model(vtl_model_t * model) {
  if (model->type == TCP_MODEL) {
    dbg_dump_tcp_model(&(model->model.tcp_model));
  } else if (model->type == IP_MODEL) {
    dbg_dump_ip_model(&(model->model.ip_model));
  }  
}
