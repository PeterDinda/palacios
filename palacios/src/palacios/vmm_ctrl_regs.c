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


#ifndef DEBUG_CTRL_REGS
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif







int handle_cr0_write(struct guest_info * info) {
  char instr[15];
  int ret;
  struct x86_instr dec_instr;

  if (info->mem_mode == PHYSICAL_MEM) { 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  } else { 
    ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  }

  if (ret != 15) {
    // I think we should inject a GPF into the guest
    PrintError("Could not read instruction (ret=%d)\n", ret);
    return -1;
  }

  if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
    PrintError("Could not decode instruction\n");
    return -1;
  }


  if (opcode_cmp(V3_OPCODE_LMSW, (const uchar_t *)(dec_instr.opcode)) == 0) {
    struct cr0_real *real_cr0  = (struct cr0_real*)&(info->ctrl_regs.cr0);
    struct cr0_real *new_cr0 = (struct cr0_real *)(dec_instr.first_operand.operand);	
    uchar_t new_cr0_val;

    PrintDebug("LMSW\n");

    new_cr0_val = (*(char*)(new_cr0)) & 0x0f;
    
    PrintDebug("OperandVal = %x\n", new_cr0_val);

    PrintDebug("Old CR0=%x\n", *real_cr0);	
    *(uchar_t*)real_cr0 &= 0xf0;
    *(uchar_t*)real_cr0 |= new_cr0_val;
    PrintDebug("New CR0=%x\n", *real_cr0);	
      

    if (info->shdw_pg_mode == SHADOW_PAGING) {
      struct cr0_real * shadow_cr0 = (struct cr0_real*)&(info->shdw_pg_state.guest_cr0);
      
      PrintDebug(" Old Shadow CR0=%x\n", *shadow_cr0);	
      *(uchar_t*)shadow_cr0 &= 0xf0;
      *(uchar_t*)shadow_cr0 |= new_cr0_val;
      PrintDebug("New Shadow CR0=%x\n", *shadow_cr0);	
    }
  } else if (opcode_cmp(V3_OPCODE_MOV2CR, (const uchar_t *)(dec_instr.opcode)) == 0) {
    PrintDebug("MOV2CR0\n");

    if (info->cpu_mode == LONG) {
      // 64 bit registers
    } else {
      // 32 bit registers
	struct cr0_32 *real_cr0 = (struct cr0_32*)&(info->ctrl_regs.cr0);
	struct cr0_32 *new_cr0= (struct cr0_32 *)(dec_instr.second_operand.operand);

	PrintDebug("OperandVal = %x, length=%d\n", *new_cr0, dec_instr.first_operand.size);


	PrintDebug("Old CR0=%x\n", *real_cr0);
	*real_cr0 = *new_cr0;
	

 	if (info->shdw_pg_mode == SHADOW_PAGING) {
 	  struct cr0_32 * shadow_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
	  
 	  PrintDebug("Old Shadow CR0=%x\n", *shadow_cr0);	
	  
 	  real_cr0->et = 1;
	  
 	  *shadow_cr0 = *new_cr0;
 	  shadow_cr0->et = 1;
	  
	  if (get_mem_mode(info) == VIRTUAL_MEM) {
	    struct cr3_32 * shadow_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.shadow_cr3);
	    
	    info->ctrl_regs.cr3 = *(addr_t*)shadow_cr3;
	  } else  {
	    info->ctrl_regs.cr3 = *(addr_t*)&(info->direct_map_pt);
	    real_cr0->pg = 1;
	  }
	  
	  PrintDebug("New Shadow CR0=%x\n",*shadow_cr0);
 	}
	PrintDebug("New CR0=%x\n", *real_cr0);
    }

  } else if (opcode_cmp(V3_OPCODE_CLTS, (const uchar_t *)(dec_instr.opcode)) == 0) {
    // CLTS
    struct cr0_32 *real_cr0 = (struct cr0_32*)&(info->ctrl_regs.cr0);
	
    real_cr0->ts = 0;

    if (info->shdw_pg_mode == SHADOW_PAGING) {
      struct cr0_32 * shadow_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
      shadow_cr0->ts = 0;
    }
  }

  info->rip += dec_instr.instr_length;

  return 0;
}


int handle_cr0_read(struct guest_info * info) {
  char instr[15];

  switch (info->cpu_mode) { 

  case REAL: 
    {

      int index = 0;
      int ret;

      PrintDebug("Real Mode read from CR0 at linear guest pa 0x%x\n",get_addr_linear(info,info->rip,&(info->segments.cs)));
      //PrintV3Segments(info);

      // The real rip address is actually a combination of the rip + CS base 
      ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
      if (ret != 15) {
	// I think we should inject a GPF into the guest
	PrintDebug("Could not read Real Mode instruction (ret=%d)\n", ret);
	return -1;
      }


      while (is_prefix_byte(instr[index])) {
	switch(instr[index]) {
	case PREFIX_CS_OVERRIDE:
	case PREFIX_SS_OVERRIDE:
	case PREFIX_DS_OVERRIDE:
	case PREFIX_ES_OVERRIDE:
	case PREFIX_FS_OVERRIDE:
	case PREFIX_GS_OVERRIDE:
	  PrintDebug("Segment Override!!\n");
	  return -1;
	  break;
	default:
	  break;
	}
	index++; 
      }

      /*
	while (is_prefix_byte(instr[index])) {
	index++; 
	}
      */

      if ((instr[index] == cr_access_byte) && 
	  (instr[index + 1] == smsw_byte) && 
	  (MODRM_REG(instr[index + 2]) == smsw_reg_byte)) {

	// SMSW (store machine status word)

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
	// success

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

	if (info->shdw_pg_mode == SHADOW_PAGING) {
	  *virt_cr0 = *(struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
	} else {
	  *virt_cr0 = *real_cr0;
	}

	PrintDebug("Returning CR0: %x\n", *virt_cr0);

	info->rip += index;

      } else {
	PrintDebug("Unknown read instr from CR0\n");
	return -1;
      }

    } 

    break;

  case PROTECTED:
    {
    
      int index = 0;
      int ret;

      PrintDebug("Protected %s Mode read from CR0 at guest %s linear rip 0x%x\n", 
		 info->mem_mode == VIRTUAL_MEM ? "Paged" : "",
		 info->mem_mode == VIRTUAL_MEM ? "virtual" : "",
		 get_addr_linear(info, info->rip, &(info->segments.cs)));

      // We need to read the instruction, which is at CS:IP, but that 
      // linear address is guest physical without PG and guest virtual with PG
      if (info->cpu_mode == PHYSICAL_MEM) { 
	// The real rip address is actually a combination of the rip + CS base 
	ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
      } else { 
	// The real rip address is actually a combination of the rip + CS base 
	ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
      }


      /*
	PrintDebug("Instr (15 bytes) at %x:\n", instr);
	PrintTraceMemDump((char*)instr, 15);
      */

      if (ret != 15) {
	// I think we should inject a GPF into the guest
	PrintDebug("Could not read Protected %s mode instruction (ret=%d)\n", 
		   info->cpu_mode == VIRTUAL_MEM ? "Paged" : "", ret);
	return -1;
      }


      while (is_prefix_byte(instr[index])) {
	switch(instr[index]) {
	case PREFIX_CS_OVERRIDE:
	case PREFIX_SS_OVERRIDE:
	case PREFIX_DS_OVERRIDE:
	case PREFIX_ES_OVERRIDE:
	case PREFIX_FS_OVERRIDE:
	case PREFIX_GS_OVERRIDE:
	  PrintDebug("Segment Override!!\n");
	  return -1;
	  break;
	default:
	  break;
	}
	index++; 
      }


      /*
	while (is_prefix_byte(instr[index])) {
	index++; 
	}
      */

      if ((instr[index] == cr_access_byte) &&
	  (instr[index+1] == mov_from_cr_byte)) {
	
	// MOV from CR0 to register

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

	if (info->shdw_pg_mode == SHADOW_PAGING) {
	  *virt_cr0 = *(struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
	  
	  if (info->mem_mode == PHYSICAL_MEM) {
	    virt_cr0->pg = 0; // clear the pg bit because guest doesn't think it's on
	  }
	  
	} else {
	  *virt_cr0 = *real_cr0;
	}

	  PrintDebug("real CR0: %x\n", *(uint_t*)real_cr0);
	  PrintDebug("returned CR0: %x\n", *(uint_t*)virt_cr0);
      
	info->rip += index;

      } else { 
	PrintDebug("Unknown read instruction from CR0\n");
	return -1;
      }
    }
    break;

  case PROTECTED_PAE:
    PrintDebug("Protected PAE Mode read to CR0 is UNIMPLEMENTED\n");
    return -1;

  case LONG:
    PrintDebug("Protected Long Mode read to CR0 is UNIMPLEMENTED\n");
    return -1;


  default:
    {
      PrintDebug("Unknown Mode read from CR0 (info->cpu_mode=0x%x)\n",info->cpu_mode);
      return -1;
    }
    break;
  }


  return 0;
}




int handle_cr3_write(struct guest_info * info) {
  if (info->cpu_mode == REAL) {
    // WHAT THE HELL DOES THIS EVEN MEAN?????

    int index = 0;
    int ret;
    char instr[15];

    PrintDebug("Real Mode Write to CR3.\n");
    // We need to read the instruction, which is at CS:IP, but that 
    // linear address is guest physical without PG and guest virtual with PG

    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);

    if (ret != 15) {
      PrintDebug("Could not read instruction (ret=%d)\n", ret);
      return -1;
    }

    while (is_prefix_byte(instr[index])) {
      switch(instr[index]) {
      case PREFIX_CS_OVERRIDE:
      case PREFIX_SS_OVERRIDE:
      case PREFIX_DS_OVERRIDE:
      case PREFIX_ES_OVERRIDE:
      case PREFIX_FS_OVERRIDE:
      case PREFIX_GS_OVERRIDE:
	PrintDebug("Segment Override!!\n");
	return -1;
	break;
      default:
	break;
      }
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

      addr_type = decode_operands16(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);

      if (addr_type != REG_OPERAND) {
	/* Mov to CR3 can only be a 32 bit register */
	return -1;
      }

      new_cr3 = (struct cr3_32 *)first_operand;

      if (info->shdw_pg_mode == SHADOW_PAGING) {
	int flushed=0;
	addr_t shadow_pt;
	struct cr3_32 * shadow_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.shadow_cr3);
	struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);

	/*

	  if (CR3_TO_PDE32(*(uint_t*)shadow_cr3) != 0) {
	    PrintDebug("Shadow Page Table\n");
	    PrintDebugPageTables((pde32_t *)CR3_TO_PDE32(*(uint_t*)shadow_cr3));
	  }
	*/

	/* Delete the current Page Tables */
	if (!CR3_32_SAME_BASE(new_cr3,guest_cr3)) { 
	  PrintDebug("New CR3 is different - flushing shadow page table\n");
	  delete_page_tables_pde32((pde32_t *)CR3_TO_PDE32(*(uint_t*)shadow_cr3));
	  flushed=1;
	} else {
	  PrintDebug("New CR3 (0x%x) has same base as previous CR3 (0x%x) - reusing shadow page table\n", *((uint_t*)new_cr3), *((uint_t*)guest_cr3));
	}

	PrintDebug("Old Shadow CR3=%x; Old Guest CR3=%x\n", 
		   *(uint_t*)shadow_cr3, *(uint_t*)guest_cr3);


	*guest_cr3 = *new_cr3;


	if (flushed) { 
	  // Something like this
	  shadow_pt =  create_new_shadow_pt32(info);
	  //shadow_pt = setup_shadow_pt32(info, CR3_TO_PDE32(*(addr_t *)new_cr3));
	} else {
	  shadow_pt = shadow_cr3->pdt_base_addr<<12;
	}

	/* Copy Various flags */
	*shadow_cr3 = *new_cr3;

	/*
	{
	  addr_t tmp_addr;
	  guest_pa_to_host_va(info, ((*(uint_t*)guest_cr3) & 0xfffff000), &tmp_addr);
	  PrintDebug("Guest PD\n");
	  PrintPD32((pde32_t *)tmp_addr);

	}
	*/

	
	shadow_cr3->pdt_base_addr = PD32_BASE_ADDR(shadow_pt);

	PrintDebug("New Shadow CR3=%x; New Guest CR3=%x\n", 
		   *(uint_t*)shadow_cr3, *(uint_t*)guest_cr3);

      }
      info->rip += index;

    } else {
      PrintDebug("Unknown Instruction\n");
      PrintTraceMemDump(instr,15);
      return -1;
    }



  } else if (info->cpu_mode == PROTECTED) {
    int index = 0;
    int ret;
    char instr[15];

    PrintDebug("Protected %s mode write to CR3 at %s 0x%x\n",
	       info->cpu_mode==PROTECTED ? "" : "Paged", 
	       info->cpu_mode==PROTECTED ? "guest physical" : "guest virtual",
	       get_addr_linear(info,info->rip,&(info->segments.cs)));

    // We need to read the instruction, which is at CS:IP, but that 
    // linear address is guest physical without PG and guest virtual with PG
    if (info->mem_mode == PHYSICAL_MEM) { 
      // The real rip address is actually a combination of the rip + CS base 
      //PrintDebug("Writing Guest CR3 Write (Physical Address)\n");
      ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    } else { 
      //PrintDebug("Writing Guest CR3 Write (Virtual Address)\n");
      // The real rip address is actually a combination of the rip + CS base 
      ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    }

    if (ret != 15) {
      PrintDebug("Could not read instruction (ret=%d)\n", ret);
      return -1;
    }

    while (is_prefix_byte(instr[index])) {
      switch(instr[index]) {
      case PREFIX_CS_OVERRIDE:
      case PREFIX_SS_OVERRIDE:
      case PREFIX_DS_OVERRIDE:
      case PREFIX_ES_OVERRIDE:
      case PREFIX_FS_OVERRIDE:
      case PREFIX_GS_OVERRIDE:
	PrintDebug("Segment Override!!\n");
	return -1;
	break;
      default:
	break;
      }
      index++; 
    }
    
    /*    
	  while (is_prefix_byte(instr[index])) {
	  index++;
	  }
    */

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

      if (info->shdw_pg_mode == SHADOW_PAGING) {
	int flushed=0;
	addr_t shadow_pt;
	struct cr3_32 * shadow_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.shadow_cr3);
	struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);


	/*
	  if (CR3_TO_PDE32(*(uint_t*)shadow_cr3) != 0) {
	    PrintDebug("Shadow Page Table\n");
	    PrintDebugPageTables((pde32_t *)CR3_TO_PDE32(*(uint_t*)shadow_cr3));
	  }
	*/

	/* Delete the current Page Tables */
	if (!CR3_32_SAME_BASE(guest_cr3,new_cr3)) { 
	  PrintDebug("New CR3 is different - flushing shadow page table\n");
	  delete_page_tables_pde32((pde32_t *)CR3_TO_PDE32(*(uint_t*)shadow_cr3));
	  flushed=1;
	} else {
	  PrintDebug("New CR3 (0x%x) has same base as previous CR3 (0x%x) - reusing shadow page table\n",*((uint_t*)new_cr3), *((uint_t*)guest_cr3));
	}

	PrintDebug("Old Shadow CR3=%x; Old Guest CR3=%x\n", 
		   *(uint_t*)shadow_cr3, *(uint_t*)guest_cr3);


	*guest_cr3 = *new_cr3;

	if (flushed) { 
	  // Something like this
	  shadow_pt =  create_new_shadow_pt32(info);
	  //shadow_pt = setup_shadow_pt32(info, CR3_TO_PDE32(*(addr_t *)new_cr3));
	} else {
	  shadow_pt =shadow_cr3->pdt_base_addr << 12;
	}


	/* Copy Various flags */
	*shadow_cr3 = *new_cr3;

	/*
	{
	  addr_t tmp_addr;
	  guest_pa_to_host_va(info, ((*(uint_t*)guest_cr3) & 0xfffff000), &tmp_addr);
	  PrintDebug("Guest PD\n");
	  PrintPD32((pde32_t *)tmp_addr);

	}
	*/

	
	shadow_cr3->pdt_base_addr = PD32_BASE_ADDR(shadow_pt);

	PrintDebug("New Shadow CR3=%x; New Guest CR3=%x\n", 
		   *(uint_t*)shadow_cr3, *(uint_t*)guest_cr3);



	if (info->mem_mode == VIRTUAL_MEM) {
	  // If we aren't in paged mode then we have to preserve the identity mapped CR3
	  info->ctrl_regs.cr3 = *(addr_t*)shadow_cr3;
	}
      }

      info->rip += index;

    } else {
      PrintDebug("Unknown Instruction\n");
      PrintTraceMemDump(instr,15);
      return -1;
    }
  } else {
    PrintDebug("Invalid operating Mode (0x%x)\n", info->cpu_mode);
    return -1;
  }

  return 0;
}




int handle_cr3_read(struct guest_info * info) {

  if (info->cpu_mode == REAL) {
    char instr[15];
    int ret;
    int index = 0;
    addr_t linear_addr = 0;

    linear_addr = get_addr_linear(info, info->rip, &(info->segments.cs));

    
    //PrintDebug("RIP Linear: %x\n", linear_addr);
    //PrintV3Segments(info);
    
    ret = read_guest_pa_memory(info, linear_addr, 15, instr);

    if (ret != 15) {
      PrintDebug("Could not read instruction (ret=%d)\n", ret);
      return -1;
    }
    
    while (is_prefix_byte(instr[index])) {
      switch(instr[index]) {
      case PREFIX_CS_OVERRIDE:
      case PREFIX_SS_OVERRIDE:
      case PREFIX_DS_OVERRIDE:
      case PREFIX_ES_OVERRIDE:
      case PREFIX_FS_OVERRIDE:
      case PREFIX_GS_OVERRIDE:
	PrintDebug("Segment Override!!\n");
	return -1;
	break;
      default:
	break;
      }
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

      addr_type = decode_operands16(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);

      if (addr_type != REG_OPERAND) {
	/* Mov to CR3 can only be a 32 bit register */
	return -1;
      }

      virt_cr3 = (struct cr3_32 *)first_operand;

      if (info->shdw_pg_mode == SHADOW_PAGING) {
	*virt_cr3 = *(struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);
      } else {
	*virt_cr3 = *real_cr3;
      }
      
      info->rip += index;
    } else {
      PrintDebug("Unknown Instruction\n");
      PrintTraceMemDump(instr,15);
      return -1;
    }


    return 0;
  } else if (info->cpu_mode == PROTECTED) {

    int index = 0;
    int ret;
    char instr[15];

   
    // We need to read the instruction, which is at CS:IP, but that 
    // linear address is guest physical without PG and guest virtual with PG
    if (info->cpu_mode == PHYSICAL_MEM) { 
      // The real rip address is actually a combination of the rip + CS base 
      ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    } else { 
      // The real rip address is actually a combination of the rip + CS base 
      ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    }

    if (ret != 15) {
      PrintDebug("Could not read instruction (ret=%d)\n", ret);
      return -1;
    }
    
    while (is_prefix_byte(instr[index])) {
      switch(instr[index]) {
      case PREFIX_CS_OVERRIDE:
      case PREFIX_SS_OVERRIDE:
      case PREFIX_DS_OVERRIDE:
      case PREFIX_ES_OVERRIDE:
      case PREFIX_FS_OVERRIDE:
      case PREFIX_GS_OVERRIDE:
	PrintDebug("Segment Override!!\n");
	return -1;
	break;
      default:
	break;
      }
      index++; 
    }

    /*
      while (is_prefix_byte(instr[index])) {
      index++;
      }
    */

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

      if (info->shdw_pg_mode == SHADOW_PAGING) {
	*virt_cr3 = *(struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);
      } else {
	*virt_cr3 = *real_cr3;
      }
      
      info->rip += index;
    } else {
      PrintDebug("Unknown Instruction\n");
      PrintTraceMemDump(instr,15);
      return -1;
    }
  } else {
    PrintDebug("Invalid operating Mode (0x%x), control registers follow\n", info->cpu_mode);
    PrintV3CtrlRegs(info);
    return -1;
  }

  return 0;
}
