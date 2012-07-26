/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Jack Lange <jacklange@cs.pitt.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jacklangel@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm_barrier.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest.h>

int v3_init_barrier(struct v3_vm_info * vm_info) {
    struct v3_barrier * barrier = &(vm_info->barrier);

    memset(barrier, 0, sizeof(struct v3_barrier));
    v3_bitmap_init(&(barrier->cpu_map), vm_info->num_cores); 
    v3_lock_init(&(barrier->lock));

    return 0;
}

int v3_deinit_barrier(struct v3_vm_info * vm_info) {
    struct v3_barrier * barrier = &(vm_info->barrier);

    v3_bitmap_deinit(&(barrier->cpu_map));
    v3_lock_deinit(&(barrier->lock));

    return 0;
}

int v3_raise_barrier_nowait(struct v3_vm_info * vm_info, struct guest_info * local_core) {
    struct v3_barrier * barrier = &(vm_info->barrier);
    addr_t flag;
    int acquired = 0;

    int local_vcpu = -1;
    int i = 0;

    flag = v3_lock_irqsave(barrier->lock);

    if (barrier->active == 0) {
	barrier->active = 1;
	acquired = 1;
    }

    v3_unlock_irqrestore(barrier->lock, flag);

    if (acquired == 0) {
	/* If we are in a core context and the barrier has already been acquired 
	   we'll be safe and let the other barrier proceed. We will still report an error 
	   though to allow possible cleanups to occur at the call site.
	*/
	if (local_core != NULL) {
	    v3_wait_at_barrier(local_core);
	}

	return -1;
    }

    // If we are raising the barrier from a core context
    //   we have to mark ourselves blocked first to avoid deadlock
    if (local_core != NULL) {
	local_vcpu = local_core->vcpu_id;
	v3_bitmap_set(&(barrier->cpu_map), local_vcpu);
    }


    // send out interrupts to force exits on all cores
    for (i = 0; i < vm_info->num_cores; i++) {
	if (vm_info->cores[i].vcpu_id != local_vcpu) {
	    v3_interrupt_cpu(vm_info, vm_info->cores[i].pcpu_id, 0);
	}
    }

    return 0;
}

int v3_wait_for_barrier(struct v3_vm_info * vm_info, struct guest_info * local_core) {
    struct v3_barrier * barrier = &(vm_info->barrier);
    int all_blocked = 0;
    int i = 0;

    if (barrier->active == 0) {
	return -1;
    }

    // wait for barrier catch on all cores
    while (all_blocked == 0) {
	all_blocked = 1;

	for (i = 0; i < vm_info->num_cores; i++) {
	    
	    // Tricky: If a core is not running then it is safe to ignore it. 
	    // Whenever we transition a core to the RUNNING state we MUST immediately wait on the barrier. 
	    // TODO: Wrap the state transitions in functions that do this automatically
	    if (vm_info->cores[i].core_run_state != CORE_RUNNING) {
		continue;
	    }

	    if (v3_bitmap_check(&(barrier->cpu_map), i) == 0) {
		// There is still a core that is not waiting at the barrier
		all_blocked = 0;
	    }
	}

	if (all_blocked == 1) {
	    break;
	}

	v3_yield(local_core,-1);
    }

    return 0;
}



/* Barrier synchronization primitive
 *   -- This call will block until all the guest cores are waiting at a common synchronization point
 *      in a yield loop. The core will block at the sync point until the barrier is lowered.
 * 
 *   ARGUMENTS: 
 *       vm_info -- The VM for which the barrier is being activated
 *       local_core -- The core whose thread this function is being called from, or NULL 
 *                     if the calling thread is not associated with a VM's core context
 */

int v3_raise_barrier(struct v3_vm_info * vm_info, struct guest_info * local_core) {
    int ret = 0;


    if ((vm_info->run_state != VM_RUNNING) || 
	(vm_info->run_state != VM_SIMULATING)) {
	return 0;
    }

    ret = v3_raise_barrier_nowait(vm_info, local_core);

    if (ret != 0) {
	return ret;
    }

    return v3_wait_for_barrier(vm_info, local_core);
}



/* Lowers a barrier that has already been raised
 *    guest cores will automatically resume execution 
 *    once this has been called
 * 
 *   TODO: Need someway to check that the barrier is active
 */

int v3_lower_barrier(struct v3_vm_info * vm_info) {
    struct v3_barrier * barrier = &(vm_info->barrier);

    
    if ((vm_info->run_state != VM_RUNNING) || 
	(vm_info->run_state != VM_SIMULATING)) {
	return 0;
    }

    // Clear the active flag, so cores won't wait 
    barrier->active = 0;

    // Clear all the cpu flags, so cores will proceed
    v3_bitmap_reset(&(barrier->cpu_map));

    return 0;
}


/* 
 * Syncronization point for guest cores
 *    -- called as part of the main VMM event loop for each core
 *    -- if a barrier has been activated then the core will signal  
 *       it has reached the barrier and sit in a yield loop until the 
 *       barrier has been lowered
 */
int v3_wait_at_barrier(struct guest_info * core) {
    struct v3_barrier * barrier = &(core->vm_info->barrier);

    if (barrier->active == 0) {
	return 0;
    }

    V3_Print("Core %d waiting at barrier\n", core->vcpu_id);

    /*  Barrier has been activated. 
     *  Wait here until it's lowered
     */
    
    
    // set cpu bit in barrier bitmap
    v3_bitmap_set(&(barrier->cpu_map), core->vcpu_id);
    V3_Print("Core %d bit set as waiting\n", core->vcpu_id);

    // wait for cpu bit to clear
    while (v3_bitmap_check(&(barrier->cpu_map), core->vcpu_id)) {
	v3_yield(core,-1);
    }

    return 0;
}
