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

struct vm_time {
    uint32_t host_cpu_freq;    // in kHZ 
    uint32_t guest_cpu_freq;   // can be lower than host CPU freq!

    sint64_t guest_host_offset;// Offset of monotonic guest time from host time
    sint64_t tsc_guest_offset; // Offset of guest TSC from monotonic guest time

    uint64_t last_update;      // Last time (in monotonic guest time) the 
                               // timers were updated

    uint64_t initial_time;     // Time when VMM started. 
    uint64_t enter_time;       // Host time the guest was last entered
    uint64_t exit_time;        // Host time the the VM was exited to
    struct v3_msr tsc_aux;     // Auxilliary MSR for RDTSCP

    // Installed Timers slaved off of the guest monotonic TSC
    uint_t num_timers;
    struct list_head timers;
};

struct v3_timer_ops {
    void (*update_timer)(struct guest_info * info, ullong_t cpu_cycles, ullong_t cpu_freq, void * priv_data);
    void (*advance_timer)(struct guest_info * info, void * private_data);
};

struct v3_timer {
    void * private_data;
    struct v3_timer_ops * ops;

    struct list_head timer_link;
};

// Basic functions for handling passage of time in palacios
void v3_init_time_core(struct guest_info * core);
int v3_init_time_vm(struct v3_vm_info * vm);

void v3_deinit_time_core(struct guest_info * core);
void v3_deinit_time_vm(struct v3_vm_info * vm);

int v3_start_time(struct guest_info * core);

int v3_time_enter_vm(struct guest_info * core);
int v3_time_exit_vm(struct guest_info * core);

int v3_adjust_time(struct guest_info * core);
int v3_offset_time(struct guest_info * core, sint64_t offset);

// Basic functions for attaching timers to the passage of time
struct v3_timer * v3_add_timer(struct guest_info * info, struct v3_timer_ops * ops, void * private_data);
int v3_remove_timer(struct guest_info * info, struct v3_timer * timer);
void v3_update_timers(struct guest_info * info);

// Functions to return the different notions of time in Palacios.
static inline uint64_t v3_get_host_time(struct vm_time *t) {
    uint64_t tmp;
    rdtscll(tmp);
    return tmp;
}

// Returns *monotonic* guest time.
static inline uint64_t v3_compute_guest_time(struct vm_time *t, uint64_t ht) {
#ifdef V3_CONFIG_TIME_HIDE_VM_COST
    V3_ASSERT(t->exit_time);
    return t->exit_time + t->guest_host_offset;
#else
    return v3_get_host_time(t) + t->guest_host_offset;
#endif
}

static inline uint64_t v3_get_guest_time(struct vm_time *t) {
    return v3_compute_guest_time(t, v3_get_host_time(t));
}

// Returns the TSC value seen by the guest
static inline uint64_t v3_compute_guest_tsc(struct vm_time *t, uint64_t ht) {
    return v3_compute_guest_time(t, ht) + t->tsc_guest_offset;
}

static inline uint64_t v3_get_guest_tsc(struct vm_time *t) {
    return v3_compute_guest_tsc(t, v3_get_host_time(t));
}

// Returns offset of guest TSC from host TSC
static inline sint64_t v3_tsc_host_offset(struct vm_time *time_state) {
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
