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
 *         Patrick G. Bridges <bridges@cs.unm.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_time.h>
#include <palacios/vm_guest.h>


// Functions for handling exits on the TSC when fully virtualizing 
// the timestamp counter.
#define TSC_MSR     0x10
#define TSC_AUX_MSR 0xC0000103

int v3_handle_rdtscp(struct guest_info *info);
int v3_handle_rdtsc(struct guest_info *info);


struct vtsc_state {

    struct v3_msr tsc_aux;     // Auxilliary MSR for RDTSCP

};



/* 
 * Handle full virtualization of the time stamp counter.  As noted
 * above, we don't store the actual value of the TSC, only the guest's
 * offset from monotonic guest's time. If the guest writes to the TSC, we
 * handle this by changing that offset.
 *
 * Possible TODO: Proper hooking of TSC read/writes?
 */ 

static int rdtsc(struct guest_info * info) {
    uint64_t tscval = v3_get_guest_tsc(&info->time_state);

    info->vm_regs.rdx = tscval >> 32;
    info->vm_regs.rax = tscval & 0xffffffffLL;

    return 0;
}

int v3_handle_rdtsc(struct guest_info * info) {
    rdtsc(info);
    
    info->vm_regs.rax &= 0x00000000ffffffffLL;
    info->vm_regs.rdx &= 0x00000000ffffffffLL;

    info->rip += 2;
    
    return 0;
}

int v3_rdtscp(struct guest_info * info) {
    int ret;
    /* First get the MSR value that we need. It's safe to futz with
     * ra/c/dx here since they're modified by this instruction anyway. */
    info->vm_regs.rcx = TSC_AUX_MSR; 
    ret = v3_handle_msr_read(info);

    if (ret != 0) {
	return ret;
    }

    info->vm_regs.rcx = info->vm_regs.rax;

    /* Now do the TSC half of the instruction */
    ret = v3_rdtsc(info);

    if (ret != 0) {
	return ret;
    }

    return 0;
}


int v3_handle_rdtscp(struct guest_info * info) {
  PrintDebug("Handling virtual RDTSCP call.\n");

    v3_rdtscp(info);

    info->vm_regs.rax &= 0x00000000ffffffffLL;
    info->vm_regs.rcx &= 0x00000000ffffffffLL;
    info->vm_regs.rdx &= 0x00000000ffffffffLL;

    info->rip += 3;
    
    return 0;
}




static int tsc_aux_msr_read_hook(struct guest_info *info, uint_t msr_num, 
				 struct v3_msr *msr_val, void *priv) {
    struct vm_time * time_state = &(info->time_state);

    V3_ASSERT(msr_num == TSC_AUX_MSR);

    msr_val->lo = time_state->tsc_aux.lo;
    msr_val->hi = time_state->tsc_aux.hi;

    return 0;
}


static int tsc_aux_msr_write_hook(struct guest_info *info, uint_t msr_num, 
			      struct v3_msr msr_val, void *priv) {
    struct vm_time * time_state = &(info->time_state);

    V3_ASSERT(msr_num == TSC_AUX_MSR);

    time_state->tsc_aux.lo = msr_val.lo;
    time_state->tsc_aux.hi = msr_val.hi;

    return 0;
}


static int tsc_msr_read_hook(struct guest_info *info, uint_t msr_num,
			     struct v3_msr *msr_val, void *priv) {
    uint64_t time = v3_get_guest_tsc(&info->time_state);

    V3_ASSERT(msr_num == TSC_MSR);

    msr_val->hi = time >> 32;
    msr_val->lo = time & 0xffffffffLL;
    
    return 0;
}


static int tsc_msr_write_hook(struct guest_info *info, uint_t msr_num,
			     struct v3_msr msr_val, void *priv) {
    struct vm_time * time_state = &(info->time_state);
    uint64_t guest_time, new_tsc;

    V3_ASSERT(msr_num == TSC_MSR);

    new_tsc = (((uint64_t)msr_val.hi) << 32) | (uint64_t)msr_val.lo;
    guest_time = v3_get_guest_time(time_state);
    time_state->tsc_guest_offset = (sint64_t)new_tsc - (sint64_t)guest_time; 

    return 0;
}


static int deinit() {
    v3_unhook_msr(vm, TSC_MSR);
    v3_unhook_msr(vm, TSC_AUX_MSR);
}


static int init() {

    time_state->tsc_aux.lo = 0;
    time_state->tsc_aux.hi = 0;



    PrintDebug("Installing TSC MSR hook.\n");
    ret = v3_hook_msr(vm, TSC_MSR, 
		      tsc_msr_read_hook, tsc_msr_write_hook, NULL);

    if (ret != 0) {
	return ret;
    }

    PrintDebug("Installing TSC_AUX MSR hook.\n");
    ret = v3_hook_msr(vm, TSC_AUX_MSR, tsc_aux_msr_read_hook, 
		      tsc_aux_msr_write_hook, NULL);

    if (ret != 0) {
	return ret;
    }
}
