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

#ifndef __VMM_CONFIG_H__
#define __VMM_CONFIG_H__



#ifdef __V3VEE__

#include <palacios/vm_guest.h>




#define MAGIC_CODE 0xf1e2d3c4

struct layout_region {
  ulong_t length;
  ulong_t final_addr;
};

struct guest_mem_layout {
  ulong_t magic;
  ulong_t num_regions;
  struct layout_region regions[0];
};



int config_guest(struct guest_info * info, void * config_ptr);





#endif // ! __V3VEE__



#endif
