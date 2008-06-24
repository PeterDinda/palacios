#include <palacios/vmm_paging.h>

#include <palacios/vmm.h>

#include <palacios/vm_guest_mem.h>


extern struct vmm_os_hooks * os_hooks;

void delete_page_tables_pde32(pde32_t * pde) {
  int i;//, j;

  if (pde == NULL) { 
    return;
  }

  for (i = 0; (i < MAX_PDE32_ENTRIES); i++) {
    if (pde[i].present) {
      pte32_t * pte = (pte32_t *)(pde[i].pt_base_addr << PAGE_POWER);

      /*
	for (j = 0; (j < MAX_PTE32_ENTRIES); j++) {
	if ((pte[j].present)) {
	os_hooks->free_page((void *)(pte[j].page_base_addr << PAGE_POWER));
	}
	}
      */
      //PrintDebug("Deleting PTE %d (%x)\n", i, pte);
      os_hooks->free_page(pte);
    }
  }

  //  PrintDebug("Deleting PDE (%x)\n", pde);
  os_hooks->free_page(pde);
}







/* We can't do a full lookup because we don't know what context the page tables are in...
 * The entry addresses could be pointing to either guest physical memory or host physical memory
 * Instead we just return the entry address, and a flag to show if it points to a pte or a large page...
 */
pde32_entry_type_t pde32_lookup(pde32_t * pd, addr_t addr, addr_t * entry) {
  pde32_t * pde_entry = &(pd[PDE32_INDEX(addr)]);

  if (!pde_entry->present) {
    *entry = 0;
    return PDE32_ENTRY_NOT_PRESENT;
  } else  {
    *entry = PAGE_ADDR(pde_entry->pt_base_addr);
    
    if (pde_entry->large_page) {
      *entry += PAGE_OFFSET(addr);
      return PDE32_ENTRY_LARGE_PAGE;
    } else {
      *entry = PDE32_T_ADDR(*pde_entry);
      return PDE32_ENTRY_PTE32;
    }
  }  
  return PDE32_ENTRY_NOT_PRESENT;
}



/* Takes a virtual addr (addr) and returns the physical addr (entry) as defined in the page table
 */
int pte32_lookup(pte32_t * pt, addr_t addr, addr_t * entry) {
  pte32_t * pte_entry = &(pt[PTE32_INDEX(addr)]);

  if (!pte_entry->present) {
    *entry = 0;
    PrintDebug("Lookup at non present page (index=%d)\n", PTE32_INDEX(addr));
    return -1;
  } else {
    *entry = PTE32_T_ADDR(*pte_entry) + PT32_PAGE_OFFSET(addr);
    return 0;
  }

  return -1;
}



pt_access_status_t can_access_pde32(pde32_t * pde, addr_t addr, pf_error_t access_type) {
  pde32_t * entry = &pde[PDE32_INDEX(addr)];

  if (entry->present == 0) {
    return PT_ENTRY_NOT_PRESENT;
  } else if ((entry->writable == 0) && (access_type.write == 1)) {
    return PT_WRITE_ERROR;
  } else if ((entry->user_page == 0) && (access_type.user == 1)) {
    // Check CR0.WP
    return PT_USER_ERROR;
  }

  return PT_ACCESS_OK;
}


pt_access_status_t can_access_pte32(pte32_t * pte, addr_t addr, pf_error_t access_type) {
  pte32_t * entry = &pte[PTE32_INDEX(addr)];

  if (entry->present == 0) {
    return PT_ENTRY_NOT_PRESENT;
  } else if ((entry->writable == 0) && (access_type.write == 1)) {
    return PT_WRITE_ERROR;
  } else if ((entry->user_page == 0) && (access_type.user == 1)) {
    // Check CR0.WP
    return PT_USER_ERROR;
  }

  return PT_ACCESS_OK;
}




/* We generate a page table to correspond to a given memory layout
 * pulling pages from the mem_list when necessary
 * If there are any gaps in the layout, we add them as unmapped pages
 */
pde32_t * create_passthrough_pde32_pts(struct guest_info * guest_info) {
  ullong_t current_page_addr = 0;
  int i, j;
  struct shadow_map * map = &(guest_info->mem_map);

  pde32_t * pde = os_hooks->allocate_pages(1);

  for (i = 0; i < MAX_PDE32_ENTRIES; i++) {
    int pte_present = 0;
    pte32_t * pte = os_hooks->allocate_pages(1);
    

    for (j = 0; j < MAX_PTE32_ENTRIES; j++) {
      struct shadow_region * region = get_shadow_region_by_addr(map, current_page_addr);

      if (!region || 
	  (region->host_type == HOST_REGION_HOOK) || 
	  (region->host_type == HOST_REGION_UNALLOCATED) || 
	  (region->host_type == HOST_REGION_MEMORY_MAPPED_DEVICE) || 
	  (region->host_type == HOST_REGION_REMOTE) ||
	  (region->host_type == HOST_REGION_SWAPPED)) {
	pte[j].present = 0;
	pte[j].writable = 0;
	pte[j].user_page = 0;
	pte[j].write_through = 0;
	pte[j].cache_disable = 0;
	pte[j].accessed = 0;
	pte[j].dirty = 0;
	pte[j].pte_attr = 0;
	pte[j].global_page = 0;
	pte[j].vmm_info = 0;
	pte[j].page_base_addr = 0;
      } else {
	addr_t host_addr;
	pte[j].present = 1;
	pte[j].writable = 1;
	pte[j].user_page = 1;
	pte[j].write_through = 0;
	pte[j].cache_disable = 0;
	pte[j].accessed = 0;
	pte[j].dirty = 0;
	pte[j].pte_attr = 0;
	pte[j].global_page = 0;
	pte[j].vmm_info = 0;

	if (guest_pa_to_host_pa(guest_info, current_page_addr, &host_addr) == -1) {
	  // BIG ERROR
	  // PANIC
	  return NULL;
	}
	
	pte[j].page_base_addr = host_addr >> 12;
	
	pte_present = 1;
      }

      current_page_addr += PAGE_SIZE;
    }

    if (pte_present == 0) { 
      os_hooks->free_page(pte);

      pde[i].present = 0;
      pde[i].writable = 0;
      pde[i].user_page = 0;
      pde[i].write_through = 0;
      pde[i].cache_disable = 0;
      pde[i].accessed = 0;
      pde[i].reserved = 0;
      pde[i].large_page = 0;
      pde[i].global_page = 0;
      pde[i].vmm_info = 0;
      pde[i].pt_base_addr = 0;
    } else {
      pde[i].present = 1;
      pde[i].writable = 1;
      pde[i].user_page = 1;
      pde[i].write_through = 0;
      pde[i].cache_disable = 0;
      pde[i].accessed = 0;
      pde[i].reserved = 0;
      pde[i].large_page = 0;
      pde[i].global_page = 0;
      pde[i].vmm_info = 0;
      pde[i].pt_base_addr = PAGE_ALIGNED_ADDR(pte);
    }

  }

  return pde;
}






void PrintPDE32(addr_t virtual_address, pde32_t * pde)
{
  PrintDebug("PDE %p -> %p : present=%x, writable=%x, user=%x, wt=%x, cd=%x, accessed=%x, reserved=%x, largePages=%x, globalPage=%x, kernelInfo=%x\n",
	     virtual_address,
	     (void *) (pde->pt_base_addr << PAGE_POWER),
	     pde->present,
	     pde->writable,
	     pde->user_page, 
	     pde->write_through,
	     pde->cache_disable,
	     pde->accessed,
	     pde->reserved,
	     pde->large_page,
	     pde->global_page,
	     pde->vmm_info);
}
  
void PrintPTE32(addr_t virtual_address, pte32_t * pte)
{
  PrintDebug("PTE %p -> %p : present=%x, writable=%x, user=%x, wt=%x, cd=%x, accessed=%x, dirty=%x, pteAttribute=%x, globalPage=%x, vmm_info=%x\n",
	     virtual_address,
	     (void*)(pte->page_base_addr << PAGE_POWER),
	     pte->present,
	     pte->writable,
	     pte->user_page,
	     pte->write_through,
	     pte->cache_disable,
	     pte->accessed,
	     pte->dirty,
	     pte->pte_attr,
	     pte->global_page,
	     pte->vmm_info);
}



void PrintPD32(pde32_t * pde)
{
  int i;

  PrintDebug("Page Directory at %p:\n", pde);
  for (i = 0; (i < MAX_PDE32_ENTRIES); i++) { 
    if ( pde[i].present) {
      PrintPDE32((addr_t)(PAGE_SIZE * MAX_PTE32_ENTRIES * i), &(pde[i]));
    }
  }
}

void PrintPT32(addr_t starting_address, pte32_t * pte) 
{
  int i;

  PrintDebug("Page Table at %p:\n", pte);
  for (i = 0; (i < MAX_PTE32_ENTRIES) ; i++) { 
    if (pte[i].present) {
      PrintPTE32(starting_address + (PAGE_SIZE * i), &(pte[i]));
    }
  }
}





void PrintDebugPageTables(pde32_t * pde)
{
  int i;
  
  PrintDebug("Dumping the pages starting with the pde page at %p\n", pde);

  for (i = 0; (i < MAX_PDE32_ENTRIES); i++) { 
    if (pde[i].present) {
      PrintPDE32((addr_t)(PAGE_SIZE * MAX_PTE32_ENTRIES * i), &(pde[i]));
      PrintPT32((addr_t)(PAGE_SIZE * MAX_PTE32_ENTRIES * i), (pte32_t *)(pde[i].pt_base_addr << PAGE_POWER));
    }
  }
}
    
