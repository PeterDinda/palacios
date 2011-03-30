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
				   int (*write_fn)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data), 
				   void * priv_data) {
    uint_t emulation_length = 0;
    uint_t emulation_iter_cnt = 0;
    addr_t tmp_rcx = 0;
    addr_t src_addr = 0;

    if (info->shdw_pg_mode == SHADOW_PAGING) {
	if (dec_instr->dst_operand.operand != write_gva) {
	    PrintError("Inconsistency between Pagefault and Instruction Decode XED_ADDR=%p, PF_ADDR=%p\n",
		       (void *)dec_instr->dst_operand.operand, (void *)write_gva);
	    return -1;
	}
    } else {
	// Nested paging (Need check??) 
    }

    /*emulation_length = ( (dec_instr->str_op_length  < (0x1000 - PAGE_OFFSET_4KB(write_gva))) ? 
			 dec_instr->str_op_length :
			 (0x1000 - PAGE_OFFSET_4KB(write_gva)));*/

    if ((dec_instr->str_op_length * (dec_instr->dst_operand.size))  < (0x1000 - PAGE_OFFSET_4KB(write_gva))) { 
	emulation_length = dec_instr->str_op_length * dec_instr->dst_operand.size;
    } else {
	emulation_length = (0x1000 - PAGE_OFFSET_4KB(write_gva));
	PrintError("Warning: emulate_string_write_op emulating %u length operation, but request is for %u length\n", 
		   emulation_length, (uint32_t)(dec_instr->str_op_length*(dec_instr->dst_operand.size)));
    }
  
    /* ** Fix emulation length so that it doesn't overrun over the src page either ** */
    emulation_iter_cnt = emulation_length / dec_instr->dst_operand.size;
    tmp_rcx = emulation_iter_cnt;
  
    if (dec_instr->op_type == V3_OP_MOVS) {

	// figure out addresses here....
	if (info->mem_mode == PHYSICAL_MEM) {
	    if (v3_gpa_to_hva(info, dec_instr->src_operand.operand, &src_addr) == -1) {
		PrintError("Could not translate write Source (Physical) to host VA\n");
		return -1;
	    }
	} else {
	    if (v3_gva_to_hva(info, dec_instr->src_operand.operand, &src_addr) == -1) {
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

    if (write_fn(info, write_gpa, (void *)dst_addr, emulation_length, priv_data) != emulation_length) {
	PrintError("Did not fully read hooked data\n");
	return -1;
    }

    if (emulation_length == dec_instr->str_op_length) {
	info->rip += dec_instr->instr_length;
    }

    return emulation_length;
}



/*
  This function is intended to handle pure read hooks, pure write hooks, and full hooks, 
  with and without backing memory for reads and writes

  A MAXIMUM OF ONE PAGE IS TRANSFERED BUT REGISTERS ARE UPDATED SO THAT
  THE INSTRUCTION CAN BE RESTARTED
  
  read_fn == NULL  
      orig_src_addr == NULL => data at read_gpa is read
      orig_src_addr != NULL => data at orig_src_addr is read
  read_fn != NULL  data is collected using read_fn

  write_fn == NULL 
      orig_dst_addr == NULL => data is written to write_gpa
      orig_dst_addr != NULL => data is written to orig_dst_addr
  write_fn != NULL 
      orig_dst_addr == NULL => data is sent to write_fn
      orig_dst_addr != NULL => data is written to orig_dst_addr, then via write_fn


*/
static int emulate_string_op(struct guest_info * info, struct x86_instr * dec_instr, 
			     addr_t read_gva, addr_t read_gpa, addr_t read_hva,
			     addr_t write_gva, addr_t write_gpa, addr_t write_hva,
			     int (*read_fn)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data), 
			     void * read_priv_data,
			     int (*write_fn)(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data),
			     void * write_priv_data)
{
    uint_t src_emulation_length = 0;
    uint_t dst_emulation_length = 0;
    uint_t emulation_length = 0;
    uint_t emulation_iter_cnt = 0;
    addr_t tmp_rcx = 0;
    addr_t src_hva, dst_hva;


    PrintDebug("emulate_string_op: read_gva=0x%p, read_gpa=0x%p, read_hva=0x%p, write_gva=0x%p, write_gpa=0x%p, write_hva=0x%p, read_fn=0x%p, read_priv_data=0x%p, write_fn=0x%p, write_priv_data=0x%p, len=0x%p\n", 
	       (void*)read_gva,(void*)read_gpa,(void*)read_hva, (void*)write_gva,(void*)write_gpa,(void*)write_hva,
	       (void*)read_fn, (void*)read_priv_data, (void*)write_fn, (void*)write_priv_data, (void*)(dec_instr->str_op_length));

    // v3_print_instr(dec_instr);
    
    // Sanity check the decoded instruction
    
    if (info->shdw_pg_mode == SHADOW_PAGING) {
	// If we're reading, we better have a sane gva
	if ((read_hva || read_fn) && (dec_instr->src_operand.operand != read_gva)) { 
	    PrintError("Inconsistency between Pagefault and Instruction Decode XED_ADDR=%p, PF_ADDR=%p (Read)\n",
		       (void *)dec_instr->src_operand.operand, (void *)read_gva);
		return -1;
	}
	// if we're writing, we better have a sane gva
	if ((write_hva || write_fn) && (dec_instr->dst_operand.operand != write_gva)) {
	    PrintError("Inconsistency between Pagefault and Instruction Decode XED_ADDR=%p, PF_ADDR=%p (Write)\n",
		       (void *)dec_instr->dst_operand.operand, (void *)write_gva);
	    return -1;
	}
    } else {
	// Nested paging (Need check??) 
    }
    

    if (dec_instr->src_operand.size != dec_instr->dst_operand.size) { 
	PrintError("Source and Destination Operands are of different sizes\n");
	return -1;
    }
    

    // We will only read up to the next page boundary

    if ((dec_instr->str_op_length * (dec_instr->src_operand.size))  < (0x1000 - PAGE_OFFSET_4KB(read_gva))) { 
	src_emulation_length = dec_instr->str_op_length * dec_instr->src_operand.size;
    } else {
	src_emulation_length = (0x1000 - PAGE_OFFSET_4KB(read_gva));
	PrintError("Warning: emulate_string_op emulating src of %u length operation, but request is for %u length\n", 
		   src_emulation_length, (uint32_t) (dec_instr->str_op_length*(dec_instr->src_operand.size)));
    }

    // We will only write up to the next page boundary

    if ((dec_instr->str_op_length * (dec_instr->dst_operand.size))  < (0x1000 - PAGE_OFFSET_4KB(write_gva))) { 
	dst_emulation_length = dec_instr->str_op_length * dec_instr->dst_operand.size;
    } else {
	dst_emulation_length = (0x1000 - PAGE_OFFSET_4KB(write_gva));
	PrintError("Warning: emulate_string_op emulating dst of %u length operation, but request is for %u length\n", 
		   dst_emulation_length, (uint32_t) (dec_instr->str_op_length*(dec_instr->dst_operand.size)));
    }

    // We will only copy the minimum of what is available to be read or written

    if (src_emulation_length<dst_emulation_length) { 
	emulation_length=src_emulation_length;
	// Note that this error is what is to be expected if you're coping to a different offset on a page
	PrintError("Warning: emulate_string_op has src length %u but dst length %u\n", src_emulation_length, dst_emulation_length);
    } else if (src_emulation_length>dst_emulation_length) { 
	emulation_length=dst_emulation_length;
	// Note that this error is what is to be expected if you're coping to a different offset on a page
	PrintError("Warning: emulate_string_op has src length %u but dst length %u\n", src_emulation_length, dst_emulation_length);
    } else {
	// equal
	emulation_length=src_emulation_length;
    }
	

    // Fetch the data

    if (read_fn) { 
	// This is a full hook - full hooks never have backing memory
	// This should use the scratch page allocated for the hook, but 
	// we do not know where that is at this point
	src_hva = (addr_t) V3_Malloc(emulation_length);  // hideous - should reuse memory
	if (!src_hva) { 
	    PrintError("Unable to allocate space for read operation in emulate_string_read_op\n");
	    return -1;
	}
	if (read_fn(info, read_gpa, (void *)src_hva, emulation_length,  read_priv_data) != emulation_length) {
	    PrintError("Did not fully read hooked data in emulate_string_op\n");
	    return -1;
	}
    } else {
	// This is ordinary memory
	if (read_hva) { 
	    // The caller told us where to read from
	    src_hva=read_hva;
	} else {
	    // We need to figure out where to read from
	    if (info->mem_mode == PHYSICAL_MEM) {
		if (v3_gpa_to_hva(info, dec_instr->src_operand.operand, &src_hva) == -1) {
		    PrintError("Could not translate write Source (Physical) to host VA\n");
		    return -1;
		}
	    } else {
		if (v3_gva_to_hva(info, dec_instr->src_operand.operand, &src_hva) == -1) {
		    PrintError("Could not translate write Source (Virtual) to host VA\n");
		    return -1;
		}
	    }
	}
    }

    // Now src_hva points to the fetched data or to the in-VM data

    // Allocate space for the write, in case we need to copy out later
    if (write_fn) { 
	// This is a full hook or a write hook
	if (write_hva) { 
	    // This is a write hook with backing memory
	    // The caller already told us where that memory is
	    dst_hva = write_hva; 
	} else {
	    // This is a full hook without backing memory
	    // Again, should use the scratch memory
	    dst_hva = (addr_t) V3_Malloc(emulation_length);  // yuck
	    if (!dst_hva) { 
		PrintError("Unable to allocate space for write operation in emulate_string_op\n");
		if (read_fn) { 
		    V3_Free((void*)src_hva); 
		}
		return -1;
	    }
	}
    } else {
	// This is ordinary memory
	if (write_hva) { 
	    // The caller told us where to write
	    dst_hva=write_hva;
	} else {
	    // We need to figure out where to write
	    if (info->mem_mode == PHYSICAL_MEM) {
		if (v3_gpa_to_hva(info, dec_instr->dst_operand.operand, &dst_hva) == -1) {
		    PrintError("Could not translate write Dest (Physical) to host VA\n");
		    return -1;
		}
	    } else {
		if (v3_gva_to_hva(info, dec_instr->dst_operand.operand, &dst_hva) == -1) {
		    PrintError("Could not translate write Dest (Virtual) to host VA\n");
		    return -1;
		}
	    }
	}
    }

    // Now dst_addr points to where we will copy the data

    // How many items to copy 
    emulation_iter_cnt = emulation_length / dec_instr->dst_operand.size;
    tmp_rcx = emulation_iter_cnt;


    // Do the actual emulation
    // The instruction implementation operates from data at src_hva to data at dest_hva
    // Furthemore, it must operate for emulation_length steps
    // And update tmp_rcx
    // And the real rcx if we do have a rep prefix
    switch (dec_instr->op_type) { 

	case V3_OP_MOVS: {
	    
	    if (dec_instr->dst_operand.size == 1) {
		movs8((addr_t *)&dst_hva, &src_hva, &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
	    } else if (dec_instr->dst_operand.size == 2) {
		movs16((addr_t *)&dst_hva, &src_hva, &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
	    } else if (dec_instr->dst_operand.size == 4) {
		movs32((addr_t *)&dst_hva, &src_hva, &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
#ifdef __V3_64BIT__
	    } else if (dec_instr->dst_operand.size == 8) {
		movs64((addr_t *)&dst_hva, &src_hva, &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
#endif
	    } else {
		PrintError("Invalid operand length\n");
		return -1;
	    }

	    // Can't these count down too? - PAD

	    info->vm_regs.rdi += emulation_length;
	    info->vm_regs.rsi += emulation_length;
	    
	    // RCX is only modified if the rep prefix is present
	    if (dec_instr->prefixes.rep == 1) {
		info->vm_regs.rcx -= emulation_iter_cnt;
	    }
	    
	} 
	    break;

	case V3_OP_STOS: {
	    
	    if (dec_instr->dst_operand.size == 1) {
		stos8((addr_t *)&dst_hva, (addr_t  *)&(info->vm_regs.rax), &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
	    } else if (dec_instr->dst_operand.size == 2) {
		stos16((addr_t *)&dst_hva, (addr_t  *)&(info->vm_regs.rax), &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
	    } else if (dec_instr->dst_operand.size == 4) {
		stos32((addr_t *)&dst_hva, (addr_t  *)&(info->vm_regs.rax), &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
#ifdef __V3_64BIT__
	    } else if (dec_instr->dst_operand.size == 8) {
		stos64((addr_t *)&dst_hva, (addr_t  *)&(info->vm_regs.rax), &tmp_rcx, (addr_t *)&(info->ctrl_regs.rflags));
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
	    
	} 
	    break;
	    
	default:  {
	    PrintError("Unimplemented String operation\n");
	    return -1;
	}
	    break;
    }

    // At this point, the data has been written over dst_hva, which 
    // is either our temporary buffer, or it's the requested target in write_hva

    if (write_fn) { 
	if (write_fn(info, write_gpa, (void *)dst_hva, emulation_length, write_priv_data) != emulation_length) {
	    PrintError("Did not fully write hooked data\n");
	    return -1;
	}
    }

    // We only goto the next instruction if we have finished operating on all the data.  
    // If we haven't we'll restart the same instruction, but with rdi/rsi/rcx updated
    // This is also how we handle going over a page boundary
    if (emulation_length == dec_instr->str_op_length) {
	info->rip += dec_instr->instr_length;
    }
    

    // Delete temporary buffers
    if (read_fn) { 
	V3_Free((void*)src_hva); 
    }
    if (write_fn && !write_hva) { 
	V3_Free((void*)dst_hva);
    }
	

    return emulation_length;
}





static int emulate_xchg_write_op(struct guest_info * info, struct x86_instr * dec_instr, 
				 addr_t write_gva, addr_t write_gpa, addr_t dst_addr, 
				 int (*write_fn)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data), 
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
    
    if (write_fn(info, write_gpa, (void *)dst_addr, dst_op_len, priv_data) != dst_op_len) {
	PrintError("Did not fully write hooked data\n");
	return -1;
    }
    
    info->rip += dec_instr->instr_length;
    
    return dst_op_len;
}
    


static int emulate_xchg_read_op(struct guest_info * info, struct x86_instr * dec_instr, 
				addr_t read_gva, addr_t read_gpa, addr_t src_addr, 
				int (*read_fn)(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data), 
				int (*write_fn)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data), 			
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


    if (read_fn(info, read_gpa, (void *)src_addr, src_op_len, priv_data) != src_op_len) {
	PrintError("Did not fully read hooked data\n");
	return -1;
    }

    if (run_op(info, dec_instr->op_type, em_src_addr, em_dst_addr, src_op_len, dst_op_len) == -1) {
	PrintError("Instruction Emulation Failed\n");
	return -1;
    }

    if (write_fn(info, read_gpa, (void *)src_addr, dst_op_len, priv_data) != dst_op_len) {
	PrintError("Did not fully write hooked data\n");
	return -1;
    }

    info->rip += dec_instr->instr_length;

    return dst_op_len;
}




int v3_emulate_write_op(struct guest_info * info, addr_t write_gva, addr_t write_gpa,  addr_t dst_addr, 
			int (*write_fn)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data), 
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
	ret = v3_read_gpa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    } else { 
	ret = v3_read_gva_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
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
	    if (v3_gpa_to_hva(info, dec_instr.src_operand.operand, &src_addr) == -1) {
		PrintError("Could not translate write Source (Physical) to host VA\n");
		return -1;
	    }
	} else {
	    if (v3_gva_to_hva(info, dec_instr.src_operand.operand, &src_addr) == -1) {
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

    if (write_fn(info, write_gpa, (void *)dst_addr, dst_op_len, priv_data) != dst_op_len) {
	PrintError("Did not fully write hooked data\n");
	return -1;
    }

    info->rip += dec_instr.instr_length;

    return dst_op_len;
}


int v3_emulate_read_op(struct guest_info * info, addr_t read_gva, addr_t read_gpa, addr_t src_addr,
		       int (*read_fn)(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data),
		       int (*write_fn)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data),  
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
	ret = v3_read_gpa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    } else { 
	ret = v3_read_gva_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
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
	// We got here due to a read fault due to a full memory hook on the 
	// region being READ. Thus our current write_fn is also for that region
	// We need the region that will be WRITTEN, which we need to look up
	// That region could be write or full hooked, in which case we need
	// the associated write function for that region.  If it's not
	// hooked, then we need the relevant hva
	//
	// This all assumes that emulate_string_op() will handle at most
	// a single page.   Therefore we can consider only the starting pages
	// for the read and write sides.   We will restart the instruction on
	// the next page, if needed. 
	addr_t write_gpa=0;
	addr_t write_gva=0;
	addr_t write_hva=0;
	int (*dest_write_fn)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data)=0;
	void *dest_write_priv_data;
	struct v3_mem_region *dest_reg;

	if (dec_instr.dst_operand.type != MEM_OPERAND) { 
	    write_gpa=0; 
	    write_gva=0;
	    write_hva=0; // should calc target here and continue
	    dest_write_fn=0;
	    dest_write_priv_data=0;
	    PrintError("Emulation of string ops with non-memory destinations currently unsupported\n");
	    v3_print_instr(&dec_instr);
	    return -1;
	} else {
	    if (info->mem_mode == PHYSICAL_MEM) {
		write_gpa = dec_instr.dst_operand.operand;
		write_gva = write_gpa;
	    } else {
		write_gva = dec_instr.dst_operand.operand;
		if (v3_gva_to_gpa(info, dec_instr.dst_operand.operand, &write_gpa) == -1) {
		    // We are going to inject "Not Present" here to try to force
		    // the guest to build a PTE we can use. 
		    // This needs to be fixed to inject the appropraite page fault
		    // given access permissions
		    struct pf_error_code c;
		    c.present=0;
		    c.write=0;
		    c.user=0;
		    c.rsvd_access=0;
		    c.ifetch=0;
		    c.rsvd=0;
		    v3_inject_guest_pf(info,write_gva,c);  
		    return 0;
		}
	    }

	    // First we need to find the region to determine if we will need to write
	    // back to it and to check access
	    if (!(dest_reg=v3_get_mem_region(info->vm_info,info->cpu_id,write_gpa))) { 
		PrintError("Could not look up region for destination of string op\n");
		v3_print_instr(&dec_instr);
		return -1;
	    }

	    
	    if (dest_reg->flags.alloced) { 
		// We will need to write back to memory in addition to any hook function
		if (v3_gpa_to_hva(info, write_gpa, &write_hva) == -1) { 
		    PrintError("Unable to convert gpa to hva in emulation of string op write\n");
		    v3_print_instr(&dec_instr);
		    return -1;
		}
	    } else {
		write_hva=0; // no actual writeback - hook function only
	    }
		    
	    // Now that we have the write_gpa, we need to find out whether it's a hooked region
	    // or just plain memory
	    
	    if (v3_find_mem_hook(info->vm_info, info->cpu_id, write_gpa, 
				 0, 0, // don't want the read function/data even if they exist
				 &dest_write_fn,&dest_write_priv_data) == -1) { 
		PrintError("Finding write destination memory hook failed\n");
		v3_print_instr(&dec_instr);
		return -1;
	    }

	    // We must have either or both of a write_hva and a dest_write_fn
	    if (!dest_write_fn && !write_hva) { 
		PrintError("Destination of string write has neither physical memory nor write hook!\n");
		v3_print_instr(&dec_instr);
		return -1;
	    }
	}
		
  
	return emulate_string_op(info,&dec_instr,
				 read_gva,read_gpa, 0,   // 0=> read hook has no backing memory
				 write_gva, write_gpa, write_hva,
				 read_fn, priv_data, // This is from the original call
				 dest_write_fn, dest_write_priv_data); // This is from our lookup

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
	    if (v3_gpa_to_hva(info, dec_instr.dst_operand.operand, &dst_addr) == -1) {
		PrintError("Could not translate Read Destination (Physical) to host VA\n");
		return -1;
	    }
	} else {
	    if (v3_gva_to_hva(info, dec_instr.dst_operand.operand, &dst_addr) == -1) {
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

    if (read_fn(info, read_gpa, (void *)src_addr, src_op_len, priv_data) != src_op_len) {
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
