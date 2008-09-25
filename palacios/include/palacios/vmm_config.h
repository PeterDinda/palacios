/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */

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
