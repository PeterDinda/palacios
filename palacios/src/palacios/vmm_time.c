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

#ifndef V3_CONFIG_DEBUG_TIME
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
 *     time in the guest. This is computed using an offsets from (1) above.
 * (3) The actual guest timestamp counter (which can be written by
 *     writing to the guest TSC MSR - MSR 0x10) from the monotonic guest TSC.
 *     This is also computed as an offset from (2) above when the TSC and
 *     this offset is updated when the TSC MSR is written.
 *
 * The value used to offset the guest TSC from the host TSC is the *sum* of all
 * of these offsets (2 and 3) above
 * 
 * Because all other devices are slaved off of the passage of time in the guest,
 * it is (2) above that drives the firing of other timers in the guest, 
 * including timer devices such as the Programmable Interrupt Timer (PIT).
 *
 * Future additions:
 * (1) Add support for temporarily skewing guest time off of where it should
 *     be to support slack simulation of guests. The idea is that simulators
 *     set this skew to be the difference between how much time passed for a 
 *     simulated feature and a real implementation of that feature, making 
 *     pass at a different rate from real time on this core. The VMM will then
 *     attempt to move this skew back towards 0 subject to resolution/accuracy
 *     constraints from various system timers.
 *   
 *     The main effort in doing this will be to get accuracy/resolution 
 *     information from each local timer and to use this to bound how much skew
 *     is removed on each exit.
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

    info->time_state.enter_time = 0;
    info->time_state.exit_time = t; 
    info->time_state.last_update = t;
    info->time_state.initial_time = t;
    info->yield_start_cycle = t;

    return 0;
}

int v3_offset_time( struct guest_info * info, sint64_t offset )
{
    struct vm_time * time_state = &(info->time_state);
    PrintDebug("Adding additional offset of %lld to guest time.\n", offset);
    time_state->guest_host_offset += offset;
    return 0;
}

#ifdef V3_CONFIG_TIME_DILATION
static uint64_t compute_target_host_time(struct guest_info * info, uint64_t guest_time)
{
    struct vm_time * time_state = &(info->time_state);
    uint64_t guest_elapsed, desired_elapsed;
    
    guest_elapsed = (guest_time - time_state->initial_time);
    desired_elapsed = (guest_elapsed * time_state->host_cpu_freq) / time_state->guest_cpu_freq;
    return time_state->initial_time + desired_elapsed;
}

static uint64_t compute_target_guest_time(struct guest_info *info)
{
    struct vm_time * time_state = &(info->time_state);
    uint64_t host_elapsed, desired_elapsed;

    host_elapsed = v3_get_host_time(time_state) - time_state->initial_time;
    desired_elapsed = (host_elapsed * time_state->guest_cpu_freq) / time_state->host_cpu_freq;

    return time_state->initial_time + desired_elapsed;

} 

/* Yield time in the host to deal with a guest that wants to run slower than 
 * the native host cycle frequency */
static int yield_host_time(struct guest_info * info) {
    struct vm_time * time_state = &(info->time_state);
    uint64_t host_time, target_host_time;
    uint64_t guest_time, old_guest_time;

    /* Now, let the host run while the guest is stopped to make the two
     * sync up. Note that this doesn't assume that guest time is stopped;
     * the offsetting in the next step will change add an offset to guest
     * time to account for the time paused even if the geust isn't 
     * usually paused in the VMM. */
    host_time = v3_get_host_time(time_state);
    old_guest_time = v3_compute_guest_time(time_state, host_time);
    target_host_time = compute_target_host_time(info, old_guest_time);

    while (target_host_time > host_time) {
	v3_yield(info);
	host_time = v3_get_host_time(time_state);
    }

    guest_time = v3_compute_guest_time(time_state, host_time);

    /* We do *not* assume the guest timer was paused in the VM. If it was
     * this offseting is 0. If it wasn't, we need this. */
    v3_offset_time(info, (sint64_t)(old_guest_time - guest_time));

    return 0;
}

static int skew_guest_time(struct guest_info * info) {
    struct vm_time * time_state = &(info->time_state);
    uint64_t target_guest_time, guest_time;
    /* Now the host may have gotten ahead of the guest because
     * yielding is a coarse grained thing. Figure out what guest time
     * we want to be at, and use the use the offsetting mechanism in 
     * the VMM to make the guest run forward. We limit *how* much we skew 
     * it forward to prevent the guest time making large jumps, 
     * however. */
    target_guest_time = compute_target_guest_time(info);
    guest_time = v3_get_guest_time(time_state);

    if (guest_time < target_guest_time) {
	sint64_t max_skew, desired_skew, skew;

	if (time_state->enter_time) {
	    /* Limit forward skew to 10% of the amount the guest has
	     * run since we last could skew time */
	    max_skew = (sint64_t)(guest_time - time_state->enter_time) / 10.0;
	} else {
	    max_skew = 0;
	}

	desired_skew = (sint64_t)(target_guest_time - guest_time);
	skew = desired_skew > max_skew ? max_skew : desired_skew;
	PrintDebug("Guest %lld cycles behind where it should be.\n",
		   desired_skew);
	PrintDebug("Limit on forward skew is %lld. Skewing forward %lld.\n",
	           max_skew, skew); 
	
	v3_offset_time(info, skew);
    }

    return 0;
}
#endif /* V3_CONFIG_TIME_DILATION */

// Control guest time in relation to host time so that the two stay 
// appropriately synchronized to the extent possible. 
int v3_adjust_time(struct guest_info * info) {

#ifdef V3_CONFIG_TIME_DILATION
    /* First deal with yielding if we want to slow down the guest */
    yield_host_time(info);

    /* Now, if the guest is too slow, (either from excess yielding above,
     * or because the VMM is doing something that takes a long time to emulate)
     * allow guest time to jump forward a bit */
    skew_guest_time(info);
#endif
    return 0;
}

/* Called immediately upon entry in the the VMM */
int 
v3_time_exit_vm( struct guest_info * info ) 
{
    struct vm_time * time_state = &(info->time_state);
    
    time_state->exit_time = v3_get_host_time(time_state);

    return 0;
}

/* Called immediately prior to entry to the VM */
int 
v3_time_enter_vm( struct guest_info * info )
{
    struct vm_time * time_state = &(info->time_state);
    uint64_t host_time;

    host_time = v3_get_host_time(time_state);
    time_state->enter_time = host_time;
#ifdef V3_CONFIG_TIME_DILATION
    { 
        uint64_t guest_time;
	sint64_t offset;
        guest_time = v3_compute_guest_time(time_state, host_time);
	// XXX we probably want to use an inline function to do these
        // time differences to deal with sign and overflow carefully
	offset = (sint64_t)guest_time - (sint64_t)host_time;
	PrintDebug("v3_time_enter_vm: guest time offset %lld from host time.\n", offset);
        time_state->guest_host_offset = offset;
    }
#else
    time_state->guest_host_offset = 0;
#endif

    return 0;
}
	

	   
struct v3_timer * v3_add_timer(struct guest_info * info, 
			       struct v3_timer_ops * ops, 
			       void * private_data) {
    struct v3_timer * timer = NULL;
    timer = (struct v3_timer *)V3_Malloc(sizeof(struct v3_timer));
    V3_ASSERT(timer != NULL);

    timer->ops = ops;
    timer->private_data = private_data;

    list_add(&(timer->timer_link), &(info->time_state.timers));
    info->time_state.num_timers++;

    return timer;
}

int v3_remove_timer(struct guest_info * info, struct v3_timer * timer) {
    list_del(&(timer->timer_link));
    info->time_state.num_timers--;

    V3_Free(timer);
    return 0;
}

void v3_update_timers(struct guest_info * info) {
    struct vm_time *time_state = &info->time_state;
    struct v3_timer * tmp_timer;
    sint64_t cycles;
    uint64_t old_time = info->time_state.last_update;

    time_state->last_update = v3_get_guest_time(time_state);
    cycles = (sint64_t)(time_state->last_update - old_time);
    V3_ASSERT(cycles >= 0);

    //    V3_Print("Updating timers with %lld elapsed cycles.\n", cycles);
    list_for_each_entry(tmp_timer, &(time_state->timers), timer_link) {
	tmp_timer->ops->update_timer(info, cycles, time_state->guest_cpu_freq, tmp_timer->private_data);
    }
}

/* 
 * Handle full virtualization of the time stamp counter.  As noted
 * above, we don't store the actual value of the TSC, only the guest's
 * offset from monotonic guest's time. If the guest writes to the TSC, we
 * handle this by changing that offset.
 *
 * Possible TODO: Proper hooking of TSC read/writes?
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


int v3_init_time_vm(struct v3_vm_info * vm) {
    int ret;

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

    PrintDebug("Registering TIME_CPUFREQ hypercall.\n");
    ret = v3_register_hypercall(vm, TIME_CPUFREQ_HCALL, 
				handle_cpufreq_hcall, NULL);

    return ret;
}

void v3_deinit_time_vm(struct v3_vm_info * vm) {
    v3_unhook_msr(vm, TSC_MSR);
    v3_unhook_msr(vm, TSC_AUX_MSR);

    v3_remove_hypercall(vm, TIME_CPUFREQ_HCALL);
}

void v3_init_time_core(struct guest_info * info) {
    struct vm_time * time_state = &(info->time_state);
    v3_cfg_tree_t * cfg_tree = info->core_cfg_data;
    char * khz = NULL;

    time_state->host_cpu_freq = V3_CPU_KHZ();
    khz = v3_cfg_val(cfg_tree, "khz");

    if (khz) {
	time_state->guest_cpu_freq = atoi(khz);
	PrintDebug("Logical Core %d (vcpu=%d) CPU frequency requested at %d khz.\n", 
		   info->pcpu_id, info->vcpu_id, time_state->guest_cpu_freq);
    } 
    
    if ( (khz == NULL) || 
	 (time_state->guest_cpu_freq <= 0)  || 
	 (time_state->guest_cpu_freq > time_state->host_cpu_freq) ) {

	time_state->guest_cpu_freq = time_state->host_cpu_freq;
    }

    PrintDebug("Logical Core %d (vcpu=%d) CPU frequency set to %d KHz (host CPU frequency = %d KHz).\n", 
	       info->pcpu_id, info->vcpu_id,
	       time_state->guest_cpu_freq, 
	       time_state->host_cpu_freq);

    time_state->initial_time = 0;
    time_state->last_update = 0;
    time_state->guest_host_offset = 0;
    time_state->tsc_guest_offset = 0;

    INIT_LIST_HEAD(&(time_state->timers));
    time_state->num_timers = 0;
    
    time_state->tsc_aux.lo = 0;
    time_state->tsc_aux.hi = 0;
}


void v3_deinit_time_core(struct guest_info * core) {
    struct vm_time * time_state = &(core->time_state);
    struct v3_timer * tmr = NULL;
    struct v3_timer * tmp = NULL;

    list_for_each_entry_safe(tmr, tmp, &(time_state->timers), timer_link) {
	v3_remove_timer(core, tmr);
    }

}
