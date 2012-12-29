/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Jack Lange <jacklange@cs.pitt.edu> 
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jacklange@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_timeout.h>



int v3_add_core_timeout(struct guest_info * core, uint64_t cycles,
			int (*callback)(struct guest_info * core, 
					void * private_data),
			void * private_data) {
    struct v3_core_timeouts * timeouts = &(core->timeouts);

    if (timeouts->timeout_active) {
	PrintError(core->vm_info, core, "Tried to activate a timeout whiel one is already active\n");
	return -1;
    }

    timeouts->callback = callback;
    timeouts->private_data = private_data;
    timeouts->timeout_active = 1;
    timeouts->next_timeout = cycles;

    return 0;}



int v3_handle_timeouts(struct guest_info * core, uint64_t guest_cycles) {
    struct v3_core_timeouts * timeouts = &(core->timeouts);

    /*
    V3_Print(core->vm_info, core, "Handling timeout from %llu guest cycles (Next timeout=%llu)\n", guest_cycles,
	     timeouts->next_timeout);
    */

    if (guest_cycles >= timeouts->next_timeout) {
	timeouts->next_timeout = 0;
	timeouts->timeout_active = 0;

	if (timeouts->callback) {

	    V3_Print(core->vm_info, core, "Calling timeout callback\n");
	    timeouts->callback(core, timeouts->private_data);
	}
    } else {
	timeouts->next_timeout -= guest_cycles;
    }

    return 0;
}
