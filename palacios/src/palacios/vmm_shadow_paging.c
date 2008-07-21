#include <palacios/vmm_shadow_paging.h>


#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>

#ifndef DEBUG_SHADOW_PAGING
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


int init_shadow_page_state(struct shadow_page_state * state) {
  state->guest_mode = PDE32;
  state->shadow_mode = PDE32;
  
  state->guest_cr3 = 0;
  state->shadow_cr3 = 0;

  return 0;
}

int handle_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  
  if (info->mem_mode == PHYSICAL_MEM) {
    // If paging is not turned on we need to handle the special cases
    return handle_special_page_fault(info, fault_addr, error_code);
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

addr_t create_new_shadow_pt32(struct guest_info * info) {
  void * host_pde = 0;

  V3_AllocPages(host_pde, 1);
  memset(host_pde, 0, PAGE_SIZE);

  return (addr_t)host_pde;
}


static int handle_pd32_nonaligned_4MB_page(struct guest_info * info, pte32_t * pt, addr_t guest_addr, pde32_4MB_t * large_shadow_pde) {
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

int handle_shadow_pagefault32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  pde32_t * guest_pd = NULL;
  pde32_t * shadow_pd = (pde32_t *)CR3_TO_PDE32(info->shdw_pg_state.shadow_cr3);
  addr_t guest_cr3 = CR3_TO_PDE32(info->shdw_pg_state.guest_cr3);
  pt_access_status_t guest_pde_access;
  pt_access_status_t shadow_pde_access;
  pde32_t * guest_pde = NULL;
  pde32_t * shadow_pde = (pde32_t *)&(shadow_pd[PDE32_INDEX(fault_addr)]);

  if (guest_pa_to_host_va(info, guest_cr3, (addr_t*)&guest_pd) == -1) {
    PrintError("Invalid Guest PDE Address: 0x%x\n", guest_cr3);
    return -1;
  }


  guest_pde = (pde32_t *)&(guest_pd[PDE32_INDEX(fault_addr)]);

  // Check the guest page permissions
  guest_pde_access = can_access_pde32(guest_pd, fault_addr, error_code);

  // Check the shadow page permissions
  shadow_pde_access = can_access_pde32(shadow_pd, fault_addr, error_code);
  
  /* This should be redone, 
     but basically the reasoning is that there can be multiple reasons for a page fault:
     If there is a permissions failure for a page present in the guest _BUT_ 
     the reason for the fault was that the page is not present in the shadow, 
     _THEN_ we have to map the shadow page in and reexecute, this will generate 
     a permissions fault which is _THEN_ valid to send to the guest
     _UNLESS_ both the guest and shadow have marked the page as not present

     whew...
  */
  if ((guest_pde_access != PT_ACCESS_OK) &&
      (
       ( (shadow_pde_access != PT_ENTRY_NOT_PRESENT) &&
	 (guest_pde_access != PT_ENTRY_NOT_PRESENT))  // aka (guest permission error)
       || 
       ( (shadow_pde_access == PT_ENTRY_NOT_PRESENT) && 
	(guest_pde_access == PT_ENTRY_NOT_PRESENT)))) {
    // inject page fault to the guest (Guest PDE fault)

	info->ctrl_regs.cr2 = fault_addr;
    raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);


    PrintDebug("Injecting PDE pf to guest: (guest access error=%d) (pf error code=%d)\n", guest_pde_access, error_code);
    return 0;

 
#ifdef DEBUG_SHADOW_PAGING
	PrintDebug("Guest CR3=%x\n", guest_cr3);
 	PrintDebug("Guest PD\n");
	PrintPD32(guest_pd);
	PrintDebug("Shadow PD\n");
	PrintPD32(shadow_pd);
#endif

    return -1;
  }


  //shadow_pde_access = can_access_pde32(shadow_pd, fault_addr, error_code);


  if (shadow_pde_access == PT_ENTRY_NOT_PRESENT) {

    shadow_pde->present = 1;
    shadow_pde->user_page = guest_pde->user_page;
    shadow_pde->large_page = guest_pde->large_page;


    // VMM Specific options
    shadow_pde->write_through = 0;
    shadow_pde->cache_disable = 0;
    shadow_pde->global_page = 0;
    //

    guest_pde->accessed = 1;
    
    if (guest_pde->large_page == 0) {
      pte32_t * shadow_pt = NULL;
      
      V3_AllocPages(shadow_pt, 1);
      memset(shadow_pt, 0, PAGE_SIZE);
      
      shadow_pde->pt_base_addr = PD32_BASE_ADDR(shadow_pt);

      shadow_pde->writable = guest_pde->writable;
    } else {
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

	raise_exception(info, MC_EXCEPTION);
	PrintError("Invalid guest address in large page (0x%x)\n", guest_start_addr);
	return -1;
      } else if (host_page_type == HOST_REGION_PHYSICAL_MEMORY) {
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
      
	  V3_AllocPages(shadow_pt, 1);
	  memset(shadow_pt, 0, PAGE_SIZE);

	  if (handle_pd32_nonaligned_4MB_page(info, shadow_pt, guest_start_addr, large_shadow_pde) == -1) {
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
	if (handle_special_page_fault(info, fault_addr, error_code) == -1) {
	  PrintError("Special Page Fault handler returned error for address: %x\n", fault_addr);
	  return -1;
	}
      }
    }

  } else if ((shadow_pde_access == PT_WRITE_ERROR) && 
	     (guest_pde->large_page = 1) && 
	     (((pde32_4MB_t *)guest_pde)->dirty == 0)) {

    //
    // Page Directory Entry marked read-only
    //

    ((pde32_4MB_t *)guest_pde)->dirty = 1;
    shadow_pde->writable = guest_pde->writable;
    return 0;

  } else if (shadow_pde_access == PT_USER_ERROR) {

    //
    // Page Directory Entry marked non-user
    //
    
    PrintDebug("Shadow Paging User access error (shadow_pde_access=0x%x, guest_pde_access=0x%x - injecting into guest\n", shadow_pde_access, guest_pde_access);
    info->ctrl_regs.cr2 = fault_addr;
    raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);
    return 0;

  } else if (shadow_pde_access == PT_ACCESS_OK) {
    pte32_t * shadow_pt = (pte32_t *)PDE32_T_ADDR((*shadow_pde));
    pte32_t * guest_pt = NULL;

    // Page Table Entry fault
    
    if (guest_pa_to_host_va(info, PDE32_T_ADDR((*guest_pde)), (addr_t*)&guest_pt) == -1) {
      PrintDebug("Invalid Guest PTE Address: 0x%x\n", PDE32_T_ADDR((*guest_pde)));
      // Machine check the guest

      raise_exception(info, MC_EXCEPTION);
      
      return 0;
    }


    if (handle_shadow_pte32_fault(info, fault_addr, error_code, shadow_pt, guest_pt)  == -1) {
      PrintError("Error handling Page fault caused by PTE\n");
      return -1;
    }

 } else {
    // Unknown error raise page fault in guest
    info->ctrl_regs.cr2 = fault_addr;
    raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);

    // For debugging we will return an error here for the time being, 
    // this probably shouldn't ever happen
    PrintDebug("Unknown Error occurred\n");
    PrintDebug("Manual Says to inject page fault into guest\n");


    return 0;

  }

  //PrintDebugPageTables(shadow_pd);
  PrintDebug("Returning end of PDE function (rip=%x)\n", info->rip);
  return 0;
}



/* 
 * We assume the the guest pte pointer has already been translated to a host virtual address
 */
int handle_shadow_pte32_fault(struct guest_info * info, 
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
  
  /* This should be redone, 
     but basically the reasoning is that there can be multiple reasons for a page fault:
     If there is a permissions failure for a page present in the guest _BUT_ 
     the reason for the fault was that the page is not present in the shadow, 
     _THEN_ we have to map the shadow page in and reexecute, this will generate 
     a permissions fault which is _THEN_ valid to send to the guest
     _UNLESS_ both the guest and shadow have marked the page as not present

     whew...
  */
  if ((guest_pte_access != PT_ACCESS_OK) && 
      ( 
       ((shadow_pte_access != PT_ENTRY_NOT_PRESENT) &&
	(guest_pte_access != PT_ENTRY_NOT_PRESENT)) // aka (guest permission error)
       ||
       ((shadow_pte_access == PT_ENTRY_NOT_PRESENT) &&
	(guest_pte_access == PT_ENTRY_NOT_PRESENT)))) {
    // Inject page fault into the guest	
    
    info->ctrl_regs.cr2 = fault_addr;
    raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);
    
    PrintDebug("Access error injecting pf to guest (guest access error=%d) (pf error code=%d)\n", guest_pte_access, *(uint_t*)&error_code);
    return 0; 
  }
  
  


  if (shadow_pte_access == PT_ACCESS_OK) {
    // Inconsistent state...
    // Guest Re-Entry will flush page tables and everything should now work
    PrintDebug("Inconsistent state... Guest re-entry should flush tlb\n");
    return 0;
  } else if (shadow_pte_access == PT_ENTRY_NOT_PRESENT) {
    addr_t shadow_pa;
    addr_t guest_pa = PTE32_T_ADDR((*guest_pte));

    // Page Table Entry Not Present

    host_region_type_t host_page_type = get_shadow_addr_type(info, guest_pa);

    if (host_page_type == HOST_REGION_INVALID) {
      // Inject a machine check in the guest

      raise_exception(info, MC_EXCEPTION);
#ifdef DEBUG_SHADOW_PAGING
      PrintDebug("Invalid Guest Address in page table (0x%x)\n", guest_pa);
      PrintDebug("fault_addr=0x%x next are guest and shadow ptes \n",fault_addr);
      PrintPTE32(fault_addr,guest_pte);
      PrintPTE32(fault_addr,shadow_pte);
      PrintDebug("Done.\n");
#endif
      return 0;

    } else if (host_page_type == HOST_REGION_PHYSICAL_MEMORY) {
      
      shadow_pa = get_shadow_addr(info, guest_pa);
      
      shadow_pte->page_base_addr = PT32_BASE_ADDR(shadow_pa);
      
      shadow_pte->present = guest_pte->present;
      shadow_pte->user_page = guest_pte->user_page;
      
      //set according to VMM policy
      shadow_pte->write_through = 0;
      shadow_pte->cache_disable = 0;
      shadow_pte->global_page = 0;
      //
      
      guest_pte->accessed = 1;
      
      if (guest_pte->dirty == 1) {
	shadow_pte->writable = guest_pte->writable;
      } else if ((guest_pte->dirty == 0) && (error_code.write == 1)) {
	shadow_pte->writable = guest_pte->writable;
	guest_pte->dirty = 1;
      } else if ((guest_pte->dirty = 0) && (error_code.write == 0)) {
	shadow_pte->writable = 0;
      }
    } else {
      // Page fault handled by hook functions
      if (handle_special_page_fault(info, fault_addr, error_code) == -1) {
	PrintError("Special Page fault handler returned error for address: %x\n", fault_addr);
	return -1;
      }
    }

  } else if ((shadow_pte_access == PT_WRITE_ERROR) &&
	     (guest_pte->dirty == 0)) {
    guest_pte->dirty = 1;
    shadow_pte->writable = guest_pte->writable;

    PrintDebug("Shadow PTE Write Error\n");

    return 0;
  } else {
    // Inject page fault into the guest	
	
    info->ctrl_regs.cr2 = fault_addr;
    raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);

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



/* Deprecated */
/*
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

*/
