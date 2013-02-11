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
 * Copyright (c) 2013, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Oscar Mondragon <omondrag@cs.unm.edu>
 *         Patrick G. Bridges <bridges@cs.unm.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_SCHEDULER_H__
#define __VMM_SCHEDULER_H__

struct vm_scheduler_impl {
	char *name;
	int (*init)();
	int (*deinit)();
	int (*vm_init)(struct v3_vm_info *vm);
	int (*vm_deinit)(struct v3_vm_info *vm);
	int (*core_init)(struct guest_info *vm);
	int (*core_deinit)(struct guest_info *vm);
	void (*schedule)(struct guest_info *vm);
	void (*yield)(struct guest_info *vm, int usec);
	int (*admit)(struct v3_vm_info *vm);
	int (*remap)(struct v3_vm_info *vm);
	int (*dvfs)(struct v3_vm_info *vm);
};

struct vm_sched_state {
	struct vm_scheduler *sched;
	void *priv_data;
};

struct vm_core_sched_state {
	struct vm_scheduler *sched;
	void *priv_data;
};

void v3_schedule(struct guest_info *core);
void v3_yield(struct guest_info *core, int usec);

int v3_scheduler_register_vm(struct v3_vm_info *vm);
int v3_scheduler_register_core(struct guest_info *vm); /* ? */
int v3_scheduler_admit_vm(struct v3_vm_info *vm);

void v3_scheduler_remap_notify(struct v3_vm_info *vm);
void v3_scheduler_dvfs_notify(struct v3_vm_info *vm);

int V3_init_scheduling();
int v3_register_scheduler(struct vm_scheduler_impl *vm);
struct vm_scheduler_impl *v3_scheduler_lookup(char *name);
int V3_enable_scheduler();

#endif /* __VMM_SCHEDULER_H__ */
