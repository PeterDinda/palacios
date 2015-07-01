/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2013, Oscar Mondragon <omondrag@cs.unm.edu>
 * Copyright (c) 2013, Patrick G. Bridges <bridges@cs.unm.edu>
 * Copyright (c) 2013, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * Author: Oscar Mondragon <omondrag@cs.unm.edu>
 *         Patrick G. Bridges <bridges@cs.unm.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm.h>
#include <palacios/vmm_time.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_hashtable.h>
#include <palacios/vmm_config.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_rbtree.h>


#ifndef V3_CONFIG_DEBUG_EXT_SCHED_EDF
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

/* Overview
 *
 * EDF Scheduling
 *
 * The EDF scheduler uses a dynamic calculated priority as scheduling criteria to choose
 * what thread will be scheduled.That priority is calculated according with the relative
 * deadline of the threads that are ready to run in the runqueue. This runqueue is a per-logical
 * core data structure used to keep the runnable virtual cores (threads) allocated to that
 * logical core.The threads with less time before its deadline will receive better priorities.
 * The runqueue is sorted each time that a vCPU becomes runnable or when its period is over.
 * At that time the vCPU is enqueue. A new scheduling decision is taken is time that an interruption
 * is received. At that time if the earliest deadline core is different that the current core,
 * the current core goes to sleep and the earliest deadline core is wake up. Each time a vCPU is scheduled,
 * the parameter "used_time" is set to zero and the current deadline is calculated using its period.
 * One vCPU uses at least slice seconds of CPU.Some extra time can be allocated to that virtual core after
 * all the other virtual cores consumes their slices.
 */

// Default configuration values for the EDF Scheduler
// time parameters in microseconds

#define MAX_PERIOD 1000000000
#define MIN_PERIOD 50000
#define MAX_SLICE 1000000000
#define MIN_SLICE 50000
#define CPU_PERCENT 80
#define DEADLINE_INTERVAL 30000000 // Period in which the missed deadline ratio is checked

typedef uint64_t time_us;

/*
 * Per-core EDF Scheduling information
 */

struct vm_core_edf_sched {
    struct guest_info *info;              // Core data struct
    struct rb_node node;                  // red-black tree node
    time_us period;                       // Amount of time (us) during which the virtual core may received a CPU allocation
    time_us slice;                        // Minimum amount of time (us) received for the virtual core during each period
    time_us current_deadline;             // Time (us) at which current virtual core period ends
    time_us used_time;                    // Amount of time (us) used whiting the current period
    time_us last_wakeup_time;             // Time at which the last wakeup started for this virtual core
    time_us remaining_time;               // Remaining time (us) to reach the expected time allocated to the virtual core
    bool extra_time;                      // Specifies if the virtual core is eligible to receive extra CPU time (For now it is true always)
    uint64_t deadline;                    // Total number of deadlines
    int deadline_percentage;              // Percentage of total missed deadlines
    uint64_t miss_deadline;               // Number of times the core has missed deadlines
    uint64_t deadline_interval;           // Total number of deadlines in the latest interval
    int deadline_percentage_interval;     // Percentage of missed deadlines in the latest interval
    uint64_t miss_deadline_interval;      // Number of times the core has missed deadlines in the last interval
    time_us print_deadline_interval;      // Last time missed deadline ratio was printed
    time_us total_time;                   // Total scheduled time for this virtual core.
    int slice_overuse;                    // Number of times a core consumes extra time
    time_us extra_time_given;             // Total extra time given to a virtual core
    time_us start_time;                   // Time at which this virtual core start to be scheduled
    time_us expected_time;                // Minimum CPU time expected to be allocated to this virtual core

};

/*
 * Scheduler configuration
 */

struct vm_edf_sched_config {
    time_us min_slice;       // Minimum allowed slice
    time_us max_slice;       // Maximum allowed slice
    time_us min_period;      // Minimum allowed period
    time_us max_period;      // Maximum allowed period
    int cpu_percent;         // Percentange of CPU utilization for the scheduler in each physical CPU (100 or less)

};

/*
 * Run queue structure. Per-logical core data structure  used to keep the runnable virtual cores (threads) allocated to that logical core
 * Contains a pointer to the red black tree, the data structure of configuration options, and other info
 */

struct vm_edf_rq{

    int cpu_u;	                                // CPU utilization (must be less or equal to the cpu_percent in vm_edf_sched_config)
    struct rb_root vCPUs_tree;	                // Red-Black Tree
    struct vm_edf_sched_config edf_config;	// Scheduling configuration structure
    int nr_vCPU;	                        // Number of cores in the runqueue
    struct vm_core_edf_sched *curr_vCPU;	// Current running CPU
    time_us last_sched_time;                    // Last time a virtual core in this runqueue was scheduled
    time_us check_deadline_time;                // Last time in which the deadlines were recalculated
    time_us smallest_period;                    // Smallest period of the virtual cores in the runqueue, used to control the period of deadlines recalculation
    time_us print_time;                         // Last time of debugging print
    time_us start_time;                         // Time at which first core started running in this runqueue
    int sched_low;                              // Incremented when the time between interruptions is large
    bool yielded;                               // CPU yielded to palacios (No core running)
};

/*
 * Basic functions for scheduling
 */

int v3_init_edf_scheduling();

/*
 * Debugging Function
 */

static void
print_parameters(time_us host_time, struct vm_edf_rq * runqueue, struct vm_core_edf_sched * core){

    time_us elapsed_time = 0;
    if(runqueue->start_time != 0)
        elapsed_time = host_time - runqueue->start_time;

    PrintDebug(core->info->vm_info, core->info,"Test: %llu %llu %d %d %llu %llu %llu %d\n",
             host_time,
	     elapsed_time,
             core->info->vcpu_id,
             core->info->pcpu_id,
             core->total_time,
	     core->expected_time,
	     core->miss_deadline,
	     core->deadline_percentage);

   runqueue->print_time = host_time;
}


/*
 * init_edf_config: Initialize scheduler configuration
 */

static void
init_edf_config(struct vm_edf_sched_config *edf_config){

    edf_config->min_slice = MIN_SLICE;
    edf_config->max_slice = MAX_SLICE;
    edf_config->min_period = MIN_PERIOD;
    edf_config->max_period = MAX_PERIOD;
    edf_config->cpu_percent = CPU_PERCENT;
}


/*
 * priv_data_init: Initialize the run queue
 */

int
priv_data_init(struct v3_vm_info *vm){

    PrintDebug(vm, VCORE_NONE,"EDF Sched. Initializing EDF Scheduling \n");

    vm->sched_priv_data = V3_Malloc( vm->avail_cores * sizeof(struct vm_edf_rq));

    if (!vm->sched_priv_data) {
	PrintError(vm, VCORE_NONE,"Cannot allocate in priv_data in priv_data_init\n");
	return -1;
    }

    int lcore = 0;

    PrintDebug(vm, VCORE_NONE,"EDF Sched. priv_data_init. Available cores %d\n", vm->avail_cores);

    for(lcore = 0; lcore < vm->avail_cores ; lcore++){

        PrintDebug(vm, VCORE_NONE,"EDF Sched. priv_data_init. Initializing logical core %d\n", lcore);

        struct vm_edf_rq * edf_rq_list =   (struct vm_edf_rq *)vm->sched_priv_data;
        struct vm_edf_rq * edf_rq = &edf_rq_list[lcore];

        edf_rq->vCPUs_tree = RB_ROOT;
        edf_rq->cpu_u=0;
        edf_rq->nr_vCPU=0;
        edf_rq->curr_vCPU=NULL;
        edf_rq->last_sched_time=0;
        edf_rq->check_deadline_time=0;
        edf_rq->sched_low=0;
        edf_rq->smallest_period=0;
        edf_rq->start_time = 0;
        edf_rq->yielded=false;
        init_edf_config(&edf_rq->edf_config);

    }

   return 0;

}

/*
 * is_admissible_core: Decides if a core is admited to the red black tree according with
 * the admisibility formula.
 */

static bool
is_admissible_core(struct vm_core_edf_sched * new_sched_core, struct vm_edf_rq *runqueue){

    int curr_utilization = runqueue->cpu_u;
    int new_utilization = curr_utilization + (100 * new_sched_core->slice / new_sched_core->period);
    int cpu_percent = (runqueue->edf_config).cpu_percent;

    if (new_utilization <= cpu_percent)
        return true;
    else
	return false;

return true;
}



/*
 * insert_core_edf: Finds a place in the tree for a newly activated core, adds the node
 * and rebalaces the tree
 */

static bool
insert_core_edf(struct vm_core_edf_sched *core, struct vm_edf_rq *runqueue){

    struct rb_node **new_core = &(runqueue->vCPUs_tree.rb_node);
    struct rb_node *parent = NULL;
    struct vm_core_edf_sched *curr_core;

    // Find out place in the tree for the new core
    while (*new_core) {

        curr_core = container_of(*new_core, struct vm_core_edf_sched, node);
        parent = *new_core;

	if (core->current_deadline < curr_core->current_deadline)
	    new_core = &((*new_core)->rb_left);
	else if (core->current_deadline > curr_core->current_deadline)
	    new_core = &((*new_core)->rb_right);
        else // Is Possible to have same current deadlines in both cores!
            return false;
    }
    // Add new node and rebalance tree.
    rb_link_node(&core->node, parent, new_core);
    v3_rb_insert_color(&core->node, &runqueue->vCPUs_tree);

    return true;
 }


/*
 * get_curr_host_time: Calculates the current host time (microseconds)
 */

static uint64_t
get_curr_host_time(struct vm_core_time *core_time){

    uint64_t cur_cycle = v3_get_host_time(core_time);
    uint64_t cpu_khz = core_time->host_cpu_freq;
    uint64_t curr_time_us = 1000 * cur_cycle / cpu_khz;

    return curr_time_us;

}


/*
 * count_cores: Function useful to count the number of cores in a runqueue (Not used for now)
 *
 */

/*
static int count_cores(struct vm_edf_rq *runqueue){

  struct rb_node *node = v3_rb_first(&runqueue->vCPUs_tree);
  struct vm_core_edf_sched *curr_core;
  int number_cores = 0;

    while(node){

        number_cores++;
        curr_core = container_of(node, struct vm_core_edf_sched, node);
        time_us host_time =  get_curr_host_time(&curr_core->info->time_state);
        PrintDebug(VM_NONE,VCORE_NONE,"Count %d. core %d, used time %llu, deadline %llu, host time  %llu extra_time %llu\n",
		   number_cores,
		   curr_core->info->vcpu_id,
		   curr_core->used_time,
		   curr_core->current_deadline,
		   host_time,
		   curr_core->extra_time_given);
        node = v3_rb_next(node);

    }

   return number_cores;
}
*/

/*
 * next_start_period: Given the current host time and the period of a given vCPU,
 * calculates the time in which its next period starts.
 *
 */

static uint64_t
next_start_period(uint64_t curr_time_us, uint64_t period_us){

    uint64_t time_period_us = curr_time_us % period_us;
    uint64_t remaining_time_us = period_us - time_period_us;
    uint64_t next_start_us = curr_time_us + remaining_time_us;

    return next_start_us;

}

/*
 * get_runqueue: Get the runqueue assigned to a virtual core.
 */

struct vm_edf_rq * get_runqueue(struct guest_info *info){

    struct vm_edf_rq *runqueue_list = (struct vm_edf_rq *) info->vm_info->sched_priv_data;
    struct vm_edf_rq *runqueue = &runqueue_list[info->pcpu_id];
    return runqueue;
}


/*
 * wakeup_core: Wakeup a given vCPU thread
 */

static void
wakeup_core(struct guest_info *info){

    struct vm_core_edf_sched *core = info->sched_priv_data;
    struct vm_edf_rq *runqueue = get_runqueue(info);
    time_us host_time = get_curr_host_time(&core->info->time_state);

    if (!info->core_thread) {
              PrintError(info->vm_info, info,"ERROR: Tried to wakeup non-existent core thread vCPU_id %d \n",info->vcpu_id);
    }
    else {

       V3_Wakeup(info->core_thread);
       if(core->start_time == 0){
            core->start_time = host_time;
            print_parameters(host_time, runqueue, core);
       }
       core->last_wakeup_time = host_time;
       runqueue->curr_vCPU = core;

    }

}


/*
 * activate_core - Moves a core to the red-black tree.
 * used time is set to zero and current deadline is calculated
 */

static int
activate_core(struct vm_core_edf_sched * core, struct vm_edf_rq *runqueue){


    if (is_admissible_core(core, runqueue)){

        uint64_t curr_time_us = get_curr_host_time(&core->info->time_state);
        uint64_t curr_deadline = next_start_period(curr_time_us, core->period);

        core->current_deadline = curr_deadline;
        core->used_time=0;

        bool ins = insert_core_edf(core, runqueue);
        /*
         * If not inserted is possible that there is other core with the same deadline.
         * Then, the deadline is modified and try again
         */
        while(!ins){
            core->current_deadline ++;
            ins = insert_core_edf(core, runqueue);
        }

        runqueue->cpu_u += 100 * core->slice / core->period;
        runqueue->nr_vCPU ++;

        if(runqueue->nr_vCPU == 1){
            runqueue->smallest_period = core->period;
        }

        else if(core->period < runqueue->smallest_period){
	  runqueue->smallest_period = core->period;
        }


        /*
         * If this is the first time to be activated pick first earliest deadline core to wakeup.
         */

         if(!runqueue->curr_vCPU){

	    struct vm_core_edf_sched *next_core;
           // Pick first earliest deadline core

            struct rb_node *node = v3_rb_first(&runqueue->vCPUs_tree);
            next_core = container_of(node, struct vm_core_edf_sched, node);

            PrintDebug(VM_NONE, VCORE_NONE,"EDF Sched. First time activation, core %d, time %llu \n", next_core->info->vcpu_id, curr_time_us);

            // Wakeup next_core (It is not neccessary to wakeup since in this case this is the first core launched by Palacios
	    // however wakeup_core is called to initialize some needed parameters.

            wakeup_core(next_core->info);
            next_core->start_time =  get_curr_host_time(&next_core->info->time_state);
            print_parameters(curr_time_us, runqueue, next_core);
            runqueue->start_time = next_core->start_time;


        }

      }
      else
          PrintError(core->info->vm_info, core->info,"EDF Sched. activate_core. CPU cannot activate the core. It is not admissible");
    return 0;

}


/*
 * edf_sched_core_init: Initializes per core data structure and
 * calls activate function.
 */


int
edf_sched_core_init(struct guest_info * info){

    struct vm_edf_rq *runqueue = get_runqueue(info);
    struct vm_core_edf_sched *core_edf;
    struct v3_time *vm_ts = &(info->vm_info->time_state);
    uint32_t tdf = vm_ts->td_denom;
    uint_t cpu_khz = V3_CPU_KHZ();

    PrintDebug(info->vm_info, info,"EDF Sched. Initializing vcore %d, tdf %d\n", info->vcpu_id,tdf);

    core_edf = (struct vm_core_edf_sched *) V3_Malloc(sizeof (struct vm_core_edf_sched));
    if (!core_edf) {
	PrintError(info->vm_info, info,"Cannot allocate private_data in edf_sched_core_init\n");
	return -1;
    }
    info->sched_priv_data = core_edf;

    // Default configuration if not specified in configuration file

    core_edf->info = info;
    core_edf->period = MIN_PERIOD;
    core_edf->slice = MIN_SLICE;
    core_edf->used_time = 0;
    core_edf->last_wakeup_time = 0;
    core_edf->remaining_time = 0;
    core_edf->deadline = 0;
    core_edf->miss_deadline = 0;
    core_edf->extra_time = true;
    core_edf->total_time = 0;
    core_edf->slice_overuse = 0;
    core_edf->extra_time_given = 0;
    core_edf->expected_time = 0;
    core_edf->start_time = 0;
    core_edf->deadline_interval = 0;
    core_edf->deadline_percentage_interval = 0;
    core_edf->miss_deadline_interval = 0;
    core_edf->print_deadline_interval = 0;


    v3_cfg_tree_t * cfg_tree = core_edf->info->vm_info->cfg_data->cfg;
    v3_cfg_tree_t * core = v3_cfg_subtree(v3_cfg_subtree(cfg_tree, "cores"), "core");

    while (core){
        char *id = v3_cfg_val(core, "vcpu_id");
        char *period = v3_cfg_val(core, "period");
        char *slice = v3_cfg_val(core, "slice");
        char *extra_time = v3_cfg_val(core, "extra_time");
        char *speed = v3_cfg_val(core, "khz");
        uint_t speed_khz = cpu_khz;

        if (atoi(id) == core_edf->info->vcpu_id){

            if(speed){
	        speed_khz = atoi(speed);
            }

            if(slice){
                core_edf->slice = atoi(slice);
            }
	    else{
	        core_edf->slice = MIN_SLICE;
            }

	    if(period){
	        core_edf->period = atoi(period);
            }
	    else{
	        core_edf->period = (core_edf->slice * cpu_khz * tdf)/speed_khz;
	        core_edf->period += 0.3*(100*core_edf->slice/core_edf->period); // Give faster vcores a little more bigger periods.
            }

	    PrintDebug(info->vm_info,info,"EDF_SCHED. Vcore %d, Pcore %d, cpu_khz %u, Period %llu Speed %d, Utilization %d, tdf %d %llu \n",
	           core_edf->info->vcpu_id,
                   core_edf->info->pcpu_id,
                   cpu_khz,
	           core_edf->period,
	           speed_khz,
		   (int)(100*core_edf->slice/core_edf->period),
		   tdf,
	           runqueue->smallest_period);

            if(extra_time){
                if (strcasecmp(extra_time, "true") == 0)
                    core_edf->extra_time = true;
                else
                    core_edf->extra_time = false;
            }
            else
                core_edf->extra_time = false;

            break;
        }
        core = v3_cfg_next_branch(core);
    }

    return activate_core(core_edf,runqueue);

}


/*
 * search_core_edf: Searches a core in the red-black tree by using its current_deadline
 */
static struct vm_core_edf_sched *
search_core_edf(time_us current_deadline, struct vm_edf_rq *runqueue){

    struct rb_node *node = runqueue->vCPUs_tree.rb_node;

    while (node) {
        struct vm_core_edf_sched *core = container_of(node, struct vm_core_edf_sched, node);

        if (current_deadline < core->current_deadline)
            node = node->rb_left;
	else if (current_deadline > core->current_deadline)
	    node = node->rb_right;
        else
            return core;
    }
    return NULL;
}


/*
 * delete_core_edf: Deletes a core from the red black tree, generally when it has
 * consumed its time slice within the current period.
 */

static bool
delete_core_edf( struct vm_core_edf_sched *core_edf  , struct vm_edf_rq *runqueue){

    struct vm_core_edf_sched *core = search_core_edf(core_edf->current_deadline, runqueue);
        if (core){

            v3_rb_erase(&core->node, &runqueue->vCPUs_tree);
	    return true;
        }
	else{
	    PrintError(VM_NONE,VCORE_NONE,"EDF Sched. delete_core_edf.Attempted to erase unexisting core");
            return false;
        }
}


/*
 * deactivate_core - Removes a core from the red-black tree.
 */

static void
deactivate_core(struct vm_core_edf_sched * core, struct vm_edf_rq *runqueue){

     if(delete_core_edf(core, runqueue)){
         runqueue->cpu_u -= 100 * core->slice / core->period;
         runqueue->nr_vCPU -- ;
     }
}

/*
 * adjust_slice - Adjust vcore parameters values
 */

static void adjust_slice(struct vm_core_edf_sched *core){

    time_us host_time = get_curr_host_time(&core->info->time_state);
    time_us used_time =  host_time - core->last_wakeup_time;

    if(core->last_wakeup_time != 0){

        core->used_time += used_time;
        core->total_time += used_time;
        if(core->start_time != 0){
            core->expected_time = ((host_time - core->start_time)/core->period)*core->slice;
	}
        if(core->total_time <= core->expected_time){
            core->remaining_time = core->expected_time - core->total_time;
	}
        else{
	    core->remaining_time =0;
	}

        if (core->used_time > core->slice){
            core->slice_overuse++;
        }
    }

}

/*
 * Check_deadlines - Check virtual cores, and re-insert in the runqueue the ones which deadline is over
 */


static void check_deadlines(struct vm_edf_rq *runqueue){

    struct rb_node *node = v3_rb_first(&runqueue->vCPUs_tree);
    struct vm_core_edf_sched *next_core = container_of(node, struct vm_core_edf_sched, node);
    time_us host_time = get_curr_host_time(&next_core->info->time_state);
    struct vm_core_edf_sched * deadline_cores[runqueue->nr_vCPU];


    int nr_dcores=0;
    int i=0;
    memset(deadline_cores, 0, runqueue->nr_vCPU);

    while(node){

        next_core = container_of(node, struct vm_core_edf_sched, node);
        if(next_core->current_deadline < host_time){
	    next_core->deadline++;
            next_core->deadline_interval++;
            deadline_cores[nr_dcores++]= next_core;
            if(next_core->used_time < next_core->slice){
                next_core->miss_deadline++;
                next_core->miss_deadline_interval++;

		/*PrintError(VM_NONE,VCORE_NONE,"Test: Core %d miss_deadlines %d, used time %llu, slice %llu, deadline %llu, host time %llu \n",
		       next_core->info->vcpu_id,
		       next_core->miss_deadline,
		       next_core->used_time,
                       next_core->slice,
                       next_core->current_deadline,
		       host_time);*/

            }
            else{
	      next_core->extra_time_given += (next_core->used_time - next_core->slice);

	      /* PrintError(VM_NONE,VCORE_NONE,"Test: Extra time, core %d, core used time %llu, slice %llu, extra time %llu, Total extra time %llu \n",
	         next_core->info->vcpu_id,
                 next_core->used_time,
	         next_core->slice,
	         (next_core->used_time - next_core->slice),
	         next_core->extra_time_given);*/

            }

        }
	else{
	    break;
        }
        node = v3_rb_next(node);
    }


    for(i=0;i<nr_dcores;i++){

        next_core = deadline_cores[nr_dcores-1-i];

        deactivate_core(next_core,runqueue);
        activate_core(next_core,runqueue);

    }
    runqueue->check_deadline_time = host_time;
}



/*
 * pick_next_core: Returns the next core to be scheduled from the red black tree
 */

static struct vm_core_edf_sched *
pick_next_core(struct vm_edf_rq *runqueue){

    /*
     * Pick first earliest deadline core
     */
    struct rb_node *node=NULL;
    struct vm_core_edf_sched *next_core=NULL;
    time_us core_deadline=0;         // Deadline of the core with more time remaining
    time_us core_extra_deadline=0;   // Deadline of the core with extra time enable and earliest deadline
    time_us max_remaining=0;
    time_us host_time = get_curr_host_time(&runqueue->curr_vCPU->info->time_state);

    if(host_time - runqueue->check_deadline_time >= runqueue->smallest_period ){
        check_deadlines(runqueue);
    }

    node = v3_rb_first(&runqueue->vCPUs_tree);
    next_core = container_of(node, struct vm_core_edf_sched, node);

    if (next_core->used_time < next_core->slice){
         return next_core;
    }

    if(next_core->total_time < next_core->expected_time){
        max_remaining = next_core->remaining_time;
        core_deadline = next_core->current_deadline;
    }

    if(next_core->extra_time){
        core_extra_deadline = next_core->current_deadline;
    }

    // Pick the next core that has not used its whole slice
    while(node){

        next_core = container_of(node, struct vm_core_edf_sched, node);

        if(next_core->remaining_time > max_remaining){
            max_remaining = next_core->extra_time_given;
            core_deadline = next_core->current_deadline;
            if(core_extra_deadline !=0 && next_core->extra_time){
	        core_extra_deadline = next_core->current_deadline;
            }
	}

        if(next_core->used_time < next_core->slice){
	  return next_core;
        }


        node = v3_rb_next(node);
    }

    if(core_extra_deadline !=0){
        next_core  = search_core_edf(core_extra_deadline, runqueue);
    }

    else if (core_deadline != 0){
        next_core  = search_core_edf(core_deadline, runqueue);
    }

     return NULL;
}


/*
 * run_next_core: Pick next core to be scheduled and wakeup it
 */

static void
run_next_core(struct guest_info *info, int usec)
{
    struct vm_core_edf_sched *core = info->sched_priv_data;
    struct vm_core_edf_sched *next_core;
    struct vm_edf_rq *runqueue = get_runqueue(info);
    time_us host_time = get_curr_host_time(&info->time_state);

     /* The next core to be scheduled is choosen from the tree (Function pick_next_core).
     * The selected core is the one with the earliest deadline and with available time
     * to use within the current period (used_time < slice)
     */
    if(!runqueue->yielded){
        adjust_slice(core);
    }
    next_core = pick_next_core(runqueue); // Pick next core to schedule

    if(host_time - core->print_deadline_interval >= DEADLINE_INTERVAL){

        if(core->deadline_interval != 0){
            core->deadline_percentage_interval = (int)100*core->miss_deadline_interval/core->deadline_interval;
        }

        PrintDebug(info->vm_info,info,"deadline: %d %llu %llu %d",
            core->info->vcpu_id,
	    core->deadline_interval,
	    core->miss_deadline_interval,
	    core->deadline_percentage_interval);

        core->print_deadline_interval = host_time;
        core->deadline_interval=0;
        core->miss_deadline_interval = 0;
        core->deadline_percentage_interval = 0;
    }

    if(!next_core){
        runqueue->yielded=true;
        V3_Yield();
        return;
    }
    runqueue->yielded=false;


    if(core->deadline != 0)
        core->deadline_percentage = (int)100*core->miss_deadline/core->deadline;

    if (core != next_core){

        print_parameters(host_time, runqueue,core);
        wakeup_core(next_core->info);
        V3_Sleep(usec);
     }

    else{
        // Necessary to update last_wakeup_time to adjust slice properly later
        next_core->last_wakeup_time = get_curr_host_time(&next_core->info->time_state);

        if(host_time - runqueue->print_time >= runqueue->smallest_period ){
            print_parameters(host_time, runqueue,core);
        }
     }

}


/*
 * edf_schedule: Scheduling function
 */

static void
edf_schedule(struct guest_info * info, int usec){

  uint64_t host_time = get_curr_host_time(&info->time_state);
  struct vm_edf_rq *runqueue = get_runqueue(info);

  /*PrintDebug(info->vm_info,info,"Test PCORE. %d host time: %llu Last sched_time: %llu, difference: %llu\n",
    info->pcpu_id,  host_time, runqueue->last_sched_time, host_time-runqueue->last_sched_time); */

  /* if( (runqueue->last_sched_time !=0) && ((host_time - runqueue->last_sched_time) > 10000)){
         PrintDebug(info->vm_info,info,"%d Test PCORE. %d LOW SCHED FREQUENCY  host time: %llu Last sched_time: %llu, difference: %llu\n",
		 ++runqueue->sched_low,info->pcpu_id,  host_time, runqueue->last_sched_time, host_time-runqueue->last_sched_time);
		 }*/

  runqueue->last_sched_time = host_time;
  run_next_core(info, usec);
    return;

}

/*
 * edf_sched_schedule: Main scheduling function. Called each time that an interruption occurs.
 *
 */

void
edf_sched_schedule(struct guest_info * info){

    edf_schedule(info, 0);
    return;
}

/*
 * edf_sched_yield: Called when yielding the logical cpu for usec is needed
 */

void
edf_sched_yield(struct guest_info * info, int usec){
    edf_schedule(info, usec);
    return;
}

/*
 * edf_sched_core_stop. Stops virtual machine. All the virtual cores yield the CPU
 */

int
edf_sched_core_stop(struct guest_info * info){

    struct vm_edf_rq * runqueue =  get_runqueue(info);
    struct rb_node *node = v3_rb_first(&runqueue->vCPUs_tree);
    struct vm_core_edf_sched *curr_core;

    while(node){

        curr_core = container_of(node, struct vm_core_edf_sched, node);
        wakeup_core(curr_core->info);
        PrintDebug(VM_NONE,VCORE_NONE,"Waking up core %d, thread (%p)\n",
		   curr_core->info->vcpu_id,
                 (struct task_struct *)info->core_thread);
        V3_Yield();
        PrintDebug(VM_NONE,VCORE_NONE,"Yielding Thread %p\n",(struct task_struct *)info->core_thread);
        node = v3_rb_next(node);

    }
   return 0;
}


/*
 * edf_sched_deinit: Frees edf scheduler data structures
 */


int
edf_sched_deinit(struct v3_vm_info *vm)
{
    PrintDebug(VM_NONE,VCORE_NONE,"Freeing vm\n");
    void *priv_data = vm->sched_priv_data;

    if (priv_data)
        V3_Free(priv_data);

    return 0;

}

/*
 * edf_sched_deinit: Frees virtual core data structures
 */

int
edf_sched_core_deinit(struct guest_info *core)
{
    PrintDebug(VM_NONE,VCORE_NONE,"Freeing core\n");
    void *priv_data = core->sched_priv_data;

    if (priv_data)
        V3_Free(priv_data);

    return 0;
}

/*
 * edf_sched_vm_int. Called when the VM starts
 */

int edf_sched_vm_init(struct v3_vm_info *vm){
    return 0;
}

int edf_sched_admit(struct v3_vm_info *vm){

    /*
     * Initialize priv_data for the vm:
     * For EDF this is done here because we need the parameter
     * avail_core which is set in v3_start_vm before the
     * v3_scheduler_admit_vm function is called.
     */

    priv_data_init(vm);

    // TODO Admission

    return 0;
}


/*
 * vm_scheduler_impl. Functions that implement vmm_scheduler interface
 */

static struct vm_scheduler_impl edf_sched = {

    .name = "edf",
    .init = NULL,
    .deinit = NULL,
    .vm_init = edf_sched_vm_init,
    .vm_deinit = edf_sched_deinit,
    .core_init = edf_sched_core_init,
    .core_stop = edf_sched_core_stop,
    .core_deinit = edf_sched_core_deinit,
    .schedule = edf_sched_schedule,
    .yield = edf_sched_yield,
    .admit = edf_sched_admit,
    .remap = NULL,
    .dvfs=NULL
};


/*
 * ext_sched_edf_init. Creates an register the EDF scheduler.
 */

static int
ext_sched_edf_init() {
    PrintDebug(VM_NONE, VCORE_NONE,"Sched. Creating (%s) scheduler\n",edf_sched.name);
    return v3_register_scheduler(&edf_sched);
}

/*
 * ext_sched_edf_vm_int. Called when the VM starts
 */

static int
ext_sched_edf_vm_init() {
    return 0;
}


/*
 * v3_extension_impl. EDF extension functions
 */


static struct v3_extension_impl sched_edf_impl = {
	.name = "EDF Scheduler",
	.init = ext_sched_edf_init,
	.vm_init = ext_sched_edf_vm_init,
        .vm_deinit = NULL,
	.core_init = NULL,
	.core_deinit = NULL,
	.on_entry = NULL,
	.on_exit = NULL
};

register_extension(&sched_edf_impl);
