/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */
/* (c) 2008, The V3VEE Project <http://www.v3vee.org> */


#include <palacios/vmm_shadow_paging.h>


#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>

#ifndef DEBUG_SHADOW_PAGING
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


/*** 
 ***  There be dragons
 ***/





DEFINE_HASHTABLE_INSERT(add_cr3_to_cache, addr_t, struct hashtable *);
DEFINE_HASHTABLE_SEARCH(find_cr3_in_cache, addr_t, struct hashtable *);
DEFINE_HASHTABLE_REMOVE(del_cr3_from_cache, addr_t, struct hashtable *, 0);


DEFINE_HASHTABLE_INSERT(add_pte_map, addr_t, addr_t);
DEFINE_HASHTABLE_SEARCH(find_pte_map, addr_t, addr_t);
DEFINE_HASHTABLE_REMOVE(del_pte_map, addr_t, addr_t, 0);




static uint_t pte_hash_fn(addr_t key) {
  return hash_long(key, 32);
}

static int pte_equals(addr_t key1, addr_t key2) {
  return (key1 == key2);
}

static uint_t cr3_hash_fn(addr_t key) {
  return hash_long(key, 32);
}

static int cr3_equals(addr_t key1, addr_t key2) {
  return (key1 == key2);
}


static int handle_shadow_pte32_fault(struct guest_info* info, 
				     addr_t fault_addr, 
				     pf_error_t error_code,
				     pte32_t * shadow_pte, 
				     pte32_t * guest_pte);

static int handle_shadow_pagefault32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code);

int init_shadow_page_state(struct guest_info * info) {
  struct shadow_page_state * state = &(info->shdw_pg_state);
  state->guest_mode = PDE32;
  state->shadow_mode = PDE32;
  
  state->guest_cr3 = 0;
  state->shadow_cr3 = 0;


  state->cr3_cache = create_hashtable(0, &cr3_hash_fn, &cr3_equals);

  state->cached_cr3 = 0;
  state->cached_ptes = NULL;

  return 0;
}




/*
 For now we'll do something a little more lightweight
int cache_page_tables32(struct guest_info * info, addr_t pde) {
  struct shadow_page_state * state = &(info->shdw_pg_state);
  addr_t pde_host_addr;
  pde32_t * tmp_pde;
  struct hashtable * pte_cache = NULL;
  int i = 0;


  pte_cache = (struct hashtable *)find_cr3_in_cache(state->cr3_cache, pde);
  if (pte_cache != NULL) {
    PrintError("CR3 already present in cache\n");
    state->current_ptes = pte_cache;
    return 1;
  } else {
    PrintError("Creating new CR3 cache entry\n");
    pte_cache = create_hashtable(0, &pte_hash_fn, &pte_equals);
    state->current_ptes = pte_cache;
    add_cr3_to_cache(state->cr3_cache, pde, pte_cache);
  }

  if (guest_pa_to_host_va(info, pde, &pde_host_addr) == -1) {
    PrintError("Could not lookup host address of guest PDE\n");
    return -1;
  }

  tmp_pde = (pde32_t *)pde_host_addr;

  add_pte_map(pte_cache, pde, pde_host_addr);


  for (i = 0; i < MAX_PDE32_ENTRIES; i++) {
    if ((tmp_pde[i].present) && (tmp_pde[i].large_page == 0)) {
      addr_t pte_host_addr;

      if (guest_pa_to_host_va(info, (addr_t)(PDE32_T_ADDR(tmp_pde[i])), &pte_host_addr) == -1) {
	PrintError("Could not lookup host address of guest PDE\n");
	return -1;
      }

      add_pte_map(pte_cache, (addr_t)(PDE32_T_ADDR(tmp_pde[i])), pte_host_addr); 
    }
  }


  return 0;
}
*/

int cache_page_tables32(struct guest_info * info, addr_t pde) {
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

  if (guest_pa_to_host_pa(info, pde, &pde_host_addr) == -1) {
    PrintError("Could not lookup host address of guest PDE\n");
    return -1;
  }

  tmp_pde = (pde32_t *)pde_host_addr;

  add_pte_map(pte_cache, pde, pde_host_addr);


  for (i = 0; i < MAX_PDE32_ENTRIES; i++) {
    if ((tmp_pde[i].present) && (tmp_pde[i].large_page == 0)) {
      addr_t pte_host_addr;

      if (guest_pa_to_host_pa(info, (addr_t)(PDE32_T_ADDR(tmp_pde[i])), &pte_host_addr) == -1) {
	PrintError("Could not lookup host address of guest PDE\n");
	return -1;
      }

      add_pte_map(pte_cache, (addr_t)(PDE32_T_ADDR(tmp_pde[i])), pte_host_addr); 
    }
  }

  return 0;

}



int v3_replace_shdw_page32(struct guest_info * info, addr_t location, pte32_t * new_page, pte32_t * old_page) {
  pde32_t * shadow_pd = (pde32_t *)CR3_TO_PDE32(info->shdw_pg_state.shadow_cr3);
  pde32_t * shadow_pde =  (pde32_t *)&(shadow_pd[PDE32_INDEX(location)]);

  if (shadow_pde->large_page == 0) {
    pte32_t * shadow_pt = (pte32_t *)PDE32_T_ADDR((*shadow_pde));
    pte32_t * shadow_pte = (pte32_t *)&(shadow_pt[PTE32_INDEX(location)]);

    //if (shadow_pte->present == 1) {
    *(uint_t *)old_page = *(uint_t *)shadow_pte;
    //}

    *(uint_t *)shadow_pte = *(uint_t *)new_page;

  } else {
    // currently unhandled
    return -1;
  }
  
  return 0;
}






int handle_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  
  if (info->mem_mode == PHYSICAL_MEM) {
    // If paging is not turned on we need to handle the special cases
    return handle_special_page_fault(info, fault_addr, fault_addr, error_code);
  } else if (info->mem_mode == VIRTUAL_MEM) {

    switch (info->cpu_mode) {
    case PROTECTED:
      return handle_shadow_pagefault32(info, fault_addr, error_code);
      break;
    case PROTECTED_PAE:
    case LONG:
    default:
      PrintError("Unhandled CPU Mode\n");
      return -1;
    }
  } else {
    PrintError("Invalid Memory mode\n");
    return -1;
  }
}

addr_t create_new_shadow_pt32() {
  void * host_pde = 0;

  host_pde = V3_AllocPages(1);
  memset(host_pde, 0, PAGE_SIZE);

  return (addr_t)host_pde;
}


static void inject_guest_pf(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  info->ctrl_regs.cr2 = fault_addr;
  v3_raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);
}


static int is_guest_pf(pt_access_status_t guest_access, pt_access_status_t shadow_access) {
  /* basically the reasoning is that there can be multiple reasons for a page fault:
     If there is a permissions failure for a page present in the guest _BUT_ 
     the reason for the fault was that the page is not present in the shadow, 
     _THEN_ we have to map the shadow page in and reexecute, this will generate 
     a permissions fault which is _THEN_ valid to send to the guest
     _UNLESS_ both the guest and shadow have marked the page as not present

     whew...
  */
  if (guest_access != PT_ACCESS_OK) {
    // Guest Access Error
    
    if ((shadow_access != PT_ENTRY_NOT_PRESENT) &&
	(guest_access != PT_ENTRY_NOT_PRESENT)) {
      // aka (guest permission error)
      return 1;
    }

    if ((shadow_access == PT_ENTRY_NOT_PRESENT) &&
	(guest_access == PT_ENTRY_NOT_PRESENT)) {      
      // Page tables completely blank, handle guest first
      return 1;
    }

    // Otherwise we'll handle the guest fault later...?
  }

  return 0;
}




/* The guest status checks have already been done,
 * only special case shadow checks remain
 */
static int handle_large_pagefault32(struct guest_info * info, 
				    addr_t fault_addr, pf_error_t error_code, 
				    pte32_t * shadow_pt, pde32_4MB_t * large_guest_pde) 
{
  pt_access_status_t shadow_pte_access = can_access_pte32(shadow_pt, fault_addr, error_code);
  pte32_t * shadow_pte = (pte32_t *)&(shadow_pt[PTE32_INDEX(fault_addr)]);
  
  if (shadow_pte_access == PT_ACCESS_OK) {
    // Inconsistent state...
    // Guest Re-Entry will flush tables and everything should now workd
    PrintDebug("Inconsistent state... Guest re-entry should flush tlb\n");
    return 0;
  }

  
  if (shadow_pte_access == PT_ENTRY_NOT_PRESENT) {
    // Get the guest physical address of the fault
    addr_t guest_fault_pa = PDE32_4MB_T_ADDR(*large_guest_pde) + PD32_4MB_PAGE_OFFSET(fault_addr);
    host_region_type_t host_page_type = get_shadow_addr_type(info, guest_fault_pa);
 

    if (host_page_type == HOST_REGION_INVALID) {
      // Inject a machine check in the guest
      PrintDebug("Invalid Guest Address in page table (0x%x)\n", guest_fault_pa);
      v3_raise_exception(info, MC_EXCEPTION);
      return 0;
    }

    if (host_page_type == HOST_REGION_PHYSICAL_MEMORY) {
      struct shadow_page_state * state = &(info->shdw_pg_state);
      addr_t shadow_pa = get_shadow_addr(info, guest_fault_pa);

      shadow_pte->page_base_addr = PT32_BASE_ADDR(shadow_pa);

      shadow_pte->present = 1;

      /* We are assuming that the PDE entry has precedence
       * so the Shadow PDE will mirror the guest PDE settings, 
       * and we don't have to worry about them here
       * Allow everything
       */
      shadow_pte->user_page = 1;

      if (find_pte_map(state->cached_ptes, PT32_PAGE_ADDR(guest_fault_pa)) != NULL) {
	// Check if the entry is a page table...
	PrintDebug("Marking page as Guest Page Table (large page)\n");
	shadow_pte->vmm_info = PT32_GUEST_PT;
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
      if (handle_special_page_fault(info, fault_addr, guest_fault_pa, error_code) == -1) {
	PrintError("Special Page Fault handler returned error for address: %x\n", fault_addr);
	return -1;
      }
    }
  } else if ((shadow_pte_access == PT_WRITE_ERROR) && 
	     (shadow_pte->vmm_info == PT32_GUEST_PT)) {

    struct shadow_page_state * state = &(info->shdw_pg_state);
    PrintDebug("Write operation on Guest PAge Table Page (large page)\n");
    state->cached_cr3 = 0;
    shadow_pte->writable = 1;

  } else {
    PrintError("Error in large page fault handler...\n");
    PrintError("This case should have been handled at the top level handler\n");
    return -1;
  }

  PrintDebug("Returning from large page fault handler\n");
  return 0;
}


static int handle_shadow_pagefault32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  pde32_t * guest_pd = NULL;
  pde32_t * shadow_pd = (pde32_t *)CR3_TO_PDE32(info->shdw_pg_state.shadow_cr3);
  addr_t guest_cr3 = CR3_TO_PDE32(info->shdw_pg_state.guest_cr3);
  pt_access_status_t guest_pde_access;
  pt_access_status_t shadow_pde_access;
  pde32_t * guest_pde = NULL;
  pde32_t * shadow_pde = (pde32_t *)&(shadow_pd[PDE32_INDEX(fault_addr)]);

  PrintDebug("Shadow page fault handler\n");

  if (guest_pa_to_host_va(info, guest_cr3, (addr_t*)&guest_pd) == -1) {
    PrintError("Invalid Guest PDE Address: 0x%x\n", guest_cr3);
    return -1;
  } 

  guest_pde = (pde32_t *)&(guest_pd[PDE32_INDEX(fault_addr)]);


  // Check the guest page permissions
  guest_pde_access = can_access_pde32(guest_pd, fault_addr, error_code);

  // Check the shadow page permissions
  shadow_pde_access = can_access_pde32(shadow_pd, fault_addr, error_code);
  
  /* Was the page fault caused by the Guest's page tables? */
  if (is_guest_pf(guest_pde_access, shadow_pde_access) == 1) {
    PrintDebug("Injecting PDE pf to guest: (guest access error=%d) (pf error code=%d)\n", 
	       guest_pde_access, error_code);
    inject_guest_pf(info, fault_addr, error_code);
    return 0;
  }

  
  if (shadow_pde_access == PT_ENTRY_NOT_PRESENT) 
    {
      pte32_t * shadow_pt =  (pte32_t *)create_new_shadow_pt32();

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
      
      shadow_pde->pt_base_addr = PD32_BASE_ADDR(shadow_pt);
      
      if (guest_pde->large_page == 0) {
	shadow_pde->writable = guest_pde->writable;
      } else {
	((pde32_4MB_t *)guest_pde)->dirty = 0;
	shadow_pde->writable = 0;
      }
    }
  else if (shadow_pde_access == PT_ACCESS_OK) 
    {
      //
      // PTE fault
      //
      pte32_t * shadow_pt = (pte32_t *)PDE32_T_ADDR((*shadow_pde));

      if (guest_pde->large_page == 0) {
	pte32_t * guest_pt = NULL;
	if (guest_pa_to_host_va(info, PDE32_T_ADDR((*guest_pde)), (addr_t*)&guest_pt) == -1) {
	  // Machine check the guest
	  PrintDebug("Invalid Guest PTE Address: 0x%x\n", PDE32_T_ADDR((*guest_pde)));
	  v3_raise_exception(info, MC_EXCEPTION);
	  return 0;
	}
	
	if (handle_shadow_pte32_fault(info, fault_addr, error_code, shadow_pt, guest_pt)  == -1) {
	  PrintError("Error handling Page fault caused by PTE\n");
	  return -1;
	}
      } else if (guest_pde->large_page == 1) {
	if (handle_large_pagefault32(info, fault_addr, error_code, shadow_pt, (pde32_4MB_t *)guest_pde) == -1) {
	  PrintError("Error handling large pagefault\n");
	  return -1;
	}
      }
    }
  else if ((shadow_pde_access == PT_WRITE_ERROR) && 
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
  else if (shadow_pde_access == PT_USER_ERROR) 
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
      PrintPDE32(fault_addr, guest_pde);
      PrintDebug("Shadow PDE: (access=%d)\n\t", shadow_pde_access);
      PrintPDE32(fault_addr, shadow_pde);
#endif

      return 0; 
    }

  PrintDebug("Returning end of PDE function (rip=%x)\n", info->rip);
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


  // Check the guest page permissions
  guest_pte_access = can_access_pte32(guest_pt, fault_addr, error_code);

  // Check the shadow page permissions
  shadow_pte_access = can_access_pte32(shadow_pt, fault_addr, error_code);
  
#ifdef DEBUG_SHADOW_PAGING
  PrintDebug("Guest PTE: (access=%d)\n\t", guest_pte_access);
  PrintPTE32(fault_addr, guest_pte);
  PrintDebug("Shadow PTE: (access=%d)\n\t", shadow_pte_access);
  PrintPTE32(fault_addr, shadow_pte);
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


  if (shadow_pte_access == PT_ENTRY_NOT_PRESENT) {

    addr_t guest_pa = PTE32_T_ADDR((*guest_pte)) +  PT32_PAGE_OFFSET(fault_addr);

    // Page Table Entry Not Present

    host_region_type_t host_page_type = get_shadow_addr_type(info, guest_pa);

    if (host_page_type == HOST_REGION_INVALID) {
      // Inject a machine check in the guest
      PrintDebug("Invalid Guest Address in page table (0x%x)\n", guest_pa);
      v3_raise_exception(info, MC_EXCEPTION);
      return 0;
    }

    // else...

    if (host_page_type == HOST_REGION_PHYSICAL_MEMORY) {
      struct shadow_page_state * state = &(info->shdw_pg_state);
      addr_t shadow_pa = get_shadow_addr(info, guest_pa);
      
      shadow_pte->page_base_addr = PT32_BASE_ADDR(shadow_pa);
      
      shadow_pte->present = guest_pte->present;
      shadow_pte->user_page = guest_pte->user_page;
      
      //set according to VMM policy
      shadow_pte->write_through = 0;
      shadow_pte->cache_disable = 0;
      shadow_pte->global_page = 0;
      //
      
      guest_pte->accessed = 1;
      
      if (find_pte_map(state->cached_ptes, PT32_PAGE_ADDR(guest_pa)) != NULL) {
	// Check if the entry is a page table...
	PrintDebug("Marking page as Guest Page Table\n", shadow_pte->writable);
	shadow_pte->vmm_info = PT32_GUEST_PT;
      }

      if (guest_pte->dirty == 1) {
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

      } else if ((guest_pte->dirty = 0) && (error_code.write == 0)) {
	shadow_pte->writable = 0;
      }



    } else {
      // Page fault handled by hook functions
      if (handle_special_page_fault(info, fault_addr, guest_pa, error_code) == -1) {
	PrintError("Special Page fault handler returned error for address: %x\n", fault_addr);
	return -1;
      }
    }

  } else if ((shadow_pte_access == PT_WRITE_ERROR) &&
	     (guest_pte->dirty == 0)) {

    PrintDebug("Shadow PTE Write Error\n");
    guest_pte->dirty = 1;
    shadow_pte->writable = guest_pte->writable;

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






/* Currently Does not work with Segmentation!!! */
int handle_shadow_invlpg(struct guest_info * info) {
  if (info->mem_mode != VIRTUAL_MEM) {
    // Paging must be turned on...
    // should handle with some sort of fault I think
    PrintError("ERROR: INVLPG called in non paged mode\n");
    return -1;
  }


  if (info->cpu_mode == PROTECTED) {
    char instr[15];
    int ret;
    int index = 0;

    ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    if (ret != 15) {
      PrintError("Could not read instruction 0x%x (ret=%d)\n", info->rip, ret);
      return -1;
    }

   
    /* Can INVLPG work with Segments?? */
    while (is_prefix_byte(instr[index])) {
      index++;
    }
    
    
    if ((instr[index] == (uchar_t)0x0f) &&
	(instr[index + 1] == (uchar_t)0x01)) {

      addr_t first_operand;
      addr_t second_operand;
      operand_type_t addr_type;
      addr_t guest_cr3 = CR3_TO_PDE32(info->shdw_pg_state.guest_cr3);

      pde32_t * guest_pd = NULL;

      if (guest_pa_to_host_va(info, guest_cr3, (addr_t*)&guest_pd) == -1) {
	PrintError("Invalid Guest PDE Address: 0x%x\n", guest_cr3);
	return -1;
      }

      


      index += 2;

      addr_type = decode_operands32(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);

      if (addr_type == MEM_OPERAND) {
	pde32_t * shadow_pd = (pde32_t *)CR3_TO_PDE32(info->shdw_pg_state.shadow_cr3);
	pde32_t * shadow_pde = (pde32_t *)&shadow_pd[PDE32_INDEX(first_operand)];
	pde32_t * guest_pde;

	//PrintDebug("PDE Index=%d\n", PDE32_INDEX(first_operand));
	//PrintDebug("FirstOperand = %x\n", first_operand);

	PrintDebug("Invalidating page for %x\n", first_operand);

	guest_pde = (pde32_t *)&(guest_pd[PDE32_INDEX(first_operand)]);

	if (guest_pde->large_page == 1) {
	  shadow_pde->present = 0;
	  PrintDebug("Invalidating Large Page\n");
	} else {
	 
	  if (shadow_pde->present == 1) {
	    pte32_t * shadow_pt = (pte32_t *)PDE32_T_ADDR((*shadow_pde));
	    pte32_t * shadow_pte = (pte32_t *)&shadow_pt[PTE32_INDEX(first_operand)];

#ifdef DEBUG_SHADOW_PAGING
	    PrintDebug("Setting not present\n");
	    PrintPTE32(first_operand, shadow_pte);
#endif

	    shadow_pte->present = 0;
	  }
	}

	info->rip += index;

      } else {
	PrintError("Invalid Operand type\n");
	return -1;
      }
    } else {
      PrintError("invalid Instruction Opcode\n");
      PrintTraceMemDump(instr, 15);
      return -1;
    }
  }

  return 0;
}


/*


static int create_pd32_nonaligned_4MB_page(struct guest_info * info, pte32_t * pt, addr_t guest_addr, pde32_4MB_t * large_shadow_pde) {
  uint_t i = 0;
  pte32_t * pte_cursor;
  addr_t guest_pa = 0;

  for (i = 0; i < 1024; i++) {
    guest_pa = guest_addr + (PAGE_SIZE * i);
    host_region_type_t host_page_type = get_shadow_addr_type(info, guest_pa);
    
    pte_cursor = &(pt[i]);

    if (host_page_type == HOST_REGION_INVALID) {
      // Currently we don't support this, but in theory we could
      PrintError("Invalid Host Memory Type\n");
      return -1;
    } else if (host_page_type == HOST_REGION_PHYSICAL_MEMORY) {
      addr_t shadow_pa = get_shadow_addr(info, guest_pa);


      pte_cursor->page_base_addr = PT32_BASE_ADDR(shadow_pa);
      pte_cursor->present = 1;
      pte_cursor->writable = large_shadow_pde->writable;
      pte_cursor->user_page = large_shadow_pde->user_page;
      pte_cursor->write_through = 0;  
      pte_cursor->cache_disable = 0;
      pte_cursor->global_page = 0;

    } else {
      PrintError("Unsupported Host Memory Type\n");
      return -1;
    }
  }
  return 0;
}


static int handle_large_pagefault32(struct guest_info * info, 
				    pde32_t * guest_pde, pde32_t * shadow_pde, 
				    addr_t fault_addr, pf_error_t error_code ) {
  struct shadow_region * mem_reg;
  pde32_4MB_t * large_guest_pde = (pde32_4MB_t *)guest_pde;
  pde32_4MB_t * large_shadow_pde = (pde32_4MB_t *)shadow_pde;
  host_region_type_t host_page_type;
  addr_t guest_start_addr = PDE32_4MB_T_ADDR(*large_guest_pde);
  //    addr_t guest_end_addr = guest_start_addr + PAGE_SIZE_4MB; // start address + 4MB
  
  
  // Check that the Guest PDE entry points to valid memory
  // else Machine Check the guest
  PrintDebug("Large Page: Page Base Addr=%x\n", guest_start_addr);
  
  host_page_type = get_shadow_addr_type(info, guest_start_addr);
  
  if (host_page_type == HOST_REGION_INVALID) {
    PrintError("Invalid guest address in large page (0x%x)\n", guest_start_addr);
    v3_raise_exception(info, MC_EXCEPTION);
    return -1;
  }
  
  // else...

  if (host_page_type == HOST_REGION_PHYSICAL_MEMORY) {

    addr_t host_start_addr = 0;
    addr_t region_end_addr = 0;
    
    // Check for a large enough region in host memory
    mem_reg = get_shadow_region_by_addr(&(info->mem_map), guest_start_addr);
    PrintDebug("Host region: host_addr=%x (guest_start=%x, end=%x)\n", 
	       mem_reg->host_addr, mem_reg->guest_start, mem_reg->guest_end);
    host_start_addr = mem_reg->host_addr + (guest_start_addr - mem_reg->guest_start);
    region_end_addr = mem_reg->host_addr + (mem_reg->guest_end - mem_reg->guest_start);
    
    PrintDebug("Host Start Addr=%x; Region End Addr=%x\n", host_start_addr, region_end_addr);
    
    
    //4f
    if (large_guest_pde->dirty == 1) { // dirty
      large_shadow_pde->writable = guest_pde->writable;
    } else if (error_code.write == 1) { // not dirty, access is write
      large_shadow_pde->writable = guest_pde->writable;
      large_guest_pde->dirty = 1;
    } else { // not dirty, access is read
      large_shadow_pde->writable = 0;
    }
    
    
    // Check if the region is at least an additional 4MB
    
    
    //4b.
    if ((PD32_4MB_PAGE_OFFSET(host_start_addr) == 0) && 
	(region_end_addr >= host_start_addr + PAGE_SIZE_4MB)) { 	// if 4MB boundary
      large_shadow_pde->page_base_addr = PD32_4MB_BASE_ADDR(host_start_addr);
    } else { 	  // else generate 4k pages
      pte32_t * shadow_pt = NULL;
      PrintDebug("Handling non aligned large page\n");
      
      shadow_pde->large_page = 0;
      
      shadow_pt = create_new_shadow_pt32();

      if (create_pd32_nonaligned_4MB_page(info, shadow_pt, guest_start_addr, large_shadow_pde) == -1) {
	PrintError("Non Aligned Large Page Error\n");
	V3_Free(shadow_pt);
	return -1;
      }
      
      
#ifdef DEBUG_SHADOW_PAGING
      PrintDebug("non-aligned Shadow PT\n");
      PrintPT32(PT32_PAGE_ADDR(fault_addr), shadow_pt);	  
#endif
      shadow_pde->pt_base_addr = PD32_BASE_ADDR(shadow_pt);
    }
    
  } else {
    // Handle hooked pages as well as other special pages
    if (handle_special_page_fault(info, fault_addr, guest_start_addr, error_code) == -1) {
      PrintError("Special Page Fault handler returned error for address: %x\n", fault_addr);
      return -1;
    }
  }

  return 0;
}
*/
