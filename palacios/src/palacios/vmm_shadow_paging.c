#include <palacios/vmm_shadow_paging.h>


#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>

extern struct vmm_os_hooks * os_hooks;


int init_shadow_page_state(struct shadow_page_state * state) {
  state->guest_mode = PDE32;
  state->shadow_mode = PDE32;
  
  state->guest_cr3 = 0;
  state->shadow_cr3 = 0;

  return 0;
}

int handle_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  if (info->cpu_mode == PROTECTED_PG) {
    return handle_shadow_pagefault32(info, fault_addr, error_code);
  } else {
    return -1;
  }
}


int handle_shadow_pagefault32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  pde32_t * guest_pde = NULL;
  pde32_t * shadow_pde = (pde32_t *)CR3_TO_PDE32(info->shdw_pg_state.shadow_cr3);
  addr_t guest_cr3 = CR3_TO_PDE32(info->shdw_pg_state.guest_cr3);

  if (guest_pa_to_host_va(info, guest_cr3, (addr_t*)&guest_pde) == -1) {
    return -1;
  }

  if (error_code.present == 0) {
    // Faulted because page was not present...
    if (shadow_pde[PDE32_INDEX(fault_addr)].present) {
      
      
    } else {
      return -1;
    }    
  }

  // Checks:
  // Shadow PDE
  // Guest PDE
  // Shadow PTE
  // Guest PTE
  // Mem Map
  
  return -1;
}


addr_t create_new_shadow_pt32(struct guest_info * info) {
  void * host_pde = 0;

  V3_AllocPages(host_pde, 1);
  memset(host_pde, 0, PAGE_SIZE);

  return (addr_t)host_pde;
}




addr_t setup_shadow_pt32(struct guest_info * info, addr_t virt_cr3) {
  addr_t cr3_guest_addr = CR3_TO_PDE32(virt_cr3);
  pde32_t * guest_pde;
  pde32_t * host_pde = NULL;
  int i;
  
  // Setup up guest_pde to point to the PageDir in host addr
  if (guest_pa_to_host_va(info, cr3_guest_addr, (addr_t*)&guest_pde) == -1) {
    return 0;
  }
  
  V3_AllocPages(host_pde, 1);
  memset(host_pde, 0, PAGE_SIZE);

  for (i = 0; i < MAX_PDE32_ENTRIES; i++) {
    if (guest_pde[i].present == 1) {
      addr_t pt_host_addr;
      addr_t host_pte;

      if (guest_pa_to_host_va(info, PDE32_T_ADDR(guest_pde[i]), &pt_host_addr) == -1) {
	return 0;
      }

      if ((host_pte = setup_shadow_pte32(info, pt_host_addr)) == 0) {
	return 0;
      }

      host_pde[i].present = 1;
      host_pde[i].pt_base_addr = PD32_BASE_ADDR(host_pte);

      //
      // Set Page DIR flags
      //
    }
  }

  PrintDebugPageTables(host_pde);

  return (addr_t)host_pde;
}



addr_t setup_shadow_pte32(struct guest_info * info, addr_t pt_host_addr) {
  pte32_t * guest_pte = (pte32_t *)pt_host_addr;
  pte32_t * host_pte = NULL;
  int i;

  V3_AllocPages(host_pte, 1);
  memset(host_pte, 0, PAGE_SIZE);

  for (i = 0; i < MAX_PTE32_ENTRIES; i++) {
    if (guest_pte[i].present == 1) {
      addr_t guest_pa = PTE32_T_ADDR(guest_pte[i]);
      shadow_mem_type_t page_type;
      addr_t host_pa = 0;

      page_type = get_shadow_addr_type(info, guest_pa);

      if (page_type == HOST_REGION_PHYSICAL_MEMORY) {
	host_pa = get_shadow_addr(info, guest_pa);
      } else {
	
	//
	// Setup various memory types
	//
      }

      host_pte[i].page_base_addr = PT32_BASE_ADDR(host_pa);
      host_pte[i].present = 1;
    }
  }

  return (addr_t)host_pte;
}


