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

#include <palacios/vmm_symspy.h>
#include <palacios/vmm.h>
#include <palacios/vmm_msr.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_sprintf.h>

#define SYMSPY_GLOBAL_MSR 0x534
#define SYMSPY_LOCAL_MSR 0x535


static int symspy_msr_read(struct guest_info * core, uint_t msr, 
		    struct v3_msr * dst, void * priv_data) {
    struct v3_symspy_global_state * global_state = &(core->vm_info->sym_vm_state.symspy_state);
    struct v3_symspy_local_state * local_state = &(core->sym_core_state.symspy_state);

    switch (msr) {
	case SYMSPY_GLOBAL_MSR:
	    dst->value = global_state->global_guest_pa;
	    break;
	case SYMSPY_LOCAL_MSR:
	    dst->value = local_state->local_guest_pa;
	    break;
	default:
	    return -1;
    }

    return 0;
}


static int symspy_msr_write(struct guest_info * core, uint_t msr, struct v3_msr src, void * priv_data) {

    if (msr == SYMSPY_GLOBAL_MSR) {
	struct v3_symspy_global_state * global_state = &(core->vm_info->sym_vm_state.symspy_state);

	PrintDebug("Symbiotic Glbal MSR write for page %p\n", (void *)(addr_t)src.value);

	if (global_state->active == 1) {
	    // unmap page
	    struct v3_mem_region * old_reg = v3_get_mem_region(core->vm_info, core->vcpu_id, 
								     (addr_t)global_state->global_guest_pa);

	    if (old_reg == NULL) {
		PrintError("Could not find previously active symbiotic page (%p)\n", 
			   (void *)(addr_t)global_state->global_guest_pa);
		return -1;
	    }

	    v3_delete_mem_region(core->vm_info, old_reg);
	}

	global_state->global_guest_pa = src.value;
	global_state->global_guest_pa &= ~0xfffLL;

	global_state->active = 1;

	// map page
	v3_add_shadow_mem(core->vm_info, V3_MEM_CORE_ANY, (addr_t)global_state->global_guest_pa, 
			  (addr_t)(global_state->global_guest_pa + PAGE_SIZE_4KB - 1), 
			  global_state->global_page_pa);
    } else if (msr == SYMSPY_LOCAL_MSR) {
	struct v3_symspy_local_state * local_state = &(core->sym_core_state.symspy_state);

	PrintDebug("Symbiotic Local MSR write for page %p\n", (void *)(addr_t)src.value);

	if (local_state->active == 1) {
	    // unmap page
	    struct v3_mem_region * old_reg = v3_get_mem_region(core->vm_info, core->vcpu_id,
								     (addr_t)local_state->local_guest_pa);

	    if (old_reg == NULL) {
		PrintError("Could not find previously active symbiotic page (%p)\n", 
			   (void *)(addr_t)local_state->local_guest_pa);
		return -1;
	    }

	    v3_delete_mem_region(core->vm_info, old_reg);
	}

	local_state->local_guest_pa = src.value;
	local_state->local_guest_pa &= ~0xfffLL;

	local_state->active = 1;

	// map page
	v3_add_shadow_mem(core->vm_info, core->vcpu_id, (addr_t)local_state->local_guest_pa, 
			  (addr_t)(local_state->local_guest_pa + PAGE_SIZE_4KB - 1), 
			  local_state->local_page_pa);
    } else {
	PrintError("Invalid Symbiotic MSR write (0x%x)\n", msr);
	return -1;
    }

    return 0;
}



int v3_init_symspy_vm(struct v3_vm_info * vm, struct v3_symspy_global_state * state) {

    state->global_page_pa = (addr_t)V3_AllocPages(1);
    state->sym_page = (struct v3_symspy_global_page *)V3_VAddr((void *)state->global_page_pa);
    memset(state->sym_page, 0, PAGE_SIZE_4KB);

    memcpy(&(state->sym_page->magic), "V3V", 3);

    v3_hook_msr(vm, SYMSPY_LOCAL_MSR, symspy_msr_read, symspy_msr_write, NULL);
    v3_hook_msr(vm, SYMSPY_GLOBAL_MSR, symspy_msr_read, symspy_msr_write, NULL);
    
    return 0;
}



int v3_init_symspy_core(struct guest_info * core, struct v3_symspy_local_state * state) {
    state->local_page_pa = (addr_t)V3_AllocPages(1);
    state->local_page = (struct v3_symspy_local_page *)V3_VAddr((void *)state->local_page_pa);
    memset(state->local_page, 0, PAGE_SIZE_4KB);

    snprintf((uint8_t *)&(state->local_page->magic), 8, "V3V.%d", core->vcpu_id);

    return 0;
}



int v3_sym_map_pci_passthrough(struct v3_vm_info * vm, uint_t bus, uint_t dev, uint_t fn) {
    struct v3_symspy_global_state * global_state = &(vm->sym_vm_state.symspy_state);
    uint_t dev_index = (bus << 8) + (dev << 3) + fn;
    uint_t major = dev_index / 8;
    uint_t minor = dev_index % 8;

    if (bus > 3) {
	PrintError("Invalid PCI bus %d\n", bus);
	return -1;
    }

    PrintDebug("Setting passthrough pci map for index=%d\n", dev_index);

    global_state->sym_page->pci_pt_map[major] |= 0x1 << minor;

    PrintDebug("pt_map entry=%x\n",   global_state->sym_page->pci_pt_map[major]);

    PrintDebug("pt map vmm addr=%p\n", global_state->sym_page->pci_pt_map);

    return 0;
}

int v3_sym_unmap_pci_passthrough(struct v3_vm_info * vm, uint_t bus, uint_t dev, uint_t fn) {
    struct v3_symspy_global_state * global_state = &(vm->sym_vm_state.symspy_state);
    uint_t dev_index = (bus << 8) + (dev << 3) + fn;
    uint_t major = dev_index / 8;
    uint_t minor = dev_index % 8;

    if (bus > 3) {
	PrintError("Invalid PCI bus %d\n", bus);
	return -1;
    }

    global_state->sym_page->pci_pt_map[major] &= ~(0x1 << minor);

    return 0;
}


struct v3_symspy_global_page * v3_sym_get_symspy_vm(struct v3_vm_info * vm) {
    return vm->sym_vm_state.symspy_state.sym_page;
}

struct v3_symspy_local_page * v3_sym_get_symspy_core(struct guest_info * core) {
    return core->sym_core_state.symspy_state.local_page;
}
