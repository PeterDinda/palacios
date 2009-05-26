#include "if.h"
#include <assert.h>

iface_t *  if_connect(string if_name, char mode) {
  char pcap_errbuf[PCAP_ERRBUF_SIZE];

  iface_t * iface = (iface_t *)malloc(sizeof(iface_t));
  iface->name = new string();

  cout << "device name : " << if_name << endl;

  *(iface->name) = if_name;
  iface->mode = mode;



  // mode is relevant only under linux
#ifdef linux 
  if (mode & IF_RD) {
    if ((iface->pcap_interface = pcap_open_live((char*)if_name.c_str(), 65536, 1, 1, pcap_errbuf)) == NULL) {
      vtl_debug("Could not initialize pcap\n");
      return NULL;
    }

    iface->pcap_fd = pcap_fileno(iface->pcap_interface);
  }

  if (mode & IF_WR) {
    char libnet_errbuf[LIBNET_ERRORBUF_SIZE];
    
    if ((iface->net_interface = libnet_init(LIBNET_LINK_ADV, (char *)if_name.c_str(), libnet_errbuf)) == NULL) {
      vtl_debug("Could not initialize libnet\n");
      return NULL;
    }
  }

#elif defined(WIN32) 
  if ((iface->pcap_interface = pcap_open_live((char*)if_name.c_str(), 65536, 1, 1, pcap_errbuf)) == NULL) {
    vtl_debug("Could not initialize pcap\n");
    return NULL;
  }
  
  pcap_setmintocopy(iface->pcap_interface, 40);	
  iface->pcap_event = pcap_getevent(iface->pcap_interface);
#endif

  return iface;
}

void if_disconnect(iface_t * iface) {
  free(iface->name);
  pcap_close(iface->pcap_interface);

}

#ifdef WIN32
HANDLE if_get_event(iface_t * iface) {
  return iface->pcap_event;
  //  return pcap_getevent(iface->pcap_interface);
}
#endif

#ifdef linux
int if_get_fd(iface_t * iface) {
  return iface->pcap_fd;
}
#endif


int if_loop(iface_t * iface, RawEthernetPacket * pkt) {
  int ret;

  ret = pcap_loop(iface->pcap_interface, 1, pkt_handler, (u_char*)pkt);

  if (ret == 0) {
    return IF_PACKET;
  } else if (ret == -2) {
    return IF_BREAK;
  } else if (ret == -1) {
    return IF_CONT;
  } else {
    return -1;
  }
}

void if_break_loop(iface_t * iface) {
  pcap_breakloop(iface->pcap_interface);
}

void pkt_handler(u_char * pkt, const struct pcap_pkthdr * pkt_header, const u_char * pkt_data) {
  RawEthernetPacket pkt2((const char *)pkt_data, (unsigned)(pkt_header->len));
  *(RawEthernetPacket *)pkt = pkt2;
  ((RawEthernetPacket*)pkt)->set_type("et");
}


int if_read_pkt(iface_t * iface, RawEthernetPacket * pkt) {
  struct pcap_pkthdr header;
  const u_char * pcap_pkt;

  pcap_pkt = pcap_next(iface->pcap_interface, &header);

  if (pcap_pkt == NULL) {
    return -1;
  }

  RawEthernetPacket pkt2((const char *)pcap_pkt, (unsigned)(header.len));
  *pkt = pkt2;

  pkt->set_type("et");

  return 0;
}



int if_write_pkt(iface_t * iface, RawEthernetPacket * pkt) {
  assert((iface != NULL) && (pkt != NULL) && (iface->net_interface != NULL));

#ifdef linux
  vtl_debug("Writing pkt size(%lu)\n", pkt->get_size());
  if (libnet_adv_write_link(iface->net_interface, 
			    (u_char *)(pkt->get_data()), 
			    pkt->get_size()) < 0) {
    vtl_debug("Libnet could not inject packet size (%lu)\n", pkt->get_size());
    return -1;
  }

#elif defined(WIN32)
  if (pcap_sendpacket(iface->pcap_interface, 
		      (u_char *)(pkt->get_data()),
		      pkt->get_size()) < 0) {
    vtl_debug("PCAP could not inject packet\n");
    return -1;
  }

#endif

  return 0;
}

int if_setup_filter(iface_t * iface, string bpf_str) {
  struct bpf_program fcode;
  bpf_u_int32 netmask; 
  bpf_u_int32 network;
  char errbuf[PCAP_ERRBUF_SIZE];
  char * filter_buf;
	

  filter_buf = (char *)malloc(bpf_str.length());
  strcpy(filter_buf, bpf_str.c_str());
  cout << "Setting Getting interface info for " << iface->name << endl;
  if (pcap_lookupnet(iface->name->c_str(), &network, &netmask, errbuf) == -1) {
    vtl_debug("Error looking up the network info\n");
    return -1;
  }

  netmask=0xffffffff;
  cout << bpf_str << endl;
  if (pcap_compile(iface->pcap_interface, &fcode, filter_buf, 1, netmask) < 0) { 
    vtl_debug("Could not compile bpf filter\n");
    return -1; 
  } 
  
  if (pcap_setfilter(iface->pcap_interface, &fcode) < 0) { 
    vtl_debug("Could not insert bpf filter\n");
    return -1; 
  } 

  return 0;
}
