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
 * Author: Jack Lange <jacklange@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_BARRIER_H__
#define __VMM_BARRIER_H__

#ifdef __V3VEE__

#include <palacios/vmm_lock.h>
#include <palacios/vmm_bitmap.h>



struct v3_barrier {
    int active;     // If 1, barrier is active, everyone must wait 
                    // If 0, barrier is clear, can proceed

    struct v3_bitmap cpu_map;

    v3_lock_t lock;
};

struct v3_vm_info;
struct guest_info;

int v3_init_barrier(struct v3_vm_info * vm_info);
int v3_deinit_barrier(struct v3_vm_info * vm_info);


int v3_raise_barrier(struct v3_vm_info * vm_info, struct guest_info * local_core);
int v3_lower_barrier(struct v3_vm_info * vm_info);

int v3_wait_at_barrier(struct guest_info * core);


/* Special Barrier activation functions. 
 *  DO NOT USE THESE UNLESS YOU KNOW WHAT YOU ARE DOING
 */
int v3_raise_barrier_nowait(struct v3_vm_info * vm_info, struct guest_info * local_core);
int v3_wait_for_barrier(struct v3_vm_info * vm_info, struct guest_info * local_core);
/* ** */



#endif

#endif
