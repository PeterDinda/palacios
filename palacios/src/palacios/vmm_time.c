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
 *     time in the guest. This is stored as an absolute number of cycles elapsed
 *     and is updated on guest entry and exit; it can also be updated explicitly
 *     in the monitor at times
 * (3) The actual guest timestamp counter (which can be written by
 *     writing to the guest TSC MSR - MSR 0x10) from the monotonic guest TSC.
 *     This is also computed as an offset from (2) above when the TSC and
 *     this offset is updated when the TSC MSR is written.
 *
 * Because all other devices are slaved off of the passage of time in the guest,
 * it is (2) above that drives the firing of other timers in the guest, 
 * including timer devices such as the Programmable Interrupt Timer (PIT).
 *
 * Future additions:
 * (1) Add support for temporarily skewing guest time off of where it should
 *     be to support slack simulation of guests. The idea is that simulators
 *     set this skew to be the difference between how much time passed for a 
 *     simulated feature and a real implementation of that feature, making time
 *     pass at a different rate from real time on this core. The VMM will then
 *     attempt to move this skew back towards 0 subject to resolution/accuracy
 *     constraints from various system timers.
 *   
 *     The main effort in doing this will be to get accuracy/resolution 
 *     information from each local timer and to use this to bound how much skew
 *     is removed on each exit.
 *
 * (2) Look more into sychronizing the offsets *across* virtual and physical 
 *     cores so that multicore guests stay mostly in sync.
 *
 * (3) Look into using the AMD TSC multiplier feature and adding explicit time
 *     dilation support to time handling.
 */


static int handle_cpufreq_hcall(struct guest_info * info, uint_t hcall_id, void * priv_data) {
    struct vm_core_time * time_state = &(info->time_state);

    info->vm_regs.rbx = time_state->guest_cpu_freq;

    PrintDebug("Guest request cpu frequency: return %ld\n", (long)info->vm_regs.rbx);
    
    return 0;
}



int v3_start_time(struct guest_info * info) {
    /* We start running with guest_time == host_time */
    uint64_t t = v3_get_host_time(&info->time_state); 

    info->time_state.initial_host_time = t;
    info->yield_start_cycle = t;

    info->time_state.last_update = 0;
    info->time_state.guest_cycles = 0;
    PrintDebug("Starting time for core %d at host time %llu/guest time %llu.\n",
	       info->vcpu_id, t, info->time_state.guest_cycles); 
    v3_yield(info);
    return 0;
}

static sint64_t 
host_to_guest_cycles(struct guest_info * info, sint64_t host_cycles) {
    struct vm_core_time * core_time_state = &(info->time_state);
    uint32_t cl_num, cl_denom;

    cl_num = core_time_state->clock_ratio_num;
    cl_denom = core_time_state->clock_ratio_denom;

    return (host_cycles * cl_num) / cl_denom;
}

/*
static sint64_t 
guest_to_host_cycles(struct guest_info * info, sint64_t guest_cycles) {
    struct vm_core_time * core_time_state = &(info->time_state);
    uint32_t cl_num, cl_denom;

    cl_num = core_time_state->clock_ratio_num;
    cl_denom = core_time_state->clock_ratio_denom;

    return (guest_cycles * cl_denom) / cl_num;
}
*/

int v3_advance_time(struct guest_info * info, uint64_t *host_cycles)
{
    uint64_t guest_cycles;

    if (info->flags & VM_TIME_SLAVE_HOST) {
	struct v3_time *vm_ts = &(info->vm_info->time_state);
        uint64_t ht = v3_get_host_time(&info->time_state);
        uint64_t host_elapsed = ht - info->time_state.initial_host_time;
	uint64_t dilated_elapsed = (host_elapsed * vm_ts->td_num) / vm_ts->td_denom;
	uint64_t guest_elapsed = host_to_guest_cycles(info, dilated_elapsed);
	guest_cycles = guest_elapsed - v3_get_guest_time(&info->time_state);
    } else if (host_cycles) {
	guest_cycles = host_to_guest_cycles(info, *host_cycles);
    } else {
	guest_cycles = 0;
    }
    
    info->time_state.guest_cycles += guest_cycles;

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
    struct vm_core_time *time_state = &info->time_state;
    struct v3_timer * tmp_timer;
    sint64_t cycles;
    uint64_t old_time = time_state->last_update;

    time_state->last_update = v3_get_guest_time(time_state);
    cycles = (sint64_t)(time_state->last_update - old_time);
    if (cycles < 0) {
	PrintError("Cycles appears to have rolled over - old time %lld, current time %lld.\n",
		   old_time, time_state->last_update);
	return;
    }

    //PrintDebug("Updating timers with %lld elapsed cycles.\n", cycles);
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
    PrintDebug("Handling virtual RDTSC call.\n");
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
    struct vm_core_time * time_state = &(info->time_state);

    V3_ASSERT(msr_num == TSC_AUX_MSR);

    msr_val->lo = time_state->tsc_aux.lo;
    msr_val->hi = time_state->tsc_aux.hi;

    return 0;
}

static int tsc_aux_msr_write_hook(struct guest_info *info, uint_t msr_num, 
			      struct v3_msr msr_val, void *priv) {
    struct vm_core_time * time_state = &(info->time_state);

    V3_ASSERT(msr_num == TSC_AUX_MSR);

    time_state->tsc_aux.lo = msr_val.lo;
    time_state->tsc_aux.hi = msr_val.hi;

    return 0;
}

static int tsc_msr_read_hook(struct guest_info *info, uint_t msr_num,
			     struct v3_msr *msr_val, void *priv) {
    uint64_t time = v3_get_guest_tsc(&info->time_state);

    PrintDebug("Handling virtual TSC MSR read call.\n");
    V3_ASSERT(msr_num == TSC_MSR);

    msr_val->hi = time >> 32;
    msr_val->lo = time & 0xffffffffLL;
    
    return 0;
}

static int tsc_msr_write_hook(struct guest_info *info, uint_t msr_num,
			     struct v3_msr msr_val, void *priv) {
    struct vm_core_time * time_state = &(info->time_state);
    uint64_t guest_time, new_tsc;

    PrintDebug("Handling virtual TSC MSR write call.\n");
    V3_ASSERT(msr_num == TSC_MSR);

    new_tsc = (((uint64_t)msr_val.hi) << 32) | (uint64_t)msr_val.lo;
    guest_time = v3_get_guest_time(time_state);
    time_state->tsc_guest_offset = (sint64_t)(new_tsc - guest_time); 

    return 0;
}

static int
handle_time_configuration(struct v3_vm_info * vm, v3_cfg_tree_t *cfg) {
    char *source, *dilation;

    vm->time_state.flags = V3_TIME_SLAVE_HOST;
    vm->time_state.td_num = vm->time_state.td_denom = 1;

    if (!cfg) return 0;

    source = v3_cfg_val(cfg, "source");
    if (source) {
	if (strcasecmp(source, "none") == 0) {
	    vm->time_state.flags &= ~V3_TIME_SLAVE_HOST;
	} else if (strcasecmp(source, "host") != 0) {
	    PrintError("Unknown time source for VM core time management.\n");
	} else {
	    PrintDebug("VM time slaved to host TSC.\n");
	}
    }

    dilation = v3_cfg_val(cfg, "dilation");
    if (dilation) {
        if (!(vm->time_state.flags & VM_TIME_SLAVE_HOST)) {
	    PrintError("Time dilation only valid when slaved to host time.\n");
	} else {
	    uint32_t num = 1, denom = 1;
	    denom = atoi(dilation);
	    if ((num > 0) && (denom > 0)) {
		vm->time_state.td_num = num;
		vm->time_state.td_denom = denom;
	    }
	}
	if ((vm->time_state.td_num != 1) 
	    || (vm->time_state.td_denom != 1)) {
	    V3_Print("Time dilated from host time by a factor of %d/%d"
		     " in guest.\n", vm->time_state.td_denom, 
		     vm->time_state.td_num);
	} else {
	    PrintError("Time dilation specifier in configuration did not"
		       " result in actual time dilation in VM.\n");
	}
    }
    return 0;
}

int v3_init_time_vm(struct v3_vm_info * vm) {
    v3_cfg_tree_t * cfg_tree = vm->cfg_data->cfg;
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

    handle_time_configuration(vm, v3_cfg_subtree(cfg_tree, "time"));

    return ret;
}

void v3_deinit_time_vm(struct v3_vm_info * vm) {
    v3_unhook_msr(vm, TSC_MSR);
    v3_unhook_msr(vm, TSC_AUX_MSR);

    v3_remove_hypercall(vm, TIME_CPUFREQ_HCALL);
}

static uint32_t
gcd ( uint32_t a, uint32_t b )
{
    uint32_t c;
    while ( a != 0 ) {
        c = a; a = b%a;  b = c;
    }
    return b;
}

static int compute_core_ratios(struct guest_info * info, 
			       uint32_t hostKhz, uint32_t guestKhz)
{
    struct vm_core_time * time_state = &(info->time_state);
    uint32_t khzGCD;

    /* Compute these using the GCD() of the guest and host CPU freq.
     * If the GCD is too small, make it "big enough" */
    khzGCD = gcd(hostKhz, guestKhz);
    if (khzGCD < 1024)
	khzGCD = 1024;

    time_state->clock_ratio_num = guestKhz / khzGCD;
    time_state->clock_ratio_denom = hostKhz / khzGCD;

    time_state->ipc_ratio_num = 1;
    time_state->ipc_ratio_denom = 1;

    return 0;
}

void v3_init_time_core(struct guest_info * info) {
    struct vm_core_time * time_state = &(info->time_state);
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
	 (time_state->guest_cpu_freq <= 0)) {
/*  || (time_state->guest_cpu_freq > time_state->host_cpu_freq) ) { */
	time_state->guest_cpu_freq = time_state->host_cpu_freq;
    }
    compute_core_ratios(info, time_state->host_cpu_freq, 
			time_state->guest_cpu_freq);
    
    time_state->flags = 0;
    if (info->vm_info->time_state.flags & V3_TIME_SLAVE_HOST) {
	time_state->flags |= VM_TIME_SLAVE_HOST;
    }
    if ((time_state->clock_ratio_denom != 1) ||
	(time_state->clock_ratio_num != 1)) {
	time_state->flags |= VM_TIME_TRAP_RDTSC;
    }

    PrintDebug("Logical Core %d (vcpu=%d) CPU frequency set to %d KHz (host CPU frequency = %d KHz).\n", 
	       info->pcpu_id, info->vcpu_id,
	       time_state->guest_cpu_freq, 
	       time_state->host_cpu_freq);
    PrintDebug("    td_mult = %d/%d, cl_mult = %u/%u, ipc_mult = %u/%u.\n",
	       info->vm_info->time_state.td_num, 
	       info->vm_info->time_state.td_denom, 
	       time_state->clock_ratio_num, time_state->clock_ratio_denom,
	       time_state->ipc_ratio_num, time_state->ipc_ratio_denom);
    PrintDebug("    source = %s, rdtsc trapping = %s\n", 
	       (time_state->flags & VM_TIME_SLAVE_HOST) ? "host" : "none",
	       (time_state->flags & VM_TIME_TRAP_RDTSC) ? "true" : "false");

    time_state->guest_cycles = 0;
    time_state->tsc_guest_offset = 0;
    time_state->last_update = 0;
    time_state->initial_host_time = 0;

    INIT_LIST_HEAD(&(time_state->timers));
    time_state->num_timers = 0;
	    
    time_state->tsc_aux.lo = 0;
    time_state->tsc_aux.hi = 0;
}


void v3_deinit_time_core(struct guest_info * core) {
    struct vm_core_time * time_state = &(core->time_state);
    struct v3_timer * tmr = NULL;
    struct v3_timer * tmp = NULL;

    list_for_each_entry_safe(tmr, tmp, &(time_state->timers), timer_link) {
	v3_remove_timer(core, tmr);
    }
}
