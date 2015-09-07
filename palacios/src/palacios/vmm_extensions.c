/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>

#include <palacios/vmm_extensions.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_hashtable.h>


static struct hashtable * ext_table = NULL;

/*
 * This is a place holder to ensure that the _v3_extensions section gets created by gcc
 */
static struct {} null_ext  __attribute__((__used__))                    \
    __attribute__((unused, __section__ ("_v3_extensions"),                \
                   aligned(sizeof(addr_t))));



static uint_t ext_hash_fn(addr_t key) {
    char * name = (char *)key;
    return v3_hash_buffer((uint8_t *)name, strlen(name));
}

static int ext_eq_fn(addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;

    return (strcmp(name1, name2) == 0);
}



int V3_init_extensions() {
    extern struct v3_extension_impl * __start__v3_extensions[];
    extern struct v3_extension_impl * __stop__v3_extensions[];
    struct v3_extension_impl ** tmp_ext = __start__v3_extensions;
    int i = 0;

    ext_table = v3_create_htable(0, ext_hash_fn, ext_eq_fn);

    while (tmp_ext != __stop__v3_extensions) {

        if (!(*tmp_ext)) {
	    PrintError(VM_NONE, VCORE_NONE, "Impossible extension\n");
	    return -1;
	}
	
	if ((*tmp_ext)->init && ((*tmp_ext)->init() != 0)) {
	    PrintError(VM_NONE, VCORE_NONE, "Could not initialize extension (%s)\n", (*tmp_ext)->name);
	    return -1;
	} 

        V3_Print(VM_NONE, VCORE_NONE, "Registering Extension (%s)\n", (*tmp_ext)->name);

	if (v3_htable_search(ext_table, (addr_t)((*tmp_ext)->name))) {
	    PrintError(VM_NONE, VCORE_NONE, "Multiple instances of Extension (%s)\n", (*tmp_ext)->name);
	    return -1;
	}

	if (v3_htable_insert(ext_table, (addr_t)((*tmp_ext)->name), (addr_t)(*tmp_ext)) == 0) {
	    PrintError(VM_NONE, VCORE_NONE, "Could not register Extension (%s)\n", (*tmp_ext)->name);
	    return -1;
	}

	tmp_ext = &(__start__v3_extensions[++i]);
    }

    return 0;
}




int V3_deinit_extensions() {
    v3_free_htable(ext_table, 0, 0);

    return 0;
}


int v3_init_ext_manager(struct v3_vm_info * vm) {
    struct v3_extensions * ext_state = &(vm->extensions);

    INIT_LIST_HEAD(&(ext_state->extensions));
    INIT_LIST_HEAD(&(ext_state->on_exits));
    INIT_LIST_HEAD(&(ext_state->on_entries));
    
    return 0;
}


int v3_deinit_ext_manager(struct v3_vm_info * vm)  {
    struct v3_extensions * ext_state = &(vm->extensions);
    struct v3_extension * ext = NULL;
    struct v3_extension * tmp = NULL;
    int i;

    /* deinit per-core state first */
    for (i = 0; i < vm->num_cores; i++) 
        v3_deinit_core_extensions(&(vm->cores[i]));

    list_for_each_entry_safe(ext, tmp, &(ext_state->extensions), node) {
        
	V3_Print(vm, VCORE_NONE, "Cleaning up Extension (%s)\n", ext->impl->name);
	if (ext->impl) { 
	    if (ext->impl->vm_deinit) {
		if (ext->impl->vm_deinit(vm, ext->priv_data) == -1) {
		    PrintError(vm, VCORE_NONE, "Error cleaning up extension (%s)\n", ext->impl->name);
		    return -1;
		}
	    }

	    if (ext->impl->on_exit)
		list_del(&ext->exit_node);
	    
	    if (ext->impl->on_entry)
		list_del(&ext->entry_node);
	}
	    
	list_del(&ext->node);
	V3_Free(ext);
	    
    }

    return 0;
}



int v3_add_extension(struct v3_vm_info * vm, const char * name, v3_cfg_tree_t * cfg) {
    struct v3_extension_impl * impl = NULL;
    struct v3_extension * ext = NULL;
    int ext_size;

    impl = (void *)v3_htable_search(ext_table, (addr_t)name);

    if (impl == NULL) {
	PrintError(vm, VCORE_NONE, "Could not find requested extension (%s)\n", name);
	return -1;
    }
    
    V3_ASSERT(vm, VCORE_NONE, impl->vm_init);

    /* this allows each extension to track its own per-core state */
    ext_size = sizeof(struct v3_extension) + (sizeof(void *) * vm->num_cores);
    ext = V3_Malloc(ext_size);
    
    if (!ext) {
	PrintError(vm, VCORE_NONE,  "Could not allocate extension\n");
	return -1;
    }

    ext->impl = impl;

    if (impl->vm_init(vm, cfg, &(ext->priv_data)) == -1) {
	PrintError(vm, VCORE_NONE,  "Error initializing Extension (%s)\n", name);
	V3_Free(ext);
	return -1;
    }

    list_add(&(ext->node), &(vm->extensions.extensions));

    if (impl->on_exit) {
	list_add(&(ext->exit_node), &(vm->extensions.on_exits));
    }

    if (impl->on_entry) {
	list_add(&(ext->entry_node), &(vm->extensions.on_entries));
    }
    
    return 0;
}

int v3_init_core_extensions(struct guest_info * core) {
    struct v3_extension * ext = NULL;
    uint32_t cpuid = core->vcpu_id;

    list_for_each_entry(ext, &(core->vm_info->extensions.extensions), node) {
	if ((ext->impl) && (ext->impl->core_init)) {
	    if (ext->impl->core_init(core, ext->priv_data, &(ext->core_ext_priv_data[cpuid])) == -1) {
		PrintError(core->vm_info, core, "Error configuring per core extension %s on core %d\n", 
			   ext->impl->name, core->vcpu_id);
		return -1;
	    }
	}
    }

    return 0;
}


int v3_deinit_core_extensions (struct guest_info * core) {
        struct v3_extension * ext = NULL;
        struct v3_extension * tmp = NULL;
        struct v3_vm_info * vm = core->vm_info;
        uint32_t cpuid = core->vcpu_id;

        list_for_each_entry_safe(ext, tmp, &(vm->extensions.extensions), node) {
            if ((ext->impl) && (ext->impl->core_deinit)) {
                if (ext->impl->core_deinit(core, ext->priv_data, ext->core_ext_priv_data[cpuid]) == -1) {
                    PrintError(core->vm_info, core, "Error tearing down per core extension %s on core %d\n",
                                ext->impl->name, cpuid);
                    return -1;
                }
            }
        }

        return 0;
}



void * v3_get_extension_state(struct v3_vm_info * vm, const char * name) {
    struct v3_extension * ext = NULL;

    list_for_each_entry(ext, &(vm->extensions.extensions), node) {
	if (strncmp(ext->impl->name, name, strlen(ext->impl->name)) == 0) {
	    return ext->priv_data;
	}
    }

    return NULL;
}


void * v3_get_ext_core_state (struct guest_info * core, const char * name) {
    struct v3_extension * ext = NULL;
    struct v3_vm_info * vm = core->vm_info;
    uint32_t cpuid = core->vcpu_id;

    list_for_each_entry(ext, &(vm->extensions.extensions), node) {
	if (strncmp(ext->impl->name, name, strlen(ext->impl->name)) == 0) {
	    return ext->core_ext_priv_data[cpuid];
	}
    }

    return NULL;
}

