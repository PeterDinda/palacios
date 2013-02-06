/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
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
#include <palacios/vmm_edf_sched.h>



#ifndef V3_CONFIG_DEBUG_EDF_SCHED
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
 * The runqueue is sorted each time that a vCPU becomes runnable. At that time the vCPU is 
 * enqueue and a new scheduling decision is taken. Each time a vCPU is scheduled, the parameter
 * slice used time is set to zero and the current deadline is calculated using its period. Once
 * the vCPU uses the logical core for slice seconds, that vCPU sleeps until its next scheduling 
 * period (when is re-inserted in the runqueue) and  yields the CPU to allow the scheduling 
 * of the vCPU with best priority in the runqueue. 
 */

// Default configuration values for the EDF Scheduler
// time parameters in microseconds 

#define MAX_PERIOD 1000000000
#define MIN_PERIOD 50000
#define MAX_SLICE 1000000000
#define MIN_SLICE 10000
#define CPU_PERCENT 100


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
 * edf_sched_init: Initialize the run queue
 */

int 
edf_sched_init(struct v3_vm_info *vm){

    PrintDebug(vm, VCORE_NONE,"EDF Sched. Initializing vm %s\n", vm->name);

    struct vm_sched_state *sched_state = &vm->sched; 
    sched_state->priv_data = V3_Malloc( vm->avail_cores * sizeof(struct vm_edf_rq));

    if (!sched_state->priv_data) {
	PrintError(vm, VCORE_NONE,"Cannot allocate in priv_data in edf_sched_init\n");
	return -1;
    }

    int lcore = 0;
  
    PrintDebug(vm, VCORE_NONE,"EDF Sched. edf_sched_init. Available cores %d\n", vm->avail_cores);

    for(lcore = 0; lcore < vm->avail_cores ; lcore++){

        PrintDebug(vm, VCORE_NONE,"EDF Sched. edf_sched_init. Initializing logical core %d\n", lcore);

        struct vm_edf_rq * edf_rq_list =   (struct vm_edf_rq *) sched_state->priv_data;
        struct vm_edf_rq * edf_rq = &edf_rq_list[lcore];
    
        edf_rq->vCPUs_tree = RB_ROOT;
        edf_rq->cpu_u=0;
        edf_rq->nr_vCPU=0;
        edf_rq->curr_vCPU=NULL;
        edf_rq->rb_leftmost=NULL;
        edf_rq->last_sched_time=0;
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

}


/*
 * count_cores: Function useful to count the number of cores in a runqueue (Not used for now)
 *
 */


/*static int count_cores(struct vm_edf_rq *runqueue){

  struct rb_node *node = v3_rb_first(&runqueue->vCPUs_tree);
  struct vm_core_edf_sched *curr_core;
  int number_cores = 0;    

    while(node){
        
        curr_core = container_of(node, struct vm_core_edf_sched, node);
        node = v3_rb_next(node);
        number_cores++;
    }

   return number_cores;
}*/ 



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

    struct vm_edf_rq *runqueue_list = (struct vm_edf_rq *) info->vm_info->sched.priv_data;
    struct vm_edf_rq *runqueue = &runqueue_list[info->pcpu_id]; 
    return runqueue;
}


/*
 * wakeup_core: Wakeup a given vCPU thread
 */

static void 
wakeup_core(struct guest_info *info){

    struct vm_core_edf_sched *core = info->core_sched.priv_data;
    struct vm_edf_rq *runqueue = get_runqueue(info);

    if (!info->core_thread) {
              PrintError(info->vm_info, info,"ERROR: Tried to wakeup non-existent core thread vCPU_id %d \n",info->vcpu_id);
    } 
    else {

        PrintDebug(info->vm_info, info,"EDF Sched. run_next_core. vcpu_id %d, logical id %d, Total time %llu, Miss_deadlines %d, slice_overuses %d extra_time %llu, thread (%p)\n", 
            core->info->vcpu_id,
            core->info->pcpu_id,
            core->total_time,
            core->miss_deadline,
            core->slice_overuse,
            core->extra_time_given,
            (struct task_struct *)info->core_thread); 
       
       V3_Wakeup(info->core_thread);
       core->last_wakeup_time = get_curr_host_time(&core->info->time_state);
       runqueue->curr_vCPU = core;

    }

}


/*
 * activate_core - Moves a core to the red-black tree.
 * used time is set to zero and current deadline is calculated 
 */

static void 
activate_core(struct vm_core_edf_sched * core, struct vm_edf_rq *runqueue){
    
    if (is_admissible_core(core, runqueue)){
	     
        uint64_t curr_time_us = get_curr_host_time(&core->info->time_state);
        uint64_t curr_deadline = next_start_period(curr_time_us, core->period);
        
        core->current_deadline = curr_deadline;
        core->used_time=0; 
        core->remaining_time=core->slice; 
        
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
        
        /*
         * If this is the first time to be activated pick first earliest deadline core to wakeup.
         */

        if(core->last_wakeup_time == 0){

            struct vm_core_edf_sched *next_core;
        
            /*
     	     * Pick first earliest deadline core
             */
            struct rb_node *node = v3_rb_first(&runqueue->vCPUs_tree);
            next_core = container_of(node, struct vm_core_edf_sched, node);
          
            // Wakeup next_core
            wakeup_core(next_core->info);
       
            //Sleep old core
  
            V3_Sleep(0);
        }
        
      }
      else 
          PrintError(core->info->vm_info, core->info,"EDF Sched. activate_core. CPU cannot activate the core. It is not admissible");	
}


/*
 * edf_sched_core_init: Initializes per core data structure and 
 * calls activate function.
 */

int 
edf_sched_core_init(struct guest_info * info){

    struct vm_edf_rq *runqueue = get_runqueue(info);
    struct vm_core_edf_sched *core_edf;

    PrintDebug(info->vm_info, info,"EDF Sched. Initializing vcore %d\n", info->vcpu_id);

    core_edf = (struct vm_core_edf_sched *) V3_Malloc(sizeof (struct vm_core_edf_sched));
    if (!core_edf) {
	PrintError(info->vm_info, info,"Cannot allocate private_data in edf_sched_core_init\n");
	return -1;
    }
    info->core_sched.priv_data = core_edf;
    
    // Default configuration if not specified in configuration file  
  
    core_edf->info = info; 
    core_edf->period = 500000;
    core_edf->slice = 50000;
    core_edf->used_time = 0;
    core_edf->last_wakeup_time = 0;
    core_edf->remaining_time = core_edf->slice;  
    core_edf->miss_deadline = 0;
    core_edf->extra_time = true;
    core_edf->total_time = 0;
    core_edf->slice_overuse = 0;
    core_edf->extra_time_given = 0;

    v3_cfg_tree_t * cfg_tree = core_edf->info->vm_info->cfg_data->cfg;
    v3_cfg_tree_t * core = v3_cfg_subtree(v3_cfg_subtree(cfg_tree, "cores"), "core");
    
    while (core){
        char *id = v3_cfg_val(core, "vcpu_id");
        char *period = v3_cfg_val(core, "period");
        char *slice = v3_cfg_val(core, "slice");
        char *extra_time = v3_cfg_val(core, "extra_time");
        
        if (atoi(id) == core_edf->info->vcpu_id){
   
            core_edf->period = atoi(period);
            core_edf->slice = atoi(slice);
            core_edf->remaining_time = core_edf->slice;  
            if (strcasecmp(extra_time, "true") == 0)
                core_edf->extra_time = true;
            else    
                core_edf->extra_time = false;
            break;
        }
        core = v3_cfg_next_branch(core);
    }

    activate_core(core_edf,runqueue); 
    return 0; 
}

/*
 * search_core_edf: Searches a core in the red-black tree by using its vcpu_id
 */
static struct vm_core_edf_sched * 
search_core_edf(struct vm_core_edf_sched *core_edf, struct vm_edf_rq *runqueue){

    struct rb_node *node = runqueue->vCPUs_tree.rb_node;
	
    while (node) {
     
        struct vm_core_edf_sched *core = container_of(node, struct vm_core_edf_sched, node);
	
        if (core_edf->current_deadline < core->current_deadline)
            node = node->rb_left;
	else if (core_edf->current_deadline > core->current_deadline)
	    node = node->rb_right;
        else
            if(core->info->vcpu_id == core_edf->info->vcpu_id){
                return core;
            }
    }
    return NULL;
}


/* 
 * delete_core_edf: Deletes a core from the red black tree, generally when it has 
 * consumed its time slice within the current period.
 */

static bool 
delete_core_edf( struct vm_core_edf_sched *core_edf  , struct vm_edf_rq *runqueue){

    struct vm_core_edf_sched *core = search_core_edf(core_edf, runqueue);
        if (core){ 

            v3_rb_erase(&core->node, &runqueue->vCPUs_tree);  
    	    return true;
        } 
	else{
	    PrintError(core->info->vm_info, core->info,"EDF Sched. delete_core_edf.Attempted to erase unexisting core");
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
 * pick_next_core: Returns the next core to be scheduled from the red black tree
 */

static struct vm_core_edf_sched * 
pick_next_core(struct vm_edf_rq *runqueue){
  
  
    /*
     * Pick first earliest deadline core
     */
    struct rb_node *node = v3_rb_first(&runqueue->vCPUs_tree);
    struct vm_core_edf_sched *next_core = container_of(node, struct vm_core_edf_sched, node);
 
    /* 
     * Verify if the earliest deadline core has used its complete slice and return it if not
     */

    if (next_core->used_time < next_core->slice){
        if(next_core->current_deadline < get_curr_host_time(&next_core->info->time_state))
            next_core->miss_deadline++; 
        return next_core;
    }
    /*
     * If slice used, pick the next core that has not used its complete slice    
     */

    else {  
        while(next_core->used_time >= next_core->slice){
            
            if(next_core->current_deadline < get_curr_host_time(&next_core->info->time_state) || !next_core->extra_time ){

                deactivate_core(next_core,runqueue); 
                activate_core(next_core,runqueue);
           
            }            

            node = v3_rb_next(node);
            if(node){
                next_core = container_of(node, struct vm_core_edf_sched, node);
            }
            else{   
                node = v3_rb_first(&runqueue->vCPUs_tree); // If all cores have used its slice return the first one
            return container_of(node, struct vm_core_edf_sched, node);
            }   

        }
    }

    return next_core;
}


static void 
adjust_slice(struct guest_info * info, int used_time, int extra_time)
{
    struct vm_core_edf_sched *core = info->core_sched.priv_data;
    struct vm_edf_rq *runqueue = get_runqueue(info);

    core->used_time = used_time;
 
    if (extra_time >= 0) {
	core->used_time += extra_time;
    }

    if( core->used_time >= core->slice){     
        deactivate_core(core,runqueue);
        activate_core(core,runqueue);
    }
}


/*
 * run_next_core: Pick next core to be scheduled and wakeup it
 */

static void 
run_next_core(struct guest_info *info, int used_time, int usec)
{
    struct vm_core_edf_sched *core = info->core_sched.priv_data;
    struct vm_core_edf_sched *next_core;
    struct vm_edf_rq *runqueue = get_runqueue(info);
   
    /* The next core to be scheduled is choosen from the tree (Function pick_next_core). 
     * The selected core is the one with the earliest deadline and with available time 
     * to use within the current period (used_time < slice)   
     */
   
     next_core = pick_next_core(runqueue); // Pick next core to schedule
          
     if (core != next_core){

         // Wakeup next_core
         wakeup_core(next_core->info);
         core->total_time += used_time;

        if (used_time > core->slice){
            core->slice_overuse++;
            core->extra_time_given += (used_time - core->slice);
        }

         // Sleep old core
  
         V3_Sleep(usec);
       
       }
}


/*
 * edf_schedule: Scheduling function
 */

static void
edf_schedule(struct guest_info * info, int usec){

    uint64_t host_time = get_curr_host_time(&info->time_state);
    struct vm_edf_rq *runqueue = get_runqueue(info);  
    struct vm_core_edf_sched *core = (struct vm_core_edf_sched *) info->core_sched.priv_data;

    uint64_t used_time = 0;
    if(core->last_wakeup_time != 0) 
        used_time =  host_time - core->last_wakeup_time;

    if(usec == 0) runqueue->last_sched_time = host_time; // Called from edf_sched_scheduled
    adjust_slice(core->info, host_time - core->last_wakeup_time, usec);

    run_next_core(core->info,used_time, usec);
    return;

}

/*
 * edf_sched_schedule: Main scheduling function. Computes amount of time in period left,
 * recomputing the current core's deadline if it has expired, then runs
 * scheduler 
 * It is called in the following cases:
 *    A vCPU becomes runnable
 *    The slice of the current vCPU was used
 *    The period of a vCPU in the runqueue starts
 *    Other case?? 
 * TODO Something to do with extra time?
 * TODO Check the use of remaining_time
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
 * edf_sched_deinit: Frees edf scheduler data structures
 */


int 
edf_sched_deinit(struct v3_vm_info *vm)
{

    struct vm_scheduler  * sched = vm->sched.sched;
    void *priv_data = vm->sched.priv_data;
    
    if (sched) 
        V3_Free(sched); 

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

    struct vm_scheduler  * sched = core->core_sched.sched;
    void *priv_data = core->core_sched.priv_data;
    
    if (sched) 
        V3_Free(sched); 

    if (priv_data) 
        V3_Free(priv_data);

    return 0;
}

static struct vm_scheduler_impl edf_sched = {
	.name = "edf",
	.init = edf_sched_init,
	.deinit = edf_sched_deinit,
	.core_init = edf_sched_core_init,
	.core_deinit = edf_sched_core_deinit,
	.schedule = edf_sched_schedule,
	.yield = edf_sched_yield
};

static int 
ext_sched_edf_init() {
	
    PrintDebug(VM_NONE, VCORE_NONE,"Sched. Creating (%s) scheduler\n",edf_sched.name);
    return v3_register_scheduler(&edf_sched);
}

static int 
ext_sched_edf_vm_init() {
    return 0;
}

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
