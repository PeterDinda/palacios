#include <palacios/vmm_mem.h>
#include <palacios/vmm.h>
#include <palacios/vmcb.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_ctrl_regs.h>


/* Segmentation is a problem here...
 *
 * When we get a memory operand, presumably we use the default segment (which is?) 
 * unless an alternate segment was specfied in the prefix...
 */


int handle_cr0_write(struct guest_info * info) {
  char instr[15];
  
  
  if (info->cpu_mode == REAL) {
    int index = 0;
    int ret;

    // The real rip address is actually a combination of the rip + CS base 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    if (ret != 15) {
      // I think we should inject a GPF into the guest
      PrintDebug("Could not read instruction (ret=%d)\n", ret);
      return -1;
    }

    while (is_prefix_byte(instr[index])) {
      index++; 
    }

    if ((instr[index] == cr_access_byte) && 
	(instr[index + 1] == lmsw_byte) && 
	(MODRM_REG(instr[index + 2]) == lmsw_reg_byte)) {
 
      addr_t first_operand;
      addr_t second_operand;
      struct cr0_real *real_cr0;
      struct cr0_real *new_cr0;
      operand_type_t addr_type;
      char new_cr0_val = 0;
      // LMSW
      // decode mod/RM
      index += 2;
 
      real_cr0 = (struct cr0_real*)&(info->ctrl_regs.cr0);

      addr_type = decode_operands16(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG16);


      if (addr_type == REG_OPERAND) {
	new_cr0 = (struct cr0_real *)first_operand;
      } else if (addr_type == MEM_OPERAND) {
	addr_t host_addr;

	if (guest_pa_to_host_va(info, first_operand + (info->segments.ds.base << 4), &host_addr) == -1) {
	  // gpf the guest
	  return -1;
	}

	new_cr0 = (struct cr0_real *)host_addr;
      } else {
	// error... don't know what to do
	return -1;
      }
		 
      if ((new_cr0->pe == 1) && (real_cr0->pe == 0)) {
	info->cpu_mode = PROTECTED;
      } else if ((new_cr0->pe == 0) && (real_cr0->pe == 1)) {
	info->cpu_mode = REAL;
      }
      
      new_cr0_val = *(char*)(new_cr0) & 0x0f;


      if (info->page_mode == SHADOW_PAGING) {
	struct cr0_real * shadow_cr0 = (struct cr0_real*)&(info->shdw_pg_state.guest_cr0);

	PrintDebug("Old CR0=%x, Old Shadow CR0=%x\n", *real_cr0, *shadow_cr0);	
	/* struct cr0_real is only 4 bits wide, 
	 * so we can overwrite the real_cr0 without worrying about the shadow fields
	 */
	*(char*)real_cr0 &= 0xf0;
	*(char*)real_cr0 |= new_cr0_val;
	
	*(char*)shadow_cr0 &= 0xf0;
	*(char*)shadow_cr0 |= new_cr0_val;

	PrintDebug("New CR0=%x, New Shadow CR0=%x\n", *real_cr0, *shadow_cr0);	
      } else {
	PrintDebug("Old CR0=%x\n", *real_cr0);	
	// for now we just pass through....
	*(char*)real_cr0 &= 0xf0;
	*(char*)real_cr0 |= new_cr0_val;

	PrintDebug("New CR0=%x\n", *real_cr0);	
      }


      info->rip += index;

    } else if ((instr[index] == cr_access_byte) && 
	       (instr[index + 1] == clts_byte)) {
      // CLTS
      PrintDebug("CLTS unhandled\n");
      return -1;

    } else if ((instr[index] == cr_access_byte) && 
	       (instr[index + 1] = mov_to_cr_byte)) {
      addr_t first_operand;
      addr_t second_operand;
      struct cr0_32 *real_cr0;
      struct cr0_32 *new_cr0;
      operand_type_t addr_type;
     
      
      index += 2;
 
      real_cr0 = (struct cr0_32*)&(info->ctrl_regs.cr0);

      addr_type = decode_operands16(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);

      if (addr_type != REG_OPERAND) {
	/* Mov to CR0 Can only be a 32 bit register */
	// FIX ME
	return -1;
      }

      new_cr0 = (struct cr0_32 *)first_operand;

      if (new_cr0->pe == 1) {
	PrintDebug("Entering Protected Mode\n");
	info->cpu_mode = PROTECTED;
      }

      if (new_cr0->pg == 1) {
	// GPF the guest??
	return -1;
      }

      if (info->page_mode == SHADOW_PAGING) {
	struct cr0_32 * shadow_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
	
	PrintDebug("Old CR0=%x, Old Shadow CR0=%x\n", *real_cr0, *shadow_cr0);	
	*real_cr0 = *new_cr0;
	real_cr0->pg = 1;

	*shadow_cr0 = *new_cr0;

	PrintDebug("New CR0=%x, New Shadow CR0=%x\n", *real_cr0, *shadow_cr0);	
      } else {
	PrintDebug("Old CR0=%x\n", *real_cr0);	
	*real_cr0 = *new_cr0;
	PrintDebug("New CR0=%x\n", *real_cr0);	
      }

      info->rip += index;

    } else {
      PrintDebug("Unsupported Instruction\n");
      // unsupported instruction, UD the guest
      return -1;
    }


  } else if (info->cpu_mode == PROTECTED) {
    int index = 0;
    int ret;

    PrintDebug("Protected Mode write to CR0\n");

    // The real rip address is actually a combination of the rip + CS base 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    if (ret != 15) {
      // I think we should inject a GPF into the guest
      PrintDebug("Could not read instruction (ret=%d)\n", ret);
      return -1;
    }

    while (is_prefix_byte(instr[index])) {
      index++; 
    }

    if ((instr[index] == cr_access_byte) && 
	(instr[index + 1] == mov_to_cr_byte)) {
    
      addr_t first_operand;
      addr_t second_operand;
      struct cr0_32 *real_cr0;
      struct cr0_32 *new_cr0;
      operand_type_t addr_type;

      index += 2;
 
      real_cr0 = (struct cr0_32*)&(info->ctrl_regs.cr0);

      addr_type = decode_operands32(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);

      if (addr_type != REG_OPERAND) {
	return -1;
      }

      new_cr0 = (struct cr0_32 *)first_operand;


      if (info->page_mode == SHADOW_PAGING) {
	struct cr0_32 * shadow_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);

	if (new_cr0->pg == 1){
	  struct cr3_32 * shadow_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.shadow_cr3);

	  info->cpu_mode = PROTECTED_PG;
	  
	  *shadow_cr0 = *new_cr0;
	  *real_cr0 = *new_cr0;

	  //
	  // Activate Shadow Paging
	  //
	  PrintDebug("Turning on paging in the guest\n");

	  info->ctrl_regs.cr3 = *(addr_t*)shadow_cr3;
	  

	} else if (new_cr0->pe == 0) {
	  info->cpu_mode = REAL;

	  *shadow_cr0 = *new_cr0;
	  *real_cr0 = *new_cr0;
	  real_cr0->pg = 1;
	}


      } else {
	*real_cr0 = *new_cr0;
      }

      info->rip += index;
    }
    
  } else { 
    PrintDebug("Unknown Mode write to CR0\n");
    return -1;
  }
  return 0;
}


int handle_cr0_read(struct guest_info * info) {
  char instr[15];

  if (info->cpu_mode == REAL) {
    int index = 0;
    int ret;

    // The real rip address is actually a combination of the rip + CS base 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    if (ret != 15) {
      // I think we should inject a GPF into the guest
      PrintDebug("Could not read Real Mode instruction (ret=%d)\n", ret);
      return -1;
    }


    while (is_prefix_byte(instr[index])) {
      index++; 
    }

    if ((instr[index] == cr_access_byte) && 
	(instr[index + 1] == smsw_byte) && 
	(MODRM_REG(instr[index + 2]) == smsw_reg_byte)) {

      addr_t first_operand;
      addr_t second_operand;
      struct cr0_real *cr0;
      operand_type_t addr_type;
      char cr0_val = 0;

      index += 2;
      
      cr0 = (struct cr0_real*)&(info->ctrl_regs.cr0);
      
      
      addr_type = decode_operands16(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG16);
      
      if (addr_type == MEM_OPERAND) {
	addr_t host_addr;
	
	if (guest_pa_to_host_va(info, first_operand + (info->segments.ds.base << 4), &host_addr) == -1) {
	  // gpf the guest
	  PrintDebug("Could not convert guest physical address to host virtual address\n");
	  return -1;
	}
	
	first_operand = host_addr;
      } else {
	// Register operand
	// Should be ok??
      }

      cr0_val = *(char*)cr0 & 0x0f;

      *(char *)first_operand &= 0xf0;
      *(char *)first_operand |= cr0_val;

      PrintDebug("index = %d, rip = %x\n", index, (ulong_t)(info->rip));
      info->rip += index;
      PrintDebug("new_rip = %x\n", (ulong_t)(info->rip));
    } else if ((instr[index] == cr_access_byte) &&
	       (instr[index+1] == mov_from_cr_byte)) {
      /* Mov from CR0
       * This can only take a 32 bit register argument in anything less than 64 bit mode.
       */
      addr_t first_operand;
      addr_t second_operand;
      operand_type_t addr_type;

      struct cr0_32 * real_cr0 = (struct cr0_32 *)&(info->ctrl_regs.cr0);

      index += 2;

      addr_type = decode_operands16(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);
     
      struct cr0_32 * virt_cr0 = (struct cr0_32 *)first_operand;
  
      if (addr_type != REG_OPERAND) {
	// invalid opcode to guest
	PrintDebug("Invalid operand type in mov from CR0\n");
	return -1;
      }

      if (info->page_mode == SHADOW_PAGING) {
	*virt_cr0 = *(struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
      } else {
	*virt_cr0 = *real_cr0;
      }

      info->rip += index;

    } else {
      PrintDebug("Unknown read instr from CR0\n");
      return -1;
    }

  } else if (info->cpu_mode == PROTECTED) {
    int index = 0;
    int ret;

    // The real rip address is actually a combination of the rip + CS base 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    if (ret != 15) {
      // I think we should inject a GPF into the guest
      PrintDebug("Could not read Proteced mode instruction (ret=%d)\n", ret);
      return -1;
    }

    while (is_prefix_byte(instr[index])) {
      index++; 
    }


    if ((instr[index] == cr_access_byte) &&
	(instr[index+1] == mov_from_cr_byte)) {
      addr_t first_operand;
      addr_t second_operand;
      operand_type_t addr_type;
      struct cr0_32 * virt_cr0;
      struct cr0_32 * real_cr0 = (struct cr0_32 *)&(info->ctrl_regs.cr0);

      index += 2;

      addr_type = decode_operands32(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);

      if (addr_type != REG_OPERAND) {
	PrintDebug("Invalid operand type in mov from CR0\n");
	return -1;
      }
      
      virt_cr0 = (struct cr0_32 *)first_operand;

      if (info->page_mode == SHADOW_PAGING) {
	*virt_cr0 = *(struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
      } else {
	*virt_cr0 = *real_cr0;
      }
      
      info->rip += index;

    } else { 
      PrintDebug("Unknown read instruction from CR0\n");
      return -1;
    }

  } else {
    PrintDebug("Unknown mode read from CR0\n");
    return -1;
  }


  return 0;
}




int handle_cr3_write(struct guest_info * info) {
  if ((info->cpu_mode == PROTECTED) || (info->cpu_mode == PROTECTED_PG)) {
    int index = 0;
    int ret;
    char instr[15];


    /* Isn't the RIP a Guest Virtual Address???????? */
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    if (ret != 15) {
      PrintDebug("Could not read instruction (ret=%d)\n", ret);
      return -1;
    }
    
    while (is_prefix_byte(instr[index])) {
      index++;
    }

    if ((instr[index] == cr_access_byte) && 
	(instr[index + 1] == mov_to_cr_byte)) {

      addr_t first_operand;
      addr_t second_operand;
      struct cr3_32 * new_cr3;
      //      struct cr3_32 * real_cr3;
      operand_type_t addr_type;

      index += 2;

      addr_type = decode_operands32(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);

      if (addr_type != REG_OPERAND) {
	/* Mov to CR3 can only be a 32 bit register */
	return -1;
      }

      new_cr3 = (struct cr3_32 *)first_operand;

      if (info->page_mode == SHADOW_PAGING) {
	addr_t shadow_pt;
	struct cr3_32 * shadow_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.shadow_cr3);
	struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);

	/* Delete the current Page Tables */
	delete_page_tables_pde32((pde32_t *)CR3_TO_PDE32(shadow_cr3));

	*guest_cr3 = *new_cr3;

	// Something like this
	shadow_pt =  create_new_shadow_pt32(info);
	//shadow_pt = setup_shadow_pt32(info, CR3_TO_PDE32(*(addr_t *)new_cr3));


	/* Copy Various flags */
	*shadow_cr3 = *new_cr3;
	
	shadow_cr3->pdt_base_addr = PD32_BASE_ADDR(shadow_pt);

	if (info->cpu_mode == PROTECTED_PG) {
	  // If we aren't in paged mode then we have to preserve the identity mapped CR3
	  info->ctrl_regs.cr3 = *(addr_t*)shadow_cr3;
	}
      }

      info->rip += index;

    } else {
      PrintDebug("Unknown Instruction\n");
      return -1;
    }
  } else {
    PrintDebug("Invalid operating Mode\n");
    return -1;
  }

  return 0;
}




int handle_cr3_read(struct guest_info * info) {
  if ((info->cpu_mode == PROTECTED) || (info->cpu_mode == PROTECTED_PG)) {
    int index = 0;
    int ret;
    char instr[15];

    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    if (ret != 15) {
      PrintDebug("Could not read instruction (ret=%d)\n", ret);
      return -1;
    }
    
    while (is_prefix_byte(instr[index])) {
      index++;
    }

    if ((instr[index] == cr_access_byte) && 
	(instr[index + 1] == mov_from_cr_byte)) {
      addr_t first_operand;
      addr_t second_operand;
      struct cr3_32 * virt_cr3;
      struct cr3_32 * real_cr3 = (struct cr3_32 *)&(info->ctrl_regs.cr3);
      operand_type_t addr_type;

      index += 2;

      addr_type = decode_operands32(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);

      if (addr_type != REG_OPERAND) {
	/* Mov to CR3 can only be a 32 bit register */
	return -1;
      }

      virt_cr3 = (struct cr3_32 *)first_operand;

      if (info->page_mode == SHADOW_PAGING) {
	*virt_cr3 = *(struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);
      } else {
	*virt_cr3 = *real_cr3;
      }
      
      info->rip += index;
    } else {
      PrintDebug("Unknown Instruction\n");
      return -1;
    }
  } else {
    PrintDebug("Invalid operating Mode\n");
    return -1;
  }

  return 0;
}
