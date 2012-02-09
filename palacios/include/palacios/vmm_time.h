/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Patrick Bridges <bridges@cs.unm.edu>
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *         Patrick Bridges <bridges@cs.unm.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_TIME_H
#define __VMM_TIME_H

#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vmm_list.h>
#include <palacios/vmm_msr.h>
#include <palacios/vmm_util.h>

struct guest_info;

/* Per-VM time information */
struct v3_time {
    uint32_t td_mult; 
};

/* Per-core time information */
struct vm_core_time {
    uint32_t host_cpu_freq;    // in kHZ 
    uint32_t guest_cpu_freq;   // can be lower than host CPU freq!

    sint64_t guest_host_offset;// Offset of monotonic guest time from host time
    sint64_t tsc_guest_offset; // Offset of guest TSC from monotonic guest time

    uint64_t last_update;      // Last time (in monotonic guest time) the 
                               // timers were updated

    uint64_t initial_time;     // Time when VMM started. 
    uint64_t enter_time;       // Host time the guest was last entered
    uint64_t exit_time;        // Host time the the VM was exited to
    uint64_t pause_time;       // Time at which the VM core was paused
    struct v3_msr tsc_aux;     // Auxilliary MSR for RDTSCP

    // Installed Timers slaved off of the guest monotonic TSC
    uint_t num_timers;
    struct list_head timers;

    // Installed timeout handlers, and the time (in monotonic guest time) of hte 
    // next timeout.
    uint64_t next_timeout; 
    struct list_head timeout_hooks;
};

struct v3_timer_ops {
    void (*update_timer)(struct guest_info * info, ullong_t cpu_cycles, ullong_t cpu_freq, void * priv_data);
    void (*advance_timer)(struct guest_info * info, void * private_data);
};

struct v3_timer {
    void * private_data;
    struct v3_timer_ops * ops;

    // Need to add accuracy/resolution fields later.

    struct list_head timer_link;
};

typedef void (*v3_timeout_callback_t)(struct guest_info * info, void * priv_data);
struct v3_timeout_hook {
    void * private_data;
    v3_timeout_callback_t callback;
    
    struct list_head hook_link;
};

// Basic functions for handling passage of time in palacios
void v3_init_time_core(struct guest_info * core);
int v3_init_time_vm(struct v3_vm_info * vm);

void v3_deinit_time_core(struct guest_info * core);
void v3_deinit_time_vm(struct v3_vm_info * vm);

int v3_start_time(struct guest_info * core);

int v3_time_enter_vm(struct guest_info * core);
int v3_time_exit_vm(struct guest_info * core);

int v3_pause_time(struct guest_info * core);
int v3_resume_time(struct guest_info * core);
int v3_offset_time(struct guest_info * core, sint64_t offset);

int v3_adjust_time(struct guest_info * core);

// Basic functions for attaching timers to the passage of time - these timers 
// should eventually specify their accuracy and resolution.
struct v3_timer * v3_add_timer(struct guest_info * info, struct v3_timer_ops * ops, void * private_data);
int v3_remove_timer(struct guest_info * info, struct v3_timer * timer);
void v3_update_timers(struct guest_info * info);

// Functions for handling one-shot timeouts in Palacios. Note that only one
// timeout is every currently outstanding (the soonest scheduled one!), and that
// all hooks are called on any timeout. If a hook gets called before the desired
// timeout time, that hook should reschedule its own timeout if desired.
struct v3_timeout_hook * v3_add_timeout_hook(struct guest_info * info, v3_timeout_callback_t callback, void * priv_data);
int v3_remove_timeout_hook(struct guest_info * info, struct v3_timeout_hook * hook);
int v3_schedule_timeout(struct guest_info * info, ullong_t cycles);
int v3_check_timeout(struct guest_info * info);

// Functions to return the different notions of time in Palacios.
static inline uint64_t v3_get_host_time(struct vm_core_time *t) {
    uint64_t tmp;
    rdtscll(tmp);
    return tmp;
}

// Returns *monotonic* guest time.
static inline uint64_t v3_compute_guest_time(struct vm_core_time *t, uint64_t ht) {
    if (t->pause_time)
    	return t->pause_time + t->guest_host_offset;
    else
    	return ht + t->guest_host_offset;
}

static inline uint64_t v3_get_guest_time(struct vm_core_time *t) {
    return v3_compute_guest_time(t, v3_get_host_time(t));
}

// Returns the TSC value seen by the guest
static inline uint64_t v3_compute_guest_tsc(struct vm_core_time *t, uint64_t ht) {
    return v3_compute_guest_time(t, ht) + t->tsc_guest_offset;
}

static inline uint64_t v3_get_guest_tsc(struct vm_core_time *t) {
    return v3_compute_guest_tsc(t, v3_get_host_time(t));
}

// Returns offset of guest TSC from host TSC
static inline sint64_t v3_tsc_host_offset(struct vm_core_time *time_state) {
    return time_state->guest_host_offset + time_state->tsc_guest_offset;
}

// Functions for handling exits on the TSC when fully virtualizing 
// the timestamp counter.
#define TSC_MSR     0x10
#define TSC_AUX_MSR 0xC0000103

int v3_handle_rdtscp(struct guest_info *info);
int v3_handle_rdtsc(struct guest_info *info);




#endif // !__V3VEE__

#endif
