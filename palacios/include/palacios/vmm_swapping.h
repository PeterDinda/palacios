/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2014, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Daniel Zuo <pengzuo2014@u.northwestern.edu>
 *         Nikhat Karimi <nikhatkarimi@gmail.com>
 *         Ahalya Srinivasan <AhalyaSrinivasan2015@u.northwestern.edu>
 *         Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMM_SWAPPING_H
#define __VMM_SWAPPING_H


#ifdef __V3VEE__ 

#include <palacios/vmm_types.h>
#include <palacios/vmm_lock.h>
#include <interfaces/vmm_file.h>

typedef enum {
    V3_SWAP_NEXT_FIT, 
    V3_SWAP_RANDOM,
    V3_SWAP_LRU,     // this is not the droid you're looking for
} v3_swapping_strategy_t;

// for inclusion in the vm struct
struct v3_swap_impl_state {
    // per-VM lock should be held when changing
    // swap state or the swapping elements of base region state
    v3_lock_t lock;
    uint32_t enable_swapping:1;
    v3_swapping_strategy_t strategy;
    uint64_t host_mem_size; // allocated space in bytes
    uint64_t swap_count; 
    uint64_t last_region_used; // for use by V3_SWAP_NEXT_FIT
    // This is the swap file on disk 
    v3_file_t swapfd; 
};


// for inclusion in the region 
struct v3_swap_region_state {
    uint64_t last_accessed;  // timestamp
};


struct v3_mem_region;

typedef struct v3_xml v3_cfg_tree_t;

int v3_init_swapping();
int v3_deinit_swapping();

int v3_init_swapping_vm(struct v3_vm_info *vm, v3_cfg_tree_t *config);
int v3_deinit_swapping_vm(struct v3_vm_info *vm);

// not needed yet
//int v3_init_swapping_core(struct guest_info *core);
//int v3_deinit_swapping_core(struct guest_info *core);

int v3_pin_region(struct v3_vm_info *vm, struct v3_mem_region *region);
int v3_unpin_region(struct v3_vm_info *vm, struct v3_mem_region *region);

// This will automatically swap out a victim if needed
int v3_swap_in_region(struct v3_vm_info *vm, struct v3_mem_region *region);
// Force a region out
int v3_swap_out_region(struct v3_vm_info *vm, struct v3_mem_region *region);

// drive LRU
void v3_touch_region(struct v3_vm_info *vm, struct v3_mem_region *region);

#endif /* ! __V3VEE__ */


#endif
