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

#include <palacios/vmm.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm_instr_emulator.h>

#ifndef DEBUG_EMULATOR
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif






// We emulate up to the next 4KB page boundry
static int emulate_string_write_op(struct guest_info * info, struct x86_instr * dec_instr, 
				   addr_t write_gva, addr_t write_gpa, addr_t * dst_addr) {
  uint_t emulation_length = 0;
  addr_t tmp_rcx = 0;
  addr_t src_addr = 0;

  if (dec_instr->op_type == V3_OP_MOVS) {
    PrintDebug("MOVS emulation\n");

    if (dec_instr->dst_operand.operand != write_gva) {
      PrintError("Inconsistency between Pagefault and Instruction Decode XED_ADDR=%p, PF_ADDR=%p\n",
		 (void *)dec_instr->dst_operand.operand, (void *)write_gva);
      return -1;
    }

    emulation_length = ( (dec_instr->str_op_length  < (0x1000 - PAGE_OFFSET_4KB(write_gva))) ? 
			 dec_instr->str_op_length :
			 (0x1000 - PAGE_OFFSET_4KB(write_gva)));
    /* ** Fix emulation length so that it doesn't overrun over the src page either ** */


    PrintDebug("STR_OP_LEN: %d, Page Len: %d\n", 
	       (uint_t)dec_instr->str_op_length, 
	       (uint_t)(0x1000 - PAGE_OFFSET_4KB(write_gva)));
    PrintDebug("Emulation length: %d\n", emulation_length);
    tmp_rcx = emulation_length;
 
    if (guest_pa_to_host_va(info, write_gpa, dst_addr) == -1) {
      PrintError("Could not translate write destination to host VA\n");
      return -1;
    }

    // figure out addresses here....
    if (info->mem_mode == PHYSICAL_MEM) {
      if (guest_pa_to_host_va(info, dec_instr->src_operand.operand, &src_addr) == -1) {
	PrintError("Could not translate write Source (Physical) to host VA\n");
	return -1;
      }
    } else {
      if (guest_va_to_host_va(info, dec_instr->src_operand.operand, &src_addr) == -1) {
	PrintError("Could not translate write Source (Virtual) to host VA\n");
	return -1;
      }
    }


    PrintDebug("Dst Operand: %p (size=%d), Src Operand: %p\n", 
	       (void *)dec_instr->dst_operand.operand, 
	       dec_instr->dst_operand.size,
	       (void *)dec_instr->src_operand.operand);
    PrintDebug("Dst Addr: %p, Src Addr: %p\n", (void *)(addr_t *)*dst_addr, (void *)src_addr);

    //return -1;


 


    if (dec_instr->dst_operand.size == 1) {
      movs8(dst_addr, &src_addr, &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
    } else if (dec_instr->dst_operand.size == 2) {
      movs16(dst_addr, &src_addr, &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
    } else if (dec_instr->dst_operand.size == 4) {
      movs32(dst_addr, &src_addr, &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
    } else {
      PrintError("Invalid operand length\n");
      return -1;
    }





    PrintDebug("RDI=%p, RSI=%p, RCX=%p\n", 
	       (void *)*(addr_t *)&(info->vm_regs.rdi), 
	       (void *)*(addr_t *)&(info->vm_regs.rsi), 
	       (void *)*(addr_t *)&(info->vm_regs.rcx)); 

    info->vm_regs.rdi += emulation_length;
    info->vm_regs.rsi += emulation_length;
    info->vm_regs.rcx -= emulation_length;
    
    PrintDebug("RDI=%p, RSI=%p, RCX=%p\n", 
	       (void *)*(addr_t *)&(info->vm_regs.rdi), 
	       (void *)*(addr_t *)&(info->vm_regs.rsi), 
	       (void *)*(addr_t *)&(info->vm_regs.rcx)); 

    if (emulation_length == dec_instr->str_op_length) {
      info->rip += dec_instr->instr_length;
    }

    return emulation_length;
  }

  


  return -1;
}


int v3_emulate_write_op(struct guest_info * info, addr_t write_gva, addr_t write_gpa, addr_t * dst_addr) {
  struct x86_instr dec_instr;
  uchar_t instr[15];
  int ret = 0;
  addr_t src_addr = 0;


  PrintDebug("Emulating Write for instruction at %p\n", (void *)(addr_t)(info->rip));
  PrintDebug("GPA=%p, GVA=%p\n", (void *)write_gpa, (void *)write_gva);

  if (info->mem_mode == PHYSICAL_MEM) { 
    ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  } else { 
    ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  }

  if (ret == -1) {
    return -1;
  }


  if (guest_pa_to_host_va(info, write_gpa, dst_addr) == -1) {
    PrintError("Could not translate write destination to host VA\n");
    return -1;
  }

  if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
    PrintError("Decoding Error\n");
    // Kick off single step emulator
    return -1;
  }
  
  if (dec_instr.is_str_op) {
    return emulate_string_write_op(info, &dec_instr, write_gva, write_gpa, dst_addr);
  }


  if ((dec_instr.dst_operand.type != MEM_OPERAND) ||
      (dec_instr.dst_operand.operand != write_gva)) {
    PrintError("Inconsistency between Pagefault and Instruction Decode XED_ADDR=%p, PF_ADDR=%p\n",
	       (void *)dec_instr.dst_operand.operand, (void *)write_gva);
    return -1;
  }


  if (dec_instr.src_operand.type == MEM_OPERAND) {
    if (info->mem_mode == PHYSICAL_MEM) {
      if (guest_pa_to_host_va(info, dec_instr.src_operand.operand, &src_addr) == -1) {
	PrintError("Could not translate write Source (Physical) to host VA\n");
	return -1;
      }
    } else {
      if (guest_va_to_host_va(info, dec_instr.src_operand.operand, &src_addr) == -1) {
	PrintError("Could not translate write Source (Virtual) to host VA\n");
	return -1;
      }
    }
  } else if (dec_instr.src_operand.type == REG_OPERAND) {
    src_addr = dec_instr.src_operand.operand;
  } else {
    src_addr = (addr_t)&(dec_instr.src_operand.operand);
  }


  PrintDebug("Dst_Addr Ptr = %p (val=%p), SRC operand = %p\n", 
	     (void *)dst_addr, (void *)*dst_addr, (void *)src_addr);


  if (dec_instr.dst_operand.size == 1) {

    switch (dec_instr.op_type) {
    case V3_OP_ADC:
      adc8((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_ADD:
      add8((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_AND:
      and8((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_OR:
      or8((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_XOR:
      xor8((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SUB:
      sub8((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;

    case V3_OP_MOV:
      mov8((addr_t *)*dst_addr, (addr_t *)src_addr);
      break;
    case V3_OP_NOT:
      not8((addr_t *)*dst_addr);
      break;
    case V3_OP_XCHG:
      xchg8((addr_t *)*dst_addr, (addr_t *)src_addr);
      break;
      

    case V3_OP_INC:
      inc8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_DEC:
      dec8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_NEG:
      neg8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETB:
      setb8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETBE:
      setbe8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETL:
      setl8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETLE:
      setle8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETNB:
      setnb8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETNBE:
      setnbe8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETNL:
      setnl8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETNLE:
      setnle8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETNO:
      setno8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETNP:
      setnp8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETNS:
      setns8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETNZ:
      setnz8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETO:
      seto8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETP:
      setp8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETS:
      sets8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SETZ:
      setz8((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;

    default:
      PrintError("Unknown 8 bit instruction\n");
      return -1;
    }

  } else if (dec_instr.dst_operand.size == 2) {

    switch (dec_instr.op_type) {
    case V3_OP_ADC:
      adc16((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_ADD:
      add16((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_AND:
      and16((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_OR:
      or16((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_XOR:
      xor16((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SUB:
      sub16((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;


    case V3_OP_INC:
      inc16((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_DEC:
      dec16((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_NEG:
      neg16((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;

    case V3_OP_MOV:
      mov16((addr_t *)*dst_addr, (addr_t *)src_addr);
      break;
    case V3_OP_NOT:
      not16((addr_t *)*dst_addr);
      break;
    case V3_OP_XCHG:
      xchg16((addr_t *)*dst_addr, (addr_t *)src_addr);
      break;
      
    default:
      PrintError("Unknown 16 bit instruction\n");
      return -1;
    }

  } else if (dec_instr.dst_operand.size == 4) {

    switch (dec_instr.op_type) {
    case V3_OP_ADC:
      adc32((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_ADD:
      add32((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_AND:
      and32((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_OR:
      or32((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_XOR:
      xor32((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_SUB:
      sub32((addr_t *)*dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;

    case V3_OP_INC:
      inc32((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_DEC:
      dec32((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;
    case V3_OP_NEG:
      neg32((addr_t *)*dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
      break;

    case V3_OP_MOV:
      mov32((addr_t *)*dst_addr, (addr_t *)src_addr);
      break;
    case V3_OP_NOT:
      not32((addr_t *)*dst_addr);
      break;
    case V3_OP_XCHG:
      xchg32((addr_t *)*dst_addr, (addr_t *)src_addr);
      break;
      
    default:
      PrintError("Unknown 32 bit instruction\n");
      return -1;
    }

  } else if (dec_instr.dst_operand.size == 8) {
    PrintError("64 bit instructions not handled\n");
    return -1;
  } else {
    PrintError("Invalid Operation Size\n");
    return -1;
  }

  info->rip += dec_instr.instr_length;

  return dec_instr.dst_operand.size;
}

