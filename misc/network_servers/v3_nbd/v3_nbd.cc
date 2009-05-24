/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <sstream>



nbd_config_t g_nbd_conf;

using namespace std;
//using namespace __gnu_cxx;



config_t g_config;

int __main (int argc, char ** argv);

#ifdef linux

int main(int argc, char ** argv) {
  return __main(argc, argv);
}

#elif WIN32

void main() {
  __main(0, NULL);
}

#endif 

int __main (int argc, char ** argv) {
  string config_file;
  SOCK vnet_sock = 0;
  struct vnet_config vnet_info;
  iface_t * iface;
  if (argc > 2) {
    usage();
    exit(0);
  }

  if (argc == 2) {
    config_file = string(argv[1]);
  } else {
    config_file = VIDS_CONF_FILE;
  }


  int * foo;
  int num_ports = GetOpenUdpPorts(&foo);
  int i;
  for (i = 0; i < num_ports; i++) {
    printf("port %d open\n", foo[i]);
  }
  

  //  g_conf.log_file = "./vids.log";

  if (config_vids(config_file) == -1) {
    cerr << "Configuration Error" << endl;
    exit(-1);
  }

  // JRL DEBUG
  debug_init(g_config[LOGFILE_TAG].c_str());
  JRLDBG("testing...\n");



  // Configure pcap filter...

  vids_loop(iface, vnet_sock, &vnet_info);

  return 0;
}


#ifdef linux
int vids_loop(iface_t * iface, SOCK vnet_sock, struct vnet_config * vnet_info) {
  fd_set all_set, read_set;
  int max_fd = -1;
  RawEthernetPacket pkt;

  FD_ZERO(&all_set);
  FD_SET(vnet_sock, &all_set);
  max_fd = vnet_sock;


  while (1) {
    int nready = 0;
    read_set = all_set;
    nready = select(max_fd + 1, &read_set, NULL, NULL, NULL);
    
    
    if (nready == -1) {
      if (errno == EINTR) {
	continue;
      } else {
	perror("Select returned error: ");
	break;
      }
    }
    

    if (FD_ISSET(vnet_sock, &read_set)) {
      //vnet_recv();
      

    }
  }

  return 0;
}

#elif WIN32
int vids_loop(iface_t * iface, SOCK vnet_sock, struct vnet_config * vnet_info) {
  int ret;
  RawEthernetPacket pkt;
  WSANETWORKEVENTS net_events;
  WSAEVENT events[2];
  DWORD event_i;
  
  events[VNET_EVENT] = WSACreateEvent();
  
  WSAEventSelect(vnet_sock, events[VNET_EVENT], FD_READ | FD_CLOSE);
  events[IF_EVENT] = if_get_event(iface);
  
  while (1) {
    event_i = WSAWaitForMultipleEvents(2, events, false, WSA_INFINITE, false);
    cout << "Wait returned" << endl;
    
    if (event_i == WAIT_FAILED) {
      cout << "ERROR: " <<   GetLastError() << endl;
      exit(-1);
    }
    event_i -= WAIT_OBJECT_0;
    
    if (event_i == VNET_EVENT) {
      
      if (WSAEnumNetworkEvents(vnet_sock, events[event_i], &net_events) == SOCKET_ERROR) {
	cout << "EnumEventsError: " << WSAGetLastError() << endl;
	exit(-1);
      }
      if (net_events.lNetworkEvents & FD_READ) {
	
	JRLDBG("Receied VNET Packet\n");
	// we received data
	
	if (vnet_info->link_type == TCP_LINK) {
	  pkt.Unserialize(vnet_sock);
	} else if (vnet_info->link_type == UDP_LINK) {
	  pkt.UdpUnserialize(vnet_sock);
	}
	
	process_outbound_pkt(&pkt);
	
	if_write_pkt(iface, &pkt);
	
      } else if (net_events.lNetworkEvents & FD_CLOSE) {
	CLOSE(vnet_sock);
	return 0;
      }
      
    }
  }
  
  return 0;
}
#endif



int config_vids(string conf_file_name) {
  if (read_config(conf_file_name, &g_config) != 0) {
    return -1;
  }

  if (g_config.count(VIDS_SERVER_TAG) > 0) {
    g_vids_conf.server_addr = ToIPAddress(g_config[VIDS_SERVER_TAG].c_str());
  } else {
    printf("Must specify VIDS server address\n");
    return -1;
  }

  if (g_config.count(VIDS_SERVER_PORT_TAG) > 0) {
    g_vids_conf.server_port = atoi(g_config[VIDS_SERVER_PORT_TAG].c_str());
  } else {
    printf("Must specify VIDS server port\n");
    return -1;
  }

  if (g_config.count(TCP_PORTS_TAG) > 0) {
    istringstream port_stream(g_config[TCP_PORTS_TAG], istringstream::in);
    int port;
    int i = 0;
    
    while (port_stream >> port) {
      if (i >= MAX_PORTS) {
	cerr << "You specified too many ports to forward, truncating..." << endl;
	break;
      }
      
      g_vids_conf.tcp_ports[i] = port;      
      i++;
    }

    g_vids_conf.num_tcp_ports = i;
  }



  if (g_config.count(VIRTUAL_MAC_TAG) > 0) {
   string_to_mac(g_config[VIRTUAL_MAC_TAG].c_str(), g_vids_conf.virtual_mac);
  }

  if (g_config.count(LOGFILE_TAG) == 0) {
    g_config[LOGFILE_TAG] = DEFAULT_LOG_FILE;
  }

  if (GetLocalMacAddress(g_config[INTERFACE_TAG], g_vids_conf.local_mac) == -1) {
    cerr << "Could not get local mac address" << endl;
    return -1;
  }


  return 0;
}


int read_config(string conf_file_name) {
  fstream conf_file(conf_file_name.c_str(), ios::in);
  char line[MAX_STRING_SIZE];

  while ((conf_file.getline(line, MAX_STRING_SIZE))) {
    string conf_line = line;
    string tag;
    string value;
    int offset, ltrim_index, rtrim_index;

    if (conf_line[0] == '#') {
      continue;
    }

    offset = conf_line.find(":", 0);
    tag = conf_line.substr(0,offset);

    // kill white space
    istringstream tag_stream(tag, istringstream::in);
    tag_stream >> tag;

    if (tag.empty()) {
      continue;
    }

    // basic whitespace trimming, we assume that the config handlers will deal with 
    // tokenizing and further formatting
    value = conf_line.substr(offset + 1, conf_line.length() - offset);
    ltrim_index = value.find_first_not_of(" \t");
    rtrim_index = value.find_last_not_of(" \t");
    value = value.substr(ltrim_index, (rtrim_index + 1) - ltrim_index);

    g_config[tag] = value;
  }
  return 0;
}


void usage() {
  cout << "Usage: vids [config_file]" << endl;
  return;
}
