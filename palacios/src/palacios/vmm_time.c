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

#include <palacios/vmm_time.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest.h>

#ifndef CONFIG_DEBUG_TIME
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

static int handle_cpufreq_hcall(struct guest_info * info, uint_t hcall_id, void * priv_data) {
    struct vm_time * time_state = &(info->time_state);

    info->vm_regs.rbx = time_state->cpu_freq;

    PrintDebug("Guest request cpu frequency: return %ld\n", (long)info->vm_regs.rbx);
    
    return 0;
}



void v3_init_time(struct guest_info * info) {
    struct vm_time * time_state = &(info->time_state);

    time_state->cpu_freq = V3_CPU_KHZ();
 
    time_state->pause_time = 0;
    time_state->last_update = 0;
    time_state->host_offset = 0;
    
    INIT_LIST_HEAD(&(time_state->timers));
    time_state->num_timers = 0;

    v3_register_hypercall(info->vm_info, TIME_CPUFREQ_HCALL, handle_cpufreq_hcall, NULL);
}

uint64_t v3_get_host_time(struct guest_info * info) {
    uint64_t tmp;
    rdtscll(tmp);
    return tmp;
}

uint64_t v3_get_guest_time(struct guest_info * info) {
    return v3_get_host_time(info) + info->time_state.host_offset;
}


int v3_start_time(struct guest_info * info) {
    /* We start running with guest_time == host_time */
    uint64_t t = v3_get_host_time(info); 

    PrintDebug("Starting initial guest time as %llu\n", t);
    info->time_state.last_update = t;
    info->time_state.pause_time = t;
    return 0;
}

int v3_pause_time(struct guest_info * info) {
    V3_ASSERT(info->time_state.pause_time == 0);
    info->time_state.pause_time = v3_get_guest_time(info);
    PrintDebug("Time paused at guest time as %llu\n", 
	       info->time_state.pause_time);
    return 0;
}

int v3_resume_time(struct guest_info * info) {
    uint64_t t = v3_get_host_time(info);
    V3_ASSERT(info->time_state.pause_time != 0);
    info->time_state.host_offset = 
	(sint64_t)info->time_state.pause_time - (sint64_t)t;
#ifdef OPTION_TIME_ADJUST_TSC_OFFSET
    /* XXX Adjust host_offset towards zero based on resolution/accuracy
     * constraints. */
#endif
    info->time_state.pause_time = 0;
    PrintDebug("Time resumed paused at guest time as %llu "
	       "offset %lld from host time.\n", t, info->time_state.host_offset);

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

    info->time_state.last_update = v3_get_guest_time(info);
    cycles = info->time_state.last_update - old_time;

    list_for_each_entry(tmp_timer, &(info->time_state.timers), timer_link) {
	tmp_timer->ops->update_timer(info, cycles, info->time_state.cpu_freq, tmp_timer->private_data);
    }
}
