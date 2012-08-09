/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VM_PERFTUNE_H__
#define __VM_PERFTUNE_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>


struct v3_yield_strategy {
    enum {
	V3_YIELD_STRATEGY_GREEDY=0,     // always untimed yields  
	V3_YIELD_STRATEGY_FRIENDLY,     // always timed yields with the following
	V3_YIELD_STRATEGY_ADAPTIVE,     // switch from untimed to timed after the threshold
    }         strategy;

    uint64_t  threshold_usec;   // the point at which we transiton from untimed to timed yield
    uint64_t  time_usec;        // the amount of time for a timed yield call

#define V3_DEFAULT_YIELD_STRATEGY       V3_YIELD_STRATEGY_GREEDY
#define V3_DEFAULT_YIELD_THRESHOLD_USEC 100
#define V3_DEFAULT_YIELD_TIME_USEC      1000
};



//
//  The idea is that the performance tuning knobs in the system are in the following 
//  structure, which is configured when the VM is created, right after extensions,
//  using the <perftune/> subtree
//
struct v3_perf_options {
    struct v3_yield_strategy yield_strategy;
};


int      v3_setup_performance_tuning(struct v3_vm_info *vm, v3_cfg_tree_t *cfg);

void     v3_strategy_driven_yield(struct guest_info *core, uint64_t time_since_last_did_work_usec);

uint64_t v3_cycle_diff_in_usec(struct guest_info *core, uint64_t earlier_cycles, uint64_t later_cycles);


#endif

#endif
