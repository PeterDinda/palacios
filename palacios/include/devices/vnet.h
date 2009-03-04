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


#ifndef __DEVICES_VNET_H__
#define __DEVICES_VNET_H__

#ifdef __V3VEE__

#include <palacios/vmm_socket.h>

struct v3_vnet {
    int vnet_sock;
    void * private_data;
};

int v3_init_vnet(struct v3_vnet * vnet, 
		 int (*rx_pkt)(),  //Fix this...
		 uint32_t ip, uint16_t port, 
		 void * private_data);

int v3_send_vnet_pkt(struct v3_vnet * vnet, uchar_t * pkt, uint_t pkt_len);

#endif

#endif
