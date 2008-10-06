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

#include <geekos/net.h>
#include <geekos/socket.h>
#include <geekos/ne2k.h>

void Init_Network() {
  init_socket_layer();
}




void test_network() {

    uchar_t local_addr[4];
    uchar_t remote_addr[4];

    local_addr[0] = 10;
    local_addr[1] = 0;
    local_addr[2] = 2;
    local_addr[3] = 21;

    set_ip_addr(local_addr);

    remote_addr[0] = 10;
    remote_addr[1] = 0;
    remote_addr[2] = 2;
    remote_addr[3] = 20;


    connect(remote_addr, 4301);

}
