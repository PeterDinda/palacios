
static int cache_page_tables_32(struct guest_info * info, addr_t pde) {
  struct shadow_page_state * state = &(info->shdw_pg_state);
  addr_t pde_host_addr;
  pde32_t * tmp_pde;
  struct hashtable * pte_cache = NULL;
  int i = 0;

  if (pde == state->cached_cr3) {
    return 1;
  }

  if (state->cached_ptes != NULL) {
    hashtable_destroy(state->cached_ptes, 0, 0);
    state->cached_ptes = NULL;
  }

  state->cached_cr3 = pde;

  pte_cache = create_hashtable(0, &pte_hash_fn, &pte_equals);
  state->cached_ptes = pte_cache;

  if (guest_pa_to_host_va(info, pde, &pde_host_addr) == -1) {
    PrintError("Could not lookup host address of guest PDE\n");
    return -1;
  }

  tmp_pde = (pde32_t *)pde_host_addr;

  add_pte_map(pte_cache, pde, pde_host_addr);


  for (i = 0; i < MAX_PDE32_ENTRIES; i++) {
    if ((tmp_pde[i].present) && (tmp_pde[i].large_page == 0)) {
      addr_t pte_host_addr;

      if (guest_pa_to_host_va(info, (addr_t)(BASE_TO_PAGE_ADDR(tmp_pde[i].pt_base_addr)), &pte_host_addr) == -1) {
	PrintError("Could not lookup host address of guest PDE\n");
	return -1;
      }

      add_pte_map(pte_cache, (addr_t)(BASE_TO_PAGE_ADDR(tmp_pde[i].pt_base_addr)), pte_host_addr); 
    }
  }

  return 0;

}



// We assume that shdw_pg_state.guest_cr3 is pointing to the page tables we want to activate
// We also assume that the CPU mode has not changed during this page table transition
static int activate_shadow_pt_32(struct guest_info * info) {
  struct cr3_32 * shadow_cr3 = (struct cr3_32 *)&(info->ctrl_regs.cr3);
  struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);
  int cached = 0;
  
  // Check if shadow page tables are in the cache
  cached = cache_page_tables_32(info, CR3_TO_PDE32_PA(*(addr_t *)guest_cr3));
  
  if (cached == -1) {
    PrintError("CR3 Cache failed\n");
    return -1;
  } else if (cached == 0) {
    addr_t shadow_pt;
    
    PrintDebug("New CR3 is different - flushing shadow page table %p\n", shadow_cr3 );
    delete_page_tables_32(CR3_TO_PDE32_VA(*(uint_t*)shadow_cr3));
    
    shadow_pt = create_new_shadow_pt();
    
    shadow_cr3->pdt_base_addr = (addr_t)V3_PAddr((void *)(addr_t)PAGE_BASE_ADDR(shadow_pt));
    PrintDebug( "Created new shadow page table %p\n", (void *)(addr_t)shadow_cr3->pdt_base_addr );
  } else {
    PrintDebug("Reusing cached shadow Page table\n");
  }
  
  shadow_cr3->pwt = guest_cr3->pwt;
  shadow_cr3->pcd = guest_cr3->pcd;
  
  return 0;
}

/* 
 * *
 * * 
 * * 32 bit Page table fault handlers
 * *
 * *
 */
static int handle_large_pagefault_32(struct guest_info * info, 
				    addr_t fault_addr, pf_error_t error_code, 
				     pte32_t * shadow_pt, pde32_4MB_t * large_guest_pde);

static int handle_shadow_pte32_fault(struct guest_info * info, 
				     addr_t fault_addr, 
				     pf_error_t error_code,
				     pte32_t * shadow_pt, 
				     pte32_t * guest_pt);


static int handle_shadow_pagefault_32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  pde32_t * guest_pd = NULL;
  pde32_t * shadow_pd = CR3_TO_PDE32_VA(info->ctrl_regs.cr3);
  addr_t guest_cr3 = CR3_TO_PDE32_PA(info->shdw_pg_state.guest_cr3);
  pt_access_status_t guest_pde_access;
  pt_access_status_t shadow_pde_access;
  pde32_t * guest_pde = NULL;
  pde32_t * shadow_pde = (pde32_t *)&(shadow_pd[PDE32_INDEX(fault_addr)]);

  PrintDebug("Shadow page fault handler: %p\n", (void*) fault_addr );

  if (guest_pa_to_host_va(info, guest_cr3, (addr_t*)&guest_pd) == -1) {
    PrintError("Invalid Guest PDE Address: 0x%p\n",  (void *)guest_cr3);
    return -1;
  } 

  guest_pde = (pde32_t *)&(guest_pd[PDE32_INDEX(fault_addr)]);


  // Check the guest page permissions
  guest_pde_access = v3_can_access_pde32(guest_pd, fault_addr, error_code);

  // Check the shadow page permissions
  shadow_pde_access = v3_can_access_pde32(shadow_pd, fault_addr, error_code);
  
  /* Was the page fault caused by the Guest's page tables? */
  if (is_guest_pf(guest_pde_access, shadow_pde_access) == 1) {
    PrintDebug("Injecting PDE pf to guest: (guest access error=%d) (pf error code=%d)\n", 
	       *(uint_t *)&guest_pde_access, *(uint_t *)&error_code);
    inject_guest_pf(info, fault_addr, error_code);
    return 0;
  }

  
  if (shadow_pde_access == PT_ACCESS_NOT_PRESENT) 
    {
      pte32_t * shadow_pt =  (pte32_t *)create_new_shadow_pt();

      shadow_pde->present = 1;
      shadow_pde->user_page = guest_pde->user_page;
      //    shadow_pde->large_page = guest_pde->large_page;
      shadow_pde->large_page = 0;
      

      // VMM Specific options
      shadow_pde->write_through = 0;
      shadow_pde->cache_disable = 0;
      shadow_pde->global_page = 0;
      //
      
      guest_pde->accessed = 1;
      
      shadow_pde->pt_base_addr = PAGE_BASE_ADDR((addr_t)V3_PAddr(shadow_pt));
      
      if (guest_pde->large_page == 0) {
	pte32_t * guest_pt = NULL;
	shadow_pde->writable = guest_pde->writable;

	if (guest_pa_to_host_va(info, BASE_TO_PAGE_ADDR(guest_pde->pt_base_addr), (addr_t*)&guest_pt) == -1) {
	  // Machine check the guest
	  PrintDebug("Invalid Guest PTE Address: 0x%p\n", (void *)BASE_TO_PAGE_ADDR(guest_pde->pt_base_addr));
	  v3_raise_exception(info, MC_EXCEPTION);
	  return 0;
	}

	if (handle_shadow_pte32_fault(info, fault_addr, error_code, shadow_pt, guest_pt)  == -1) {
	  PrintError("Error handling Page fault caused by PTE\n");
	  return -1;
	}
      } else {
	// ??  What if guest pde is dirty a this point?
	((pde32_4MB_t *)guest_pde)->dirty = 0;
	shadow_pde->writable = 0;
      }
    }
  else if (shadow_pde_access == PT_ACCESS_OK) 
    {
      //
      // PTE fault
      //
      pte32_t * shadow_pt = (pte32_t *)V3_VAddr( (void*)(addr_t) BASE_TO_PAGE_ADDR(shadow_pde->pt_base_addr) );

      if (guest_pde->large_page == 0) {
	pte32_t * guest_pt = NULL;

	if (guest_pa_to_host_va(info, BASE_TO_PAGE_ADDR(guest_pde->pt_base_addr), (addr_t*)&guest_pt) == -1) {
	  // Machine check the guest
	  PrintDebug("Invalid Guest PTE Address: 0x%p\n", (void *)BASE_TO_PAGE_ADDR(guest_pde->pt_base_addr));
	  v3_raise_exception(info, MC_EXCEPTION);
	  return 0;
	}
	
	if (handle_shadow_pte32_fault(info, fault_addr, error_code, shadow_pt, guest_pt)  == -1) {
	  PrintError("Error handling Page fault caused by PTE\n");
	  return -1;
	}
      } else if (guest_pde->large_page == 1) {
	if (handle_large_pagefault_32(info, fault_addr, error_code, shadow_pt, (pde32_4MB_t *)guest_pde) == -1) {
	  PrintError("Error handling large pagefault\n");
	  return -1;
	}
      }
    }
  else if ((shadow_pde_access == PT_ACCESS_WRITE_ERROR) && 
	   (guest_pde->large_page == 1) && 
	   (((pde32_4MB_t *)guest_pde)->dirty == 0)) 
    {
      //
      // Page Directory Entry marked read-only
      // Its a large page and we need to update the dirty bit in the guest
      //

      PrintDebug("Large page write error... Setting dirty bit and returning\n");
      ((pde32_4MB_t *)guest_pde)->dirty = 1;
      shadow_pde->writable = guest_pde->writable;
      return 0;
      
    } 
  else if (shadow_pde_access == PT_ACCESS_USER_ERROR) 
    {
      //
      // Page Directory Entry marked non-user
      //      
      PrintDebug("Shadow Paging User access error (shadow_pde_access=0x%x, guest_pde_access=0x%x)\n", 
		 shadow_pde_access, guest_pde_access);
      inject_guest_pf(info, fault_addr, error_code);
      return 0;
    }
  else 
    {
      // inject page fault in guest
      inject_guest_pf(info, fault_addr, error_code);
      PrintDebug("Unknown Error occurred (shadow_pde_access=%d)\n", shadow_pde_access);
      PrintDebug("Manual Says to inject page fault into guest\n");
#ifdef DEBUG_SHADOW_PAGING
      PrintDebug("Guest PDE: (access=%d)\n\t", guest_pde_access);
      PrintPTEntry(PAGE_PD32, fault_addr, guest_pde);
      PrintDebug("Shadow PDE: (access=%d)\n\t", shadow_pde_access);
      PrintPTEntry(PAGE_PD32, fault_addr, shadow_pde);
#endif

      return 0; 
    }

  PrintDebug("Returning end of PDE function (rip=%p)\n", (void *)(addr_t)(info->rip));
  return 0;
}



/* The guest status checks have already been done,
 * only special case shadow checks remain
 */
static int handle_large_pagefault_32(struct guest_info * info, 
				    addr_t fault_addr, pf_error_t error_code, 
				    pte32_t * shadow_pt, pde32_4MB_t * large_guest_pde) 
{
  pt_access_status_t shadow_pte_access = v3_can_access_pte32(shadow_pt, fault_addr, error_code);
  pte32_t * shadow_pte = (pte32_t *)&(shadow_pt[PTE32_INDEX(fault_addr)]);
  addr_t guest_fault_pa = BASE_TO_PAGE_ADDR_4MB(large_guest_pde->page_base_addr) + PAGE_OFFSET_4MB(fault_addr);  

  struct v3_shadow_region * shdw_reg = v3_get_shadow_region(info, guest_fault_pa);

 
  if ((shdw_reg == NULL) || 
      (shdw_reg->host_type == SHDW_REGION_INVALID)) {
    // Inject a machine check in the guest
    PrintDebug("Invalid Guest Address in page table (0x%p)\n", (void *)guest_fault_pa);
    v3_raise_exception(info, MC_EXCEPTION);
    return -1;
  }

  if (shadow_pte_access == PT_ACCESS_OK) {
    // Inconsistent state...
    // Guest Re-Entry will flush tables and everything should now workd
    PrintDebug("Inconsistent state... Guest re-entry should flush tlb\n");
    return 0;
  }

  
  if (shadow_pte_access == PT_ACCESS_NOT_PRESENT) {
    // Get the guest physical address of the fault

    if ((shdw_reg->host_type == SHDW_REGION_ALLOCATED) || 
	(shdw_reg->host_type == SHDW_REGION_WRITE_HOOK)) {
      struct shadow_page_state * state = &(info->shdw_pg_state);
      addr_t shadow_pa = v3_get_shadow_addr(shdw_reg, guest_fault_pa);

      shadow_pte->page_base_addr = PAGE_BASE_ADDR(shadow_pa);

      shadow_pte->present = 1;

      /* We are assuming that the PDE entry has precedence
       * so the Shadow PDE will mirror the guest PDE settings, 
       * and we don't have to worry about them here
       * Allow everything
       */
      shadow_pte->user_page = 1;

      if (find_pte_map(state->cached_ptes, PAGE_ADDR(guest_fault_pa)) != NULL) {
	// Check if the entry is a page table...
	PrintDebug("Marking page as Guest Page Table (large page)\n");
	shadow_pte->vmm_info = PT32_GUEST_PT;
	shadow_pte->writable = 0;
      } else if (shdw_reg->host_type == SHDW_REGION_WRITE_HOOK) {
	shadow_pte->writable = 0;
      } else {
	shadow_pte->writable = 1;
      }

      //set according to VMM policy
      shadow_pte->write_through = 0;
      shadow_pte->cache_disable = 0;
      shadow_pte->global_page = 0;
      //
      
    } else {
      // Handle hooked pages as well as other special pages
      //      if (handle_special_page_fault(info, fault_addr, guest_fault_pa, error_code) == -1) {

      if (v3_handle_mem_full_hook(info, fault_addr, guest_fault_pa, shdw_reg, error_code) == -1) {
	PrintError("Special Page Fault handler returned error for address: %p\n", (void *)fault_addr);
	return -1;
      }
    }
  } else if (shadow_pte_access == PT_ACCESS_WRITE_ERROR) {

    if (shdw_reg->host_type == SHDW_REGION_WRITE_HOOK) {

      if (v3_handle_mem_wr_hook(info, fault_addr, guest_fault_pa, shdw_reg, error_code) == -1) {
	PrintError("Special Page Fault handler returned error for address: %p\n", (void *)fault_addr);
	return -1;
      }
    } else if (shadow_pte->vmm_info == PT32_GUEST_PT) {
      struct shadow_page_state * state = &(info->shdw_pg_state);
      PrintDebug("Write operation on Guest PAge Table Page (large page)\n");
      state->cached_cr3 = 0;
      shadow_pte->writable = 1;
    }

  } else {
    PrintError("Error in large page fault handler...\n");
    PrintError("This case should have been handled at the top level handler\n");
    return -1;
  }

  PrintDebug("Returning from large page fault handler\n");
  return 0;
}




/* 
 * We assume the the guest pte pointer has already been translated to a host virtual address
 */
static int handle_shadow_pte32_fault(struct guest_info * info, 
				     addr_t fault_addr, 
				     pf_error_t error_code,
				     pte32_t * shadow_pt, 
				     pte32_t * guest_pt) {

  pt_access_status_t guest_pte_access;
  pt_access_status_t shadow_pte_access;
  pte32_t * guest_pte = (pte32_t *)&(guest_pt[PTE32_INDEX(fault_addr)]);;
  pte32_t * shadow_pte = (pte32_t *)&(shadow_pt[PTE32_INDEX(fault_addr)]);
  addr_t guest_pa = BASE_TO_PAGE_ADDR((addr_t)(guest_pte->page_base_addr)) +  PAGE_OFFSET(fault_addr);

  struct v3_shadow_region * shdw_reg =  v3_get_shadow_region(info, guest_pa);

  if ((shdw_reg == NULL) || 
      (shdw_reg->host_type == SHDW_REGION_INVALID)) {
    // Inject a machine check in the guest
    PrintDebug("Invalid Guest Address in page table (0x%p)\n", (void *)guest_pa);
    v3_raise_exception(info, MC_EXCEPTION);
    return 0;
  }

  // Check the guest page permissions
  guest_pte_access = v3_can_access_pte32(guest_pt, fault_addr, error_code);

  // Check the shadow page permissions
  shadow_pte_access = v3_can_access_pte32(shadow_pt, fault_addr, error_code);
  
#ifdef DEBUG_SHADOW_PAGING
  PrintDebug("Guest PTE: (access=%d)\n\t", guest_pte_access);
  PrintPTEntry(PAGE_PT32, fault_addr, guest_pte);
  PrintDebug("Shadow PTE: (access=%d)\n\t", shadow_pte_access);
  PrintPTEntry(PAGE_PT32, fault_addr, shadow_pte);
#endif
  
  /* Was the page fault caused by the Guest's page tables? */
  if (is_guest_pf(guest_pte_access, shadow_pte_access) == 1) {
    PrintDebug("Access error injecting pf to guest (guest access error=%d) (pf error code=%d)\n", 
	       guest_pte_access, *(uint_t*)&error_code);    
    inject_guest_pf(info, fault_addr, error_code);
    return 0; 
  }
  
  
  if (shadow_pte_access == PT_ACCESS_OK) {
    // Inconsistent state...
    // Guest Re-Entry will flush page tables and everything should now work
    PrintDebug("Inconsistent state... Guest re-entry should flush tlb\n");
    return 0;
  }


  if (shadow_pte_access == PT_ACCESS_NOT_PRESENT) {
    // Page Table Entry Not Present
    PrintDebug("guest_pa =%p\n", (void *)guest_pa);

    if ((shdw_reg->host_type == SHDW_REGION_ALLOCATED) ||
	(shdw_reg->host_type == SHDW_REGION_WRITE_HOOK)) {
      struct shadow_page_state * state = &(info->shdw_pg_state);
      addr_t shadow_pa = v3_get_shadow_addr(shdw_reg, guest_pa);
      
      shadow_pte->page_base_addr = PAGE_BASE_ADDR(shadow_pa);
      
      shadow_pte->present = guest_pte->present;
      shadow_pte->user_page = guest_pte->user_page;
      
      //set according to VMM policy
      shadow_pte->write_through = 0;
      shadow_pte->cache_disable = 0;
      shadow_pte->global_page = 0;
      //
      
      guest_pte->accessed = 1;
      
      if (find_pte_map(state->cached_ptes, PAGE_ADDR(guest_pa)) != NULL) {
	// Check if the entry is a page table...
	PrintDebug("Marking page as Guest Page Table %d\n", shadow_pte->writable);
	shadow_pte->vmm_info = PT32_GUEST_PT;
      }

      if (shdw_reg->host_type == SHDW_REGION_WRITE_HOOK) {
	shadow_pte->writable = 0;
      } else if (guest_pte->dirty == 1) {
	shadow_pte->writable = guest_pte->writable;
      } else if ((guest_pte->dirty == 0) && (error_code.write == 1)) {
	shadow_pte->writable = guest_pte->writable;
	guest_pte->dirty = 1;
	
	if (shadow_pte->vmm_info == PT32_GUEST_PT) {
	  // Well that was quick...
	  struct shadow_page_state * state = &(info->shdw_pg_state);
	  PrintDebug("Immediate Write operation on Guest PAge Table Page\n");
	  state->cached_cr3 = 0;
	}

      } else if ((guest_pte->dirty == 0) && (error_code.write == 0)) {  // was =
	shadow_pte->writable = 0;
      }

    } else {
      // Page fault handled by hook functions

      if (v3_handle_mem_full_hook(info, fault_addr, guest_pa, shdw_reg, error_code) == -1) {
	PrintError("Special Page fault handler returned error for address: %p\n",  (void *)fault_addr);
	return -1;
      }
    }
  } else if (shadow_pte_access == PT_ACCESS_WRITE_ERROR) {
    guest_pte->dirty = 1;

    if (shdw_reg->host_type == SHDW_REGION_WRITE_HOOK) {
      if (v3_handle_mem_wr_hook(info, fault_addr, guest_pa, shdw_reg, error_code) == -1) {
	PrintError("Special Page fault handler returned error for address: %p\n",  (void *)fault_addr);
	return -1;
      }
    } else {
      PrintDebug("Shadow PTE Write Error\n");
      shadow_pte->writable = guest_pte->writable;
    }

    if (shadow_pte->vmm_info == PT32_GUEST_PT) {
      struct shadow_page_state * state = &(info->shdw_pg_state);
      PrintDebug("Write operation on Guest PAge Table Page\n");
      state->cached_cr3 = 0;
    }

    return 0;

  } else {
    // Inject page fault into the guest	
    inject_guest_pf(info, fault_addr, error_code);
    PrintError("PTE Page fault fell through... Not sure if this should ever happen\n");
    PrintError("Manual Says to inject page fault into guest\n");
    return -1;
  }

  PrintDebug("Returning end of function\n");
  return 0;
}
