/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


static inline int activate_shadow_pt_32(struct guest_info * core) {
    struct cr3_32 * shadow_cr3 = (struct cr3_32 *)&(core->ctrl_regs.cr3);
    struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(core->shdw_pg_state.guest_cr3);
    addr_t gpa = BASE_TO_PAGE_ADDR_4KB(guest_cr3->pdt_base_addr);
    struct shdw_pg_data * shdw_pg = NULL;

    PrintDebug("Activating 32 Bit cacheable page tables\n");
    shdw_pg = find_shdw_pt(core->vm_info, gpa, PAGE_PD32);
    
    PrintError("shdw_pg returned as %p for CR3:%p\n", shdw_pg, (void *)gpa);

    if (shdw_pg == NULL) {
	shdw_pg = create_shdw_pt(core->vm_info, gpa, PAGE_PD32);

	// update current reverse map entries...
	// We are now using this page in a PT, so:
	//     any existing writable mappings must be updated
	update_rmap_entries(core->vm_info, gpa);
    }

    PrintDebug("shdw_pg now exists...\n");

    shadow_cr3->pdt_base_addr = PAGE_BASE_ADDR_4KB(shdw_pg->hpa);
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
/*
static int handle_4MB_shadow_pagefault_pde_32(struct guest_info * core,  addr_t fault_addr, pf_error_t error_code, 
					      pt_access_status_t shadow_pde_access, pde32_4MB_t * large_shadow_pde, 
					      pde32_4MB_t * large_guest_pde);
*/
static int handle_4MB_shadow_pagefault_pte_32(struct guest_info * core,  addr_t fault_addr, pf_error_t error_code, 
					      pte32_t * shadow_pt, pde32_4MB_t * large_guest_pde,  struct shdw_pg_data * pt_pg_data);

static int handle_pte_shadow_pagefault_32(struct guest_info * core, addr_t fault_addr, pf_error_t error_code,
					  pte32_t * shadow_pt,  pte32_t * guest_pt,  struct shdw_pg_data * pt_pg_data);




static inline int handle_shadow_pagefault_32(struct guest_info * core, addr_t fault_addr, pf_error_t error_code) {
    pde32_t * guest_pd = NULL;
    pde32_t * shadow_pd = CR3_TO_PDE32_VA(core->ctrl_regs.cr3);
    addr_t guest_cr3 = CR3_TO_PDE32_PA(core->shdw_pg_state.guest_cr3);
    pt_access_status_t guest_pde_access;
    pt_access_status_t shadow_pde_access;
    pde32_t * guest_pde = NULL;
    pde32_t * shadow_pde = (pde32_t *)&(shadow_pd[PDE32_INDEX(fault_addr)]);

    PrintDebug("Shadow cache page fault handler: %p\n", (void *)fault_addr );
    PrintDebug("Handling PDE32 Fault\n");

    if (v3_gpa_to_hva(core, guest_cr3, (addr_t*)&guest_pd) == -1) {
	PrintError("Invalid Guest PDE Address: 0x%p\n",  (void *)guest_cr3);
	return -1;
    } 

    guest_pde = (pde32_t *)&(guest_pd[PDE32_INDEX(fault_addr)]);

 // Check the guest page permissions
    guest_pde_access = v3_can_access_pde32(guest_pd, fault_addr, error_code);

    // Check the shadow page permissions
    shadow_pde_access = v3_can_access_pde32(shadow_pd, fault_addr, error_code);
  
    /* Was the page fault caused by the Guest's page tables? */
    if (v3_is_guest_pf(guest_pde_access, shadow_pde_access) == 1) {
	PrintDebug("Injecting PDE pf to guest: (guest access error=%d) (shdw access error=%d)  (pf error code=%d)\n", 
		   *(uint_t *)&guest_pde_access, *(uint_t *)&shadow_pde_access, *(uint_t *)&error_code);
	if (v3_inject_guest_pf(core, fault_addr, error_code) == -1) {
	    PrintError("Could not inject guest page fault for vaddr %p\n", (void *)fault_addr);
	    return -1;
	}
	return 0;
    }



    if (shadow_pde_access == PT_ACCESS_USER_ERROR) {
	// 
	// PDE Entry marked non user
	//
	PrintDebug("Shadow Paging User access error (shadow_pde_access=0x%x, guest_pde_access=0x%x)\n", 
		   shadow_pde_access, guest_pde_access);
	
	if (v3_inject_guest_pf(core, fault_addr, error_code) == -1) {
	    PrintError("Could not inject guest page fault for vaddr %p\n", (void *)fault_addr);
	    return -1;
	}
	return 0;
    } else if ((shadow_pde_access == PT_ACCESS_WRITE_ERROR) && 
	       (guest_pde->large_page == 1)) {
	
	((pde32_4MB_t *)guest_pde)->dirty = 1;
	shadow_pde->writable = guest_pde->writable;
	return 0;
    } else if ((shadow_pde_access != PT_ACCESS_NOT_PRESENT) &&
	       (shadow_pde_access != PT_ACCESS_OK)) {
    	// inject page fault in guest
	if (v3_inject_guest_pf(core, fault_addr, error_code) == -1) {
	    PrintError("Could not inject guest page fault for vaddr %p\n", (void *)fault_addr);
	    return -1;
	}
	PrintDebug("Unknown Error occurred (shadow_pde_access=%d)\n", shadow_pde_access);
	PrintDebug("Manual Says to inject page fault into guest\n");
	return 0;
    }


    pte32_t * shadow_pt = NULL;
    pte32_t * guest_pt = NULL;


    /*  Set up cache state */
    addr_t gpa = BASE_TO_PAGE_ADDR_4KB(guest_pde->pt_base_addr);


    struct shdw_pg_data * shdw_page = NULL;
    page_type_t pt_type = PAGE_PT32;

    if (guest_pde->large_page == 1) {
	// Handle Large pages, for this we use the PAGE_4MB type...
	pt_type = PAGE_4MB;
    }

    shdw_page = find_shdw_pt(core->vm_info, gpa, pt_type);
	
    if (shdw_page == NULL) {
	shdw_page = create_shdw_pt(core->vm_info, gpa, pt_type);

	if (pt_type == PAGE_PT32) {
	    // update current reverse map entries...
	    // We are now using this page in a PT, so:
	    //     any existing writable mappings must be updated
	    update_rmap_entries(core->vm_info, gpa);
	}
    }    

    
    struct shdw_pg_data * parent_page = find_shdw_pt(core->vm_info, guest_cr3, PAGE_PD32);
    
    if (parent_page != NULL) {
	// add back pointer to PDE, if it exists
	link_shdw_pg(shdw_page, parent_page, PAGE_ADDR_4KB(fault_addr));
    }


    // Get the next shadow page  level, allocate if not present

    if (shadow_pde_access == PT_ACCESS_NOT_PRESENT) {

	/* Currently we do not support large pages
	   This requires us to scan the large page for Page table pages, and split the entries if they exist. 
	   Its easier to just ignore this for now...
 
	   if ((core->use_large_pages == 1) && (guest_pde->large_page == 1)) {
	   // Check underlying physical memory map to see if a large page is viable
	   addr_t gpa_4MB = BASE_TO_PAGE_ADDR_4MB(((pde32_4MB_t *)guest_pde)->page_base_addr);
	   uint32_t page_size = v3_get_max_page_size(core, gpa_4MB, PROTECTED);
	   
	   if (page_size == PAGE_SIZE_4MB) {
	   PrintDebug("using large page for fault_addr %p (gpa=%p)\n", (void *)fault_addr, (void *)gpa_4MB); 
	   if (handle_4MB_shadow_pagefault_pde_32(core, fault_addr, error_code, shadow_pde_access,
	   (pde32_4MB_t *)shadow_pde, (pde32_4MB_t *)guest_pde) == -1) {
	   PrintError("Error handling large pagefault with large page\n");
	   return -1;
	   }
	   
	   return 0;
	   }
	   }
	*/



	
	shadow_pt = (pte32_t *)(shdw_page->hva);



	shadow_pde->present = 1;
	shadow_pde->user_page = guest_pde->user_page;


	if (guest_pde->large_page == 0) {
	    shadow_pde->writable = guest_pde->writable;
	} else {
	    // This large page flag is temporary until we can get a working cache....
	    ((pde32_4MB_t *)guest_pde)->vmm_info = V3_LARGE_PG;

	    if (error_code.write) {
		shadow_pde->writable = guest_pde->writable;
		((pde32_4MB_t *)guest_pde)->dirty = 1;
	    } else {
		shadow_pde->writable = 0;
		((pde32_4MB_t *)guest_pde)->dirty = 0;
	    }
	}
      
	// VMM Specific options
	shadow_pde->write_through = guest_pde->write_through;
	shadow_pde->cache_disable = guest_pde->cache_disable;
	shadow_pde->global_page = guest_pde->global_page;
	//
      
	guest_pde->accessed = 1;
      
	shadow_pde->pt_base_addr = PAGE_BASE_ADDR(shdw_page->hpa);
    } else {
	shadow_pt = (pte32_t *)V3_VAddr((void *)BASE_TO_PAGE_ADDR(shadow_pde->pt_base_addr));
    }

    
    if (guest_pde->large_page == 0) {
	if (v3_gpa_to_hva(core, BASE_TO_PAGE_ADDR(guest_pde->pt_base_addr), (addr_t*)&guest_pt) == -1) {
	    // Machine check the guest
	    PrintDebug("Invalid Guest PTE Address: 0x%p\n", (void *)BASE_TO_PAGE_ADDR(guest_pde->pt_base_addr));
	    v3_raise_exception(core, MC_EXCEPTION);
	    return 0;
	}

	if (handle_pte_shadow_pagefault_32(core, fault_addr, error_code, shadow_pt, guest_pt, shdw_page)  == -1) {
	    PrintError("Error handling Page fault caused by PTE\n");
	    return -1;
	}
    } else {
	if (handle_4MB_shadow_pagefault_pte_32(core, fault_addr, error_code, shadow_pt, (pde32_4MB_t *)guest_pde, shdw_page) == -1) {
	    PrintError("Error handling large pagefault\n");
	    return -1;
	}	
    }

    return 0;
}




static int handle_pte_shadow_pagefault_32(struct guest_info * core, addr_t fault_addr, pf_error_t error_code,
					  pte32_t * shadow_pt, pte32_t * guest_pt, struct shdw_pg_data * pt_pg_data) {

    pt_access_status_t guest_pte_access;
    pt_access_status_t shadow_pte_access;
    pte32_t * guest_pte = (pte32_t *)&(guest_pt[PTE32_INDEX(fault_addr)]);;
    pte32_t * shadow_pte = (pte32_t *)&(shadow_pt[PTE32_INDEX(fault_addr)]);
    addr_t guest_pa = BASE_TO_PAGE_ADDR((addr_t)(guest_pte->page_base_addr)) +  PAGE_OFFSET(fault_addr);

    struct v3_mem_region * shdw_reg =  v3_get_mem_region(core->vm_info, core->vcpu_id, guest_pa);

    if (shdw_reg == NULL) {
	// Inject a machine check in the guest
	PrintDebug("Invalid Guest Address in page table (0x%p)\n", (void *)guest_pa);
	v3_raise_exception(core, MC_EXCEPTION);
	return 0;
    }

    // Check the guest page permissions
    guest_pte_access = v3_can_access_pte32(guest_pt, fault_addr, error_code);

    // Check the shadow page permissions
    shadow_pte_access = v3_can_access_pte32(shadow_pt, fault_addr, error_code);
  
  
    /* Was the page fault caused by the Guest's page tables? */
    if (v3_is_guest_pf(guest_pte_access, shadow_pte_access) == 1) {

	PrintDebug("Access error injecting pf to guest (guest access error=%d) (pf error code=%d)\n", 
		   guest_pte_access, *(uint_t*)&error_code);
	

	//   inject:
	if (v3_inject_guest_pf(core, fault_addr, error_code) == -1) {
	    PrintError("Could not inject guest page fault for vaddr %p\n", (void *)fault_addr);
	    return -1;
	}	

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

	if ((shdw_reg->flags.alloced == 1) && (shdw_reg->flags.read == 1)) {
	    addr_t shadow_pa = 0;

	    if (v3_gpa_to_hpa(core, guest_pa, &shadow_pa) == -1) {
		PrintError("could not translate page fault address (%p)\n", (void *)guest_pa);
		return -1;
	    }

	    shadow_pte->page_base_addr = PAGE_BASE_ADDR(shadow_pa);

	    PrintDebug("\tMapping shadow page (%p)\n", (void *)BASE_TO_PAGE_ADDR(shadow_pte->page_base_addr));
      
	    shadow_pte->present = guest_pte->present;
	    shadow_pte->user_page = guest_pte->user_page;
      
	    //set according to VMM policy
	    shadow_pte->write_through = guest_pte->write_through;
	    shadow_pte->cache_disable = guest_pte->cache_disable;
	    shadow_pte->global_page = guest_pte->global_page;
	    //
      
	    guest_pte->accessed = 1;
      
	    if (guest_pte->dirty == 1) {
		shadow_pte->writable = guest_pte->writable;
	    } else if ((guest_pte->dirty == 0) && (error_code.write == 1)) {
		shadow_pte->writable = guest_pte->writable;
		guest_pte->dirty = 1;
	    } else if ((guest_pte->dirty == 0) && (error_code.write == 0)) {
		shadow_pte->writable = 0;
	    }


	    if (shdw_reg->flags.write == 0) {
		shadow_pte->writable = 0;
	    }


	    // Add this PTE to the reverse map...
	    // This allows us to update this PTE entry if it gets turned into a PT page
	    add_rmap(core->vm_info, pt_pg_data, PAGE_ADDR_4KB(guest_pa), PAGE_ADDR_4KB(fault_addr));

	    // Check for cache entries and mark page read-only, plus tag
	    {
		struct shdw_pg_data * pt_page = NULL;
		addr_t pg_gpa = PAGE_ADDR_4KB(guest_pa);

		pt_page = find_shdw_pt(core->vm_info, pg_gpa, PAGE_PT32);
		
		if (pt_page == NULL) {
		    pt_page = find_shdw_pt(core->vm_info, pg_gpa, PAGE_PD32);
		}

		if (pt_page != NULL) {
		    PrintError("Found PT page (small), marking RD-ONLY (va=%p), (gpa=%p)\n", 
			       (void *)fault_addr, (void *)pg_gpa);
		    // This is a page table page... 
		    shadow_pte->writable = 0;
		    shadow_pte->vmm_info = V3_CACHED_PG;
		}
	    }

	} else {
	    // Page fault on unhandled memory region
	    
	    if (shdw_reg->unhandled(core, fault_addr, guest_pa, shdw_reg, error_code) == -1) {
		PrintError("Special Page fault handler returned error for address: %p\n",  (void *)fault_addr);
		return -1;
	    }
	}
    } else if (shadow_pte_access == PT_ACCESS_WRITE_ERROR) {
	guest_pte->dirty = 1;

	// check for cache tag and handle invalidations if it exists.
	if (shadow_pte->vmm_info == V3_CACHED_PG) {
	    addr_t pg_gpa = PAGE_ADDR_4KB(guest_pa);

	    PrintError("Evicting on a small page\n");


	    if (evict_shdw_pg(core->vm_info, pg_gpa, PAGE_PD32) == -1) {
		PrintError("Error Evicting PAGE_PD32 cache entry\n");
		return -1;
	    }

	    if (evict_shdw_pg(core->vm_info, pg_gpa, PAGE_PT32) == -1) {
		PrintError("Error Evicting PAGE_PT32 cache entry\n");
		return -1;
	    }

	    shadow_pte->vmm_info &= ~V3_CACHED_PG;
	}


	if (shdw_reg->flags.write == 1) {
	    PrintDebug("Shadow PTE Write Error\n");
	    shadow_pte->writable = guest_pte->writable;
	} else {
	    if (shdw_reg->unhandled(core, fault_addr, guest_pa, shdw_reg, error_code) == -1) {
		PrintError("Special Page fault handler returned error for address: %p\n",  (void *)fault_addr);
		return -1;
	    }
	}


	return 0;

    } else {
	// Inject page fault into the guest	
	if (v3_inject_guest_pf(core, fault_addr, error_code) == -1) {
	    PrintError("Could not inject guest page fault for vaddr %p\n", (void *)fault_addr);
	    return -1;
	}

	PrintError("PTE Page fault fell through... Not sure if this should ever happen\n");
	PrintError("Manual Says to inject page fault into guest\n");
	return -1;
    }

    return 0;
}

// Handle a 4MB page fault with small pages in the PTE
static int handle_4MB_shadow_pagefault_pte_32(struct guest_info * core, 
					      addr_t fault_addr, pf_error_t error_code, 
					      pte32_t * shadow_pt, pde32_4MB_t * large_guest_pde, 
					      struct shdw_pg_data * pt_pg_data) 
{
    pt_access_status_t shadow_pte_access = v3_can_access_pte32(shadow_pt, fault_addr, error_code);
    pte32_t * shadow_pte = (pte32_t *)&(shadow_pt[PTE32_INDEX(fault_addr)]);
    addr_t guest_fault_pa = BASE_TO_PAGE_ADDR_4MB(large_guest_pde->page_base_addr) + PAGE_OFFSET_4MB(fault_addr);  


    PrintDebug("Handling 4MB fault (guest_fault_pa=%p) (error_code=%x)\n", (void *)guest_fault_pa, *(uint_t*)&error_code);
    PrintDebug("ShadowPT=%p, LargeGuestPDE=%p\n", shadow_pt, large_guest_pde);

    struct v3_mem_region * shdw_reg = v3_get_mem_region(core->vm_info, core->vcpu_id, guest_fault_pa);

 
    if (shdw_reg == NULL) {
	// Inject a machine check in the guest
	PrintDebug("Invalid Guest Address in page table (0x%p)\n", (void *)guest_fault_pa);
	v3_raise_exception(core, MC_EXCEPTION);
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

	if ((shdw_reg->flags.alloced == 1) && 
	    (shdw_reg->flags.read  == 1)) {
	    addr_t shadow_pa = 0;


	    if (v3_gpa_to_hpa(core, guest_fault_pa, &shadow_pa) == -1) {
		PrintError("could not translate page fault address (%p)\n", (void *)guest_fault_pa);
		return -1;
	    }

	    shadow_pte->page_base_addr = PAGE_BASE_ADDR(shadow_pa);

	    PrintDebug("\tMapping shadow page (%p)\n", (void *)BASE_TO_PAGE_ADDR(shadow_pte->page_base_addr));

	    shadow_pte->present = 1;

	    /* We are assuming that the PDE entry has precedence
	     * so the Shadow PDE will mirror the guest PDE settings, 
	     * and we don't have to worry about them here
	     * Allow everything
	     */
	    shadow_pte->user_page = 1;

	    //set according to VMM policy
	    shadow_pte->write_through = large_guest_pde->write_through;
	    shadow_pte->cache_disable = large_guest_pde->cache_disable;
	    shadow_pte->global_page = large_guest_pde->global_page;
	    //
      

	    if (shdw_reg->flags.write == 0) {
		shadow_pte->writable = 0;
	    } else {
		shadow_pte->writable = 1;
	    }


	    // Add this PTE to the reverse map...
	    // This allows us to update this PTE entry if it gets turned into a PT page sometime in the future
	    add_rmap(core->vm_info, pt_pg_data, PAGE_ADDR_4KB(guest_fault_pa), PAGE_ADDR_4KB(fault_addr));

	    // Check for cache entries and mark page read-only, plus tag
	    {
		struct shdw_pg_data * pt_page = NULL;
		addr_t pg_gpa = PAGE_ADDR_4KB(guest_fault_pa);

		pt_page = find_shdw_pt(core->vm_info, pg_gpa, PAGE_PT32);
		
		if (pt_page == NULL) {
		    pt_page = find_shdw_pt(core->vm_info, pg_gpa, PAGE_PD32);
		}

		if (pt_page != NULL) {
		    // This is a page table page... 
		    PrintError("Found PT page (large), marking RD-ONLY (va=%p), (gpa=%p)\n", 
			       (void *)fault_addr, (void *)pg_gpa);

		    shadow_pte->writable = 0;
		    shadow_pte->vmm_info = V3_CACHED_PG;
		}

	    }

	} else {
	    if (shdw_reg->unhandled(core, fault_addr, guest_fault_pa, shdw_reg, error_code) == -1) {
		PrintError("Special Page Fault handler returned error for address: %p\n", (void *)fault_addr);
		return -1;
	    }
	}
    } else if (shadow_pte_access == PT_ACCESS_WRITE_ERROR) {

	// check for cache tag and handle invalidations if it exists.
	if (shadow_pte->vmm_info == V3_CACHED_PG) {
	    addr_t pg_gpa = PAGE_ADDR_4KB(guest_fault_pa);
	    PrintError("Evicting on a large page\n");

	    if (evict_shdw_pg(core->vm_info, pg_gpa, PAGE_PD32) == -1) {
		PrintError("Error Evicting PAGE_PD32 cache entry\n");
		return -1;
	    }

	    if (evict_shdw_pg(core->vm_info, pg_gpa, PAGE_PT32) == -1) {
		PrintError("Error Evicting PAGE_PT32 cache entry\n");
		return -1;
	    }

	    shadow_pte->vmm_info &= ~V3_CACHED_PG;
	}


	if (shdw_reg->flags.write == 0) {
	    if (shdw_reg->unhandled(core, fault_addr, guest_fault_pa, shdw_reg, error_code) == -1) {
		PrintError("Special Page Fault handler returned error for address: %p\n", (void *)fault_addr);
		return -1;
	    }
	} else {
	    // set writable after cache eviction, unless overruled by region setting
	    shadow_pte->writable = 1;
	}

    } else {
	PrintError("Error in large page fault handler...\n");
	PrintError("This case should have been handled at the top level handler\n");
	return -1;
    }

    PrintDebug("Returning from large page->small page fault handler\n");
    return 0;
}


/* If we start to optimize we should look up the guest pages in the cache... */
static inline int handle_shadow_invlpg_32(struct guest_info * core, addr_t vaddr) {
    pde32_t * shadow_pd = (pde32_t *)CR3_TO_PDE32_VA(core->ctrl_regs.cr3);
    pde32_t * shadow_pde = (pde32_t *)&shadow_pd[PDE32_INDEX(vaddr)];

    addr_t guest_cr3 =  CR3_TO_PDE32_PA(core->shdw_pg_state.guest_cr3);
    pde32_t * guest_pd = NULL;
    pde32_t * guest_pde;

    if (v3_gpa_to_hva(core, guest_cr3, (addr_t*)&guest_pd) == -1) {
	PrintError("Invalid Guest PDE Address: 0x%p\n",  (void *)guest_cr3);
	return -1;
    }
  
    guest_pde = (pde32_t *)&(guest_pd[PDE32_INDEX(vaddr)]);
  
    // Should we back propagate the invalidations, because they might be cached...?? 
    

    if (guest_pde->large_page == 1) {
	shadow_pde->present = 0;
	PrintError("\tInvalidating Large Page (gpa=%p)\n", (void *)BASE_TO_PAGE_ADDR_4MB(guest_pde->pt_base_addr));
    } else if (shadow_pde->present == 1) {
	pte32_t * shadow_pt = (pte32_t *)(addr_t)BASE_TO_PAGE_ADDR_4KB(shadow_pde->pt_base_addr);
	pte32_t * shadow_pte = (pte32_t *) V3_VAddr( (void*) &shadow_pt[PTE32_INDEX(vaddr)] );

	

	//	    PrintError("\tInvalidating small page\n");


	shadow_pte->present = 0;
    } else {

	PrintError("What the fuck?\n");
    }
    return 0;
}


