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
#include <palacios/vmm_msr.h>
#include <palacios/vmm_mem.h>

#define SYM_MSR_NUM 0x535




static int msr_read(uint_t msr, struct v3_msr * dst, void * priv_data) {
    struct guest_info * info = (struct guest_info *)priv_data;
    struct v3_sym_state * state = &(info->sym_state);

    dst->value = state->guest_pg_addr;

    return 0;
}

static int msr_write(uint_t msr, struct v3_msr src, void * priv_data) {
    struct guest_info * info = (struct guest_info *)priv_data;
    struct v3_sym_state * state = &(info->sym_state);

    if (state->active == 1) {
	// unmap page
	struct v3_shadow_region * old_reg = v3_get_shadow_region(info, (addr_t)state->guest_pg_addr);

	if (old_reg == NULL) {
	    PrintError("Could not find previously active symbiotic page (%p)\n", (void *)state->guest_pg_addr);
	    return -1;
	}

	v3_delete_shadow_region(info, old_reg);
    }

    state->guest_pg_addr = src.value;
    state->guest_pg_addr &= ~0xfffLL;

    state->active = 1;

    // map page
    v3_add_shadow_mem(info, (addr_t)state->guest_pg_addr, 
		      (addr_t)(state->guest_pg_addr + PAGE_SIZE_4KB - 1), 
		      state->sym_page_pa);

    return 0;
}



int v3_init_sym_iface(struct guest_info * info) {
    struct v3_sym_state * state = &(info->sym_state);
    
    memset(state, 0, sizeof(struct v3_sym_state));

    PrintDebug("Allocating symbiotic page\n");
    state->sym_page_pa = (addr_t)V3_AllocPages(1);
    state->sym_page = (struct v3_sym_interface *)V3_VAddr((void *)state->sym_page_pa);

    PrintDebug("Clearing symbiotic page\n");
    memset(state->sym_page, 0, PAGE_SIZE_4KB);

    PrintDebug("hooking MSR\n");
    v3_hook_msr(info, SYM_MSR_NUM, msr_read, msr_write, info);

    PrintDebug("Done\n");
    return 0;
}

int v3_sym_map_pci_passthrough(struct guest_info * info, uint_t bus, uint_t dev, uint_t fn) {
    struct v3_sym_state * state = &(info->sym_state);
    uint_t dev_index = (bus << 16) + (dev << 8) + fn;
    uint_t major = dev_index / 8;
    uint_t minor = dev_index % 8;

    state->sym_page->pci_pt_map[major] |= 0x1 << minor;

    return 0;
}

int v3_sym_unmap_pci_passthrough(struct guest_info * info, uint_t bus, uint_t dev, uint_t fn) {
    struct v3_sym_state * state = &(info->sym_state);
    uint_t dev_index = (bus << 16) + (dev << 8) + fn;
    uint_t major = dev_index / 8;
    uint_t minor = dev_index % 8;

    state->sym_page->pci_pt_map[major] &= ~(0x1 << minor);

    return 0;
}
