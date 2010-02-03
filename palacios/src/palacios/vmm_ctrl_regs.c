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
#include <palacios/vmm_direct_paging.h>

#ifndef CONFIG_DEBUG_CTRL_REGS
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static int handle_lmsw(struct guest_info * info, struct x86_instr * dec_instr);
static int handle_clts(struct guest_info * info, struct x86_instr * dec_instr);
static int handle_mov_to_cr0(struct guest_info * info, struct x86_instr * dec_instr);


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
    
    if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
	PrintError("Could not decode instruction\n");
	return -1;
    }

    
    if (dec_instr.op_type == V3_OP_LMSW) {
	if (handle_lmsw(info, &dec_instr) == -1) {
	    return -1;
	}
    } else if (dec_instr.op_type == V3_OP_MOV2CR) {
	if (handle_mov_to_cr0(info, &dec_instr) == -1) {
	    return -1;
	}
    } else if (dec_instr.op_type == V3_OP_CLTS) {
	if (handle_clts(info, &dec_instr) == -1) {
	    return -1;
	}
    } else {
	PrintError("Unhandled opcode in handle_cr0_write\n");
	return -1;
    }
    
    info->rip += dec_instr.instr_length;
    
    return 0;
}




// The CR0 register only has flags in the low 32 bits
// The hardware does a format check to make sure the high bits are zero
// Because of this we can ignore the high 32 bits here
static int handle_mov_to_cr0(struct guest_info * info, struct x86_instr * dec_instr) {
    // 32 bit registers
    struct cr0_32 * shadow_cr0 = (struct cr0_32 *)&(info->ctrl_regs.cr0);
    struct cr0_32 * new_cr0 = (struct cr0_32 *)(dec_instr->src_operand.operand);
    struct cr0_32 * guest_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
    uint_t paging_transition = 0;
    
    PrintDebug("MOV2CR0 (MODE=%s)\n", v3_cpu_mode_to_str(info->cpu_mode));
    
    PrintDebug("OperandVal = %x, length=%d\n", *(uint_t *)new_cr0, dec_instr->src_operand.size);
    
    PrintDebug("Old CR0=%x\n", *(uint_t *)shadow_cr0);
    PrintDebug("Old Guest CR0=%x\n", *(uint_t *)guest_cr0);	
    
    
    // We detect if this is a paging transition
    if (guest_cr0->pg != new_cr0->pg) {
	paging_transition = 1;
    }  
    
    // Guest always sees the value they wrote
    *guest_cr0 = *new_cr0;
    
    // This value must always be set to 1 
    guest_cr0->et = 1;    
    
    // Set the shadow register to catch non-virtualized flags
    *shadow_cr0 = *guest_cr0;
    
    // Paging is always enabled
    shadow_cr0->pg = 1;  
    
    // Was there a paging transition
    // Meaning we need to change the page tables
    if (paging_transition) {
	if (v3_get_vm_mem_mode(info) == VIRTUAL_MEM) {
	    
	    struct efer_64 * guest_efer  = (struct efer_64 *)&(info->shdw_pg_state.guest_efer);
	    struct efer_64 * shadow_efer = (struct efer_64 *)&(info->ctrl_regs.efer);
	    
	    // Check long mode LME to set LME
	    if (guest_efer->lme == 1) {
		PrintDebug("Enabing Long Mode\n");
		guest_efer->lma = 1;
		
		shadow_efer->lma = 1;
		shadow_efer->lme = 1;
		
		PrintDebug("New EFER %p\n", (void *)*(addr_t *)(shadow_efer));
	    }
	    
	    PrintDebug("Activating Shadow Page Tables\n");
	    
	    if (v3_activate_shadow_pt(info) == -1) {
		PrintError("Failed to activate shadow page tables\n");
		return -1;
	    }
	} else {

	    shadow_cr0->wp = 1;
	    
	    if (v3_activate_passthrough_pt(info) == -1) {
		PrintError("Failed to activate passthrough page tables\n");
		return -1;
	    }
	}
    }
    
    
    PrintDebug("New Guest CR0=%x\n",*(uint_t *)guest_cr0);  
    PrintDebug("New CR0=%x\n", *(uint_t *)shadow_cr0);
    
    return 0;
}




static int handle_clts(struct guest_info * info, struct x86_instr * dec_instr) {
    // CLTS
    struct cr0_32 * real_cr0 = (struct cr0_32*)&(info->ctrl_regs.cr0);
    
    real_cr0->ts = 0;
    
    if (info->shdw_pg_mode == SHADOW_PAGING) {
	struct cr0_32 * guest_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
	guest_cr0->ts = 0;
    }
    return 0;
}


static int handle_lmsw(struct guest_info * info, struct x86_instr * dec_instr) {
    struct cr0_real * real_cr0  = (struct cr0_real *)&(info->ctrl_regs.cr0);
    // XED is a mess, and basically reverses the operand order for an LMSW
    struct cr0_real * new_cr0 = (struct cr0_real *)(dec_instr->dst_operand.operand);	
    uchar_t new_cr0_val;
    
    PrintDebug("LMSW\n");
    
    new_cr0_val = (*(char*)(new_cr0)) & 0x0f;
    
    PrintDebug("OperandVal = %x\n", new_cr0_val);
    
    // We can just copy the new value through
    // we don't need to virtualize the lower 4 bits
    PrintDebug("Old CR0=%x\n", *(uint_t *)real_cr0);	
    *(uchar_t*)real_cr0 &= 0xf0;
    *(uchar_t*)real_cr0 |= new_cr0_val;
    PrintDebug("New CR0=%x\n", *(uint_t *)real_cr0);	
    
    
    // If Shadow paging is enabled we push the changes to the virtualized copy of cr0
    if (info->shdw_pg_mode == SHADOW_PAGING) {
	struct cr0_real * guest_cr0 = (struct cr0_real*)&(info->shdw_pg_state.guest_cr0);
	
	PrintDebug("Old Guest CR0=%x\n", *(uint_t *)guest_cr0);	
	*(uchar_t*)guest_cr0 &= 0xf0;
	*(uchar_t*)guest_cr0 |= new_cr0_val;
	PrintDebug("New Guest CR0=%x\n", *(uint_t *)guest_cr0);	
    }
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
    
    
    if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
	PrintError("Could not decode instruction\n");
	return -1;
    }
    
    if (dec_instr.op_type == V3_OP_MOVCR2) {
	PrintDebug("MOVCR2 (mode=%s)\n", v3_cpu_mode_to_str(info->cpu_mode));

	if ((v3_get_vm_cpu_mode(info) == LONG) || 
	    (v3_get_vm_cpu_mode(info) == LONG_32_COMPAT)) {
	    struct cr0_64 * dst_reg = (struct cr0_64 *)(dec_instr.dst_operand.operand);
	
	    if (info->shdw_pg_mode == SHADOW_PAGING) {
		struct cr0_64 * guest_cr0 = (struct cr0_64 *)&(info->shdw_pg_state.guest_cr0);
		*dst_reg = *guest_cr0;
	    } else {
		struct cr0_64 * shadow_cr0 = (struct cr0_64 *)&(info->ctrl_regs.cr0);
		*dst_reg = *shadow_cr0;
	    }

	    PrintDebug("returned CR0: %p\n", (void *)*(addr_t *)dst_reg);
	} else {
	    struct cr0_32 * dst_reg = (struct cr0_32 *)(dec_instr.dst_operand.operand);
	
	    if (info->shdw_pg_mode == SHADOW_PAGING) {
		struct cr0_32 * guest_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);
		*dst_reg = *guest_cr0;
	    } else {
		struct cr0_32 * shadow_cr0 = (struct cr0_32 *)&(info->ctrl_regs.cr0);
		*dst_reg = *shadow_cr0;
	    }

	    PrintDebug("returned CR0: %x\n", *(uint_t*)dst_reg);
	}

    } else if (dec_instr.op_type == V3_OP_SMSW) {
	struct cr0_real * shadow_cr0 = (struct cr0_real *)&(info->ctrl_regs.cr0);
	struct cr0_real * dst_reg = (struct cr0_real *)(dec_instr.dst_operand.operand);
	char cr0_val = *(char*)shadow_cr0 & 0x0f;
	
	PrintDebug("SMSW\n");
	
	// The lower 4 bits of the guest/shadow CR0 are mapped through
	// We can treat nested and shadow paging the same here
	*(char *)dst_reg &= 0xf0;
	*(char *)dst_reg |= cr0_val;
	
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
    
    if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
	PrintError("Could not decode instruction\n");
	return -1;
    }
    
    if (dec_instr.op_type == V3_OP_MOV2CR) {
	PrintDebug("MOV2CR3 (cpu_mode=%s)\n", v3_cpu_mode_to_str(info->cpu_mode));
	
	if (info->shdw_pg_mode == SHADOW_PAGING) {
	    PrintDebug("Old Shadow CR3=%p; Old Guest CR3=%p\n", 
		       (void *)(addr_t)(info->ctrl_regs.cr3), 
		       (void*)(addr_t)(info->shdw_pg_state.guest_cr3));
	    
	    
	    // We update the guest CR3    
	    if (info->cpu_mode == LONG) {
		struct cr3_64 * new_cr3 = (struct cr3_64 *)(dec_instr.src_operand.operand);
		struct cr3_64 * guest_cr3 = (struct cr3_64 *)&(info->shdw_pg_state.guest_cr3);
		*guest_cr3 = *new_cr3;
	    } else {
		struct cr3_32 * new_cr3 = (struct cr3_32 *)(dec_instr.src_operand.operand);
		struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);
		*guest_cr3 = *new_cr3;
	    }


	    // If Paging is enabled in the guest then we need to change the shadow page tables
	    if (info->mem_mode == VIRTUAL_MEM) {
		if (v3_activate_shadow_pt(info) == -1) {
		    PrintError("Failed to activate 32 bit shadow page table\n");
		    return -1;
		}
	    }
	    
	    PrintDebug("New Shadow CR3=%p; New Guest CR3=%p\n", 
		       (void *)(addr_t)(info->ctrl_regs.cr3), 
		       (void*)(addr_t)(info->shdw_pg_state.guest_cr3));
	    
	} else if (info->shdw_pg_mode == NESTED_PAGING) {
	    
	    // This is just a passthrough operation which we probably don't need here
	    if (info->cpu_mode == LONG) {
		struct cr3_64 * new_cr3 = (struct cr3_64 *)(dec_instr.src_operand.operand);
		struct cr3_64 * guest_cr3 = (struct cr3_64 *)&(info->ctrl_regs.cr3);
		*guest_cr3 = *new_cr3;
	    } else {
		struct cr3_32 * new_cr3 = (struct cr3_32 *)(dec_instr.src_operand.operand);
		struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(info->ctrl_regs.cr3);
		*guest_cr3 = *new_cr3;
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
    
    if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
	PrintError("Could not decode instruction\n");
	return -1;
    }
    
    if (dec_instr.op_type == V3_OP_MOVCR2) {
	PrintDebug("MOVCR32 (mode=%s)\n", v3_cpu_mode_to_str(info->cpu_mode));
	
	if (info->shdw_pg_mode == SHADOW_PAGING) {
	    
	    if ((v3_get_vm_cpu_mode(info) == LONG) || 
		(v3_get_vm_cpu_mode(info) == LONG_32_COMPAT)) {
		struct cr3_64 * dst_reg = (struct cr3_64 *)(dec_instr.dst_operand.operand);
		struct cr3_64 * guest_cr3 = (struct cr3_64 *)&(info->shdw_pg_state.guest_cr3);
		*dst_reg = *guest_cr3;
	    } else {
		struct cr3_32 * dst_reg = (struct cr3_32 *)(dec_instr.dst_operand.operand);
		struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);
		*dst_reg = *guest_cr3;
	    }
	    
	} else if (info->shdw_pg_mode == NESTED_PAGING) {
	    
	    // This is just a passthrough operation which we probably don't need here
	    if ((v3_get_vm_cpu_mode(info) == LONG) || 
		(v3_get_vm_cpu_mode(info) == LONG_32_COMPAT)) {
		struct cr3_64 * dst_reg = (struct cr3_64 *)(dec_instr.dst_operand.operand);
		struct cr3_64 * guest_cr3 = (struct cr3_64 *)&(info->ctrl_regs.cr3);
		*dst_reg = *guest_cr3;
	    } else {
		struct cr3_32 * dst_reg = (struct cr3_32 *)(dec_instr.dst_operand.operand);
		struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(info->ctrl_regs.cr3);
		*dst_reg = *guest_cr3;
	    }
	}
	
    } else {
	PrintError("Unhandled opcode in handle_cr3_read\n");
	return -1;
    }
    
    info->rip += dec_instr.instr_length;
    
    return 0;
}


// We don't need to virtualize CR4, all we need is to detect the activation of PAE
int v3_handle_cr4_read(struct guest_info * info) {
    //  PrintError("CR4 Read not handled\n");
    // Do nothing...
    return 0;
}

int v3_handle_cr4_write(struct guest_info * info) {
    uchar_t instr[15];
    int ret;
    int flush_tlb=0;
    struct x86_instr dec_instr;
    v3_cpu_mode_t cpu_mode = v3_get_vm_cpu_mode(info);
    
    if (info->mem_mode == PHYSICAL_MEM) { 
	ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    } else { 
	ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    }
    
    if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
	PrintError("Could not decode instruction\n");
	return -1;
    }
    
    if (dec_instr.op_type != V3_OP_MOV2CR) {
	PrintError("Invalid opcode in write to CR4\n");
	return -1;
    }
    
    // Check to see if we need to flush the tlb
    
    if (v3_get_vm_mem_mode(info) == VIRTUAL_MEM) { 
	struct cr4_32 * new_cr4 = (struct cr4_32 *)(dec_instr.src_operand.operand);
	struct cr4_32 * cr4 = (struct cr4_32 *)&(info->ctrl_regs.cr4);
	
	// if pse, pge, or pae have changed while PG (in any mode) is on
	// the side effect is a TLB flush, which means we need to
	// toss the current shadow page tables too
	//
	// 
	// TODO - PAE FLAG needs to be special cased
	if ((cr4->pse != new_cr4->pse) || 
	    (cr4->pge != new_cr4->pge) || 
	    (cr4->pae != new_cr4->pae)) { 
	    PrintDebug("Handling PSE/PGE/PAE -> TLBFlush case, flag set\n");
	    flush_tlb = 1;
	    
	}
    }
    

    if ((cpu_mode == PROTECTED) || (cpu_mode == PROTECTED_PAE)) {
	struct cr4_32 * new_cr4 = (struct cr4_32 *)(dec_instr.src_operand.operand);
	struct cr4_32 * cr4 = (struct cr4_32 *)&(info->ctrl_regs.cr4);
	
	PrintDebug("OperandVal = %x, length = %d\n", *(uint_t *)new_cr4, dec_instr.src_operand.size);
	PrintDebug("Old CR4=%x\n", *(uint_t *)cr4);
	
	if ((info->shdw_pg_mode == SHADOW_PAGING)) { 
	    if (v3_get_vm_mem_mode(info) == PHYSICAL_MEM) {
		
		if ((cr4->pae == 0) && (new_cr4->pae == 1)) {
		    PrintDebug("Creating PAE passthrough tables\n");
		    
		    // create 32 bit PAE direct map page table
		    if (v3_reset_passthrough_pts(info) == -1) {
			PrintError("Could not create 32 bit PAE passthrough pages tables\n");
			return -1;
		    }

		    // reset cr3 to new page tables
		    info->ctrl_regs.cr3 = *(addr_t*)&(info->direct_map_pt);
		    
		} else if ((cr4->pae == 1) && (new_cr4->pae == 0)) {
		    // Create passthrough standard 32bit pagetables
		    PrintError("Switching From PAE to Protected mode not supported\n");
		    return -1;
		} 
	    }
	}
	
	*cr4 = *new_cr4;
	PrintDebug("New CR4=%x\n", *(uint_t *)cr4);
	
    } else if ((cpu_mode == LONG) || (cpu_mode == LONG_32_COMPAT)) {
	struct cr4_64 * new_cr4 = (struct cr4_64 *)(dec_instr.src_operand.operand);
	struct cr4_64 * cr4 = (struct cr4_64 *)&(info->ctrl_regs.cr4);
	
	PrintDebug("Old CR4=%p\n", (void *)*(addr_t *)cr4);
	PrintDebug("New CR4=%p\n", (void *)*(addr_t *)new_cr4);
	
	if (new_cr4->pae == 0) {
	    // cannot turn off PAE in long mode GPF the guest
	    PrintError("Cannot disable PAE in long mode, should send GPF\n");
	    return -1;
	}
	
	*cr4 = *new_cr4;
	
    } else {
	PrintError("CR4 write not supported in CPU_MODE: %s\n", v3_cpu_mode_to_str(cpu_mode));
	return -1;
    }
    
    
    if (flush_tlb) {
	PrintDebug("Handling PSE/PGE/PAE -> TLBFlush (doing flush now!)\n");
	if (v3_activate_shadow_pt(info) == -1) {
	    PrintError("Failed to activate shadow page tables when emulating TLB flush in handling cr4 write\n");
	    return -1;
	}
    }
    
    
    info->rip += dec_instr.instr_length;
    return 0;
}


int v3_handle_efer_read(struct guest_info * core, uint_t msr, struct v3_msr * dst, void * priv_data) {
    PrintDebug("EFER Read HI=%x LO=%x\n", core->shdw_pg_state.guest_efer.hi, core->shdw_pg_state.guest_efer.lo);
    
    dst->value = core->shdw_pg_state.guest_efer.value;
    
    return 0;
}



// TODO: this is a disaster we need to clean this up...
int v3_handle_efer_write(struct guest_info * core, uint_t msr, struct v3_msr src, void * priv_data) {
    //struct efer_64 * new_efer = (struct efer_64 *)&(src.value);
    struct efer_64 * shadow_efer = (struct efer_64 *)&(core->ctrl_regs.efer);
    struct v3_msr * guest_efer = &(core->shdw_pg_state.guest_efer);
    
    PrintDebug("EFER Write\n");
    PrintDebug("EFER Write Values: HI=%x LO=%x\n", src.hi, src.lo);
    //PrintDebug("Old EFER=%p\n", (void *)*(addr_t*)(shadow_efer));
    
    // We virtualize the guests efer to hide the SVME and LMA bits
    guest_efer->value = src.value;
    
    
    // Enable/Disable Syscall
    shadow_efer->sce = src.value & 0x1;
    
    return 0;
}
