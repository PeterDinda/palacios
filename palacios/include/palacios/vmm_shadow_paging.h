#ifndef __VMM_SHADOW_PAGING_H
#define __VMM_SHADOW_PAGING_H


#ifdef __V3VEE__

#include <palacios/vmm_util.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm_hashtable.h>

struct shadow_page_state {

  // these two reflect the top-level page directory
  // of the guest page table
  paging_mode_t           guest_mode;
  ullong_t                guest_cr3;         // points to guest's current page table

  // Should this be here??
  ullong_t guest_cr0;

  // these two reflect the top-level page directory 
  // of the shadow page table
  paging_mode_t           shadow_mode;
  ullong_t                shadow_cr3;


  // Hash table that ties a CR3 value to a hash table pointer for the PT entries
  struct hashtable *  cr3_cache;
  // Hash table that contains a mapping of guest pte addresses to host pte addresses
  struct hashtable *  cached_ptes;
  addr_t cached_cr3;

};



struct guest_info;



int cache_page_tables32(struct guest_info * info, addr_t  pde);

int init_shadow_page_state(struct guest_info * info);

addr_t create_new_shadow_pt32();

int handle_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code);
int handle_shadow_invlpg(struct guest_info * info);




int v3_replace_shdw_page(struct guest_info * info, addr_t location, void * new_page, void* old_page);
int v3_replace_shdw_page32(struct guest_info * info, addr_t location, pte32_t * new_page, pte32_t * old_page); 

#endif // ! __V3VEE__

#endif
