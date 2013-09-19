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

#ifndef __VMM_CPU_MAPPER_H__
#define __VMM_CPU_MAPPER_H__

struct vm_cpu_mapper_impl {
	char *name;
	int (*init)();
	int (*deinit)();
	int (*vm_init)(struct v3_vm_info *vm, unsigned int cpu_mask);
	int (*vm_deinit)(struct v3_vm_info *vm);
	int (*admit_core)(struct v3_vm_info * vm, int vcore_id, int target_cpu);
	int (*admit)(struct v3_vm_info *vm);
        // should really be departure options...

};

int v3_cpu_mapper_register_vm(struct v3_vm_info *vm, unsigned int cpu_mask);
int v3_cpu_mapper_admit_vm(struct v3_vm_info *vm);
int v3_cpu_mapper_admit_core(struct v3_vm_info * vm, int vcore_id, int target_cpu);

int V3_init_cpu_mapper();
int V3_deinit_cpu_mapper();

int v3_register_cpu_mapper(struct vm_cpu_mapper_impl *vm);
struct vm_cpu_mapper_impl *v3_unregister_cpu_mapper(char *name);
struct vm_cpu_mapper_impl *v3_cpu_mapper_lookup(char *name);

int V3_enable_cpu_mapper();
int V3_disable_cpu_mapper();

#endif /* __VMM_cpu_mapper_H__ */
