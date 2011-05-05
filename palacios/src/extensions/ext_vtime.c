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
 *     time in the guest. This is computed using an offsets from (1) above.
 * (3) The actual guest timestamp counter (which can be written by
 *     writing to the guest TSC MSR - MSR 0x10) from the monotonic guest TSC.
 *     This is also computed as an offset from (2) above when the TSC and
 *     this offset is updated when the TSC MSR is written.
 *
 * The value used to offset the guest TSC from the host TSC is the *sum* of all
 * of these offsets (2 and 3) above
 * 
 * Because all other devices are slaved off of the passage of time in the guest,
 * it is (2) above that drives the firing of other timers in the guest, 
 * including timer devices such as the Programmable Interrupt Timer (PIT).
 *
 * Future additions:
 * (1) Add support for temporarily skewing guest time off of where it should
 *     be to support slack simulation of guests. The idea is that simulators
 *     set this skew to be the difference between how much time passed for a 
 *     simulated feature and a real implementation of that feature, making 
 *     pass at a different rate from real time on this core. The VMM will then
 *     attempt to move this skew back towards 0 subject to resolution/accuracy
 *     constraints from various system timers.
 *   
 *     The main effort in doing this will be to get accuracy/resolution 
 *     information from each local timer and to use this to bound how much skew
 *     is removed on each exit.
 */



struct vtime_state {
    uint32_t guest_cpu_freq;   // can be lower than host CPU freq!
    uint64_t initial_time;     // Time when VMM started. 
    sint64_t guest_host_offset;// Offset of monotonic guest time from host time
};




static int offset_time( struct guest_info * info, sint64_t offset )
{
    struct vm_time * time_state = &(info->time_state);
//    PrintDebug("Adding additional offset of %lld to guest time.\n", offset);
    time_state->guest_host_offset += offset;
    return 0;
}


// Control guest time in relation to host time so that the two stay 
// appropriately synchronized to the extent possible. 
int v3_adjust_time(struct guest_info * info) {
    struct vm_time * time_state = &(info->time_state);
    uint64_t host_time, target_host_time;
    uint64_t guest_time, target_guest_time, old_guest_time;
    uint64_t guest_elapsed, host_elapsed, desired_elapsed;

    /* Compute the target host time given how much time has *already*
     * passed in the guest */
    guest_time = v3_get_guest_time(time_state);
    guest_elapsed = (guest_time - time_state->initial_time);
    desired_elapsed = (guest_elapsed * time_state->host_cpu_freq) / time_state->guest_cpu_freq;
    target_host_time = time_state->initial_time + desired_elapsed;

    /* Now, let the host run while the guest is stopped to make the two
     * sync up. */
    host_time = v3_get_host_time(time_state);
    old_guest_time = v3_get_guest_time(time_state);

    while (target_host_time > host_time) {
	v3_yield(info);
	host_time = v3_get_host_time(time_state);
    }

    guest_time = v3_get_guest_time(time_state);

    // We do *not* assume the guest timer was paused in the VM. If it was
    // this offseting is 0. If it wasn't we need this.
   offset_time(info, (sint64_t)old_guest_time - (sint64_t)guest_time);

    /* Now the host may have gotten ahead of the guest because
     * yielding is a coarse grained thing. Figure out what guest time
     * we want to be at, and use the use the offsetting mechanism in 
     * the VMM to make the guest run forward. We limit *how* much we skew 
     * it forward to prevent the guest time making large jumps, 
     * however. */
    host_elapsed = host_time - time_state->initial_time;
    desired_elapsed = (host_elapsed * time_state->guest_cpu_freq) / time_state->host_cpu_freq;
    target_guest_time = time_state->initial_time + desired_elapsed;

    if (guest_time < target_guest_time) {
	uint64_t max_skew, desired_skew, skew;

	if (time_state->enter_time) {
	    max_skew = (time_state->exit_time - time_state->enter_time) / 10;
	} else {
	    max_skew = 0;
	}

	desired_skew = target_guest_time - guest_time;
	skew = desired_skew > max_skew ? max_skew : desired_skew;
/*	PrintDebug("Guest %llu cycles behind where it should be.\n",
		   desired_skew);
	PrintDebug("Limit on forward skew is %llu. Skewing forward %llu.\n",
	           max_skew, skew); */
	
	offset_time(info, skew);
    }
    
    return 0;
}


static int init() {
    khz = v3_cfg_val(cfg_tree, "khz");

    if (khz) {
	time_state->guest_cpu_freq = atoi(khz);
	PrintDebug("Core %d CPU frequency requested at %d khz.\n", 
		   info->pcpu_id, time_state->guest_cpu_freq);
    } 
    
    if ( (khz == NULL) || 
	 (time_state->guest_cpu_freq <= 0)  || 
	 (time_state->guest_cpu_freq > time_state->host_cpu_freq) ) {

	time_state->guest_cpu_freq = time_state->host_cpu_freq;
    }


}
