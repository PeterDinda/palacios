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

#include <palacios/vmm.h>
#include <palacios/vm_guest.h>


void     v3_strategy_driven_yield(struct guest_info *core, uint64_t time_since_last_did_work_usec)
{
    // yield according to strategy
    switch (core->vm_info->perf_options.yield_strategy.strategy) { 
	case V3_YIELD_STRATEGY_GREEDY:
	    v3_yield(core,-1);
	    break;
	case V3_YIELD_STRATEGY_FRIENDLY:
	    v3_yield(core,core->vm_info->perf_options.yield_strategy.time_usec);
	    break;
	case V3_YIELD_STRATEGY_ADAPTIVE:
	    if (time_since_last_did_work_usec > core->vm_info->perf_options.yield_strategy.threshold_usec) { 
		v3_yield(core,core->vm_info->perf_options.yield_strategy.time_usec);
	    } else {
		v3_yield(core,-1);
	    }
	default:
	    PrintError("Unknown yield strategy (%d) using GREEDY\n",core->vm_info->perf_options.yield_strategy.strategy);
	    v3_yield(core,-1);
	    break;
    }
}



uint64_t v3_cycle_diff_in_usec(struct guest_info *core, uint64_t first, uint64_t second)
{
    uint64_t cycle_diff = second - first;
    uint64_t mhz = (core->time_state.host_cpu_freq) / 1000; // KHZ->MHZ

    return cycle_diff / mhz ;  // cycles / (millioncycles/sec) = sec/million = usec
}

static void set_yield_defaults(struct v3_vm_info *vm)
{
    vm->perf_options.yield_strategy.strategy = V3_DEFAULT_YIELD_STRATEGY;
    vm->perf_options.yield_strategy.threshold_usec = V3_DEFAULT_YIELD_THRESHOLD_USEC;
    vm->perf_options.yield_strategy.time_usec = V3_DEFAULT_YIELD_TIME_USEC;
}
    

static void set_yield(struct v3_vm_info *vm, v3_cfg_tree_t *cfg)

{
    char *t;

    set_yield_defaults(vm);
    
    // now override

    t = v3_cfg_val(cfg, "strategy");

    if (t) { 
	if (!strcasecmp(t,"greedy")) { 
	    vm->perf_options.yield_strategy.strategy = V3_YIELD_STRATEGY_GREEDY;
	    V3_Print("Setting yield strategy to GREEDY\n");
	} else if (!strcasecmp(t, "friendly")) { 
	    vm->perf_options.yield_strategy.strategy = V3_YIELD_STRATEGY_FRIENDLY;
	    V3_Print("Setting yield strategy to FRIENDLY\n");
	} else if (!strcasecmp(t, "adaptive")) { 
	    vm->perf_options.yield_strategy.strategy = V3_YIELD_STRATEGY_ADAPTIVE;
	    V3_Print("Setting yield strategy to ADAPTIVE\n");
	} else {
	    V3_Print("Unknown yield strategy '%s', using default\n",t);
	}
    } else {
	V3_Print("Yield strategy not given, using default\n");
    }

    t = v3_cfg_val(cfg, "threshold");
    
    if (t) { 
	vm->perf_options.yield_strategy.threshold_usec = atoi(t);
	V3_Print("Setting yield threshold to %llu\n",vm->perf_options.yield_strategy.threshold_usec);
    } else {
	V3_Print("Yield threshold not given, using default\n");
    }


    t = v3_cfg_val(cfg, "time");
    
    if (t) { 
	vm->perf_options.yield_strategy.time_usec = atoi(t);
	V3_Print("Setting yield time to %llu\n",vm->perf_options.yield_strategy.time_usec);
    } else {
	V3_Print("Yield time not given, using default\n");
    }
    
    
}

    


/*
<vm>
  <perftune>
     <group name="yield">
        <strategy>greedy,friendly,adaptive</strategy>
        <threshold>us</threshold>
        <time>us</time>
     </group>
     <group name="something else">
        <group-specific>....</group-specific>
     </group>
     ...
  </perftune>
</vm>
*/
int      v3_setup_performance_tuning(struct v3_vm_info *vm, v3_cfg_tree_t *cfg)
{
    v3_cfg_tree_t *t = v3_cfg_subtree(cfg,"perftune");
    
    if (!t) { 
	V3_Print("No performance tuning tree - using defaults\n");
	set_yield_defaults(vm);
	return 0;
    }

    t = v3_cfg_subtree(t,"group");
    
    while (t) {
	char *id = v3_cfg_val(t,"name");
	if (!id) { 
	    V3_Print("Skipping performance parameter group without name\n");
	} else {
	    if (!strcasecmp(id,"yield")) { 
		set_yield(vm,t);
	    } else {
		V3_Print("Skipping unknown performance parameter group\n");
	    }
	}
	t = v3_cfg_next_branch(t);
    }
    
    return 0;
}

