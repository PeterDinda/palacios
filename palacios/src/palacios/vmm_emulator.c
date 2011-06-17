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
 * Authors: Jack Lange <jarusl@cs.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu> (full hook/string ops)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_instr_emulator.h>
#include <palacios/vmm_ctrl_regs.h>

#ifndef V3_CONFIG_DEBUG_EMULATOR
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static int run_op(struct guest_info * info, v3_op_type_t op_type, 
		  addr_t src_addr, addr_t dst_addr, 
		  int src_op_size, int dst_op_size) {

    if (src_op_size == 1) {
	PrintDebug("Executing 8 bit instruction\n");

	switch (op_type) {
	    case V3_OP_ADC:
		adc8((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_ADD:
		add8((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_AND:
		and8((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_OR:
		or8((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_XOR:
		xor8((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SUB:
		sub8((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;

	    case V3_OP_MOV:
		mov8((addr_t *)dst_addr, (addr_t *)src_addr);
		break;

	    case V3_OP_MOVZX:
		movzx8((addr_t *)dst_addr, (addr_t *)src_addr, dst_op_size);
		break;
	    case V3_OP_MOVSX:
		movsx8((addr_t *)dst_addr, (addr_t *)src_addr, dst_op_size);
		break;

	    case V3_OP_NOT:
		not8((addr_t *)dst_addr);
		break;
	    case V3_OP_XCHG:
		xchg8((addr_t *)dst_addr, (addr_t *)src_addr);
		break;
      

	    case V3_OP_INC:
		inc8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_DEC:
		dec8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_NEG:
		neg8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETB:
		setb8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETBE:
		setbe8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETL:
		setl8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETLE:
		setle8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETNB:
		setnb8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETNBE:
		setnbe8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETNL:
		setnl8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETNLE:
		setnle8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETNO:
		setno8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETNP:
		setnp8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETNS:
		setns8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETNZ:
		setnz8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETO:
		seto8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETP:
		setp8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETS:
		sets8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SETZ:
		setz8((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;

	    default:
		PrintError("Unknown 8 bit instruction\n");
		return -1;
	}

    } else if (src_op_size == 2) {
	PrintDebug("Executing 16 bit instruction\n");

	switch (op_type) {
	    case V3_OP_ADC:
		adc16((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_ADD:
		add16((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_AND:
		and16((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_OR:
		or16((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_XOR:
		xor16((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SUB:
		sub16((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;


	    case V3_OP_INC:
		inc16((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_DEC:
		dec16((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_NEG:
		neg16((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;

	    case V3_OP_MOV:
		mov16((addr_t *)dst_addr, (addr_t *)src_addr);
		break;
	    case V3_OP_MOVZX:
		movzx16((addr_t *)dst_addr, (addr_t *)src_addr, dst_op_size);
		break;
	    case V3_OP_MOVSX:
		movsx16((addr_t *)dst_addr, (addr_t *)src_addr, dst_op_size);
		break;
	    case V3_OP_NOT:
		not16((addr_t *)dst_addr);
		break;
	    case V3_OP_XCHG:
		xchg16((addr_t *)dst_addr, (addr_t *)src_addr);
		break;
      
	    default:
		PrintError("Unknown 16 bit instruction\n");
		return -1;
	}

    } else if (src_op_size == 4) {
	PrintDebug("Executing 32 bit instruction\n");

	switch (op_type) {
	    case V3_OP_ADC:
		adc32((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_ADD:
		add32((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_AND:
		and32((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_OR:
		or32((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_XOR:
		xor32((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SUB:
		sub32((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;

	    case V3_OP_INC:
		inc32((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_DEC:
		dec32((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_NEG:
		neg32((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;

	    case V3_OP_MOV:
		mov32((addr_t *)dst_addr, (addr_t *)src_addr);
		break;

	    case V3_OP_NOT:
		not32((addr_t *)dst_addr);
		break;
	    case V3_OP_XCHG:
		xchg32((addr_t *)dst_addr, (addr_t *)src_addr);
		break;
      
	    default:
		PrintError("Unknown 32 bit instruction\n");
		return -1;
	}

#ifdef __V3_64BIT__
    } else if (src_op_size == 8) {
	PrintDebug("Executing 64 bit instruction\n");

	switch (op_type) {
	    case V3_OP_ADC:
		adc64((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_ADD:
		add64((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_AND:
		and64((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_OR:
		or64((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_XOR:
		xor64((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_SUB:
		sub64((addr_t *)dst_addr, (addr_t *)src_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;

	    case V3_OP_INC:
		inc64((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_DEC:
		dec64((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;
	    case V3_OP_NEG:
		neg64((addr_t *)dst_addr, (addr_t *)&(info->ctrl_regs.rflags));
		break;

	    case V3_OP_MOV:
		mov64((addr_t *)dst_addr, (addr_t *)src_addr);
		break;

	    case V3_OP_NOT:
		not64((addr_t *)dst_addr);
		break;
	    case V3_OP_XCHG:
		xchg64((addr_t *)dst_addr, (addr_t *)src_addr);
		break;
      
	    default:
		PrintError("Unknown 64 bit instruction\n");
		return -1;
	}
#endif

    } else {
	PrintError("Invalid Operation Size\n");
	return -1;
    }

    return 0;
}



/* Returns the number of bytes written, or -1 if there is an error */
static int run_str_op(struct guest_info * core, struct x86_instr * instr, 
		      addr_t src_addr, addr_t dst_addr, 
		      int op_size, int rep_cnt) {

    addr_t tmp_rcx = rep_cnt;
    int emulation_length = op_size * rep_cnt;
    struct rflags * flags_reg = (struct rflags *)&(core->ctrl_regs.rflags);


    PrintDebug("Emulation_len=%d, tmp_rcx=%d\n", emulation_length, (uint_t)tmp_rcx);


    if (instr->op_type == V3_OP_MOVS) {
	if (op_size== 1) {
	    movs8((addr_t *)&dst_addr, &src_addr, &tmp_rcx, (addr_t *)&(core->ctrl_regs.rflags));
	} else if (op_size == 2) {
	    movs16((addr_t *)&dst_addr, &src_addr, &tmp_rcx, (addr_t *)&(core->ctrl_regs.rflags));
	} else if (op_size == 4) {
	    movs32((addr_t *)&dst_addr, &src_addr, &tmp_rcx, (addr_t *)&(core->ctrl_regs.rflags));
#ifdef __V3_64BIT__
	} else if (op_size == 8) {
	    movs64((addr_t *)&dst_addr, &src_addr, &tmp_rcx, (addr_t *)&(core->ctrl_regs.rflags));
#endif
	} else {
	    PrintError("Invalid operand length\n");
	    return -1;
	}

	if (flags_reg->df == 0) {
	    core->vm_regs.rdi += emulation_length;
	    core->vm_regs.rsi += emulation_length;
	} else {
	    core->vm_regs.rdi -= emulation_length;
	    core->vm_regs.rsi -= emulation_length;
	}

	// RCX is only modified if the rep prefix is present
	if (instr->prefixes.rep == 1) {
	    core->vm_regs.rcx -= rep_cnt;
	}

     } else if (instr->op_type == V3_OP_STOS) {
	if (op_size == 1) {
	    stos8((addr_t *)&dst_addr, (addr_t  *)&(core->vm_regs.rax), &tmp_rcx, (addr_t *)&(core->ctrl_regs.rflags));
	} else if (op_size == 2) {
	    stos16((addr_t *)&dst_addr, (addr_t  *)&(core->vm_regs.rax), &tmp_rcx, (addr_t *)&(core->ctrl_regs.rflags));
	} else if (op_size == 4) {
	    stos32((addr_t *)&dst_addr, (addr_t  *)&(core->vm_regs.rax), &tmp_rcx, (addr_t *)&(core->ctrl_regs.rflags));
#ifdef __V3_64BIT__
	} else if (op_size == 8) {
	    stos64((addr_t *)&dst_addr, (addr_t  *)&(core->vm_regs.rax), &tmp_rcx, (addr_t *)&(core->ctrl_regs.rflags));
#endif
	} else {
	    PrintError("Invalid operand length\n");
	    return -1;
	}



	if (flags_reg->df == 0) {
	    core->vm_regs.rdi += emulation_length;
	} else {
	    core->vm_regs.rdi -= emulation_length;
	}

	// RCX is only modified if the rep prefix is present
	if (instr->prefixes.rep == 1) {
	    core->vm_regs.rcx -= rep_cnt;
	}
    } else {
	PrintError("Unimplemented String operation\n");
	return -1;
    }
    
    return emulation_length;
}



int v3_emulate(struct guest_info * core, struct x86_instr * instr, 
	       int mem_op_size, addr_t mem_hva_src, addr_t mem_hva_dst) {

    addr_t src_hva = 0;
    addr_t dst_hva = 0;


    if (instr->src_operand.type == MEM_OPERAND) {
	src_hva = mem_hva_src;
    } else if (instr->src_operand.type == REG_OPERAND) {
	src_hva = instr->src_operand.operand;
    } else {
	src_hva = (addr_t)&(instr->src_operand.operand);
    }

    if (instr->dst_operand.type == MEM_OPERAND) {
	dst_hva = mem_hva_dst;
    } else if (instr->dst_operand.type == REG_OPERAND) {
	dst_hva = instr->dst_operand.operand;
    } else {
	dst_hva = (addr_t)&(instr->dst_operand.operand);
    }
 

    if (instr->is_str_op == 0) {
	int src_op_len = instr->src_operand.size;
	int dst_op_len = instr->dst_operand.size;
	
	run_op(core, instr->op_type, src_hva, dst_hva, src_op_len, dst_op_len);

	return dst_op_len;
    } else {
	// String Operation
	int rep_cnt = 0;

	/* Both src and dst operand sizes should be identical */
	rep_cnt = mem_op_size / instr->src_operand.size;

	return run_str_op(core, instr, src_hva, dst_hva, instr->src_operand.size, rep_cnt);
    }


    return -1;
}
