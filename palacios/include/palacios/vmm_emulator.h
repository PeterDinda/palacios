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

#ifndef __VMM_EMULATOR_H__
#define __VMM_EMULATOR_H__

#ifdef __V3VEE__

#include <palacios/vmm_list.h>
#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_paging.h>





struct emulated_page {
  addr_t page_addr;
  addr_t va;
  pte32_t pte;
  struct list_head page_list;
};

struct saved_page {
  addr_t va;
  pte32_t pte;
  struct list_head page_list;
};


struct write_region {
  void * write_data;
  
  uint_t length;
  int (*write)(addr_t write_addr, void * src, uint_t length, void * priv_data);
  addr_t write_addr;
  void * private_data;
  
  struct list_head write_list;
};


struct emulation_state {
  uint_t num_emulated_pages;
  struct list_head emulated_pages;

  uint_t num_saved_pages;
  struct list_head saved_pages;

  uint_t num_write_regions;
  struct list_head write_regions;

  uint_t running : 1;
  uint_t instr_length;

  uint_t tf_enabled : 1;
};


int v3_init_emulator(struct guest_info * info);


int v3_emulation_exit_handler(struct guest_info * info);

int v3_emulate_memory_write(struct guest_info * info, addr_t fault_gva,
			    int (*write)(addr_t write_addr, void * src, uint_t length, void * priv_data), 
			    addr_t write_addr, void * private_data);
int v3_emulate_memory_read(struct guest_info * info, addr_t fault_gva, 
			   int (*read)(addr_t read_addr, void * dst, uint_t length, void * priv_data), 
			   addr_t read_addr, void * private_data);


#endif // !__V3VEE__

#endif
