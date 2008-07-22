#ifndef __VMM_SHADOW_PAGING_H
#define __VMM_SHADOW_PAGING_H


#ifdef __V3VEE__

#include <palacios/vmm_util.h>
#include <palacios/vmm_paging.h>

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


};



struct guest_info;





int init_shadow_page_state(struct guest_info * info);

addr_t create_new_shadow_pt32(struct guest_info * info);

int handle_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code);
int handle_shadow_invlpg(struct guest_info * info);

#endif // ! __V3VEE__

#endif
