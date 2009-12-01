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

void v3_init_time(struct guest_info * info) {
    struct vm_time * time_state = &(info->time_state);

    time_state->cpu_freq = V3_CPU_KHZ();
 
    time_state->guest_tsc = 0;
    time_state->cached_host_tsc = 0;
    // time_state->pending_cycles = 0;
  
    INIT_LIST_HEAD(&(time_state->timers));
    time_state->num_timers = 0;
}


int v3_add_timer(struct guest_info * info, struct vm_timer_ops * ops, void * private_data) {
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



void v3_update_time(struct guest_info * info, ullong_t cycles) {
    struct vm_timer * tmp_timer;
  
    info->time_state.guest_tsc += cycles;

    list_for_each_entry(tmp_timer, &(info->time_state.timers), timer_link) {
	tmp_timer->ops->update_time(cycles, info->time_state.cpu_freq, tmp_timer->private_data);
    }
  


    //info->time_state.pending_cycles = 0;
}
