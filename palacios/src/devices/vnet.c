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

#include <devices/vnet.h>
#include <palacios/vmm.h>

static int vnet_server(void * arg) {
    // select loop
}

int v3_init_vnet(struct v3_vnet * vnet, 
		 int (*rx_pkt)(), // fix this...
		 uint32_t ip, uint16_t port, 
		 void * private_data) {
    // connect

    // setup listener
    V3_CREATE_THREAD(vnet_server, NULL, "VNET Server");

    return -1;
}

int v3_send_vnet_pkt(struct v3_vnet * vnet, uchar_t * pkt, uint_t pkt_len) {
    
    return -1;
}
