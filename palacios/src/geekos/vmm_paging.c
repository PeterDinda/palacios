#include <geekos/vmm_paging.h>

#include <geekos/vmm.h>



extern struct vmm_os_hooks * os_hooks;

void delete_page_tables_pde32(vmm_pde_t * pde) {
  int i, j;

  if (pde == NULL) { 
    return;
  }

  for (i = 0; (i < MAX_PAGE_DIR_ENTRIES); i++) {
    if (pde[i].present) {
      vmm_pte_t * pte = (vmm_pte_t *)(pde[i].pt_base_addr << PAGE_POWER);
      
      for (j = 0; (j < MAX_PAGE_TABLE_ENTRIES); j++) {
	if ((pte[j].present)) {
	  os_hooks->free_page((void *)(pte[j].page_base_addr << PAGE_POWER));
	}
      }
      
      os_hooks->free_page(pte);
    }
  }

  os_hooks->free_page(pde);
}


int init_shadow_page_state(shadow_page_state_t * state) {
  state->guest_mode = PDE32;
  state->shadow_mode = PDE32;
  
  state->guest_cr3.r_reg = 0;
  state->shadow_cr3.r_reg = 0;

  return 0;
}
  

int wholesale_update_shadow_page_state(shadow_page_state_t * state, shadow_map_t * mem_map) {
  unsigned i, j;
  vmm_pde_t * guest_pde;
  vmm_pde_t * shadow_pde;


  // For now, we'll only work with PDE32
  if (state->guest_mode != PDE32) { 
    return -1;
  }


  
  shadow_pde = (vmm_pde_t *)(CR3_TO_PDE(state->shadow_cr3.e_reg.low));  
  guest_pde = (vmm_pde_t *)(os_hooks->paddr_to_vaddr((void*)CR3_TO_PDE(state->guest_cr3.e_reg.low)));

  // Delete the current page table
  delete_page_tables_pde32(shadow_pde);

  shadow_pde = os_hooks->allocate_pages(1);


  state->shadow_cr3.e_reg.low = (addr_t)shadow_pde;

  state->shadow_mode = PDE32;

  
  for (i = 0; i < MAX_PAGE_DIR_ENTRIES; i++) { 
    shadow_pde[i] = guest_pde[i];

    // The shadow can be identical to the guest if it's not present
    if (!shadow_pde[i].present) { 
      continue;
    }

    if (shadow_pde[i].large_pages) { 
      // large page - just map it through shadow map to generate its physical location
      addr_t guest_addr = PAGE_ADDR(shadow_pde[i].pt_base_addr);
      addr_t host_addr;
      shadow_region_t * ent;

      ent = get_shadow_region_by_addr(mem_map, guest_addr);
      
      if (!ent) { 
	// FIXME Panic here - guest is trying to map to physical memory
	// it does not own in any way!
	return -1;
      }

      // FIXME Bounds check here to see if it's trying to trick us
      
      switch (ent->host_type) { 
      case HOST_REGION_PHYSICAL_MEMORY:
	// points into currently allocated physical memory, so we just
	// set up the shadow to point to the mapped location
	if (guest_paddr_to_host_paddr(ent, guest_addr, &host_addr)) { 
	  // Panic here
	  return -1;
	}

	shadow_pde[i].pt_base_addr = PAGE_ALIGNED_ADDR(host_addr);
	// FIXME set vmm_info bits here
	break;
      case HOST_REGION_UNALLOCATED:
	// points to physical memory that is *allowed* but that we
	// have not yet allocated.  We mark as not present and set a
	// bit to remind us to allocate it later
	shadow_pde[i].present = 0;
	// FIXME Set vminfo bits here so that we know that we will be
	// allocating it later
	break;
      case HOST_REGION_NOTHING:
	// points to physical memory that is NOT ALLOWED.   
	// We will mark it as not present and set a bit to remind
	// us that it's bad later and insert a GPF then
	shadow_pde[i].present = 0;
	break;
      case HOST_REGION_MEMORY_MAPPED_DEVICE:
      case HOST_REGION_REMOTE:
      case HOST_REGION_SWAPPED:
      default:
	// Panic.  Currently unhandled
	return -1;
	break;
      }
    } else {
      vmm_pte_t * guest_pte;
      vmm_pte_t * shadow_pte;
      addr_t guest_addr;
      addr_t guest_pte_host_addr;
      shadow_region_t * ent;

      // small page - set PDE and follow down to the child table
      shadow_pde[i] = guest_pde[i];

      guest_addr = PAGE_ADDR(guest_pde[i].pt_base_addr);

      // Allocate a new second level page table for the shadow
      shadow_pte = os_hooks->allocate_pages(1);

      // make our first level page table in the shadow point to it
      shadow_pde[i].pt_base_addr = PAGE_ALIGNED_ADDR(shadow_pte);
      
      ent = get_shadow_region_by_addr(mem_map, guest_addr);
      

      /* JRL: This is bad.... */
      // For now the guest Page Table must always be mapped to host physical memory
      /* If we swap out a page table or if it isn't present for some reason, this turns real ugly */

      if ((!ent) || (ent->host_type != HOST_REGION_PHYSICAL_MEMORY)) { 
	// FIXME Panic here - guest is trying to map to physical memory
	// it does not own in any way!
	return -1;
      }

      // Address of the relevant second level page table in the guest
      if (guest_paddr_to_host_paddr(ent, guest_addr, &guest_pte_host_addr)) { 
	// Panic here
	return -1;
      }


      // host_addr now contains the host physical address for the guest's 2nd level page table
      // Now we transform it to relevant virtual address
      guest_pte = os_hooks->paddr_to_vaddr((void *)guest_pte_host_addr);

      // Now we walk through the second level guest page table
      // and clone it into the shadow
      for (j = 0; j < MAX_PAGE_TABLE_ENTRIES; j++) { 
	shadow_pte[j] = guest_pte[j];

	addr_t guest_addr = PAGE_ADDR(shadow_pte[j].page_base_addr);
	
	shadow_region_t * ent;

	ent = get_shadow_region_by_addr(mem_map, guest_addr);
      
	if (!ent) { 
	  // FIXME Panic here - guest is trying to map to physical memory
	  // it does not own in any way!
	  return -1;
	}

	switch (ent->host_type) { 
	case HOST_REGION_PHYSICAL_MEMORY:
	  {
	    addr_t host_addr;
	    
	    // points into currently allocated physical memory, so we just
	    // set up the shadow to point to the mapped location
	    if (guest_paddr_to_host_paddr(ent, guest_addr, &host_addr)) { 
	      // Panic here
	      return -1;
	    }
	    
	    shadow_pte[j].page_base_addr = PAGE_ALIGNED_ADDR(host_addr);
	    // FIXME set vmm_info bits here
	    break;
	  }
	case HOST_REGION_UNALLOCATED:
	  // points to physical memory that is *allowed* but that we
	  // have not yet allocated.  We mark as not present and set a
	  // bit to remind us to allocate it later
	  shadow_pte[j].present = 0;
	  // FIXME Set vminfo bits here so that we know that we will be
	  // allocating it later
	  break;
	case HOST_REGION_NOTHING:
	  // points to physical memory that is NOT ALLOWED.   
	  // We will mark it as not present and set a bit to remind
	  // us that it's bad later and insert a GPF then
	  shadow_pte[j].present = 0;
	  break;
	case HOST_REGION_MEMORY_MAPPED_DEVICE:
	case HOST_REGION_REMOTE:
	case HOST_REGION_SWAPPED:
	default:
	  // Panic.  Currently unhandled
	  return -1;
	break;
	}
      }
    }
  }
  return 0;
}
      



/* We generate a page table to correspond to a given memory layout
 * pulling pages from the mem_list when necessary
 * If there are any gaps in the layout, we add them as unmapped pages
 */
vmm_pde_t * create_passthrough_pde32_pts(shadow_map_t * map) {
  ullong_t current_page_addr = 0;
  int i, j;


  vmm_pde_t * pde = os_hooks->allocate_pages(1);

  for (i = 0; i < MAX_PAGE_DIR_ENTRIES; i++) {
    int pte_present = 0;
    vmm_pte_t * pte = os_hooks->allocate_pages(1);
    

    for (j = 0; j < MAX_PAGE_TABLE_ENTRIES; j++) {
      shadow_region_t * region = get_shadow_region_by_addr(map, current_page_addr);

      if (!region || 
	  (region->host_type == HOST_REGION_NOTHING) || 
	  (region->host_type == HOST_REGION_UNALLOCATED) || 
	  (region->host_type == HOST_REGION_MEMORY_MAPPED_DEVICE) || 
	  (region->host_type == HOST_REGION_REMOTE) ||
	  (region->host_type == HOST_REGION_SWAPPED)) {
	pte[j].present = 0;
	pte[j].flags = 0;
	pte[j].accessed = 0;
	pte[j].dirty = 0;
	pte[j].pte_attr = 0;
	pte[j].global_page = 0;
	pte[j].vmm_info = 0;
	pte[j].page_base_addr = 0;
      } else {
	addr_t host_addr;
	pte[j].present = 1;
	pte[j].flags = VM_READ | VM_WRITE | VM_EXEC | VM_USER;   
	
	pte[j].accessed = 0;
	pte[j].dirty = 0;
	pte[j].pte_attr = 0;
	pte[j].global_page = 0;
	pte[j].vmm_info = 0;

	if (guest_paddr_to_host_paddr(region, current_page_addr, &host_addr) == -1) {
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
      VMMFree(pte);

      pde[i].present = 0;
      pde[i].flags = 0;
      pde[i].accessed = 0;
      pde[i].reserved = 0;
      pde[i].large_pages = 0;
      pde[i].global_page = 0;
      pde[i].vmm_info = 0;
      pde[i].pt_base_addr = 0;
    } else {
      pde[i].present = 1;
      pde[i].flags = VM_READ | VM_WRITE | VM_EXEC | VM_USER;
      pde[i].accessed = 0;
      pde[i].reserved = 0;
      pde[i].large_pages = 0;
      pde[i].global_page = 0;
      pde[i].vmm_info = 0;
      pde[i].pt_base_addr = PAGE_ALIGNED_ADDR(pte);
    }

  }

  return pde;
}







void PrintPDE(void * virtual_address, vmm_pde_t * pde)
{
  PrintDebug("PDE %p -> %p : present=%x, flags=%x, accessed=%x, reserved=%x, largePages=%x, globalPage=%x, kernelInfo=%x\n",
	      virtual_address,
	      (void *) (pde->pt_base_addr << PAGE_POWER),
	      pde->present,
	      pde->flags,
	      pde->accessed,
	      pde->reserved,
	      pde->large_pages,
	      pde->global_page,
	      pde->vmm_info);
}
  
void PrintPTE(void * virtual_address, vmm_pte_t * pte)
{
  PrintDebug("PTE %p -> %p : present=%x, flags=%x, accessed=%x, dirty=%x, pteAttribute=%x, globalPage=%x, vmm_info=%x\n",
	      virtual_address,
	      (void*)(pte->page_base_addr << PAGE_POWER),
	      pte->present,
	      pte->flags,
	      pte->accessed,
	      pte->dirty,
	      pte->pte_attr,
	      pte->global_page,
	      pte->vmm_info);
}



void PrintPD(vmm_pde_t * pde)
{
  int i;

  PrintDebug("Page Directory at %p:\n", pde);
  for (i = 0; (i < MAX_PAGE_DIR_ENTRIES) && pde[i].present; i++) { 
    PrintPDE((void*)(PAGE_SIZE * MAX_PAGE_TABLE_ENTRIES * i), &(pde[i]));
  }
}

void PrintPT(void * starting_address, vmm_pte_t * pte) 
{
  int i;

  PrintDebug("Page Table at %p:\n", pte);
  for (i = 0; (i < MAX_PAGE_TABLE_ENTRIES) && pte[i].present; i++) { 
    PrintPTE(starting_address + (PAGE_SIZE * i), &(pte[i]));
  }
}





void PrintDebugPageTables(vmm_pde_t * pde)
{
  int i;
  
  PrintDebug("Dumping the pages starting with the pde page at %p\n", pde);

  for (i = 0; (i < MAX_PAGE_DIR_ENTRIES) && pde[i].present; i++) { 
    PrintPDE((void *)(PAGE_SIZE * MAX_PAGE_TABLE_ENTRIES * i), &(pde[i]));
    PrintPT((void *)(PAGE_SIZE * MAX_PAGE_TABLE_ENTRIES * i), (void *)(pde[i].pt_base_addr << PAGE_POWER));
  }
}
    
    

#if 0

pml4e64_t * generate_guest_page_tables_64(vmm_mem_layout_t * layout, vmm_mem_list_t * list) {
  pml4e64_t * pml = os_hooks->allocate_pages(1);
  int i, j, k, m;
  ullong_t current_page_addr = 0;
  uint_t layout_index = 0;
  uint_t list_index = 0;
  ullong_t layout_addr = 0;
  uint_t num_entries = layout->num_pages;  // The number of pages left in the layout

  for (m = 0; m < MAX_PAGE_MAP_ENTRIES_64; m++ ) {
    if (num_entries == 0) {
      pml[m].present = 0;
      pml[m].writable = 0;
      pml[m].user = 0;
      pml[m].pwt = 0;
      pml[m].pcd = 0;
      pml[m].accessed = 0;
      pml[m].reserved = 0;
      pml[m].zero = 0;
      pml[m].vmm_info = 0;
      pml[m].pdp_base_addr_lo = 0;
      pml[m].pdp_base_addr_hi = 0;
      pml[m].available = 0;
      pml[m].no_execute = 0;
    } else {
      pdpe64_t * pdpe = os_hooks->allocate_pages(1);
      
      pml[m].present = 1;
      pml[m].writable = 1;
      pml[m].user = 1;
      pml[m].pwt = 0;
      pml[m].pcd = 0;
      pml[m].accessed = 0;
      pml[m].reserved = 0;
      pml[m].zero = 0;
      pml[m].vmm_info = 0;
      pml[m].pdp_base_addr_lo = PAGE_ALLIGNED_ADDR(pdpe) & 0xfffff;
      pml[m].pdp_base_addr_hi = 0;
      pml[m].available = 0;
      pml[m].no_execute = 0;

      for (k = 0; k < MAX_PAGE_DIR_PTR_ENTRIES_64; k++) {
	if (num_entries == 0) {
	  pdpe[k].present = 0;
	  pdpe[k].writable = 0;
	  pdpe[k].user = 0;
	  pdpe[k].pwt = 0;
	  pdpe[k].pcd = 0;
	  pdpe[k].accessed = 0;
	  pdpe[k].reserved = 0;
	  pdpe[k].large_pages = 0;
	  pdpe[k].zero = 0;
	  pdpe[k].vmm_info = 0;
	  pdpe[k].pd_base_addr_lo = 0;
	  pdpe[k].pd_base_addr_hi = 0;
	  pdpe[k].available = 0;
	  pdpe[k].no_execute = 0;
	} else {
	  pde64_t * pde = os_hooks->allocate_pages(1);

	  pdpe[k].present = 1;
	  pdpe[k].writable = 1;
	  pdpe[k].user = 1;
	  pdpe[k].pwt = 0;
	  pdpe[k].pcd = 0;
	  pdpe[k].accessed = 0;
	  pdpe[k].reserved = 0;
	  pdpe[k].large_pages = 0;
	  pdpe[k].zero = 0;
	  pdpe[k].vmm_info = 0;
	  pdpe[k].pd_base_addr_lo = PAGE_ALLIGNED_ADDR(pde) & 0xfffff;
	  pdpe[k].pd_base_addr_hi = 0;
	  pdpe[k].available = 0;
	  pdpe[k].no_execute = 0;



	  for (i = 0; i < MAX_PAGE_DIR_ENTRIES_64; i++) {
	    if (num_entries == 0) { 
	      pde[i].present = 0;
	      pde[i].flags = 0;
	      pde[i].accessed = 0;
	      pde[i].reserved = 0;
	      pde[i].large_pages = 0;
	      pde[i].reserved2 = 0;
	      pde[i].vmm_info = 0;
	      pde[i].pt_base_addr_lo = 0;
	      pde[i].pt_base_addr_hi = 0;
	      pde[i].available = 0;
	      pde[i].no_execute = 0;
	    } else {
	      pte64_t * pte = os_hooks->allocate_pages(1);
	      
	      pde[i].present = 1;
	      pde[i].flags = VM_READ | VM_WRITE | VM_EXEC | VM_USER;
	      pde[i].accessed = 0;
	      pde[i].reserved = 0;
	      pde[i].large_pages = 0;
	      pde[i].reserved2 = 0;
	      pde[i].vmm_info = 0;
	      pde[i].pt_base_addr_lo = PAGE_ALLIGNED_ADDR(pte) & 0xfffff;
	      pde[i].pt_base_addr_hi = 0;
	      pde[i].available = 0;
	      pde[i].no_execute = 0;

	      
	      for (j = 0; j < MAX_PAGE_TABLE_ENTRIES_64; j++) {
		layout_addr = get_mem_layout_addr(layout, layout_index);
		
		if ((current_page_addr < layout_addr) || (num_entries == 0)) {
		  // We have a gap in the layout, fill with unmapped page
		  pte[j].present = 0;
		  pte[j].flags = 0;
		  pte[j].accessed = 0;
		  pte[j].dirty = 0;
		  pte[j].pte_attr = 0;
		  pte[j].global_page = 0;
		  pte[j].vmm_info = 0;
		  pte[j].page_base_addr_lo = 0;
		  pte[j].page_base_addr_hi = 0;
		  pte[j].available = 0;
		  pte[j].no_execute = 0;

		  current_page_addr += PAGE_SIZE;
		} else if (current_page_addr == layout_addr) {
		  // Set up the Table entry to map correctly to the layout region
		  layout_region_t * page_region = get_mem_layout_region(layout, layout_addr);
		  
		  if (page_region->type == UNMAPPED) {
		    pte[j].present = 0;
		    pte[j].flags = 0;
		  } else {
		    pte[j].present = 1;
		    pte[j].flags = VM_READ | VM_WRITE | VM_EXEC | VM_USER;
		  }	    
		  
		  pte[j].accessed = 0;
		  pte[j].dirty = 0;
		  pte[j].pte_attr = 0;
		  pte[j].global_page = 0;
		  pte[j].vmm_info = 0;
		  pte[j].available = 0;
		  pte[j].no_execute = 0;

		  if (page_region->type == UNMAPPED) {
		    pte[j].page_base_addr_lo = 0;
		    pte[j].page_base_addr_hi = 0;
		  } else if (page_region->type == SHARED) {
		    addr_t host_addr = page_region->host_addr + (layout_addr - page_region->start);
		    
		    pte[j].page_base_addr_lo = PAGE_ALLIGNED_ADDR(host_addr) & 0xfffff;
		    pte[j].page_base_addr_hi = 0;
		    pte[j].vmm_info = SHARED_PAGE;
		  } else if (page_region->type == GUEST) {
		    addr_t list_addr =  get_mem_list_addr(list, list_index++);
		    
		    if (list_addr == -1) {
		      // error
		      // cleanup...
		      //free_guest_page_tables(pde);
		      return NULL;
		    }
		    PrintDebug("Adding guest page (%x)\n", list_addr);
		    pte[j].page_base_addr_lo = PAGE_ALLIGNED_ADDR(list_addr) & 0xfffff;
		    pte[j].page_base_addr_hi = 0;

		    // Reset this when we move over to dynamic page allocation
		    //	    pte[j].vmm_info = GUEST_PAGE;	    
		    pte[j].vmm_info = SHARED_PAGE;
		  }
		  
		  num_entries--;
		  current_page_addr += PAGE_SIZE;
		  layout_index++;
		} else {
		  // error
		  PrintDebug("Error creating page table...\n");
		  // cleanup
		  //		  free_guest_page_tables64(pde);
		  return NULL;
		}
	      }
	    }
	  }
	}
      }
    }
  }
  return pml;
}

#endif
