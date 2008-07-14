#include <palacios/vmm_mem.h>
#include <palacios/vmm.h>
#include <palacios/vmcb.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_ctrl_regs.h>



extern void SerialMemDump(unsigned char *start, int n);


#if VMM_DEBUG
void PrintCtrlRegs(struct guest_info *info)
{
  struct cr0_32 cr0 = *((struct cr0_32 *) &(info->ctrl_regs.cr0));
  struct cr2_32 cr2 = *((struct cr2_32 *) &(info->ctrl_regs.cr2));
  struct cr3_32 cr3 = *((struct cr3_32 *) &(info->ctrl_regs.cr3));
  struct cr4_32 cr4 = *((struct cr4_32 *) &(info->ctrl_regs.cr4));
  struct rflags rflags = *((struct rflags *) &(info->ctrl_regs.rflags));

  PrintDebug("CR0:     pe    0x%x\n",cr0.pe);
  PrintDebug("CR0:     mp    0x%x\n",cr0.mp);
  PrintDebug("CR0:     em    0x%x\n",cr0.em);
  PrintDebug("CR0:     ts    0x%x\n",cr0.ts);
  PrintDebug("CR0:     et    0x%x\n",cr0.et);
  PrintDebug("CR0:     ne    0x%x\n",cr0.ne);
  PrintDebug("CR0:     rsvd1 0x%x\n",cr0.rsvd1);
  PrintDebug("CR0:     wp    0x%x\n",cr0.wp);
  PrintDebug("CR0:     rsvd2 0x%x\n",cr0.rsvd2);
  PrintDebug("CR0:     am    0x%x\n",cr0.am);
  PrintDebug("CR0:     rsvd3 0x%x\n",cr0.rsvd3);
  PrintDebug("CR0:     nw    0x%x\n",cr0.nw);
  PrintDebug("CR0:     cd    0x%x\n",cr0.cd);
  PrintDebug("CR0:     pg    0x%x\n",cr0.pg);

  PrintDebug("CR2:     pfadd 0x%x\n",cr2.pf_vaddr);

  PrintDebug("CR3:     rsvd1 0x%x\n",cr3.rsvd1);
  PrintDebug("CR3:     pwt   0x%x\n",cr3.pwt);
  PrintDebug("CR3:     pcd   0x%x\n",cr3.pcd);
  PrintDebug("CR3:     rsvd2 0x%x\n",cr3.rsvd2);
  PrintDebug("CR3:     pdt   0x%x\n",cr3.pdt_base_addr);

  PrintDebug("CR4:     vme   0x%x\n",cr4.vme);
  PrintDebug("CR4:     pvi   0x%x\n",cr4.pvi);
  PrintDebug("CR4:     tsd   0x%x\n",cr4.tsd);
  PrintDebug("CR4:     de    0x%x\n",cr4.de);
  PrintDebug("CR4:     pse   0x%x\n",cr4.pse);
  PrintDebug("CR4:     pae   0x%x\n",cr4.pae);
  PrintDebug("CR4:     mce   0x%x\n",cr4.mce);
  PrintDebug("CR4:     pge   0x%x\n",cr4.pge);
  PrintDebug("CR4:     pce   0x%x\n",cr4.pce);
  PrintDebug("CR4:     osfx  0x%x\n",cr4.osf_xsr);
  PrintDebug("CR4:     osx   0x%x\n",cr4.osx);
  PrintDebug("CR4:     rsvd1 0x%x\n",cr4.rsvd1);

  PrintDebug("RFLAGS:  cf    0x%x\n",rflags.cf);
  PrintDebug("RFLAGS:  rsvd1 0x%x\n",rflags.rsvd1);
  PrintDebug("RFLAGS:  pf    0x%x\n",rflags.pf);
  PrintDebug("RFLAGS:  rsvd2 0x%x\n",rflags.rsvd2);
  PrintDebug("RFLAGS:  af    0x%x\n",rflags.af);
  PrintDebug("RFLAGS:  rsvd3 0x%x\n",rflags.rsvd3);
  PrintDebug("RFLAGS:  zf    0x%x\n",rflags.zf);
  PrintDebug("RFLAGS:  sf    0x%x\n",rflags.sf);
  PrintDebug("RFLAGS:  tf    0x%x\n",rflags.tf);
  PrintDebug("RFLAGS:  intr  0x%x\n",rflags.intr);
  PrintDebug("RFLAGS:  df    0x%x\n",rflags.df);
  PrintDebug("RFLAGS:  of    0x%x\n",rflags.of);
  PrintDebug("RFLAGS:  iopl  0x%x\n",rflags.iopl);
  PrintDebug("RFLAGS:  nt    0x%x\n",rflags.nt);
  PrintDebug("RFLAGS:  rsvd4 0x%x\n",rflags.rsvd4);
  PrintDebug("RFLAGS:  rf    0x%x\n",rflags.rf);
  PrintDebug("RFLAGS:  vm    0x%x\n",rflags.vm);
  PrintDebug("RFLAGS:  ac    0x%x\n",rflags.ac);
  PrintDebug("RFLAGS:  vif   0x%x\n",rflags.vif);
  PrintDebug("RFLAGS:  id    0x%x\n",rflags.id);
  PrintDebug("RFLAGS:  rsvd5 0x%x\n",rflags.rsvd5);
  PrintDebug("RFLAGS:  rsvd6 0x%x\n",rflags.rsvd6);

}
#else
void PrintCtrlRegs(struct guest_info *info)
{}
#endif

/* Segmentation is a problem here...
 *
 * When we get a memory operand, presumably we use the default segment (which is?) 
 * unless an alternate segment was specfied in the prefix...
 */


int handle_cr0_write(struct guest_info * info) {
  char instr[15];
  
  
  switch (info->cpu_mode) { 
  case REAL: 
    {
      int index = 0;
      int ret;
      
      PrintDebug("Real Mode write to CR0 at linear guest pa 0x%x\n",get_addr_linear(info,info->rip,&(info->segments.cs)));

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
	  PrintDebug("Memory operand in real mode write to CR0 is UNIMPLEMENTED\n");
	  // error... don't know what to do
	  return -1;
	}
		 
	if ((new_cr0->pe == 1) && (real_cr0->pe == 0)) {
	  info->cpu_mode = PROTECTED;
	} else if ((new_cr0->pe == 0) && (real_cr0->pe == 1)) {
	  info->cpu_mode = REAL;
	}
      
	new_cr0_val = *(char*)(new_cr0) & 0x0f;


	if (info->shdw_pg_mode == SHADOW_PAGING) {
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
	PrintDebug("CLTS unhandled in CR0 write\n");
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
	  PrintDebug("Moving to CR0 from non-register operand in CR0 write\n");
	  /* Mov to CR0 Can only be a 32 bit register */
	  // FIX ME
	  return -1;
	}

	new_cr0 = (struct cr0_32 *)first_operand;

	if (new_cr0->pe == 1) {
	  PrintDebug("Entering Protected Mode\n");
	  info->cpu_mode = PROTECTED;
	}

	if (new_cr0->pe == 0) { 
	  PrintDebug("Entering Real Mode\n");
	  info->cpu_mode = REAL;
	}
	  

	if (new_cr0->pg == 1) {
	  PrintDebug("Paging is already turned on in switch to protected mode in CR0 write\n");

	  // GPF the guest??
	  return -1;
	}

	if (info->shdw_pg_mode == SHADOW_PAGING) {
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

    } 
    break;
 
  case PROTECTED: 
    {

      int index = 0;
      int ret;

      PrintDebug("Protected %s Mode write to CR0 at guest %s linear rip 0x%x\n", 
		 info->mem_mode == VIRTUAL_MEM ? "Paged" : "",
		 info->mem_mode == VIRTUAL_MEM ? "virtual" : "",
		 get_addr_linear(info, info->rip, &(info->segments.cs)));

      // OK, now we will read the instruction
      // The only difference between PROTECTED and PROTECTED_PG is whether we read
      // from guest_pa or guest_va
      if (info->mem_mode == PHYSICAL_MEM) { 
	// The real rip address is actually a combination of the rip + CS base 
	ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
      } else { 
	ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
      }
	

      if (ret != 15) {
	// I think we should inject a GPF into the guest
	PrintDebug("Could not read instruction (ret=%d)\n", ret);
	return -1;
      }

      while (is_prefix_byte(instr[index])) {
	index++; 
      }

      struct cr0_32 * shadow_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);

      struct cr0_32 * real_cr0 = (struct cr0_32*)&(info->ctrl_regs.cr0);

      if ((instr[index] == cr_access_byte) && 
	  (instr[index + 1] == mov_to_cr_byte)) {

	// MOV to CR0
    
	addr_t first_operand;
	addr_t second_operand;
	struct cr0_32 *new_cr0;
	operand_type_t addr_type;

	index += 2;
 

	addr_type = decode_operands32(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);

	if (addr_type != REG_OPERAND) {
	  PrintDebug("Non-register operand in write to CR0\n");
	  return -1;
	}

	new_cr0 = (struct cr0_32 *)first_operand;



	if (info->shdw_pg_mode == SHADOW_PAGING) {
	  struct cr0_32 * shadow_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);

	  if (new_cr0->pg == 1){
	    // This should be new_cr0->pg && !(old_cr->pg), right?
	    // and then a case for turning paging off?

	    struct cr3_32 * shadow_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.shadow_cr3);

	    info->mem_mode = VIRTUAL_MEM;
	  
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

      } else if ((instr[index] == 0x0f) &&
		 (instr[index + 1] == 0x06)) { 
	// CLTS instruction
	PrintDebug("CLTS instruction - clearing TS flag of real and shadow CR0\n");
	shadow_cr0->ts = 0;
	real_cr0->ts = 0;
	
	index+=2;
	
	info->rip+=index;

      } else {
	PrintDebug("Unkown instruction: \n");
	SerialMemDump(instr,15);
	return -1;
      }
    }
    break;
    
  case PROTECTED_PAE:
    PrintDebug("Protected PAE Mode write to CR0 is UNIMPLEMENTED\n");
    return -1;

  case LONG:
    PrintDebug("Protected Long Mode write to CR0 is UNIMPLEMENTED\n");
    return -1;

  default: 
    {
      PrintDebug("Unknown Mode write to CR0 (info->cpu_mode=0x%x\n)",info->cpu_mode);
      return -1;
    }
    break;

  }

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

      if (ret != 15) {
	// I think we should inject a GPF into the guest
	PrintDebug("Could not read Protected %s mode instruction (ret=%d)\n", 
		   info->cpu_mode == VIRTUAL_MEM ? "Paged" : "", ret);
	return -1;
      }

      while (is_prefix_byte(instr[index])) {
	index++; 
      }


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
  if (info->cpu_mode == PROTECTED) {
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
      PrintDebug("Writing Guest CR3 Write (Physical Address)\n");
      ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    } else { 
      PrintDebug("Writing Guest CR3 Write (Virtual Address)\n");
      // The real rip address is actually a combination of the rip + CS base 
      ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    }

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

      if (info->shdw_pg_mode == SHADOW_PAGING) {
	addr_t shadow_pt;
	struct cr3_32 * shadow_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.shadow_cr3);
	struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);


	  if (CR3_TO_PDE32(*(uint_t*)shadow_cr3) != 0) {
	    PrintDebug("Shadow Page Table\n");
	    PrintDebugPageTables((pde32_t *)CR3_TO_PDE32(*(uint_t*)shadow_cr3));
	  }

	/* Delete the current Page Tables */
	delete_page_tables_pde32((pde32_t *)CR3_TO_PDE32(*(uint_t*)shadow_cr3));

	PrintDebug("Old Shadow CR3=%x; Old Guest CR3=%x\n", 
		   *(uint_t*)shadow_cr3, *(uint_t*)guest_cr3);


	*guest_cr3 = *new_cr3;



	// Something like this
	shadow_pt =  create_new_shadow_pt32(info);
	//shadow_pt = setup_shadow_pt32(info, CR3_TO_PDE32(*(addr_t *)new_cr3));

	/* Copy Various flags */
	*shadow_cr3 = *new_cr3;

	{
	  addr_t tmp_addr;
	  guest_pa_to_host_va(info, ((*(uint_t*)guest_cr3) & 0xfffff000), &tmp_addr);
	  PrintDebug("Guest PD\n");
	  PrintPD32((pde32_t *)tmp_addr);

	}
	

	
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
      SerialMemDump(instr,15);
      return -1;
    }
  } else {
    PrintDebug("Invalid operating Mode (0x%x)\n", info->cpu_mode);
    return -1;
  }

  return 0;
}




int handle_cr3_read(struct guest_info * info) {
  if (info->cpu_mode == PROTECTED ) {
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

      if (info->shdw_pg_mode == SHADOW_PAGING) {
	*virt_cr3 = *(struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);
      } else {
	*virt_cr3 = *real_cr3;
      }
      
      info->rip += index;
    } else {
      PrintDebug("Unknown Instruction\n");
      SerialMemDump(instr,15);
      return -1;
    }
  } else {
    PrintDebug("Invalid operating Mode (0x%x), control registers follow\n", info->cpu_mode);
    PrintCtrlRegs(info);
    return -1;
  }

  return 0;
}
