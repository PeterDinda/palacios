#include <palacios/vmm_shadow_paging.h>


#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>



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
      // currently not handled
      return -1;
      break;
    default:
      return -1;
    }
  } else {
    PrintDebug("Invalid Memory mode\n");
    return -1;
  }
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
    PrintDebug("Invalid Guest PDE Address: 0x%x\n", guest_cr3);
    return -1;
  }


  guest_pde = (pde32_t *)&(guest_pd[PDE32_INDEX(fault_addr)]);

  // Check the guest page permissions
  guest_pde_access = can_access_pde32(guest_pd, fault_addr, error_code);

  if (guest_pde_access != PT_ACCESS_OK) {
    // inject page fault to the guest (Guest PDE fault)

    info->ctrl_regs.cr2 = fault_addr;
    raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);

    PrintDebug("Injecting PDE pf to guest: (guest access error=%d) (pf error code=%d)\n", guest_pde_access, error_code);
    PrintDebug("Guest CR3=%x\n", guest_cr3);
    PrintPD32(guest_pd);

    V3_ASSERT(0);
    return 0;
  }


  shadow_pde_access = can_access_pde32(shadow_pd, fault_addr, error_code);


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


      /* JRL: THIS COULD BE A PROBLEM....
       * Currently we only support large pages if the region is mapped contiguosly in shadow memory
       * Lets hope this is the case...
       */

      // Check that the Guest PDE entry points to valid memory
      // else Machine Check the guest
      host_page_type = get_shadow_addr_type(info, guest_start_addr);

      if (host_page_type == HOST_REGION_INVALID) {

	raise_exception(info, MC_EXCEPTION);
	PrintDebug("Invalid guest address in large page (0x%x)\n", guest_start_addr);
	return -1;
      } else if (host_page_type == HOST_REGION_PHYSICAL_MEMORY) {
	addr_t host_start_addr = 0;
	addr_t region_end_addr = 0;

	// Check for a large enough region in host memory
	mem_reg = get_shadow_region_by_addr(&(info->mem_map), guest_start_addr);
	host_start_addr = mem_reg->host_addr + (guest_start_addr - mem_reg->guest_start);
	region_end_addr = mem_reg->host_addr + (mem_reg->guest_end - mem_reg->guest_start);

	// Check if the region is at least an additional 4MB
	if (region_end_addr <= host_start_addr + PAGE_SIZE_4MB) {
	  PrintDebug("Large page over non contiguous host memory... Not handled\n");
	  return -1;
	}

	//4b.
	large_shadow_pde->page_base_addr = PD32_4MB_BASE_ADDR(host_start_addr);

	//4f
	if (large_guest_pde->dirty == 1) { // dirty
	  large_shadow_pde->writable = guest_pde->writable;
	} else if (error_code.write == 1) { // not dirty, access is write
	  large_shadow_pde->writable = guest_pde->writable;
	  large_guest_pde->dirty = 1;
	} else { // not dirty, access is read
	  large_shadow_pde->writable = 0;
	}

      } else {
	// Handle hooked pages as well as other special pages
	if (handle_special_page_fault(info, fault_addr, error_code) == -1) {
	  PrintDebug("Special Page Fault handler returned error for address: %x\n", fault_addr);
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
    
    PrintDebug("Shadow Paging User access error\n");
    return -1;
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
      PrintDebug("Error handling Page fault caused by PTE\n");
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
    return -1;
  }

  //PrintDebugPageTables(shadow_pd);
  PrintDebug("Returning end of PDE function\n");
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

  
  if (guest_pte_access != PT_ACCESS_OK) {
    // Inject page fault into the guest	
    
    info->ctrl_regs.cr2 = fault_addr;
    raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);
    
    PrintDebug("Access error injecting pf to guest\n");
    return 0;
  }
  
  
  shadow_pte_access = can_access_pte32(shadow_pt, fault_addr, error_code);

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

      PrintDebug("Invalid Guest Address in page table (0x%x)\n", guest_pa);
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
	PrintDebug("Special Page fault handler returned error for address: %x\n", fault_addr);
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

    PrintDebug("PTE Page fault fell through... Not sure if this should ever happen\n");
    PrintDebug("Manual Says to inject page fault into guest\n");
    return -1;
  }

  PrintDebug("Returning end of function\n");
  return 0;
}



addr_t create_new_shadow_pt32(struct guest_info * info) {
  void * host_pde = 0;

  V3_AllocPages(host_pde, 1);
  memset(host_pde, 0, PAGE_SIZE);

  return (addr_t)host_pde;
}



/* Currently Does not work with Segmentation!!! */
int handle_shadow_invlpg(struct guest_info * info) {
  if (info->mem_mode != VIRTUAL_MEM) {
    // Paging must be turned on...
    // should handle with some sort of fault I think
    PrintDebug("ERROR: INVLPG called in non paged mode\n");
    return -1;
  }


  if (info->cpu_mode == PROTECTED) {
    char instr[15];
    int ret;
    int index = 0;

    ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    if (ret != 15) {
      PrintDebug("Could not read instruction 0x%x (ret=%d)\n", info->rip, ret);
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

      index += 2;

      addr_type = decode_operands32(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);

      if (addr_type == MEM_OPERAND) {
	pde32_t * shadow_pd = (pde32_t *)CR3_TO_PDE32(info->shdw_pg_state.shadow_cr3);
	pde32_t * shadow_pde = (pde32_t *)&shadow_pd[PDE32_INDEX(first_operand)];

	//PrintDebug("PDE Index=%d\n", PDE32_INDEX(first_operand));
	//PrintDebug("FirstOperand = %x\n", first_operand);

	if (shadow_pde->large_page == 1) {
	  shadow_pde->present = 0;
	} else {
	  if (shadow_pde->present == 1) {
	    pte32_t * shadow_pt = (pte32_t *)PDE32_T_ADDR((*shadow_pde));
	    pte32_t * shadow_pte = (pte32_t *)&shadow_pt[PTE32_INDEX(first_operand)];

	    shadow_pte->present = 0;
	  }
	}

	info->rip += index;

      } else {
	PrintDebug("Invalid Operand type\n");
	return -1;
      }
    } else {
      PrintDebug("invalid Instruction Opcode\n");
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
