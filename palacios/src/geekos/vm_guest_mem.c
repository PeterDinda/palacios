#include <geekos/vm_guest_mem.h>
#include <geekos/vmm.h>
#include <geekos/vmm_paging.h>

extern struct vmm_os_hooks * os_hooks;


/**********************************/
/* GROUP 0                        */
/**********************************/

int host_va_to_host_pa(addr_t host_va, addr_t * host_pa) {
  if ((os_hooks) && (os_hooks)->vaddr_to_paddr) {

    *host_pa = (addr_t)(os_hooks)->vaddr_to_paddr((void *)host_va);
  
    if (*host_pa == 0) {
      return -1;
    }
  } else {
    return -1;
  }
  return 0;
}


int host_pa_to_host_va(addr_t host_pa, addr_t * host_va) {
  if ((os_hooks) && (os_hooks)->paddr_to_vaddr) {

    *host_va = (addr_t)(os_hooks)->paddr_to_vaddr((void *)host_pa);
    
    if (*host_va == 0) {
      return -1;
    }
  } else {
    return -1;
  }
  return 0;
}



int guest_pa_to_host_pa(struct guest_info * guest_info, addr_t guest_pa, addr_t * host_pa) {
  // we use the shadow map here...
  if (lookup_shadow_map_addr(&(guest_info->mem_map), guest_pa, host_pa) != HOST_REGION_PHYSICAL_MEMORY) {
    return -1;
  }

  return 0;
}


/* !! Currently not implemented !! */
// This is a scan of the shadow map
// For now we ignore it
// 
int host_pa_to_guest_pa(struct guest_info * guest_info, addr_t host_pa, addr_t * guest_pa) {
  *guest_pa = 0;

  return -1;
}



/**********************************/
/* GROUP 1                        */
/**********************************/


/* !! Currently not implemented !! */
// This will return negative until we implement host_pa_to_guest_pa()
int host_va_to_guest_pa(struct guest_info * guest_info, addr_t host_va, addr_t * guest_pa) {
  addr_t host_pa;
  *guest_pa = 0;

  if (host_va_to_host_pa(host_va, &host_pa) != 0) {
    return -1;
  }

  if (host_pa_to_guest_pa(guest_info, host_pa, guest_pa) != 0) {
    return -1;
  }

  return 0;
}




int guest_pa_to_host_va(struct guest_info * guest_info, addr_t guest_pa, addr_t * host_va) {
  addr_t host_pa;

  *host_va = 0;

  if (guest_pa_to_host_pa(guest_info, guest_pa, &host_pa) != 0) {
    return -1;
  }
  
  if (host_pa_to_host_va(host_pa, host_va) != 0) {
    return -1;
  }

  return 0;
}


int guest_va_to_guest_pa(struct guest_info * guest_info, addr_t guest_va, addr_t * guest_pa) {
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
	addr_t guest_pde = CR3_TO_PDE32(guest_info->shdw_pg_state.guest_cr3.r_reg);

	if (guest_pa_to_host_va(guest_info, guest_pde, (addr_t *)&pde) == -1) {
	  return -1;
	}

	switch (pde32_lookup(pde, guest_va, &tmp_pa)) {
	case NOT_PRESENT: 
	  *guest_pa = 0;
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



/* !! Currently not implemented !! */
/* This will be a real pain.... its your standard page table walker in guest memory
 * 
 * For now we ignore it...
 */
int guest_pa_to_guest_va(struct guest_info * guest_info, addr_t guest_pa, addr_t * guest_va) {
  *guest_va = 0;
  return -1;
}


/**********************************/
/* GROUP 2                        */
/**********************************/


int guest_va_to_host_pa(struct guest_info * guest_info, addr_t guest_va, addr_t * host_pa) {
  addr_t guest_pa;

  *host_pa = 0;

  if (guest_va_to_guest_pa(guest_info, guest_va, &guest_pa) != 0) {
    return -1;
  }
  
  if (guest_pa_to_host_pa(guest_info, guest_pa, host_pa) != 0) {
    return -1;
  }

  return 0;
}

/* !! Currently not implemented !! */
int host_pa_to_guest_va(struct guest_info * guest_info, addr_t host_pa, addr_t * guest_va) {
  addr_t guest_pa;

  *guest_va = 0;

  if (host_pa_to_guest_pa(guest_info, host_pa, &guest_pa) != 0) {
    return -1;
  }

  if (guest_pa_to_guest_va(guest_info, guest_pa, guest_va) != 0) {
    return -1;
  }

  return 0;
}




int guest_va_to_host_va(struct guest_info * guest_info, addr_t guest_va, addr_t * host_va) {
  addr_t guest_pa;
  addr_t host_pa;

  *host_va = 0;

  if (guest_va_to_guest_pa(guest_info, guest_va, &guest_pa) != 0) {
    return -1;
  }

  if (guest_pa_to_host_pa(guest_info, guest_pa, &host_pa) != 0) {
    return -1;
  }

  if (host_pa_to_host_va(host_pa, host_va) != 0) {
    return -1;
  }

  return 0;
}


/* !! Currently not implemented !! */
int host_va_to_guest_va(struct guest_info * guest_info, addr_t host_va, addr_t * guest_va) {
  addr_t host_pa;
  addr_t guest_pa;

  *guest_va = 0;

  if (host_va_to_host_pa(host_va, &host_pa) != 0) {
    return -1;
  }

  if (host_pa_to_guest_pa(guest_info, host_pa, &guest_pa) != 0) {
    return -1;
  }

  if (guest_pa_to_guest_va(guest_info, guest_pa, guest_va) != 0) {
    return -1;
  }

  return 0;
}






/* This is a straight address conversion + copy, 
 *   except for the tiny little issue of crossing page boundries.....
 */
int read_guest_va_memory(struct guest_info * guest_info, addr_t guest_va, int count, char * dest) {
  addr_t cursor = guest_va;

  while (count > 0) {
    int dist_to_pg_edge = (PAGE_OFFSET(cursor) + PAGE_SIZE) - cursor;
    int bytes_to_copy = (dist_to_pg_edge > count) ? count : dist_to_pg_edge;
    addr_t host_addr;

    if (guest_va_to_host_va(guest_info, cursor, &host_addr) != 0) {
      return -1;
    }

    memcpy(dest, (void*)cursor, bytes_to_copy);

    count -= bytes_to_copy;
    cursor += bytes_to_copy;    
  }

  return 0;
}






/* This is a straight address conversion + copy, 
 *   except for the tiny little issue of crossing page boundries.....
 */
int read_guest_pa_memory(struct guest_info * guest_info, addr_t guest_pa, int count, char * dest) {
  addr_t cursor = guest_pa;

  while (count > 0) {
    int dist_to_pg_edge = (PAGE_OFFSET(cursor) + PAGE_SIZE) - cursor;
    int bytes_to_copy = (dist_to_pg_edge > count) ? count : dist_to_pg_edge;
    addr_t host_addr;

    if (guest_pa_to_host_va(guest_info, cursor, &host_addr) != 0) {
      return -1;
    }

    memcpy(dest, (void*)cursor, bytes_to_copy);

    count -= bytes_to_copy;
    cursor += bytes_to_copy;    
  }

  return 0;
}

