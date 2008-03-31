#include <geekos/vm_guest_mem.c>

#include <geekos/vmm_paging.h>

extern struct vmm_os_hooks * os_hooks;



int guest_va_to_guest_pa(guest_info_t * guest_info, addr_t guest_va, addr_t * guest_pa) {
  if (guest_info->page_mode == SHADOW_PAGING) {
    switch (guest_info->cpu_mode) {
    case REAL:
    case PROTECTED:
    case LONG:
    case PROTECTED_PAE:
      // guest virtual address is the same as the physical
      *guest_pa = guest_va;
      return 0;
    case PROTECTED_PG:
      {
	addr_t tmp_pa;
	pde32_t * pde;
	addr_t guest_pde = CR3_TO_PDE32(guest_info->shadow_page_state.guest_cr3);

	if (guest_pa_to_host_va(guest_info, guest_pde, (addr_t *)&pde) == -1) {
	  return -1;
	}

	switch (pde32_lookup(pde, guest_va, &tmp_pa)) {
	case NOT_PRESENT: 
	  *guest_page = 0;
	  return -1;
	case LARGE_PAGE:
	  *guest_pa = tmp_pa;
	  return 0;
	case PTE32:
	  {
	    pte32_t * pte;

	    if (guest_pa_to_host_va(guest_info, tmp_pa, (addr_t*)&pte) == -1) {
	      return -1;
	    }
	    
	    if (pte32_lookup(pte, guest_va, guest_pa) != 0) {
	      return -1;
	    }

	    return 0;	    
	  }
	default:
	  return -1;
	}
      }
      case PROTECTED_PAE_PG:
	{
	  // Fill in
	}
      case LONG_PG:
	{
	  // Fill in
	}
    default:
      return -1;
    }
  } else if (guest_info->page_mode == NESTED_PAGING) {

    // Fill in

  } else {
    return -1;
  }


  return 0;
}







int guest_pa_to_host_va(guest_info_t * guest_info, addr_t guest_pa, addr_t * host_va) {
  addr_t host_pa;

  if (guest_pa_to_host_pa(guest_info, guest_pa, &host_pa) != 0) {
    return -1;
  }
  
  if (host_pa_to_host_va(host_pa, host_va) != 0) {
    return -1;
  }

  return 0;
}


int guest_pa_to_host_pa(guest_info_t * guest_info, addr_t guest_pa, addr_t * host_pa) {
  // we use the shadow map here...
  if (lookup_shadow_map_addr(guest_info->shadow_map, guest_pa, host_pa) != HOST_REGION_PHYSICAL_MEMORY) {
    return -1;
  }
				  
  return 0;
}




int host_va_to_host_pa(addr_t host_va, addr_t * host_pa) {
  *host_pa = os_hooks->vaddr_to_paddr(host_va);
  
  if (*host_pa == 0) {
    return -1;
  }

  return 0;
}


int host_pa_to_host_va(addr_t host_pa, addr_t * host_va) {
  *host_va = os_hooks->paddr_to_vaddr(host_pa);

  if (*host_va == 0) {
    return -1;
  }

  return 0;
}
