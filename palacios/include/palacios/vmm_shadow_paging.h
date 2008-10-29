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

#ifndef __VMM_SHADOW_PAGING_H__
#define __VMM_SHADOW_PAGING_H__


#ifdef __V3VEE__

#include <palacios/vmm_util.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm_hashtable.h>


struct shadow_page_state {

  // virtualized control registers
  v3_reg_t guest_cr3;
  v3_reg_t guest_cr0;

  // these two reflect the top-level page directory 
  // of the shadow page table
  // v3_reg_t                shadow_cr3;


  // Hash table that ties a CR3 value to a hash table pointer for the PT entries
  struct hashtable *  cr3_cache;
  // Hash table that contains a mapping of guest pte addresses to host pte addresses
  struct hashtable *  cached_ptes;
  addr_t cached_cr3;

};



struct guest_info;





int v3_init_shadow_page_state(struct guest_info * info);



int v3_handle_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code);
int v3_handle_shadow_invlpg(struct guest_info * info);


int v3_activate_shadow_pt(struct guest_info * info);


/* TODO: Change to static functions
 * External visibility not needed
 */
addr_t v3_create_new_shadow_pt();
int v3_cache_page_tables32(struct guest_info * info, addr_t  pde);
int v3_replace_shdw_page32(struct guest_info * info, addr_t location, pte32_t * new_page, pte32_t * old_page); 
/* *** */


int v3_replace_shdw_page(struct guest_info * info, addr_t location, void * new_page, void * old_page);

#endif // ! __V3VEE__

#endif
