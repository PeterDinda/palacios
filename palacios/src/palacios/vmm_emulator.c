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

#ifndef CONFIG_DEBUG_EMULATOR
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static int run_op(struct guest_info * info, v3_op_type_t op_type, addr_t src_addr, addr_t dst_addr, int src_op_size, int dst_op_size);

// We emulate up to the next 4KB page boundry
static int emulate_string_write_op(struct guest_info * info, struct x86_instr * dec_instr, 
				   addr_t write_gva, addr_t write_gpa, addr_t dst_addr, 
				   int (*write_fn)(addr_t guest_addr, void * src, uint_t length, void * priv_data), 
				   void * priv_data) {
    uint_t emulation_length = 0;
    uint_t emulation_iter_cnt = 0;
    addr_t tmp_rcx = 0;
    addr_t src_addr = 0;

    if (dec_instr->dst_operand.operand != write_gva) {
	PrintError("Inconsistency between Pagefault and Instruction Decode XED_ADDR=%p, PF_ADDR=%p\n",
		   (void *)dec_instr->dst_operand.operand, (void *)write_gva);
	return -1;
    }
  
    /*emulation_length = ( (dec_instr->str_op_length  < (0x1000 - PAGE_OFFSET_4KB(write_gva))) ? 
			 dec_instr->str_op_length :
			 (0x1000 - PAGE_OFFSET_4KB(write_gva)));*/
     emulation_length = ( (dec_instr->str_op_length * (dec_instr->dst_operand.size)  < (0x1000 - PAGE_OFFSET_4KB(write_gva))) ? 
			 dec_instr->str_op_length * dec_instr->dst_operand.size :
			 (0x1000 - PAGE_OFFSET_4KB(write_gva)));
  
    /* ** Fix emulation length so that it doesn't overrun over the src page either ** */
    emulation_iter_cnt = emulation_length / dec_instr->dst_operand.size;
    tmp_rcx = emulation_iter_cnt;
  
    if (dec_instr->op_type == V3_OP_MOVS) {

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

	if (dec_instr->dst_operand.size == 1) {
	    movs8((addr_t *)&dst_addr, &src_addr, &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
	} else if (dec_instr->dst_operand.size == 2) {
	    movs16((addr_t *)&dst_addr, &src_addr, &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
	} else if (dec_instr->dst_operand.size == 4) {
	    movs32((addr_t *)&dst_addr, &src_addr, &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
#ifdef __V3_64BIT__
	} else if (dec_instr->dst_operand.size == 8) {
	    movs64((addr_t *)&dst_addr, &src_addr, &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
#endif
	} else {
	    PrintError("Invalid operand length\n");
	    return -1;
	}

	info->vm_regs.rdi += emulation_length;
	info->vm_regs.rsi += emulation_length;

	// RCX is only modified if the rep prefix is present
	if (dec_instr->prefixes.rep == 1) {
	    info->vm_regs.rcx -= emulation_iter_cnt;
	}

    } else if (dec_instr->op_type == V3_OP_STOS) {

	if (dec_instr->dst_operand.size == 1) {
	    stos8((addr_t *)&dst_addr, (addr_t  *)&(info->vm_regs.rax), &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
	} else if (dec_instr->dst_operand.size == 2) {
	    stos16((addr_t *)&dst_addr, (addr_t  *)&(info->vm_regs.rax), &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
	} else if (dec_instr->dst_operand.size == 4) {
	    stos32((addr_t *)&dst_addr, (addr_t  *)&(info->vm_regs.rax), &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
#ifdef __V3_64BIT__
	} else if (dec_instr->dst_operand.size == 8) {
	    stos64((addr_t *)&dst_addr, (addr_t  *)&(info->vm_regs.rax), &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
#endif
	} else {
	    PrintError("Invalid operand length\n");
	    return -1;
	}

	info->vm_regs.rdi += emulation_length;
    
	// RCX is only modified if the rep prefix is present
	if (dec_instr->prefixes.rep == 1) {
	    info->vm_regs.rcx -= emulation_iter_cnt;
	}

    } else {
	PrintError("Unimplemented String operation\n");
	return -1;
    }

    if (write_fn(write_gpa, (void *)dst_addr, emulation_length, priv_data) != emulation_length) {
	PrintError("Did not fully read hooked data\n");
	return -1;
    }

    if (emulation_length == dec_instr->str_op_length) {
	info->rip += dec_instr->instr_length;
    }

    return emulation_length;
}


static int emulate_xchg_write_op(struct guest_info * info, struct x86_instr * dec_instr, 
				 addr_t write_gva, addr_t write_gpa, addr_t dst_addr, 
				 int (*write_fn)(addr_t guest_addr, void * src, uint_t length, void * priv_data), 
				 void * priv_data) {
    addr_t src_addr = 0;
    addr_t em_dst_addr = 0;
    int src_op_len = 0;
    int dst_op_len = 0;  
    PrintDebug("Emulating XCHG write\n");

    if (dec_instr->src_operand.type == MEM_OPERAND) {
	if (info->shdw_pg_mode == SHADOW_PAGING) {
	    if (dec_instr->src_operand.operand != write_gva) {
		PrintError("XCHG: Inconsistency between Pagefault and Instruction Decode XED_ADDR=%p, PF_ADDR=%p\n",
			   (void *)dec_instr->src_operand.operand, (void *)write_gva);
		return -1;
	    }
	}

	src_addr = dst_addr;
    } else if (dec_instr->src_operand.type == REG_OPERAND) {
	src_addr = dec_instr->src_operand.operand;
    } else {
	src_addr = (addr_t)&(dec_instr->src_operand.operand);
    }



    if (dec_instr->dst_operand.type == MEM_OPERAND) {
        if (info->shdw_pg_mode == SHADOW_PAGING) {
	    if (dec_instr->dst_operand.operand != write_gva) {
		PrintError("XCHG: Inconsistency between Pagefault and Instruction Decode XED_ADDR=%p, PF_ADDR=%p\n",
			   (void *)dec_instr->dst_operand.operand, (void *)write_gva);
		return -1;
	    }
	} else {
	    //check that the operand (GVA) maps to the the faulting GPA
	}
	
	em_dst_addr = dst_addr;
    } else if (dec_instr->src_operand.type == REG_OPERAND) {
	em_dst_addr = dec_instr->src_operand.operand;
    } else {
	em_dst_addr = (addr_t)&(dec_instr->src_operand.operand);
    }
    
    dst_op_len = dec_instr->dst_operand.size;
    src_op_len = dec_instr->src_operand.size;
    
    PrintDebug("Dst_Addr = %p, SRC operand = %p\n", 
	       (void *)dst_addr, (void *)src_addr);
    
    
    if (run_op(info, dec_instr->op_type, src_addr, em_dst_addr, src_op_len, dst_op_len) == -1) {
	PrintError("Instruction Emulation Failed\n");
	return -1;
    }
    
    if (write_fn(write_gpa, (void *)dst_addr, dst_op_len, priv_data) != dst_op_len) {
	PrintError("Did not fully write hooked data\n");
	return -1;
    }
    
    info->rip += dec_instr->instr_length;
    
    return dst_op_len;
}
    


static int emulate_xchg_read_op(struct guest_info * info, struct x86_instr * dec_instr, 
				addr_t read_gva, addr_t read_gpa, addr_t src_addr, 
				int (*read_fn)(addr_t guest_addr, void * dst, uint_t length, void * priv_data), 
				int (*write_fn)(addr_t guest_addr, void * src, uint_t length, void * priv_data), 			
				void * priv_data) {
    addr_t em_src_addr = 0;
    addr_t em_dst_addr = 0;
    int src_op_len = 0;
    int dst_op_len = 0;

    PrintDebug("Emulating XCHG Read\n");

    if (dec_instr->src_operand.type == MEM_OPERAND) {
	if (dec_instr->src_operand.operand != read_gva) {
	    PrintError("XCHG: Inconsistency between Pagefault and Instruction Decode XED_ADDR=%p, PF_ADDR=%p\n",
		       (void *)dec_instr->src_operand.operand, (void *)read_gva);
	    return -1;
	}
    
	em_src_addr = src_addr;
    } else if (dec_instr->src_operand.type == REG_OPERAND) {
	em_src_addr = dec_instr->src_operand.operand;
    } else {
	em_src_addr = (addr_t)&(dec_instr->src_operand.operand);
    }



    if (dec_instr->dst_operand.type == MEM_OPERAND) {
	if (info->shdw_pg_mode == SHADOW_PAGING) {
	    if (dec_instr->dst_operand.operand != read_gva) {
		PrintError("XCHG: Inconsistency between Pagefault and Instruction Decode XED_ADDR=%p, PF_ADDR=%p\n",
			   (void *)dec_instr->dst_operand.operand, (void *)read_gva);
		return -1;
	    }
        } else {
	    //check that the operand (GVA) maps to the the faulting GPA
	}

	em_dst_addr = src_addr;
    } else if (dec_instr->src_operand.type == REG_OPERAND) {
	em_dst_addr = dec_instr->src_operand.operand;
    } else {
	em_dst_addr = (addr_t)&(dec_instr->src_operand.operand);
    }

    dst_op_len = dec_instr->dst_operand.size;
    src_op_len = dec_instr->src_operand.size;

    PrintDebug("Dst_Addr = %p, SRC operand = %p\n", 
	       (void *)em_dst_addr, (void *)em_src_addr);


    if (read_fn(read_gpa, (void *)src_addr, src_op_len, priv_data) != src_op_len) {
	PrintError("Did not fully read hooked data\n");
	return -1;
    }

    if (run_op(info, dec_instr->op_type, em_src_addr, em_dst_addr, src_op_len, dst_op_len) == -1) {
	PrintError("Instruction Emulation Failed\n");
	return -1;
    }

    if (write_fn(read_gpa, (void *)src_addr, dst_op_len, priv_data) != dst_op_len) {
	PrintError("Did not fully write hooked data\n");
	return -1;
    }

    info->rip += dec_instr->instr_length;

    return dst_op_len;
}




int v3_emulate_write_op(struct guest_info * info, addr_t write_gva, addr_t write_gpa,  addr_t dst_addr, 
			int (*write_fn)(addr_t guest_addr, void * src, uint_t length, void * priv_data), 
			void * priv_data) {
    struct x86_instr dec_instr;
    uchar_t instr[15];
    int ret = 0;
    addr_t src_addr = 0;
    int src_op_len = 0;
    int dst_op_len = 0;

    PrintDebug("Emulating Write for instruction at %p\n", (void *)(addr_t)(info->rip));
    PrintDebug("GVA=%p Dst_Addr=%p\n", (void *)write_gva, (void *)dst_addr);

    if (info->mem_mode == PHYSICAL_MEM) { 
	ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    } else { 
	ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    }

    if (ret == -1) {
	return -1;
    }

    if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
	PrintError("Decoding Error\n");
	// Kick off single step emulator
	return -1;
    }
  
    /* 
     * Instructions needing to be special cased.... *
     */
    if (dec_instr.is_str_op) {
	return emulate_string_write_op(info, &dec_instr, write_gva, write_gpa, dst_addr, write_fn, priv_data);
    } else if (dec_instr.op_type == V3_OP_XCHG) {
	return emulate_xchg_write_op(info, &dec_instr, write_gva, write_gpa, dst_addr, write_fn, priv_data);
    }


    if (info->shdw_pg_mode == SHADOW_PAGING) {
	if ((dec_instr.dst_operand.type != MEM_OPERAND) ||
	    (dec_instr.dst_operand.operand != write_gva)) {
	    PrintError("Inconsistency between Pagefault and Instruction Decode XED_ADDR=%p, PF_ADDR=%p\n",
		       (void *)dec_instr.dst_operand.operand, (void *)write_gva);
	    return -1;
	}
    } else {
	//check that the operand (GVA) maps to the the faulting GPA
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

    dst_op_len = dec_instr.dst_operand.size;
    src_op_len = dec_instr.src_operand.size;

    PrintDebug("Dst_Addr = %p, SRC operand = %p\n", 
	       (void *)dst_addr, (void *)src_addr);


    if (run_op(info, dec_instr.op_type, src_addr, dst_addr, src_op_len, dst_op_len) == -1) {
	PrintError("Instruction Emulation Failed\n");
	return -1;
    }

    if (write_fn(write_gpa, (void *)dst_addr, dst_op_len, priv_data) != dst_op_len) {
	PrintError("Did not fully write hooked data\n");
	return -1;
    }

    info->rip += dec_instr.instr_length;

    return dst_op_len;
}


int v3_emulate_read_op(struct guest_info * info, addr_t read_gva, addr_t read_gpa, addr_t src_addr,
		       int (*read_fn)(addr_t guest_addr, void * dst, uint_t length, void * priv_data),
		       int (*write_fn)(addr_t guest_addr, void * src, uint_t length, void * priv_data),  
		       void * priv_data) {
    struct x86_instr dec_instr;
    uchar_t instr[15];
    int ret = 0;
    addr_t dst_addr = 0;
    int src_op_len = 0;
    int dst_op_len = 0;

    PrintDebug("Emulating Read for instruction at %p\n", (void *)(addr_t)(info->rip));
    PrintDebug("GVA=%p\n", (void *)read_gva);

    if (info->mem_mode == PHYSICAL_MEM) { 
	ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    } else { 
	ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    }
    
    if (ret == -1) {
	PrintError("Could not read instruction for Emulated Read at %p\n", (void *)(addr_t)(info->rip));
	return -1;
    }


    if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
	PrintError("Decoding Error\n");
	// Kick off single step emulator
	return -1;
    }
  
    if (dec_instr.is_str_op) {
	PrintError("String operations not implemented on fully hooked regions\n");
	return -1;
    } else if (dec_instr.op_type == V3_OP_XCHG) {
	return emulate_xchg_read_op(info, &dec_instr, read_gva, read_gpa, src_addr, read_fn, write_fn, priv_data);
    }

    if (info->shdw_pg_mode == SHADOW_PAGING) {
	if ((dec_instr.src_operand.type != MEM_OPERAND) ||
	    (dec_instr.src_operand.operand != read_gva)) {
	    PrintError("Inconsistency between Pagefault and Instruction Decode XED_ADDR=%p, PF_ADDR=%p operand_type=%d\n",
		       (void *)dec_instr.src_operand.operand, (void *)read_gva, dec_instr.src_operand.type);
	    return -1;
	}
    } else {
	//check that the operand (GVA) maps to the the faulting GPA
    }

    if (dec_instr.dst_operand.type == MEM_OPERAND) {
	if (info->mem_mode == PHYSICAL_MEM) {
	    if (guest_pa_to_host_va(info, dec_instr.dst_operand.operand, &dst_addr) == -1) {
		PrintError("Could not translate Read Destination (Physical) to host VA\n");
		return -1;
	    }
	} else {
	    if (guest_va_to_host_va(info, dec_instr.dst_operand.operand, &dst_addr) == -1) {
		PrintError("Could not translate Read Destination (Virtual) to host VA\n");
		return -1;
	    }
	}
    } else if (dec_instr.dst_operand.type == REG_OPERAND) {
	dst_addr = dec_instr.dst_operand.operand;
    } else {
	dst_addr = (addr_t)&(dec_instr.dst_operand.operand);
    }

    src_op_len = dec_instr.src_operand.size;
    dst_op_len = dec_instr.dst_operand.size;

    PrintDebug("Dst_Addr = %p, SRC Addr = %p\n", 
	       (void *)dst_addr, (void *)src_addr);

    if (read_fn(read_gpa, (void *)src_addr, src_op_len, priv_data) != src_op_len) {
	PrintError("Did not fully read hooked data\n");
	return -1;
    }

    if (run_op(info, dec_instr.op_type, src_addr, dst_addr, src_op_len, dst_op_len) == -1) {
	PrintError("Instruction Emulation Failed\n");
	return -1;
    }

    info->rip += dec_instr.instr_length;

    return src_op_len;
}






static int run_op(struct guest_info * info, v3_op_type_t op_type, addr_t src_addr, addr_t dst_addr, int src_op_size, int dst_op_size) {

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
