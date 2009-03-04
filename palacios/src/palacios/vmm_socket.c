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


#include <palacios/vmm_socket.h>
#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_stddef.h>


struct v3_socket_hooks * sock_hooks = 0;

void V3_Init_Sockets(struct v3_socket_hooks * hooks) {
    PrintInfo("Initializing Socket Interface\n");
    sock_hooks = hooks;
    PrintDebug("V3 sockets inited\n");

    return;
}



