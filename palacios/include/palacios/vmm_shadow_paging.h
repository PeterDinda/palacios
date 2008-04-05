#ifndef __VMM_SHADOW_PAGING_H
#define __VMM_SHADOW_PAGING_H



#include <palacios/vmm_util.h>



#include <palacios/vmm_paging.h>

struct shadow_page_state {

  // these two reflect the top-level page directory
  // of the guest page table
  paging_mode_t           guest_mode;
  reg_ex_t                guest_cr3;         // points to guest's current page table

  // Should this be here??
  reg_ex_t                guest_cr0;

  // these two reflect the top-level page directory 
  // the shadow page table
  paging_mode_t           shadow_mode;
  reg_ex_t                shadow_cr3;


};






struct guest_info;


int init_shadow_page_state(struct shadow_page_state * state);

// This function will cause the shadow page table to be deleted
// and rewritten to reflect the guest page table and the shadow map
int wholesale_update_shadow_page_state(struct guest_info * guest_info);




#endif
