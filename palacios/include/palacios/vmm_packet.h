/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Lei Xia <lxia@northwestern.edu> 
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu> 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMM_PACKET_H__
#define __VMM_PACKET_H__

#include <palacios/vmm.h>

#ifdef __V3VEE__

#define V3_send_Raw(pkt, size, data) ({				\
	    extern struct v3_packet_hooks * packet_hooks;		\
	    int ret;									\
	    if ((packet_hooks) && (packet_hooks)->send) {	\
		ret = (packet_hooks)->send(pkt,size,data);		\
	    }							\
	    ret;						\
	})

#endif


struct v3_packet_hooks {

    int (*send)(const char * pkt, const int size, void * private_data);

};


extern void V3_Init_Packet(struct v3_packet_hooks * hooks);

#endif
