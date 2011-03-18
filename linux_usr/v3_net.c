/* 
 * V3 Control utility for Palacios network services
 * (c) Lei Xia, 2010
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/ioctl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <string.h>
 
#include "v3_ctrl.h"

struct v3_network {
    unsigned char socket;
    unsigned char packet;
    unsigned char vnet;
};

int main(int argc, char* argv[]) {
    int v3_fd = 0;
    struct v3_network net;
    int i;

    if (argc <= 1) {
	printf("Usage: ./v3_mem [socket] [packet] [vnet]\n");
	return -1;
    }

    for (i = 1; i < argc; i++){
	if(!strcasecmp (argv[i], "packet")){
	    net.packet = 1;
	}else if(!strcasecmp (argv[i], "socket")){
	    net.socket = 1;
	}else if(!strcasecmp (argv[i], "vnet")){
	    net.vnet = 1;
	}else {
	    printf("unknown v3 network service: %s, ignored\n", argv[i]);
	}
    }

    printf("Network service: socket: %d, packet: %d, vnet: %d\n", net.socket, net.packet, net.vnet);

    v3_fd = open(v3_dev, O_RDONLY);

    if (v3_fd == -1) {
	printf("Error opening V3Vee control device\n");
	return -1;
    }

    ioctl(v3_fd, V3_START_NETWORK, &net); 


    /* Close the file descriptor.  */ 
    close(v3_fd); 
 

    return 0; 
} 

