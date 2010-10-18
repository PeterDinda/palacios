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

#ifndef __VMM_TIME_H
#define __VMM_TIME_H

#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vmm_list.h>
#include <palacios/vmm_msr.h>
#include <palacios/vmm_util.h>

struct guest_info;

struct vm_time {
    uint32_t cpu_freq;         // in kHZ in terms of guest CPU speed
                               // which ideally can be different lower than
                               // host CPU speed!
         
    uint32_t time_mult;        // Fields for computing monotonic guest time
    uint32_t time_div;         // from host (tsc) time
    sint64_t time_offset;      

    sint64_t tsc_time_offset;  // Offset for computing guest TSC value from
                               // monotonic guest time
    
    uint64_t last_update;      // Last time (in monotonic guest time) the 
                               // timers were updated

    uint64_t pause_time;       // Cache value to help calculate the guest_tsc
    
    struct v3_msr tsc_aux;     // Auxilliary MSR for RDTSCP

    // Installed Timers slaved off of the guest monotonic TSC
    uint_t num_timers;
    struct list_head timers;
};




struct vm_timer_ops {
    void (*update_timer)(struct guest_info * info, ullong_t cpu_cycles, ullong_t cpu_freq, void * priv_data);
    void (*advance_timer)(struct guest_info * info, void * private_data);
};

struct vm_timer {
    void * private_data;
    struct vm_timer_ops * ops;

    struct list_head timer_link;
};




int v3_add_timer(struct guest_info * info, struct vm_timer_ops * ops, void * private_data);
int v3_remove_timer(struct guest_info * info, struct vm_timer * timer);
void v3_update_timers(struct guest_info * info);


void v3_init_time(struct guest_info * info);
int v3_start_time(struct guest_info * info);
int v3_pause_time(struct guest_info * info);
int v3_resume_time(struct guest_info * info);

// Returns host time
static inline uint64_t v3_get_host_time(struct vm_time *t) {
    uint64_t tmp;
    rdtscll(tmp);
    return tmp;
}

// Returns *monotonic* guest time.
static inline uint64_t v3_get_guest_time(struct vm_time *t) {
    if (t->pause_time) return t->pause_time;
    else return v3_get_host_time(t) + t->time_offset;
}

// Returns the TSC value seen by the guest
static inline uint64_t v3_get_guest_tsc(struct vm_time *t) {
    return v3_get_guest_time(t) + t->tsc_time_offset;
}

#define TSC_MSR     0x10
#define TSC_AUX_MSR 0xC0000103

int v3_handle_rdtscp(struct guest_info *info);
int v3_handle_rdtsc(struct guest_info *info);

#endif // !__V3VEE__

#endif
