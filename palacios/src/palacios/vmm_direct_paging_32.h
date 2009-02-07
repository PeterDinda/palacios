#ifndef __VMM_DIRECT_PAGING_32_H__
#define __VMM_DIRECT_PAGING_32_H__

#include <palacios/vmm_mem.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vm_guest.h>

static pde32_t * create_pde32() {
  void * pde = 0;
  pde = V3_VAddr(V3_AllocPages(1));
  memset(pde, 0, PAGE_SIZE);

  return (pde32_t *) pde;
}


static pte32_t * create_pte32() {
  void * pte = 0;
  pte = V3_VAddr(V3_AllocPages(1));
  memset(pte, 0, PAGE_SIZE);

  return (pte32_t *) pte;
}


static inline pde32_t * create_direct_passthrough_pts_32(struct guest_info * info) {
  return create_pde32();
}


static inline int handle_passthrough_pagefault_32(struct guest_info * info, 
							   addr_t fault_addr, 
							   pf_error_t error_code) {
  // Check to see if pde and pte exist (create them if not)
  pde32_t * pde = CR3_TO_PDE32_VA(info->ctrl_regs.cr3);
  int pde_index = PDE32_INDEX(fault_addr);
  int pte_index = PTE32_INDEX(fault_addr);

  if (pde[pde_index].present != 1) {

    PrintError("Creating new page table for PTE index: %d\n", pde_index);

    pte32_t * pte = create_pte32();
    addr_t host_addr;

    if(guest_pa_to_host_pa(info, fault_addr, &host_addr) == -1) return -1;

    struct v3_shadow_region * region =  v3_get_shadow_region(info, PAGE_BASE_ADDR(host_addr));

    pte[pte_index].present = 1;
    pte[pte_index].writable = 1;
    pte[pte_index].user_page = 1;
    pte[pte_index].page_base_addr = PAGE_BASE_ADDR(host_addr);
    
    pde[pde_index].present = 1;
    pde[pde_index].writable = 1;
    pde[pde_index].user_page = 1;
    pde[pde_index].pt_base_addr = PAGE_BASE_ADDR((addr_t)V3_PAddr(pte));
    
    if (region->host_type == SHDW_REGION_WRITE_HOOK) {
      pte[pte_index].writable = 0;
    }
    
    PrintError("Fault Addr: 0x%p\nHost Addr: 0x%p\n", (void*)fault_addr, (void*)host_addr);
    
  } else {
    pte32_t * pte = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pde[pde_index].pt_base_addr));
    
    if (pte[pte_index].present != 1) {
      addr_t host_addr;
      
      if (guest_pa_to_host_pa(info, fault_addr, &host_addr) == -1) return -1;
      
      struct v3_shadow_region * region =  v3_get_shadow_region(info, PAGE_BASE_ADDR(host_addr));
      
      pte[pte_index].present = 1;
      pte[pte_index].writable = 1;
      pte[pte_index].user_page = 1;
      pte[pte_index].page_base_addr = PAGE_BASE_ADDR(host_addr);
      
      if (region->host_type == SHDW_REGION_WRITE_HOOK) {
	pte[pte_index].writable = 0;
      }
    }
  }
  return 0;
}


#endif
