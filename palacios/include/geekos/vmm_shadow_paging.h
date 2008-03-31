#ifndef __VMM_SHADOW_PAGING_H
#define __VMM_SHADOW_PAGING_H


#include <geekos/vmm_paging.h>

#include <geekos/vmm_util.h>

typedef struct shadow_page_state {

  // these two reflect the top-level page directory
  // of the guest page table
  paging_mode_t           guest_mode;
  reg_ex_t                guest_cr3;         // points to guest's current page table

  // Should thi sbe here
  reg_ex_t                guest_cr0;

  // these two reflect the top-level page directory 
  // the shadow page table
  paging_mode_t           shadow_mode;
  reg_ex_t                shadow_cr3;


} shadow_page_state_t;



int init_shadow_page_state(shadow_page_state_t * state);

// This function will cause the shadow page table to be deleted
// and rewritten to reflect the guest page table and the shadow map
int wholesale_update_shadow_page_state(shadow_page_state_t * state, shadow_map_t * mem_map);




#endif
