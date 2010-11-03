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


#include <palacios/vmm_msr.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest.h>


void v3_init_msr_map(struct v3_vm_info * vm) {
    struct v3_msr_map * msr_map  = &(vm->msr_map);

    PrintDebug("Initializing MSR map.\n");

    INIT_LIST_HEAD(&(msr_map->hook_list));
    msr_map->num_hooks = 0;

    msr_map->arch_data = NULL;
    msr_map->update_map = NULL;
}

int v3_handle_msr_write(struct guest_info * info) {
    uint_t msr_num = info->vm_regs.rcx;
    struct v3_msr msr_val;
    struct v3_msr_hook * hook = NULL;

    PrintDebug("MSR write for msr 0x%x\n", msr_num);

    hook = v3_get_msr_hook(info->vm_info, msr_num);

    if (!hook) {
        PrintError("Hook for MSR write %d not found\n", msr_num);
        return -1;
    }

    msr_val.value = 0;
    msr_val.lo = info->vm_regs.rax;
    msr_val.hi = info->vm_regs.rdx;

    if (hook->write(info, msr_num, msr_val, hook->priv_data) == -1) {
        PrintError("Error in MSR hook Write\n");
        return -1;
    }

    info->rip += 2;

    return 0;
}


int v3_handle_msr_read(struct guest_info * info) {
    uint_t msr_num = info->vm_regs.rcx;
    struct v3_msr msr_val;
    struct v3_msr_hook * hook = NULL;

    hook = v3_get_msr_hook(info->vm_info, msr_num);

    if (!hook) {
        PrintError("Hook for MSR read %d not found\n", msr_num);
        return -1;
    }

    msr_val.value = 0;

    if (hook->read(info, msr_num, &msr_val, hook->priv_data) == -1) {
        PrintError("Error in MSR hook Read\n");
        return -1;
    }

    info->vm_regs.rax = msr_val.lo;
    info->vm_regs.rdx = msr_val.hi;

    info->rip += 2;
    return 0;
}

int v3_hook_msr(struct v3_vm_info * vm, uint_t msr, 
		int (*read)(struct guest_info * core, uint_t msr, struct v3_msr * dst, void * priv_data),
		int (*write)(struct guest_info * core, uint_t msr, struct v3_msr src, void * priv_data),
		void * priv_data) {

    struct v3_msr_map * msr_map = &(vm->msr_map);
    struct v3_msr_hook * hook = NULL;

    hook = (struct v3_msr_hook *)V3_Malloc(sizeof(struct v3_msr_hook));

    if (hook == NULL) {
	PrintError("Could not allocate msr hook for MSR %d\n", msr);
	return -1;
    }

    hook->read = read;
    hook->write = write;
    hook->msr = msr;
    hook->priv_data = priv_data;

    msr_map->num_hooks++;

    list_add(&(hook->link), &(msr_map->hook_list));

    if (msr_map->update_map) {
	msr_map->update_map(vm, msr, 
			    (read == NULL) ? 0 : 1,
			    (write == NULL) ? 0 : 1);
    }

    return 0;
}


int v3_unhook_msr(struct v3_vm_info * vm, uint_t msr) {
    PrintError("Unhooking MSRs currently not supported\n");
    return -1;
}



struct v3_msr_hook * v3_get_msr_hook(struct v3_vm_info * vm, uint_t msr) {
    struct v3_msr_map * msr_map = &(vm->msr_map);
    struct v3_msr_hook * hook = NULL;

    list_for_each_entry(hook, &(msr_map->hook_list), link) {
	if (hook->msr == msr) {
	    return hook;
	}
    }

    return NULL;
}


void v3_refresh_msr_map(struct v3_vm_info * vm) {
    struct v3_msr_map * msr_map = &(vm->msr_map);
    struct v3_msr_hook * hook = NULL;

    if (msr_map->update_map == NULL) {
	PrintError("Trying to refresh an MSR map with no backend\n");
	return;
    }

    list_for_each_entry(hook, &(msr_map->hook_list), link) {
	PrintDebug("updating MSR map for msr %d\n", hook->msr);
	msr_map->update_map(vm, hook->msr, 	
			    (hook->read == NULL) ? 0 : 1,
			    (hook->write == NULL) ? 0 : 1);
    }
}

void v3_print_msr_map(struct v3_vm_info * vm) {
    struct v3_msr_map * msr_map = &(vm->msr_map);
    struct v3_msr_hook * hook = NULL;

    list_for_each_entry(hook, &(msr_map->hook_list), link) {
	V3_Print("MSR HOOK (MSR=0x%x) (read=0x%p) (write=0x%p)\n",
		   hook->msr, hook->read, hook->write);
    }
}
