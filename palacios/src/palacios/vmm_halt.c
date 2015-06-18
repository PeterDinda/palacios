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
#include <palacios/vmm_lowlevel.h> 
#include <palacios/vmm_perftune.h>

#ifndef V3_CONFIG_DEBUG_HALT
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


//
// This should trigger a #GP if cpl != 0, otherwise, yield to host
//

int v3_handle_halt(struct guest_info * info) 
{
    
    if (info->cpl != 0) { 
	v3_raise_exception(info, GPF_EXCEPTION);
    } else {
	uint64_t start_cycles;
	
	PrintDebug(info->vm_info, info, "CPU Yield\n");

	start_cycles  = v3_get_host_time(&info->time_state);

	while (!v3_intr_pending(info) && (info->vm_info->run_state == VM_RUNNING)) {
            uint64_t t, cycles;

	    t = v3_get_host_time(&info->time_state);

	    /* Yield, allowing time to pass while yielded */
	    v3_strategy_driven_yield(info, v3_cycle_diff_in_usec(info, start_cycles, t));

	    cycles = v3_get_host_time(&info->time_state) - t;

	    v3_advance_time(info, &cycles);

	    v3_update_timers(info);
	    

    	    
	    /* At this point, we either have some combination of 
	       interrupts, including perhaps a timer interrupt, or 
	       no interrupt.
	    */
	    if (!v3_intr_pending(info)) {
		/* if no interrupt, then we do halt */
		/* asm("hlt"); */
	    }

	    // participate in any barrier that might be raised
	    v3_wait_at_barrier(info);

	    // stop if the VM is being halted or core is being reset
	    if (info->core_run_state == CORE_STOPPED || info->core_run_state == CORE_RESETTING) { 
		break;
	    }

	}

	/* V3_Print(info->vm_info, info, "palacios: done with halt\n"); */
	
	info->rip += 1;
    }

    return 0;
}
