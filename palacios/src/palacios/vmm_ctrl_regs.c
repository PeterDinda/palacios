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


// First Attempt = 494 lines
// current = 106 lines
int v3_handle_cr0_write(struct guest_info * info) {
  uchar_t instr[15];
  int ret;
  struct x86_instr dec_instr;

  if (info->mem_mode == PHYSICAL_MEM) { 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  } else { 
    ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  }

  /* The IFetch will already have faulted in the necessary bytes for the full instruction
    if (ret != 15) {
    // I think we should inject a GPF into the guest
    PrintError("Could not read instruction (ret=%d)\n", ret);
    return -1;
    }
  */

  if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
    PrintError("Could not decode instruction\n");
    return -1;
  }


  if (v3_opcode_cmp(V3_OPCODE_LMSW, (const uchar_t *)(dec_instr.opcode)) == 0) {
    struct cr0_real *real_cr0  = (struct cr0_real*)&(info->ctrl_regs.cr0);
    struct cr0_real *new_cr0 = (struct cr0_real *)(dec_instr.src_operand.operand);	
    uchar_t new_cr0_val;

    PrintDebug("LMSW\n");

    new_cr0_val = (*(char*)(new_cr0)) & 0x0f;
    
    PrintDebug("OperandVal = %x\n", new_cr0_val);

    PrintDebug("Old CR0=%x\n", *(uint_t *)real_cr0);	
    *(uchar_t*)real_cr0 &= 0xf0;
    *(uchar_t*)real_cr0 |= new_cr0_val;
    PrintDebug("New CR0=%x\n", *(uint_t *)real_cr0);	
      

    if (info->shdw_pg_mode == SHADOW_PAGING) {
      struct cr0_real * shadow_cr0 = (struct cr0_real*)&(info->shdw_pg_state.guest_cr0);
      
      PrintDebug(" Old Shadow CR0=%x\n", *(uint_t *)shadow_cr0);	
      *(uchar_t*)shadow_cr0 &= 0xf0;
      *(uchar_t*)shadow_cr0 |= new_cr0_val;
      PrintDebug("New Shadow CR0=%x\n", *(uint_t *)shadow_cr0);	
    }
  } else if (v3_opcode_cmp(V3_OPCODE_MOV2CR, (const uchar_t *)(dec_instr.opcode)) == 0) {
    PrintDebug("MOV2CR0\n");

    if (info->cpu_mode == LONG) {
      // 64 bit registers
    } else {
      // 32 bit registers
	struct cr0_32 *real_cr0 = (struct cr0_32*)&(info->ctrl_regs.cr0);
	struct cr0_32 *new_cr0= (struct cr0_32 *)(dec_instr.src_operand.operand);

	PrintDebug("OperandVal = %x, length=%d\n", *(uint_t *)new_cr0, dec_instr.src_operand.size);


	PrintDebug("Old CR0=%x\n", *(uint_t *)real_cr0);
	*real_cr0 = *new_cr0;
	

 	if (info->shdw_pg_mode == SHADOW_PAGING) {
 	  struct cr0_32 * shadow_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
	  
 	  PrintDebug("Old Shadow CR0=%x\n", *(uint_t *)shadow_cr0);	
	  
 	  real_cr0->et = 1;
	  
 	  *shadow_cr0 = *new_cr0;
 	  shadow_cr0->et = 1;
	  
	  if (v3_get_mem_mode(info) == VIRTUAL_MEM) {
	    struct cr3_32 * shadow_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.shadow_cr3);
	    
	    info->ctrl_regs.cr3 = *(addr_t*)shadow_cr3;
	  } else  {
	    info->ctrl_regs.cr3 = *(addr_t*)&(info->direct_map_pt);
	    real_cr0->pg = 1;
	  }
	  
	  PrintDebug("New Shadow CR0=%x\n",*(uint_t *)shadow_cr0);
 	}
	PrintDebug("New CR0=%x\n", *(uint_t *)real_cr0);
    }

  } else if (v3_opcode_cmp(V3_OPCODE_CLTS, (const uchar_t *)(dec_instr.opcode)) == 0) {
    // CLTS
    struct cr0_32 *real_cr0 = (struct cr0_32*)&(info->ctrl_regs.cr0);
	
    real_cr0->ts = 0;

    if (info->shdw_pg_mode == SHADOW_PAGING) {
      struct cr0_32 * shadow_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
      shadow_cr0->ts = 0;
    }
  } else {
    PrintError("Unhandled opcode in handle_cr0_write\n");
    return -1;
  }

  info->rip += dec_instr.instr_length;

  return 0;
}


// First attempt = 253 lines
// current = 51 lines
int v3_handle_cr0_read(struct guest_info * info) {
  uchar_t instr[15];
  int ret;
  struct x86_instr dec_instr;

  if (info->mem_mode == PHYSICAL_MEM) { 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  } else { 
    ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  }

  /* The IFetch will already have faulted in the necessary bytes for the full instruction
     if (ret != 15) {
     // I think we should inject a GPF into the guest
     PrintError("Could not read instruction (ret=%d)\n", ret);
     return -1;
     }
  */

  if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
    PrintError("Could not decode instruction\n");
    return -1;
  }
  
  if (v3_opcode_cmp(V3_OPCODE_MOVCR2, (const uchar_t *)(dec_instr.opcode)) == 0) {
    struct cr0_32 * virt_cr0 = (struct cr0_32 *)(dec_instr.dst_operand.operand);
    struct cr0_32 * real_cr0 = (struct cr0_32 *)&(info->ctrl_regs.cr0);
    
    PrintDebug("MOVCR2\n");
    PrintDebug("CR0 at 0x%p\n", (void *)real_cr0);

    if (info->shdw_pg_mode == SHADOW_PAGING) {
      *virt_cr0 = *(struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
    } else {
      *virt_cr0 = *real_cr0;
    }
    
    PrintDebug("real CR0: %x\n", *(uint_t*)real_cr0);
    PrintDebug("returned CR0: %x\n", *(uint_t*)virt_cr0);
  } else if (v3_opcode_cmp(V3_OPCODE_SMSW, (const uchar_t *)(dec_instr.opcode)) == 0) {
    struct cr0_real *real_cr0= (struct cr0_real*)&(info->ctrl_regs.cr0);
    struct cr0_real *virt_cr0 = (struct cr0_real *)(dec_instr.dst_operand.operand);
    char cr0_val = *(char*)real_cr0 & 0x0f;
    
    PrintDebug("SMSW\n");

    PrintDebug("CR0 at 0x%p\n", real_cr0);

    *(char *)virt_cr0 &= 0xf0;
    *(char *)virt_cr0 |= cr0_val;
    
  } else {
    PrintError("Unhandled opcode in handle_cr0_read\n");
    return -1;
  }

  info->rip += dec_instr.instr_length;

  return 0;
}



// First Attempt = 256 lines
// current = 65 lines
int v3_handle_cr3_write(struct guest_info * info) {
  int ret;
  uchar_t instr[15];
  struct x86_instr dec_instr;

  if (info->mem_mode == PHYSICAL_MEM) { 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  } else { 
    ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  }

  /* The IFetch will already have faulted in the necessary bytes for the full instruction
     if (ret != 15) {
     // I think we should inject a GPF into the guest
     PrintError("Could not read instruction (ret=%d)\n", ret);
     return -1;
     }
  */

  if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
    PrintError("Could not decode instruction\n");
    return -1;
  }

  if (v3_opcode_cmp(V3_OPCODE_MOV2CR, (const uchar_t *)(dec_instr.opcode)) == 0) {

    PrintDebug("MOV2CR3\n");

    PrintDebug("CR3 at 0x%p\n", &(info->ctrl_regs.cr3));

    if (info->shdw_pg_mode == SHADOW_PAGING) {
      struct cr3_32 * new_cr3 = (struct cr3_32 *)(dec_instr.src_operand.operand);	
      struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);
      struct cr3_32 * shadow_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.shadow_cr3);
      int cached = 0;
      

      PrintDebug("Old Shadow CR3=%x; Old Guest CR3=%x\n", 
		 *(uint_t*)shadow_cr3, *(uint_t*)guest_cr3);
      

      cached = v3_cache_page_tables32(info, (addr_t)V3_PAddr((void *)(addr_t)CR3_TO_PDE32((void *)*(addr_t *)new_cr3)));

      if (cached == -1) {
	PrintError("CR3 Cache failed\n");
	return -1;
      } else if (cached == 0) {
	addr_t shadow_pt;
	
	PrintDebug("New CR3 is different - flushing shadow page table\n");	
	
	delete_page_tables_pde32((pde32_t *)CR3_TO_PDE32(*(uint_t*)shadow_cr3));
	
	shadow_pt =  v3_create_new_shadow_pt32();
	
	shadow_cr3->pdt_base_addr = (addr_t)V3_PAddr((void *)(addr_t)PD32_BASE_ADDR(shadow_pt));
      } else {
	PrintDebug("Reusing cached shadow Page table\n");
      }
      
      shadow_cr3->pwt = new_cr3->pwt;
      shadow_cr3->pcd = new_cr3->pcd;
      
      // What the hell...
      *guest_cr3 = *new_cr3;
      
      PrintDebug("New Shadow CR3=%x; New Guest CR3=%x\n", 
		 *(uint_t*)shadow_cr3, *(uint_t*)guest_cr3);

      if (info->mem_mode == VIRTUAL_MEM) {
	// If we aren't in paged mode then we have to preserve the identity mapped CR3
	info->ctrl_regs.cr3 = *(addr_t*)shadow_cr3;
      }
    }
  } else {
    PrintError("Unhandled opcode in handle_cr3_write\n");
    return -1;
  }

  info->rip += dec_instr.instr_length;

  return 0;
}



// first attempt = 156 lines
// current = 36 lines
int v3_handle_cr3_read(struct guest_info * info) {
  uchar_t instr[15];
  int ret;
  struct x86_instr dec_instr;

  if (info->mem_mode == PHYSICAL_MEM) { 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  } else { 
    ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  }

  /* The IFetch will already have faulted in the necessary bytes for the full instruction
     if (ret != 15) {
     // I think we should inject a GPF into the guest
     PrintError("Could not read instruction (ret=%d)\n", ret);
     return -1;
     }
  */

  if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
    PrintError("Could not decode instruction\n");
    return -1;
  }

  if (v3_opcode_cmp(V3_OPCODE_MOVCR2, (const uchar_t *)(dec_instr.opcode)) == 0) {
    PrintDebug("MOVCR32\n");
    struct cr3_32 * virt_cr3 = (struct cr3_32 *)(dec_instr.dst_operand.operand);

    PrintDebug("CR3 at 0x%p\n", &(info->ctrl_regs.cr3));

    if (info->shdw_pg_mode == SHADOW_PAGING) {
      *virt_cr3 = *(struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);
    } else {
      *virt_cr3 = *(struct cr3_32 *)&(info->ctrl_regs.cr3);
    }
  } else {
    PrintError("Unhandled opcode in handle_cr3_read\n");
    return -1;
  }

  info->rip += dec_instr.instr_length;

  return 0;
}
