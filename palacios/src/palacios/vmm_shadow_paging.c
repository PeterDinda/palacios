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


#include <palacios/vmm_shadow_paging.h>


#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_ctrl_regs.h>

#include <palacios/vmm_hashtable.h>

#include <palacios/vmm_direct_paging.h>

#ifndef DEBUG_SHADOW_PAGING
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


/*** 
 ***  There be dragons
 ***/


struct shadow_page_data {
    v3_reg_t cr3;
    addr_t page_pa;
  
    struct list_head page_list_node;
};



static struct shadow_page_data * create_new_shadow_pt(struct guest_info * info);
static void inject_guest_pf(struct guest_info * info, addr_t fault_addr, pf_error_t error_code);
static int is_guest_pf(pt_access_status_t guest_access, pt_access_status_t shadow_access);


#include "vmm_shadow_paging_32.h"
#include "vmm_shadow_paging_32pae.h"
#include "vmm_shadow_paging_64.h"



int v3_init_shadow_page_state(struct guest_info * info) {
    struct shadow_page_state * state = &(info->shdw_pg_state);
  
    state->guest_cr3 = 0;
    state->guest_cr0 = 0;

    INIT_LIST_HEAD(&(state->page_list));
  
    return 0;
}



// Reads the guest CR3 register
// creates new shadow page tables
// updates the shadow CR3 register to point to the new pts
int v3_activate_shadow_pt(struct guest_info * info) {
    switch (v3_get_vm_cpu_mode(info)) {

	case PROTECTED:
	    return activate_shadow_pt_32(info);
	case PROTECTED_PAE:
	    return activate_shadow_pt_32pae(info);
	case LONG:
	case LONG_32_COMPAT:
	case LONG_16_COMPAT:
	    return activate_shadow_pt_64(info);
	default:
	    PrintError("Invalid CPU mode: %s\n", v3_cpu_mode_to_str(v3_get_vm_cpu_mode(info)));
	    return -1;
    }

    return 0;
}



// This must flush any caches
// and reset the cr3 value to the correct value
int v3_invalidate_shadow_pts(struct guest_info * info) {
    return v3_activate_shadow_pt(info);
}


int v3_handle_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  
    if (v3_get_vm_mem_mode(info) == PHYSICAL_MEM) {
	// If paging is not turned on we need to handle the special cases
	return v3_handle_passthrough_pagefault(info, fault_addr, error_code);
    } else if (v3_get_vm_mem_mode(info) == VIRTUAL_MEM) {

	switch (v3_get_vm_cpu_mode(info)) {
	    case PROTECTED:
		return handle_shadow_pagefault_32(info, fault_addr, error_code);
		break;
	    case PROTECTED_PAE:
		return handle_shadow_pagefault_32pae(info, fault_addr, error_code);
	    case LONG:
	    case LONG_32_COMPAT:
	    case LONG_16_COMPAT:
		return handle_shadow_pagefault_64(info, fault_addr, error_code);
		break;
	    default:
		PrintError("Unhandled CPU Mode: %s\n", v3_cpu_mode_to_str(v3_get_vm_cpu_mode(info)));
		return -1;
	}
    } else {
	PrintError("Invalid Memory mode\n");
	return -1;
    }
}


int v3_handle_shadow_invlpg(struct guest_info * info) {
    uchar_t instr[15];
    struct x86_instr dec_instr;
    int ret = 0;
    addr_t vaddr = 0;

    if (v3_get_vm_mem_mode(info) != VIRTUAL_MEM) {
	// Paging must be turned on...
	// should handle with some sort of fault I think
	PrintError("ERROR: INVLPG called in non paged mode\n");
	return -1;
    }

    if (v3_get_vm_mem_mode(info) == PHYSICAL_MEM) { 
	ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    } else { 
	ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
    }

    if (ret == -1) {
	PrintError("Could not read instruction into buffer\n");
	return -1;
    }

    if (v3_decode(info, (addr_t)instr, &dec_instr) == -1) {
	PrintError("Decoding Error\n");
	return -1;
    }
  
    if ((dec_instr.op_type != V3_OP_INVLPG) || 
	(dec_instr.num_operands != 1) ||
	(dec_instr.dst_operand.type != MEM_OPERAND)) {
	PrintError("Decoder Error: Not a valid INVLPG instruction...\n");
	return -1;
    }

    vaddr = dec_instr.dst_operand.operand;

    info->rip += dec_instr.instr_length;

    switch (v3_get_vm_cpu_mode(info)) {
	case PROTECTED:
	    return handle_shadow_invlpg_32(info, vaddr);
	case PROTECTED_PAE:
	    return handle_shadow_invlpg_32pae(info, vaddr);
	case LONG:
	case LONG_32_COMPAT:
	case LONG_16_COMPAT:
	    return handle_shadow_invlpg_64(info, vaddr);
	default:
	    PrintError("Invalid CPU mode: %s\n", v3_cpu_mode_to_str(v3_get_vm_cpu_mode(info)));
	    return -1;
    }
}




static struct shadow_page_data * create_new_shadow_pt(struct guest_info * info) {
    struct shadow_page_state * state = &(info->shdw_pg_state);
    v3_reg_t cur_cr3 = info->ctrl_regs.cr3;
    struct shadow_page_data * page_tail = NULL;
    addr_t shdw_page = 0;

    if (!list_empty(&(state->page_list))) {
	page_tail = list_tail_entry(&(state->page_list), struct shadow_page_data, page_list_node);
    
	if (page_tail->cr3 != cur_cr3) {
	    PrintDebug("Reusing old shadow Page: %p (cur_CR3=%p)(page_cr3=%p) \n",
		       (void *)(addr_t)page_tail->page_pa, 
		       (void *)(addr_t)cur_cr3, 
		       (void *)(addr_t)(page_tail->cr3));

	    list_move(&(page_tail->page_list_node), &(state->page_list));

	    memset(V3_VAddr((void *)(page_tail->page_pa)), 0, PAGE_SIZE_4KB);


	    return page_tail;
	}
    }

    // else  

    page_tail = (struct shadow_page_data *)V3_Malloc(sizeof(struct shadow_page_data));
    page_tail->page_pa = (addr_t)V3_AllocPages(1);

    PrintDebug("Allocating new shadow Page: %p (cur_cr3=%p)\n", 
	       (void *)(addr_t)page_tail->page_pa, 
	       (void *)(addr_t)cur_cr3);

    page_tail->cr3 = cur_cr3;
    list_add(&(page_tail->page_list_node), &(state->page_list));

    shdw_page = (addr_t)V3_VAddr((void *)(page_tail->page_pa));
    memset((void *)shdw_page, 0, PAGE_SIZE_4KB);

    return page_tail;
}


static void inject_guest_pf(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
    if (info->enable_profiler) {
	info->profiler.guest_pf_cnt++;
    }

    info->ctrl_regs.cr2 = fault_addr;
    v3_raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);
}


static int is_guest_pf(pt_access_status_t guest_access, pt_access_status_t shadow_access) {
    /* basically the reasoning is that there can be multiple reasons for a page fault:
       If there is a permissions failure for a page present in the guest _BUT_
       the reason for the fault was that the page is not present in the shadow,
       _THEN_ we have to map the shadow page in and reexecute, this will generate
       a permissions fault which is _THEN_ valid to send to the guest
       _UNLESS_ both the guest and shadow have marked the page as not present

       whew...
    */
    if (guest_access != PT_ACCESS_OK) {
	// Guest Access Error

	if ((shadow_access != PT_ACCESS_NOT_PRESENT) &&
	    (guest_access != PT_ACCESS_NOT_PRESENT)) {
	    // aka (guest permission error)
	    return 1;
	}

	if ((shadow_access == PT_ACCESS_NOT_PRESENT) &&
	    (guest_access == PT_ACCESS_NOT_PRESENT)) {
	    // Page tables completely blank, handle guest first
	    return 1;
	}

	// Otherwise we'll handle the guest fault later...?
    }

    return 0;
}


