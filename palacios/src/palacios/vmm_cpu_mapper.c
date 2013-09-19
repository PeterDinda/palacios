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
#include <palacios/vmm_cpu_mapper.h>
#include <palacios/vmm_hashtable.h>

#ifndef V3_CONFIG_DEBUG_CPU_MAPPER
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

static char default_strategy[] = "default";
static struct hashtable * master_cpu_mapper_table = NULL;
static int create_default_cpu_mapper();
static int destroy_default_cpu_mapper();

static struct vm_cpu_mapper_impl *cpu_mapper = NULL;

static uint_t cpu_mapper_hash_fn(addr_t key) {
    char * name = (char *)key;
    return v3_hash_buffer((uint8_t *)name, strlen(name));
}

static int cpu_mapper_eq_fn(addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;

    return (strcmp(name1, name2) == 0);
}

int V3_init_cpu_mapper() {

     PrintDebug(VM_NONE, VCORE_NONE,"Initializing cpu_mapper");

    master_cpu_mapper_table = v3_create_htable(0, cpu_mapper_hash_fn, cpu_mapper_eq_fn);
    return create_default_cpu_mapper();
}


int V3_deinit_cpu_mapper() {

    destroy_default_cpu_mapper();
    v3_free_htable(master_cpu_mapper_table, 1, 1);
    return 0;
}

int v3_register_cpu_mapper(struct vm_cpu_mapper_impl *s) {

    PrintDebug(VM_NONE, VCORE_NONE,"Registering cpu_mapper (%s)\n", s->name);

    if (v3_htable_search(master_cpu_mapper_table, (addr_t)(s->name))) {
        PrintError(VM_NONE, VCORE_NONE, "Multiple instances of cpu_mapper (%s)\n", s->name);
        return -1;
    }

    if (v3_htable_insert(master_cpu_mapper_table,
                         (addr_t)(s->name),
                         (addr_t)(s)) == 0) {
        PrintError(VM_NONE, VCORE_NONE, "Could not register cpu_mapper (%s)\n", s->name);
        return -1;
    }

    return 0;
}

struct vm_cpu_mapper_impl *v3_unregister_cpu_mapper(char *name) {

    PrintDebug(VM_NONE, VCORE_NONE,"Unregistering cpu_mapper (%s)\n",name);

    struct vm_cpu_mapper_impl *f = (struct vm_cpu_mapper_impl *) v3_htable_remove(master_cpu_mapper_table,(addr_t)(name),0);

    if (!f) { 
	PrintError(VM_NONE,VCORE_NONE,"Could not find cpu_mapper (%s)\n",name);
	return NULL;
    } else {
	return f;
    }
}


struct vm_cpu_mapper_impl *v3_cpu_mapper_lookup(char *name)
{
    return (struct vm_cpu_mapper_impl *)v3_htable_search(master_cpu_mapper_table, (addr_t)(name));
}

int V3_enable_cpu_mapper() {
    char *mapper_name;

    cpu_mapper = NULL;
    mapper_name = v3_lookup_option("cpu_mapper");

    if (mapper_name) {
	cpu_mapper = v3_cpu_mapper_lookup(mapper_name);
    }

    if (!cpu_mapper) {
        cpu_mapper = v3_cpu_mapper_lookup(default_strategy);
    }

    if (!cpu_mapper) {
	PrintError(VM_NONE, VCORE_NONE,"Specified Palacios cpu_mapper \"%s\" not found.\n", default_strategy);
	return -1;
    }

    PrintDebug(VM_NONE, VCORE_NONE,"cpu_mapper %s found",cpu_mapper->name);

    if (cpu_mapper->init) {
	return cpu_mapper->init();
    } else {
	return 0;
    }
}

int V3_disable_cpu_mapper()
{
    if (cpu_mapper->deinit) { 
	return cpu_mapper->deinit();
    } else {
	return 0;
    }
}

int v3_cpu_mapper_register_vm(struct v3_vm_info *vm,unsigned int cpu_mask) {
    if (cpu_mapper->vm_init) {
	return cpu_mapper->vm_init(vm,cpu_mask);
    } else {
	return 0;
    }
}

int v3_cpu_mapper_admit_vm(struct v3_vm_info *vm) {
    if (cpu_mapper->admit) {
	return cpu_mapper->admit(vm);
    } else {
	return 0;
    }
}


int v3_cpu_mapper_admit_core(struct v3_vm_info * vm, int vcore_id, int target_cpu) {
    if (cpu_mapper->admit_core) {
	return cpu_mapper->admit_core(vm,vcore_id,target_cpu);
    } else {
	return 0;
    }
}



int default_mapper_vm_init(struct v3_vm_info *vm, unsigned int cpu_mask)
{

    PrintDebug(vm, VCORE_NONE,"mapper. default_mapper_init\n");

    uint32_t i;
    int vcore_id = 0;
    uint8_t * core_mask = (uint8_t *)&cpu_mask;

    for (i = 0, vcore_id = vm->num_cores - 1; vcore_id >= 0; i++) {

        int major = 0;
	int minor = 0;
	struct guest_info * core = &(vm->cores[vcore_id]);
	char * specified_cpu = v3_cfg_val(core->core_cfg_data, "target_cpu");
	uint32_t core_idx = 0;

	if (specified_cpu != NULL) {
	    core_idx = atoi(specified_cpu);

	    if (core_idx < 0) {
		PrintError(vm, VCORE_NONE, "Target CPU out of bounds (%d) \n", core_idx);
	    }

	    i--; // We reset the logical core idx. Not strictly necessary I guess...
	} else {
	    core_idx = i;
	}

	major = core_idx / 8;
	minor = core_idx % 8;

	if ((core_mask[major] & (0x1 << minor)) == 0) {
	    PrintError(vm, VCORE_NONE, "Logical CPU %d not available for virtual core %d; not started\n",
		       core_idx, vcore_id);

	    if (specified_cpu != NULL) {
		PrintError(vm, VCORE_NONE, "CPU was specified explicitly (%d). HARD ERROR\n", core_idx);
		v3_stop_vm(vm);
		return -1;
	    }

	    continue;

	}

    core->pcpu_id = core_idx;
    vcore_id--;
    }

    if (vcore_id >= 0) {
	v3_stop_vm(vm);
	return -1;
    }

    return 0;

}

int default_mapper_admit_core(struct v3_vm_info * vm, int vcore_id, int target_cpu){
    return 0;
}


int default_mapper_admit(struct v3_vm_info *vm){
    return 0;
}


static struct vm_cpu_mapper_impl default_mapper_impl = {
    .name = "default",
    .init = NULL,
    .deinit = NULL,
    .vm_init = default_mapper_vm_init,
    .vm_deinit = NULL,
    .admit_core = default_mapper_admit_core,
    .admit = default_mapper_admit

};

static int create_default_cpu_mapper()
{
	v3_register_cpu_mapper(&default_mapper_impl);
	return 0;
}

static int destroy_default_cpu_mapper()
{
	v3_unregister_cpu_mapper(default_mapper_impl.name);
	return 0;
}
