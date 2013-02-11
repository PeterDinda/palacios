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
#include <palacios/vm_guest.h>
#include <palacios/vmm_scheduler.h>
#include <palacios/vmm_hashtable.h>

#ifndef V3_CONFIG_DEBUG_SCHEDULER
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

static char default_strategy[] = "host";
static struct hashtable * master_scheduler_table = NULL;
static int create_host_scheduler();

static struct vm_scheduler_impl *scheduler = NULL;

static uint_t scheduler_hash_fn(addr_t key) {
    char * name = (char *)key;
    return v3_hash_buffer((uint8_t *)name, strlen(name));
}

static int scheduler_eq_fn(addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;

    return (strcmp(name1, name2) == 0);
}

int V3_init_scheduling() {
   
     PrintDebug(VM_NONE, VCORE_NONE,"Initializing scheduler");

    master_scheduler_table = v3_create_htable(0, scheduler_hash_fn, scheduler_eq_fn);
    return create_host_scheduler();
}


int v3_register_scheduler(struct vm_scheduler_impl *s) {

    PrintDebug(VM_NONE, VCORE_NONE,"Registering Scheduler (%s)\n", s->name);

    if (v3_htable_search(master_scheduler_table, (addr_t)(s->name))) {
        PrintError(VM_NONE, VCORE_NONE, "Multiple instances of scheduler (%s)\n", s->name);
        return -1;
    }
  
    if (v3_htable_insert(master_scheduler_table,
                         (addr_t)(s->name),
                         (addr_t)(s)) == 0) {
        PrintError(VM_NONE, VCORE_NONE, "Could not register scheduler (%s)\n", s->name);
        return -1;
    }

    return 0;
}

struct vm_scheduler_impl *v3_scheduler_lookup(char *name)
{
    return (struct vm_scheduler_impl *)v3_htable_search(master_scheduler_table, (addr_t)(name));
}

int V3_enable_scheduler() {
    /* XXX Lookup the specified scheduler to use for palacios and use it */
    
    scheduler = v3_scheduler_lookup(default_strategy);
    PrintDebug(VM_NONE, VCORE_NONE,"Sched. Scheduler %s found",scheduler->name);
    
    if (!scheduler) {
	PrintError(VM_NONE, VCORE_NONE,"Specified Palacios scheduler \"%s\" not found.\n", default_strategy);
	return -1;
    }
    if (scheduler->init) {
	return scheduler->init();
    } else {
	return 0;
    }
}

int v3_scheduler_register_vm(struct v3_vm_info *vm) {
    if (scheduler->vm_init) {
    	return scheduler->vm_init(vm);
    } else {
	return 0;
    }
}
int v3_scheduler_register_core(struct guest_info *core) {
    if (scheduler->core_init) {
    	return scheduler->core_init(core);
    } else {
	return 0;
    }
}
int v3_scheduler_admit_vm(struct v3_vm_info *vm) {
    if (scheduler->admit) {
    	return scheduler->admit(vm);
    } else {
	return 0;
    }
}
int v3_scheduler_notify_remap(struct v3_vm_info *vm) {
    if (scheduler->remap) {
    	return scheduler->remap(vm);
    } else {
	return 0;
    }
}
int v3_scheduler_notify_dvfs(struct v3_vm_info *vm) {
    if (scheduler->dvfs) {
    	return scheduler->dvfs(vm);
    } else {
	return 0;
    }
}
void v3_schedule(struct guest_info *core) {
    if (scheduler->schedule) {
    	scheduler->schedule(core);
    }
    return;
}
void v3_yield(struct guest_info *core, int usec) {
    if (scheduler->yield) {
    	scheduler->yield(core, usec);
    } 
    return;
}

int host_sched_vm_init(struct v3_vm_info *vm)
{

    PrintDebug(vm, VCORE_NONE,"Sched. host_sched_init\n"); 

    char * schedule_hz_str = v3_cfg_val(vm->cfg_data->cfg, "schedule_hz");
    uint32_t sched_hz = 100; 	


    if (schedule_hz_str) {
	sched_hz = atoi(schedule_hz_str);
    }

    PrintDebug(vm, VCORE_NONE,"CPU_KHZ = %d, schedule_freq=%p\n", V3_CPU_KHZ(), 
	       (void *)(addr_t)sched_hz);

    uint64_t yield_cycle_period = (V3_CPU_KHZ() * 1000) / sched_hz;
    vm->sched_priv_data = (void *)yield_cycle_period; 

    return 0;
}

int host_sched_core_init(struct guest_info *core)
{
    PrintDebug(core->vm_info, core,"Sched. host_sched_core_init\n"); 

    uint64_t t = v3_get_host_time(&core->time_state); 
    core->sched_priv_data = (void *)t;

    return 0;
}

void host_sched_schedule(struct guest_info *core)
{
    uint64_t cur_cycle;
    cur_cycle = v3_get_host_time(&core->time_state);

    if (cur_cycle > ( (uint64_t)core->sched_priv_data + (uint64_t)core->vm_info->sched_priv_data)) {
	
        V3_Yield();
      
        uint64_t yield_start_cycle = (uint64_t) core->sched_priv_data;
        yield_start_cycle +=  (uint64_t)core->vm_info->sched_priv_data;
        core->sched_priv_data = (void *)yield_start_cycle;
      
    }
}

/* 
 * unconditional cpu yield 
 * if the yielding thread is a guest context, the guest quantum is reset on resumption 
 * Non guest context threads should call this function with a NULL argument
 *
 * usec <0  => the non-timed yield is used
 * usec >=0 => the timed yield is used, which also usually implies interruptible
 */
void host_sched_yield(struct guest_info * core, int usec) {
    uint64_t yield_start_cycle;
    if (usec < 0) {
        V3_Yield();
    } else {
        V3_Sleep(usec);
    }
    yield_start_cycle = (uint64_t) core->sched_priv_data
                        + (uint64_t)core->vm_info->sched_priv_data;
    core->sched_priv_data = (void *)yield_start_cycle;
}


int host_sched_admit(struct v3_vm_info *vm){
    return 0;
}

static struct vm_scheduler_impl host_sched_impl = {
    .name = "host",
    .init = NULL,
    .deinit = NULL,
    .vm_init = host_sched_vm_init,
    .vm_deinit = NULL,
    .core_init = host_sched_core_init,
    .core_deinit = NULL,
    .schedule = host_sched_schedule,
    .yield = host_sched_yield,
    .admit = host_sched_admit,
    .remap = NULL,
    .dvfs=NULL
};

static int create_host_scheduler()
{
	v3_register_scheduler(&host_sched_impl);
	return 0;
}
