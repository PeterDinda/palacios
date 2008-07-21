#include <palacios/vm_guest_mem.h>
#include <palacios/vmm.h>
#include <palacios/vmm_paging.h>

extern struct vmm_os_hooks * os_hooks;


/**********************************/
/* GROUP 0                        */
/**********************************/

int host_va_to_host_pa(addr_t host_va, addr_t * host_pa) {
  if ((os_hooks) && (os_hooks)->vaddr_to_paddr) {

    *host_pa = (addr_t)(os_hooks)->vaddr_to_paddr((void *)host_va);
  
    if (*host_pa == 0) {
      PrintError("In HVA->HPA: Invalid HVA(%x)->HPA lookup\n", host_va);
      return -1;
    }
  } else {
    PrintError("In HVA->HPA: os_hooks not defined\n");
    return -1;
  }
  return 0;
}


int host_pa_to_host_va(addr_t host_pa, addr_t * host_va) {
  if ((os_hooks) && (os_hooks)->paddr_to_vaddr) {

    *host_va = (addr_t)(os_hooks)->paddr_to_vaddr((void *)host_pa);
    
    if (*host_va == 0) {
      PrintError("In HPA->HVA: Invalid HPA(%x)->HVA lookup\n", host_pa);
      return -1;
    }
  } else {
    PrintError("In HPA->HVA: os_hooks not defined\n");
    return -1;
  }
  return 0;
}



int guest_pa_to_host_pa(struct guest_info * guest_info, addr_t guest_pa, addr_t * host_pa) {
  // we use the shadow map here...
  if (lookup_shadow_map_addr(&(guest_info->mem_map), guest_pa, host_pa) != HOST_REGION_PHYSICAL_MEMORY) {
    PrintError("In GPA->HPA: Could not find address in shadow map (addr=%x)\n", guest_pa);
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
  PrintError("ERROR!!! HPA->GPA currently not implemented!!!\n");

  return -1;
}



/**********************************/
/* GROUP 1                        */
/**********************************/


/* !! Currently not implemented !! */
// This will return negative until we implement host_pa_to_guest_pa()
int host_va_to_guest_pa(struct guest_info * guest_info, addr_t host_va, addr_t * guest_pa) {
  addr_t host_pa = 0;
  *guest_pa = 0;

  if (host_va_to_host_pa(host_va, &host_pa) != 0) {
    PrintError("In HVA->GPA: Invalid HVA(%x)->HPA lookup\n", host_va);
    return -1;
  }

  if (host_pa_to_guest_pa(guest_info, host_pa, guest_pa) != 0) {
    PrintError("In HVA->GPA: Invalid HPA(%x)->GPA lookup\n", host_pa);
    return -1;
  }

  return 0;
}




int guest_pa_to_host_va(struct guest_info * guest_info, addr_t guest_pa, addr_t * host_va) {
  addr_t host_pa = 0;

  *host_va = 0;

  if (guest_pa_to_host_pa(guest_info, guest_pa, &host_pa) != 0) {
    PrintError("In GPA->HVA: Invalid GPA(%x)->HPA lookup\n", guest_pa);
    return -1;
  }
  
  if (host_pa_to_host_va(host_pa, host_va) != 0) {
    PrintError("In GPA->HVA: Invalid HPA(%x)->HVA lookup\n", host_pa);
    return -1;
  }

  return 0;
}


int guest_va_to_guest_pa(struct guest_info * guest_info, addr_t guest_va, addr_t * guest_pa) {
  if (guest_info->mem_mode == PHYSICAL_MEM) {
    // guest virtual address is the same as the physical
    *guest_pa = guest_va;
    return 0;
  }



  // Guest Is in Paged mode
  switch (guest_info->cpu_mode) {
  case PROTECTED:
    {
      addr_t tmp_pa = 0;
      pde32_t * pde = 0;
      addr_t guest_pde = 0;
      
      if (guest_info->shdw_pg_mode == SHADOW_PAGING) {
	guest_pde = CR3_TO_PDE32(guest_info->shdw_pg_state.guest_cr3);
      } else if (guest_info->shdw_pg_mode == NESTED_PAGING) {
	guest_pde = CR3_TO_PDE32(guest_info->ctrl_regs.cr3);
      }
      
      if (guest_pa_to_host_va(guest_info, guest_pde, (addr_t *)&pde) == -1) {
	PrintError("In GVA->GPA: Invalid GPA(%x)->HVA PDE32 lookup\n", guest_pde);
	return -1;
      }
      
      
      switch (pde32_lookup(pde, guest_va, &tmp_pa)) {
      case PDE32_ENTRY_NOT_PRESENT: 
	*guest_pa = 0;
	return -1;
      case PDE32_ENTRY_LARGE_PAGE:
	*guest_pa = tmp_pa;
	return 0;
      case PDE32_ENTRY_PTE32:
	{
	  pte32_t * pte = 0;
	  
	  
	  if (guest_pa_to_host_va(guest_info, tmp_pa, (addr_t*)&pte) == -1) {
	    PrintError("In GVA->GPA: Invalid GPA(%x)->HVA PTE32 lookup\n", guest_pa);
	    return -1;
	  }
	  
	  //PrintDebug("PTE host addr=%x, GVA=%x, GPA=%x(should be 0)\n", pte, guest_va, *guest_pa);
	 
	  if (pte32_lookup(pte, guest_va, guest_pa) != 0) {
	    PrintError("In GVA->GPA: PTE32 Lookup failure GVA=%x; PTE=%x\n", guest_va, pte);
	    //	      PrintPT32(PDE32_INDEX(guest_va) << 22, pte);
	    return -1;
	  }
	  
	  return 0;
	}
      default:
	return -1;
      }
    }
  case PROTECTED_PAE:
    {
      // Fill in
    }
  case LONG:
    {
      // Fill in
    }
  default:
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
  PrintError("ERROR!!: GPA->GVA Not Implemented!!\n");
  return -1;
}


/**********************************/
/* GROUP 2                        */
/**********************************/


int guest_va_to_host_pa(struct guest_info * guest_info, addr_t guest_va, addr_t * host_pa) {
  addr_t guest_pa = 0;

  *host_pa = 0;

  if (guest_va_to_guest_pa(guest_info, guest_va, &guest_pa) != 0) {
    PrintError("In GVA->HPA: Invalid GVA(%x)->GPA lookup\n", guest_va);
    return -1;
  }
  
  if (guest_pa_to_host_pa(guest_info, guest_pa, host_pa) != 0) {
    PrintError("In GVA->HPA: Invalid GPA(%x)->HPA lookup\n", guest_pa);
    return -1;
  }

  return 0;
}

/* !! Currently not implemented !! */
int host_pa_to_guest_va(struct guest_info * guest_info, addr_t host_pa, addr_t * guest_va) {
  addr_t guest_pa = 0;

  *guest_va = 0;

  if (host_pa_to_guest_pa(guest_info, host_pa, &guest_pa) != 0) {
    PrintError("In HPA->GVA: Invalid HPA(%x)->GPA lookup\n", host_pa);
    return -1;
  }

  if (guest_pa_to_guest_va(guest_info, guest_pa, guest_va) != 0) {
    PrintError("In HPA->GVA: Invalid GPA(%x)->GVA lookup\n", guest_pa);
    return -1;
  }

  return 0;
}




int guest_va_to_host_va(struct guest_info * guest_info, addr_t guest_va, addr_t * host_va) {
  addr_t guest_pa = 0;
  addr_t host_pa = 0;

  *host_va = 0;

  if (guest_va_to_guest_pa(guest_info, guest_va, &guest_pa) != 0) {
    PrintError("In GVA->HVA: Invalid GVA(%x)->GPA lookup\n", guest_va);
    return -1;
  }

  if (guest_pa_to_host_pa(guest_info, guest_pa, &host_pa) != 0) {
    PrintError("In GVA->HVA: Invalid GPA(%x)->HPA lookup\n", guest_pa);
    return -1;
  }

  if (host_pa_to_host_va(host_pa, host_va) != 0) {
    PrintError("In GVA->HVA: Invalid HPA(%x)->HVA lookup\n", host_pa);
    return -1;
  }

  return 0;
}


/* !! Currently not implemented !! */
int host_va_to_guest_va(struct guest_info * guest_info, addr_t host_va, addr_t * guest_va) {
  addr_t host_pa = 0;
  addr_t guest_pa = 0;

  *guest_va = 0;

  if (host_va_to_host_pa(host_va, &host_pa) != 0) {
    PrintError("In HVA->GVA: Invalid HVA(%x)->HPA lookup\n", host_va);
    return -1;
  }

  if (host_pa_to_guest_pa(guest_info, host_pa, &guest_pa) != 0) {
    PrintError("In HVA->GVA: Invalid HPA(%x)->GPA lookup\n", host_va);
    return -1;
  }

  if (guest_pa_to_guest_va(guest_info, guest_pa, guest_va) != 0) {
    PrintError("In HVA->GVA: Invalid GPA(%x)->GVA lookup\n", guest_pa);
    return -1;
  }

  return 0;
}






/* This is a straight address conversion + copy, 
 *   except for the tiny little issue of crossing page boundries.....
 */
int read_guest_va_memory(struct guest_info * guest_info, addr_t guest_va, int count, char * dest) {
  addr_t cursor = guest_va;
  int bytes_read = 0;



  while (count > 0) {
    int dist_to_pg_edge = (PAGE_ADDR(cursor) + PAGE_SIZE) - cursor;
    int bytes_to_copy = (dist_to_pg_edge > count) ? count : dist_to_pg_edge;
    addr_t host_addr = 0;

    
    if (guest_va_to_host_va(guest_info, cursor, &host_addr) != 0) {
      PrintDebug("Invalid GVA(%x)->HVA lookup\n", cursor);
      return bytes_read;
    }
    
    

    memcpy(dest + bytes_read, (void*)host_addr, bytes_to_copy);
    
    bytes_read += bytes_to_copy;
    count -= bytes_to_copy;
    cursor += bytes_to_copy;    
  }

  return bytes_read;
}






/* This is a straight address conversion + copy, 
 *   except for the tiny little issue of crossing page boundries.....
 */
int read_guest_pa_memory(struct guest_info * guest_info, addr_t guest_pa, int count, char * dest) {
  addr_t cursor = guest_pa;
  int bytes_read = 0;

  while (count > 0) {
    int dist_to_pg_edge = (PAGE_ADDR(cursor) + PAGE_SIZE) - cursor;
    int bytes_to_copy = (dist_to_pg_edge > count) ? count : dist_to_pg_edge;
    addr_t host_addr = 0;

    if (guest_pa_to_host_va(guest_info, cursor, &host_addr) != 0) {
      return bytes_read;
    }    
    
    /*
      PrintDebug("Trying to read %d bytes\n", bytes_to_copy);
      PrintDebug("Dist to page edge=%d\n", dist_to_pg_edge);
      PrintDebug("PAGE_ADDR=0x%x\n", PAGE_ADDR(cursor));
      PrintDebug("guest_pa=0x%x\n", guest_pa);
    */
    
    memcpy(dest + bytes_read, (void*)host_addr, bytes_to_copy);

    bytes_read += bytes_to_copy;
    count -= bytes_to_copy;
    cursor += bytes_to_copy;
  }

  return bytes_read;
}




/* This is a straight address conversion + copy, 
 *   except for the tiny little issue of crossing page boundries.....
 */
int write_guest_pa_memory(struct guest_info * guest_info, addr_t guest_pa, int count, char * src) {
  addr_t cursor = guest_pa;
  int bytes_written = 0;

  while (count > 0) {
    int dist_to_pg_edge = (PAGE_ADDR(cursor) + PAGE_SIZE) - cursor;
    int bytes_to_copy = (dist_to_pg_edge > count) ? count : dist_to_pg_edge;
    addr_t host_addr;

    if (guest_pa_to_host_va(guest_info, cursor, &host_addr) != 0) {
      return bytes_written;
    }


    memcpy((void*)host_addr, src + bytes_written, bytes_to_copy);

    bytes_written += bytes_to_copy;
    count -= bytes_to_copy;
    cursor += bytes_to_copy;    
  }

  return bytes_written;
}

