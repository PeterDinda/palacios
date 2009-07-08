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


#include <iostream>
#include <fstream>
#include <stdio.h>
#include <sstream>
#include <list>

#ifdef linux 
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(WIN32) && !defined(__CYGWIN__)

#endif


#include "v3_disk.h"
#include "raw.h"
#include "iso.h"

#define NBD_KEY "V3_NBD_1"


#define NBD_READ_CMD 0x1
#define NBD_WRITE_CMD 0x2
#define NBD_CAPACITY_CMD 0x3

#define NBD_STATUS_OK 0x00
#define NBD_STATUS_ERR 0xff


#define DEFAULT_LOG_FILE "./status.log"
#define DEFAULT_CONF_FILE "v3_nbd.ini"


#define DEFAULT_PORT 9500
#define MAX_STRING_SIZE 1024
#define MAX_DISKS 32

#define LOGFILE_TAG "logfile"
#define PORT_TAG  "port"
#define DISKS_TAG "disks"

// Turn on 64 bit file offset support (see 'man fseeko')
#define _FILE_OFFSET_BITS 64


using namespace std;
//using namespace __gnu_cxx;


struct eqsock {
    bool operator()(const SOCK sock1, const SOCK sock2) const {
	return sock1 == sock2;
    }
};


// Server Port that we'll listen on
int server_port;

// List of disks being served 
// eqstr from vtl (config.h)
map<const string, v3_disk *, eqstr> disks;

// List of open connections
map<const SOCK, v3_disk *, eqsock> conns;


// Enable Debugging
static const int enable_debug = 1;


void usage();
int config_nbd(string conf_file_name);
int serv_loop(int serv_sock);
void setup_disk(string disk_tag, config_t &config_map);

int handle_new_connection(SOCK new_conn);
int handle_disk_request(SOCK conn, v3_disk * disk);

int handle_capacity_request(SOCK conn, v3_disk * disk);
int handle_read_request(SOCK conn, v3_disk * disk);
int handle_write_request(SOCK conn, v3_disk * disk);

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
  SOCK serv_sock;



  if (argc > 2) {
    usage();
    exit(0);
  }

  // init global maps
  disks.clear();
  conns.clear();


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
  serv_sock = CreateAndSetupTcpSocket();
  
  if (serv_sock == -1) {
      cerr << "Could not create server socket, exiting..." << endl;
      exit(-1);
  }

  if (BindSocket(serv_sock, server_port) == -1) {
      cerr << "Could not bind socket to port: " << server_port << endl;
      exit(-1);
  }

  if (ListenSocket(serv_sock) == -1) {
      cerr << "Could not listen on server socket (port=" << server_port << ")" << endl;
      exit(-1);
  }


  vtl_debug("Starting Server Loop\n");
  serv_loop(serv_sock);

  return 0;
}


#ifdef linux
int serv_loop(int serv_sock) {
    fd_set all_set, read_set;
    int max_fd = -1;
    RawEthernetPacket pkt;


    list<SOCK> pending_cons;

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
		vtl_debug("Select returned error\n");
		perror("Select returned error: ");
		exit(-1);
	    }
	}
    

	if (FD_ISSET(serv_sock, &read_set)) {
	    SOCK conn_socket;
	    struct sockaddr_in rem_addr;
	    socklen_t addr_len = sizeof(struct sockaddr_in);
	    // new connection
	    conn_socket = accept(serv_sock, (struct sockaddr *)&rem_addr, &addr_len);

	    vtl_debug("New Connection...\n");

	    if (conn_socket < 0) {
		if (errno == EINTR) {
		    continue;
		} else {
		    vtl_debug("Accept returned error\n");
		    exit(-1);
		}
	    }

	    pending_cons.push_front(conn_socket);

	    FD_SET(conn_socket, &all_set);

	    if (conn_socket > max_fd) {
		max_fd = conn_socket;
	    }

	    if (--nready <= 0) continue;
	}

	
	// handle open connections
	for (map<SOCK, v3_disk *, eqsock>::iterator con_iter = conns.begin();
	     con_iter != conns.end(); ) {
	    SOCK tmp_sock = con_iter->first;
	    v3_disk * tmp_disk = con_iter->second;

	    if (FD_ISSET(con_iter->first, &read_set)) {

		if (handle_disk_request(tmp_sock, tmp_disk) == -1) {
		    vtl_debug("Error: Could not complete disk request\n");

		    map<SOCK, v3_disk *, eqsock>::iterator tmp_iter = con_iter;
		    con_iter++;

		    tmp_disk->detach();


		    FD_CLR(tmp_sock, &all_set);
		    close(tmp_sock);

		    conns.erase(tmp_iter);
		} else {
		    con_iter++;
		}

		if (--nready <= 0) break;
	    }
	}

	if (nready <= 0) continue;

	// check pending connections
	for (list<SOCK>::iterator pending_iter = pending_cons.begin();
	     pending_iter != pending_cons.end();) {
	    
	    if (FD_ISSET(*pending_iter, &read_set)) {
		if (handle_new_connection(*pending_iter) == -1) {
		    // error
		    vtl_debug("Error: Could not connect to disk\n");
		    FD_CLR(*pending_iter, &all_set);
		}
		list<SOCK>::iterator tmp_iter = pending_iter;
		pending_iter++;

		pending_cons.erase(tmp_iter);
		
		if (--nready <= 0) break;
	    } else {
		pending_iter++;
	    }
	}
	
	if (nready <= 0) continue;
	


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


// byte 1: command (read = 1, write = 2, capacity = 3)
// byte 2 - 4: zero
int handle_disk_request(SOCK conn, v3_disk * disk) {
    char buf[4];

    int read_len = Receive(conn, buf, 4, true);
    
    if (read_len == 0) {
	vtl_debug("Detaching from disk (conn=%d)\n", conn);
	return -1;
    }

    if (read_len == -1) {
	vtl_debug("Could not read command\n");
	return -1;
    }

    if ((buf[1] != 0) || (buf[2] != 0) || (buf[3] != 0)) {
	// error
	vtl_debug("Invalid command padding\n");
	return -1;
    }

    switch (buf[0]) {
	case NBD_CAPACITY_CMD:
	    return handle_capacity_request(conn, disk);
	case NBD_READ_CMD:
	    return handle_read_request(conn, disk);
	case NBD_WRITE_CMD:
	    return handle_write_request(conn, disk);
	default:
	    vtl_debug("Invalid Disk Command %d\n", buf[0]);
	    return -1;
    }

    return 0;
}


// send: 
//    8 bytes : capacity
int handle_capacity_request(SOCK conn, v3_disk * disk) {
    off_t capacity = disk->get_capacity();

    vtl_debug("Returing capacity %d\n", capacity);

    return Send(conn, (char *)&capacity, 8, true);
}

// receive:
//    8 bytes : offset
//    4 bytes : length
// send:
//    1 byte  : status
//    4 bytes : return length
//    x bytes : data
int handle_read_request(SOCK conn, v3_disk * disk) {
    off_t offset = 0;
    unsigned int length = 0;
    unsigned char * buf = NULL;
    unsigned int ret_len = 0;
    unsigned char status = NBD_STATUS_OK;

    vtl_debug("Read Request\n");



    if (Receive(conn, (char *)&offset, 8, true) <= 0) {
	vtl_debug("Error receiving read offset\n");
	return -1;
    }

    vtl_debug("Read Offset %d\n", offset);

    if (Receive(conn, (char *)&length, 4, true) <= 0) {
	vtl_debug("Error receiving read length\n");
	return -1;
    }

    vtl_debug("Read length: %d\n", length);

    buf = new unsigned char[length];
    
    ret_len = disk->read(buf, offset, length);

    vtl_debug("Read %d bytes from source disk\n", ret_len);

    if (ret_len == 0) {
	vtl_debug("Read Error\n");
	status = NBD_STATUS_ERR;
    }

    vtl_debug("Sending Status byte (%d)\n", status);

    if (Send(conn, (char *)&status, 1, true) <= 0) {
	vtl_debug("Error Sending Read Status\n");
	return -1;
    }

    vtl_debug("Sending Ret Len: %d\n", ret_len);

    if (Send(conn, (char *)&ret_len, 4, true) <= 0) {
	vtl_debug("Error Sending Read Length\n");
	return -1;
    }



    if (ret_len > 0) {
	vtl_debug("Sending Data\n");

	SetNoDelaySocket(conn, false);

	if (Send(conn, (char *)buf, ret_len, true)  <= 0) {
	    vtl_debug("Error sending Read Data\n");
	    return -1;
	}

	SetNoDelaySocket(conn, true);
    }

    vtl_debug("Read Complete\n");

    delete buf;

    return 0;
}

// receive: 
//    8 bytes : offset
//    4 bytes : length
//    x bytes : data
// send : 
//    1 bytes : status
int handle_write_request(SOCK conn, v3_disk * disk) {
    off_t offset = 0;
    unsigned int length = 0;
    unsigned char * buf = NULL;
    unsigned int ret_len = 0;
    unsigned char status = NBD_STATUS_OK;

    vtl_debug("Write Request\n");
    
    if (Receive(conn, (char *)&offset, 8, true) <= 0) {
	vtl_debug("Error receiving write offset\n");
	return -1;
    }

    vtl_debug("Write Offset %d\n", offset);

    if (Receive(conn, (char *)&length, 4, true) <= 0) {
	vtl_debug("Error receiving write length\n");
	return -1;
    }

    vtl_debug("Write length: %d\n", length);

    buf = new unsigned char[length];
    
    vtl_debug("Receiving Data\n");

    if (Receive(conn, (char *)buf, length, true)  <= 0) {
	vtl_debug("Error receiving Write Data\n");
	return -1;
    }

    vtl_debug("Wrote %d bytes to source disk\n", ret_len);

    if (disk->write(buf, offset, length) != length) {
	vtl_debug("Write Error\n");
	status = NBD_STATUS_ERR;
    }

    vtl_debug("Sending Status byte (%d)\n", status);

    if (Send(conn, (char *)&status, 1, true) <= 0) {
	vtl_debug("Error Sending Wrte Status\n");
	return -1;
    }

    vtl_debug("Write Complete\n");

    delete buf;

    return 0;
}


/* Negotiation:
 * <NBD_KEY> <Disk Tag>\n
 */

int handle_new_connection(SOCK new_conn) {
    string input;
    string key_str;
    string tag_str;
    v3_disk * disk = NULL;

    GetLine(new_conn, input);

    vtl_debug("New Connection: %s\n", input.c_str());

    {
	istringstream is(input, istringstream::in);
	is >> key_str >> tag_str;
    }

    if (key_str != NBD_KEY) {
	vtl_debug("Error: Invalid NBD key string (%s)\n", key_str.c_str());
	return -1;
    }

    if (disks.count(tag_str) == 0) {
	vtl_debug("Error: Requesting disk that does not exist (%s)\n", tag_str.c_str());
	return -1;
    }

    disk = disks[tag_str];

    if (!disk) {
	vtl_debug("Disk (%s) Does not exist\n", tag_str.c_str());
	return -1;
    }

    if (disk->locked == 1) {
	vtl_debug("Attempting to attach to a device already in use\n");
	return -1;
    }

    conns[new_conn] = disk;

    disk->attach();


    vtl_debug("Connected to disk %s\n", tag_str.c_str());

    return 0;
}


int config_nbd(string conf_file_name) {
    config_t config_map;
    
    if (read_config(conf_file_name, &config_map) != 0) {
	cerr << "Could not read config file..." << endl;
	return -1;
    }

    if (config_map.count(LOGFILE_TAG) == 0) {
	config_map[LOGFILE_TAG] = DEFAULT_LOG_FILE;
    }

    vtl_debug_init(config_map[LOGFILE_TAG], enable_debug);


    if (config_map.count(PORT_TAG) > 0) {
	server_port = atoi(config_map[PORT_TAG].c_str());
    } else {
	server_port = DEFAULT_PORT;
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
	    
	    setup_disk(disk_tag, config_map);
	    i++;
	}	
    } else {
	cerr << "Must specify a set of disks" << endl;
	return -1;
    }
    

    return 0;
}

void setup_disk(string disk_tag, config_t &config_map) {
    string file_tag = disk_tag +  ".file";
    string type_tag = disk_tag + ".type";
    
    v3_disk * disk;
    string type;


    cout << "Setting up " << disk_tag.c_str() << endl;

    if ((config_map.count(file_tag) == 0) && 
	(config_map.count(type_tag) == 0)) {
	cerr << "Missing Disk configuration directive for " << disk_tag << endl;
    }

    type = config_map[type_tag];  

    if (type == "RAW") {
	disk = new raw_disk(config_map[file_tag]);
    } else if (type == "ISO") {
	vtl_debug("Setting up ISO\n");
	disk = new iso_image(config_map[file_tag]);
    }

    disks[disk_tag] = disk;

    return;
}






void usage() {
  cout << "Usage: v3_nbd [config_file]" << endl;
  return;
}
