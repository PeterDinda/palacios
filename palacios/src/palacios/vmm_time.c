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

#include <palacios/vmm_time.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest.h>

#ifndef CONFIG_DEBUG_TIME
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

/* Overview 
 *
 * Time handling in VMMs is challenging, and Palacios uses the highest 
 * resolution, lowest overhead timer on modern CPUs that it can - the 
 * processor timestamp counter (TSC). Note that on somewhat old processors
 * this can be problematic; in particular, older AMD processors did not 
 * have a constant rate timestamp counter in the face of power management
 * events. However, the latest Intel and AMD CPUs all do (should...) have a 
 * constant rate TSC, and Palacios relies on this fact.
 * 
 * Basically, Palacios keeps track of three quantities as it runs to manage
 * the passage of time:
 * (1) The host timestamp counter - read directly from HW and never written
 * (2) A monotonic guest timestamp counter used to measure the progression of
 *     time in the guest. This is computed as a multipler/offset from (1) above
 * (3) The actual guest timestamp counter (which can be written by
 *     writing to the guest TSC MSR - MSR 0x10) from the monotonic guest TSC.
 *     This is also computed as an offset from (2) above when the TSC and
 *     this offset is updated when the TSC MSR is written.
 *
 * Because all other devices are slaved off of the passage of time in the guest,
 * it is (2) above that drives the firing of other timers in the guest, 
 * including timer devices such as the Programmable Interrupt Timer (PIT).
 *
 *  
 *
 */


static int handle_cpufreq_hcall(struct guest_info * info, uint_t hcall_id, void * priv_data) {
    struct vm_time * time_state = &(info->time_state);

    info->vm_regs.rbx = time_state->guest_cpu_freq;

    PrintDebug("Guest request cpu frequency: return %ld\n", (long)info->vm_regs.rbx);
    
    return 0;
}



int v3_start_time(struct guest_info * info) {
    /* We start running with guest_time == host_time */
    uint64_t t = v3_get_host_time(&info->time_state); 

    PrintDebug("Starting initial guest time as %llu\n", t);
    info->time_state.last_update = t;
    info->time_state.initial_time = t;
    info->yield_start_cycle = t;
    return 0;
}

// If the guest is supposed to run slower than the host, yield out until
// the host time is appropriately far along;
int v3_adjust_time(struct guest_info * info) {
    struct vm_time * time_state = &(info->time_state);
    if (time_state->host_cpu_freq == time_state->guest_cpu_freq) {
	time_state->guest_host_offset = 0;
    } else {
	uint64_t guest_time, host_time, target_host_time;
	guest_time = v3_get_guest_time(time_state);
	host_time = v3_get_host_time(time_state);
	target_host_time = (host_time - time_state->initial_time) *
	    time_state->host_cpu_freq / time_state->guest_cpu_freq;
	while (host_time < target_host_time) {
	    v3_yield(info);
	    host_time = v3_get_host_time(time_state);
	}
	time_state->guest_host_offset = guest_time - host_time;

    }
    return 0;
}

int v3_add_timer(struct guest_info * info, struct vm_timer_ops * ops, 
	     void * private_data) {
    struct vm_timer * timer = NULL;
    timer = (struct vm_timer *)V3_Malloc(sizeof(struct vm_timer));
    V3_ASSERT(timer != NULL);

    timer->ops = ops;
    timer->private_data = private_data;

    list_add(&(timer->timer_link), &(info->time_state.timers));
    info->time_state.num_timers++;

    return 0;
}

int v3_remove_timer(struct guest_info * info, struct vm_timer * timer) {
    list_del(&(timer->timer_link));
    info->time_state.num_timers--;

    V3_Free(timer);
    return 0;
}

void v3_update_timers(struct guest_info * info) {
    struct vm_timer * tmp_timer;
    uint64_t old_time = info->time_state.last_update;
    uint64_t cycles;

    info->time_state.last_update = v3_get_guest_time(&info->time_state);
    cycles = info->time_state.last_update - old_time;

    list_for_each_entry(tmp_timer, &(info->time_state.timers), timer_link) {
	tmp_timer->ops->update_timer(info, cycles, info->time_state.guest_cpu_freq, tmp_timer->private_data);
    }
}

/* 
 * Handle full virtualization of the time stamp counter.  As noted
 * above, we don't store the actual value of the TSC, only the guest's
 * offset from the host TSC. If the guest write's the to TSC, we handle
 * this by changing that offset.
 */ 

int v3_rdtsc(struct guest_info * info) {
    uint64_t tscval = v3_get_guest_tsc(&info->time_state);
    info->vm_regs.rdx = tscval >> 32;
    info->vm_regs.rax = tscval & 0xffffffffLL;
    return 0;
}

int v3_handle_rdtsc(struct guest_info * info) {
    v3_rdtsc(info);
    
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
    if (ret) return ret;
    info->vm_regs.rcx = info->vm_regs.rax;

    /* Now do the TSC half of the instruction, which may hit the normal 
     * TSC hook if it exists */
    ret = v3_rdtsc(info);
    if (ret) return ret;
    
    return 0;
}


int v3_handle_rdtscp(struct guest_info * info) {

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

static int init_vm_time(struct v3_vm_info *vm_info) {
    int ret;

    PrintDebug("Installing TSC MSR hook.\n");
    ret = v3_hook_msr(vm_info, TSC_MSR, 
		      tsc_msr_read_hook, tsc_msr_write_hook, NULL);

    PrintDebug("Installing TSC_AUX MSR hook.\n");
    if (ret) return ret;
    ret = v3_hook_msr(vm_info, TSC_AUX_MSR, tsc_aux_msr_read_hook, 
		      tsc_aux_msr_write_hook, NULL);
    if (ret) return ret;

    PrintDebug("Registering TIME_CPUFREQ hypercall.\n");
    ret = v3_register_hypercall(vm_info, TIME_CPUFREQ_HCALL, 
				handle_cpufreq_hcall, NULL);
    return ret;
}

void v3_init_time(struct guest_info * info) {
    struct vm_time * time_state = &(info->time_state);
    static int one_time = 0;

    time_state->host_cpu_freq = V3_CPU_KHZ();
    time_state->guest_cpu_freq = V3_CPU_KHZ();
 
    time_state->initial_time = 0;
    time_state->last_update = 0;
    time_state->guest_host_offset = 0;
    time_state->tsc_guest_offset = 0;

    INIT_LIST_HEAD(&(time_state->timers));
    time_state->num_timers = 0;
    
    time_state->tsc_aux.lo = 0;
    time_state->tsc_aux.hi = 0;

    if (!one_time) {
	init_vm_time(info->vm_info);
	one_time = 1;
    }
}






