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

#include <palacios/vmm_time.h>

struct v3_yield_strategy {
    enum {
	V3_YIELD_STRATEGY_GREEDY=0,     // always untimed yields  
	V3_YIELD_STRATEGY_FRIENDLY,     // always timed yields with the following
	V3_YIELD_STRATEGY_ADAPTIVE,     // switch from untimed to timed after the threshold
    }         strategy;

    uint64_t  threshold_usec;   // the point at which we transiton from untimed to timed yield
    uint64_t  time_usec;        // the amount of time for a timed yield call

#define V3_DEFAULT_YIELD_STRATEGY       V3_YIELD_STRATEGY_FRIENDLY
#define V3_DEFAULT_YIELD_THRESHOLD_USEC 100
#define V3_DEFAULT_YIELD_TIME_USEC      10000
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

// The following three macros are intended to make it easy to
// use strategy-driven yield.  Call the first one when you are out of work
// then call the second when each time that you want to yield because you are
// out of work, and then call the third one when you have work to do again
//
// This assumes the thread is locked to a core and may behave strangely if 
// this is not the case.   

#define  V3_NO_WORK(core) {				   \
  uint64_t _v3_strat_local_first=0, _v3_strat_local_cur=0; \
  _v3_strat_local_first=v3_get_host_time(core ? &(core->time_state) : 0); 
  
  
#define  V3_STILL_NO_WORK(core)            \
  _v3_strat_local_cur=v3_get_host_time(core ? &(core->time_state) : 0);              \
  v3_strategy_driven_yield(core,v3_cycle_diff_in_usec(core,_v3_strat_local_first,_v3_strat_local_cur)); 

#define  V3_HAVE_WORK_AGAIN(core) }

#endif

#endif
