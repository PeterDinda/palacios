#include <geekos/vmm_shadow_paging.h>

#include <geekos/vmm.h>

extern struct vmm_os_hooks * os_hooks;


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

  
  for (i = 0; i < MAX_PDE32_ENTRIES; i++) { 
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
      for (j = 0; j < MAX_PTE32_ENTRIES; j++) { 
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
      

