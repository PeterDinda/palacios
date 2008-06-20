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
  
  switch (info->cpu_mode) {
  case PROTECTED_PG:
    return handle_shadow_pagefault32(info, fault_addr, error_code);
    break;
  case PROTECTED_PAE_PG:
  case LONG_PG:
    // currently not handled
    return -1;
    break;
  case REAL:
  case PROTECTED:
  case PROTECTED_PAE:
  case LONG:
    // If paging is not turned on we need to handle the special cases
    return handle_special_page_fault(info, fault_addr, error_code);
    break;
  default:
    return -1;
  }
}


int handle_shadow_pagefault32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  pde32_t * guest_pde = NULL;
  pde32_t * shadow_pde = (pde32_t *)CR3_TO_PDE32(info->shdw_pg_state.shadow_cr3);
  addr_t guest_cr3 = CR3_TO_PDE32(info->shdw_pg_state.guest_cr3);
  pt_access_status_t guest_pde_access;
  pt_access_status_t shadow_pde_access;
  pde32_t * guest_pde_entry = NULL;
  pde32_t * shadow_pde_entry = (pde32_t *)&(shadow_pde[PDE32_INDEX(fault_addr)]);

  if (guest_pa_to_host_va(info, guest_cr3, (addr_t*)&guest_pde) == -1) {
    PrintDebug("Invalid Guest PDE Address: 0x%x\n", guest_cr3);
    return -1;
  }


  guest_pde_entry = (pde32_t *)&(guest_pde[PDE32_INDEX(fault_addr)]);

  // Check the guest page permissions
  guest_pde_access = can_access_pde32(guest_pde, fault_addr, error_code);

  if (guest_pde_access != PT_ACCESS_OK) {
    // inject page fault to the guest (Guest PDE fault)

    info->ctrl_regs.cr2 = fault_addr;
    raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);

    return 0;
  }

  shadow_pde_access = can_access_pde32(shadow_pde, fault_addr, error_code);


  if (shadow_pde_access == PT_ENTRY_NOT_PRESENT) {
    pte32_t * shadow_pte = NULL;

    V3_AllocPages(shadow_pte, 1);
    memset(shadow_pte, 0, PAGE_SIZE);

    shadow_pde_entry->pt_base_addr = PD32_BASE_ADDR(shadow_pte);
    

    shadow_pde_entry->present = 1;
    shadow_pde_entry->user_page = guest_pde_entry->user_page;
    
    // VMM Specific options
    shadow_pde_entry->write_through = 0;
    shadow_pde_entry->cache_disable = 0;
    shadow_pde_entry->global_page = 0;
    //

    guest_pde_entry->accessed = 1;

    if (guest_pde_entry->large_page == 0) {
      shadow_pde_entry->writable = guest_pde_entry->writable;
    } else {
      /*
       * Check the Intel manual because we are ignoring Large Page issues here
       * Also be wary of hooked pages
       */
    }

  } else if (shadow_pde_access == PT_WRITE_ERROR) {

    //
    // Page Directory Entry marked read-only
    //

    PrintDebug("Shadow Paging Write Error\n");
    return -1;
  } else if (shadow_pde_access == PT_USER_ERROR) {

    //
    // Page Directory Entry marked non-user
    //
    
    PrintDebug("Shadow Paging User access error\n");
    return -1;
  } else if (shadow_pde_access == PT_ACCESS_OK) {
    pte32_t * shadow_pte = (pte32_t *)PDE32_T_ADDR((*shadow_pde_entry));
    pte32_t * guest_pte = NULL;

    // Page Table Entry fault
    
    if (guest_pa_to_host_va(info, PDE32_T_ADDR((*guest_pde_entry)), (addr_t*)&guest_pte) == -1) {
      PrintDebug("Invalid Guest PTE Address: 0x%x\n", PDE32_T_ADDR((*guest_pde_entry)));
      // Machine check the guest

      raise_exception(info, MC_EXCEPTION);
      
      return 0;
    }


    if (handle_shadow_pte32_fault(info, fault_addr, error_code, shadow_pte, guest_pte)  == -1) {
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

  PrintDebugPageTables(shadow_pde);

  return 0;
}



/* 
 * We assume the the guest pte pointer has already been translated to a host virtual address
 */
int handle_shadow_pte32_fault(struct guest_info * info, 
			      addr_t fault_addr, 
			      pf_error_t error_code,
			      pte32_t * shadow_pte, 
			      pte32_t * guest_pte) {

  pt_access_status_t guest_pte_access;
  pt_access_status_t shadow_pte_access;
  pte32_t * guest_pte_entry = (pte32_t *)&(guest_pte[PTE32_INDEX(fault_addr)]);;
  pte32_t * shadow_pte_entry = (pte32_t *)&(shadow_pte[PTE32_INDEX(fault_addr)]);


  // Check the guest page permissions
  guest_pte_access = can_access_pte32(guest_pte, fault_addr, error_code);

  
  if (guest_pte_access != PT_ACCESS_OK) {
    // Inject page fault into the guest	
    
    info->ctrl_regs.cr2 = fault_addr;
    raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);
    
    return 0;
  }
  
  
  shadow_pte_access = can_access_pte32(shadow_pte, fault_addr, error_code);

  if (shadow_pte_access == PT_ACCESS_OK) {
    // Inconsistent state...
    // Guest Re-Entry will flush page tables and everything should now work
    return 0;
  } else if (shadow_pte_access == PT_ENTRY_NOT_PRESENT) {
    addr_t shadow_pa;
    addr_t guest_pa = PTE32_T_ADDR((*guest_pte_entry));

    // Page Table Entry Not Present

    host_region_type_t host_page_type = get_shadow_addr_type(info, guest_pa);

    if (host_page_type == HOST_REGION_INVALID) {
      // Inject a machine check in the guest

      raise_exception(info, MC_EXCEPTION);

      PrintDebug("Invalid Guest Address in page table (0x%x)\n", guest_pa);
      return 0;

    } else if (host_page_type == HOST_REGION_PHYSICAL_MEMORY) {
      
      shadow_pa = get_shadow_addr(info, guest_pa);
      
      shadow_pte_entry->page_base_addr = PT32_BASE_ADDR(shadow_pa);
      
      shadow_pte_entry->present = guest_pte_entry->present;
      shadow_pte_entry->user_page = guest_pte_entry->user_page;
      
      //set according to VMM policy
      shadow_pte_entry->write_through = 0;
      shadow_pte_entry->cache_disable = 0;
      shadow_pte_entry->global_page = 0;
      //
      
      guest_pte_entry->accessed = 1;
      
      if (guest_pte_entry->dirty == 1) {
	shadow_pte_entry->writable = guest_pte_entry->writable;
      } else if ((guest_pte_entry->dirty == 0) && (error_code.write == 1)) {
	shadow_pte_entry->writable = guest_pte_entry->writable;
	guest_pte_entry->dirty = 1;
      } else if ((guest_pte_entry->dirty = 0) && (error_code.write == 0)) {
	shadow_pte_entry->writable = 0;
      }
    } else {
      // Page fault handled by hook functions
      if (handle_special_page_fault(info, fault_addr, error_code) == -1) {
	PrintDebug("Special Page fault handler returned error for address: %x\n", fault_addr);
	return -1;
      }
    }

  } else if ((shadow_pte_access == PT_WRITE_ERROR) &&
	     (guest_pte_entry->dirty == 0)) {
    guest_pte_entry->dirty = 1;
    shadow_pte_entry->writable = guest_pte_entry->writable;

    return 0;
  } else {
    // Inject page fault into the guest	
	
    info->ctrl_regs.cr2 = fault_addr;
    raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);

    PrintDebug("PTE Page fault fell through... Not sure if this should ever happen\n");
    PrintDebug("Manual Says to inject page fault into guest\n");
    return -1;
  }

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
  if (info->cpu_mode == PROTECTED_PG) {
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
	pde32_t * shadow_pde_entry = (pde32_t *)&shadow_pd[PDE32_INDEX(first_operand)];

	//PrintDebug("PDE Index=%d\n", PDE32_INDEX(first_operand));
	//PrintDebug("FirstOperand = %x\n", first_operand);

	if (shadow_pde_entry->large_page == 1) {
	  shadow_pde_entry->present = 0;
	} else {
	  if (shadow_pde_entry->present == 1) {
	    pte32_t * shadow_pt = (pte32_t *)PDE32_T_ADDR((*shadow_pde_entry));
	    pte32_t * shadow_pte_entry = (pte32_t *)&shadow_pt[PTE32_INDEX(first_operand)];

	    shadow_pte_entry->present = 0;
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
