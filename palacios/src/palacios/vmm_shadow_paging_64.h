
static inline int activate_shadow_pt_64(struct guest_info * info) {
  struct cr3_64 * shadow_cr3 = (struct cr3_64 *)&(info->ctrl_regs.cr3);
  struct cr3_64 * guest_cr3 = (struct cr3_64 *)&(info->shdw_pg_state.guest_cr3);
  addr_t shadow_pt = 0;
  
  shadow_pt = create_new_shadow_pt(info);

  shadow_cr3->pml4t_base_addr = (addr_t)V3_PAddr((void *)(addr_t)PAGE_BASE_ADDR_4KB(shadow_pt));
  PrintDebug("Creating new 64 bit shadow page table %p\n", (void *)BASE_TO_PAGE_ADDR(shadow_cr3->pml4t_base_addr))

  
  shadow_cr3->pwt = guest_cr3->pwt;
  shadow_cr3->pcd = guest_cr3->pcd;

  return 0;
}






/* 
 * *
 * * 
 * * 64 bit Page table fault handlers
 * *
 * *
 */
/*
static int handle_2MB_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code,
				       pte64_t * shadow_pt, pde64_2MB_t * large_guest_pde);

static int handle_pte_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code,
				       pte64_t * shadow_pt, pte64_t * guest_pt);

static int handle_pde_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code,
				       pde64_t * shadow_pd, pde64_t * guest_pd);

static int handle_pdpe_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code,
				       pdpe64_t * shadow_pdp, pdpe64_t * guest_pdp);
*/

static inline int handle_shadow_pagefault_64(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  /*  pml4e64_t * guest_pml = NULL;
  pml4e64_t * shadow_pml = CR3_TO_PML4E64_VA(info->ctrl_regs.cr3);
  addr_t guest_cr3 = CR3_TO_PML4E64_PA(info->shdw_pg_state.guest_cr3);
  pt_access_status_t guest_pml_access;
  pt_access_status_t shadow_pml_access;
  pml4e64_t * guest_pmle = NULL;
  //  pml4e64_t * shadow_pmle = ;
  */
  
  

  PrintError("64 bit shadow paging not implemented\n");
  return -1;
}


static inline int handle_shadow_invlpg_64(struct guest_info * info, addr_t vaddr) {
  PrintError("64 bit shadow paging not implemented\n");
  return -1;
}
