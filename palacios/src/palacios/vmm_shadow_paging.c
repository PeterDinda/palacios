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




#ifdef CONFIG_SHADOW_PAGING_TELEMETRY
#include <palacios/vmm_telemetry.h>
#endif

#ifdef CONFIG_SYMBIOTIC_SWAP
#include <palacios/vmm_sym_swap.h>
#endif

#ifndef CONFIG_DEBUG_SHADOW_PAGING
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif



static struct hashtable * master_shdw_pg_table = NULL;

static uint_t shdw_pg_hash_fn(addr_t key) {
    char * name = (char *)key;
    return v3_hash_buffer((uint8_t *)name, strlen(name));
}

static int shdw_pg_eq_fn(addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;

    return (strcmp(name1, name2) == 0);
}


int V3_init_shdw_paging() {
    extern struct v3_shdw_pg_impl * __start__v3_shdw_pg_impls[];
    extern struct v3_shdw_pg_impl * __stop__v3_shdw_pg_impls[];
    struct v3_shdw_pg_impl ** tmp_impl = __start__v3_shdw_pg_impls;
    int i = 0;

    master_shdw_pg_table = v3_create_htable(0, shdw_pg_hash_fn, shdw_pg_eq_fn);


    while (tmp_impl != __stop__v3_shdw_pg_impls) {
	V3_Print("Registering Shadow Paging Impl (%s)\n", (*tmp_impl)->name);

	if (v3_htable_search(master_shdw_pg_table, (addr_t)((*tmp_impl)->name))) {
	    PrintError("Multiple instances of shadow paging impl (%s)\n", (*tmp_impl)->name);
	    return -1;
	}

	if (v3_htable_insert(master_shdw_pg_table, 
			     (addr_t)((*tmp_impl)->name),
			     (addr_t)(*tmp_impl)) == 0) {
	    PrintError("Could not register shadow paging impl (%s)\n", (*tmp_impl)->name);
	    return -1;
	}

	tmp_impl = &(__start__v3_shdw_pg_impls[++i]);
    }

    return 0;
}



/*** 
 ***  There be dragons
 ***/


#ifdef CONFIG_SHADOW_PAGING_TELEMETRY
static void telemetry_cb(struct v3_vm_info * vm, void * private_data, char * hdr) {
    int i = 0;
    for (i = 0; i < vm->num_cores; i++) {
	struct guest_info * core = &(vm->cores[i]);

	V3_Print("%s Guest Page faults: %d\n", hdr, core->shdw_pg_state.guest_faults);
    }
}
#endif



int v3_init_shdw_pg_state(struct guest_info * core) {
    struct v3_shdw_pg_state * state = &(core->shdw_pg_state);
    struct v3_shdw_pg_impl * impl = core->vm_info->shdw_impl.current_impl;
  

    state->guest_cr3 = 0;
    state->guest_cr0 = 0;
    state->guest_efer.value = 0x0LL;


    if (impl->local_init(core) == -1) {
	PrintError("Error in Shadow paging local initialization (%s)\n", impl->name);
	return -1;
    }


#ifdef CONFIG_SHADOW_PAGING_TELEMETRY
    v3_add_telemetry_cb(core->vm_info, telemetry_cb, NULL);
#endif
  
    return 0;
}



int v3_init_shdw_impl(struct v3_vm_info * vm) {
    struct v3_shdw_impl_state * impl_state = &(vm->shdw_impl);
    v3_cfg_tree_t * pg_cfg = v3_cfg_subtree(vm->cfg_data->cfg, "paging");
    char * pg_mode = v3_cfg_val(pg_cfg, "mode");
    char * pg_strat = v3_cfg_val(pg_cfg, "strategy");
    struct v3_shdw_pg_impl * impl = NULL;
   
    PrintDebug("Checking if shadow paging requested.\n");
    if (pg_mode && (strcasecmp(pg_mode, "nested") == 0)) {
	PrintDebug("Nested paging specified - not initializing shadow paging.\n");
	return 0;
    }
	
    V3_Print("Initialization of Shadow Paging implementation\n");

    impl = (struct v3_shdw_pg_impl *)v3_htable_search(master_shdw_pg_table, (addr_t)pg_strat);

    if (impl == NULL) {
	PrintError("Could not find shadow paging impl (%s)\n", pg_strat);
	return -1;
    }
   
    impl_state->current_impl = impl;

    if (impl->init(vm, pg_cfg) == -1) {
	PrintError("Could not initialize Shadow paging implemenation (%s)\n", impl->name);
	return -1;
    }

    


    return 0;
}


// Reads the guest CR3 register
// creates new shadow page tables
// updates the shadow CR3 register to point to the new pts
int v3_activate_shadow_pt(struct guest_info * core) {
    struct v3_shdw_impl_state * state = &(core->vm_info->shdw_impl);
    struct v3_shdw_pg_impl * impl = state->current_impl;
    return impl->activate_shdw_pt(core);
}



// This must flush any caches
// and reset the cr3 value to the correct value
int v3_invalidate_shadow_pts(struct guest_info * core) {
    struct v3_shdw_impl_state * state = &(core->vm_info->shdw_impl);
    struct v3_shdw_pg_impl * impl = state->current_impl;
    return impl->invalidate_shdw_pt(core);
}


int v3_handle_shadow_pagefault(struct guest_info * core, addr_t fault_addr, pf_error_t error_code) {
  
    if (v3_get_vm_mem_mode(core) == PHYSICAL_MEM) {
	// If paging is not turned on we need to handle the special cases
	return v3_handle_passthrough_pagefault(core, fault_addr, error_code);
    } else if (v3_get_vm_mem_mode(core) == VIRTUAL_MEM) {
	struct v3_shdw_impl_state * state = &(core->vm_info->shdw_impl);
	struct v3_shdw_pg_impl * impl = state->current_impl;

	return impl->handle_pagefault(core, fault_addr, error_code);
    } else {
	PrintError("Invalid Memory mode\n");
	return -1;
    }
}


int v3_handle_shadow_invlpg(struct guest_info * core) {
    uchar_t instr[15];
    struct x86_instr dec_instr;
    int ret = 0;
    addr_t vaddr = 0;

    if (v3_get_vm_mem_mode(core) != VIRTUAL_MEM) {
	// Paging must be turned on...
	// should handle with some sort of fault I think
	PrintError("ERROR: INVLPG called in non paged mode\n");
	return -1;
    }

    if (v3_get_vm_mem_mode(core) == PHYSICAL_MEM) { 
	ret = v3_read_gpa_memory(core, get_addr_linear(core, core->rip, &(core->segments.cs)), 15, instr);
    } else { 
	ret = v3_read_gva_memory(core, get_addr_linear(core, core->rip, &(core->segments.cs)), 15, instr);
    }

    if (ret == -1) {
	PrintError("Could not read instruction into buffer\n");
	return -1;
    }

    if (v3_decode(core, (addr_t)instr, &dec_instr) == -1) {
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

    core->rip += dec_instr.instr_length;

    {
	struct v3_shdw_impl_state * state = &(core->vm_info->shdw_impl);
	struct v3_shdw_pg_impl * impl = state->current_impl;

	return impl->handle_invlpg(core, vaddr);
    }
}






int v3_inject_guest_pf(struct guest_info * core, addr_t fault_addr, pf_error_t error_code) {
    core->ctrl_regs.cr2 = fault_addr;

#ifdef CONFIG_SHADOW_PAGING_TELEMETRY
    core->shdw_pg_state.guest_faults++;
#endif

    return v3_raise_exception_with_error(core, PF_EXCEPTION, *(uint_t *)&error_code);
}


int v3_is_guest_pf(pt_access_status_t guest_access, pt_access_status_t shadow_access) {
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

	/*
	  if ((shadow_access == PT_ACCESS_NOT_PRESENT) &&
	  (guest_access == PT_ACCESS_NOT_PRESENT)) {
	  // Page tables completely blank, handle guest first
	  return 1;
	  }
	*/

	if (guest_access == PT_ACCESS_NOT_PRESENT) {
	    // Page tables completely blank, handle guest first
	    return 1;
	}
	
	// Otherwise we'll handle the guest fault later...?
    }

    return 0;
}


