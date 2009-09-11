/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_halt.h>
#include <palacios/vmm_intr.h>


#ifndef CONFIG_DEBUG_HALT
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif



//
// This should trigger a #GP if cpl != 0, otherwise, yield to host
//

int v3_handle_halt(struct guest_info * info) {

    if (info->cpl != 0) { 
	v3_raise_exception(info, GPF_EXCEPTION);
    } else {
    
	uint64_t yield_start = 0;
	uint64_t yield_stop = 0;
	uint32_t gap = 0;
	
	PrintDebug("CPU Yield\n");
	
	rdtscll(yield_start);
	v3_yield(info);
	rdtscll(yield_stop);
    
    
	//v3_update_time(info, yield_stop - yield_start);
	gap = yield_stop - yield_start;

	/*  WARNING!!! WARNING!!!
	 *  
	 * DO NOT REMOVE THIS CONDITIONAL!!!
	 *
	 * It is common for an OS to issue an IO op, and then sit in a halt loop
	 * waiting for the device to complete and raise an irq.
	 * If you remove this then the timer interrupt will ALWAYS subvert the completion 
	 * interrupt and stall the guest.
	 */
	if (!v3_intr_pending(info)) {
	    v3_raise_irq(info, 0);
	}

	
	PrintDebug("CPU Yield Done (%d cycles)\n", gap);
	
	info->rip += 1;
    }

    return 0;
}