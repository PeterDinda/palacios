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

#include <string>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <sstream>

#ifdef linux 
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(WIN32) && !defined(__CYGWIN__)

#endif

#include "vtl.h"


#define DEFAULT_LOG_FILE "./status.log"
#define DEFAULT_CONF_FILE "v3_nbd.ini"


#define DEFAULT_PORT 9500
#define MAX_STRING_SIZE 1024
#define MAX_DISKS 32

#define LOGFILE_TAG "logfile"
#define IP_ADDR_TAG "address"
#define PORT_TAG  "port"
#define DISKS_TAG "disks"


using namespace std;
//using namespace __gnu_cxx;


typedef enum {ISO, RAW} disk_type_t;

struct disk_info {
    string filename;
    string tag;
    disk_type_t type;
};

// eqstr from vtl (config.h)
typedef map<const string, struct disk_info, eqstr> disk_list_t;

struct nbd_config {
    unsigned long server_addr;
    int server_port;
    disk_list_t disks;
    int num_disks;
};



static const int enable_debug = 1;
static struct nbd_config g_nbd_conf;

void usage();
int config_nbd(string conf_file_name);
int serv_loop(int serv_sock);
void setup_disk(string disk_tag);


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
  int serv_sock;
  if (argc > 2) {
    usage();
    exit(0);
  }

  if (argc == 2) {
    config_file = string(argv[1]);
  } else {
    config_file = DEFAULT_CONF_FILE;
  }

 
  if (config_nbd(config_file) == -1) {
    cerr << "Configuration Error" << endl;
    exit(-1);
  }

  // setup network sockets

  vtl_debug("Starting Server Loop\n");
  serv_loop(serv_sock);

  return 0;
}


#ifdef linux
int serv_loop(int serv_sock) {
  fd_set all_set, read_set;
  int max_fd = -1;
  RawEthernetPacket pkt;

  FD_ZERO(&all_set);
  FD_SET(serv_sock, &all_set);
  max_fd = serv_sock;


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
    

    if (FD_ISSET(serv_sock, &read_set)) {
      //vnet_recv();


    }
  }

  return 0;
}

#elif WIN32
int serv_loop(iface_t * iface, SOCK vnet_sock, struct vnet_config * vnet_info) {
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



int config_nbd(string conf_file_name) {
    config_t config_map;
    
    if (read_config(conf_file_name, &config_map) != 0) {
	cerr << "Could not read config file..." << endl;
	return -1;
    }

    if (config_map.count(IP_ADDR_TAG) > 0) {
	g_nbd_conf.server_addr = ToIPAddress(config_map[IP_ADDR_TAG].c_str());
    } 

    if (config_map.count(PORT_TAG) > 0) {
	g_nbd_conf.server_port = atoi(config_map[PORT_TAG].c_str());
    } else {
	g_nbd_conf.server_port = DEFAULT_PORT;
    }
	
    if (config_map.count(DISKS_TAG) > 0) {
	istringstream disk_stream(config_map[DISKS_TAG], istringstream::in);
	string disk_tag;
	int i = 0;
	
	while (disk_stream >> disk_tag) {

	    if (i >= MAX_DISKS) {
		cerr << "You specified too many disks, truncating..." << endl;
		break;
	    }
	    
	    setup_disk(disk_tag);

	    i++;
	}
	
	g_nbd_conf.num_disks = i;
    } else {
	cerr << "Must specify a set of disks" << endl;
	return -1;
    }
    
    
    if (config_map.count(LOGFILE_TAG) == 0) {
	config_map[LOGFILE_TAG] = DEFAULT_LOG_FILE;
    }
    

    vtl_debug_init(config_map[LOGFILE_TAG], enable_debug);


    return 0;
}

void setup_disk(string disk_tag) {
    printf("Setting up %s\n", disk_tag.c_str());
}



void usage() {
  cout << "Usage: v3_nbd [config_file]" << endl;
  return;
}
