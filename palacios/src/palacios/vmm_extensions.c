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
	V3_Print("Registering Extension (%s)\n", (*tmp_ext)->name);

	if (v3_htable_search(ext_table, (addr_t)((*tmp_ext)->name))) {
	    PrintError("Multiple instances of Extension (%s)\n", (*tmp_ext)->name);
	    return -1;
	}

	if (v3_htable_insert(ext_table, (addr_t)((*tmp_ext)->name), (addr_t)(*tmp_ext)) == 0) {
	    PrintError("Could not register Extension (%s)\n", (*tmp_ext)->name);
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

	PrintError("I should really do something here... \n");
	return -1;
}



int v3_add_extension(struct v3_vm_info * vm, const char * name, v3_cfg_tree_t * cfg) {
    struct v3_extension_impl * impl = NULL;
    struct v3_extension * ext = NULL;

    impl = (void *)v3_htable_search(ext_table, (addr_t)name);

    if (impl == NULL) {
	PrintError("Could not find requested extension (%s)\n", name);
	return -1;
    }
    
    V3_ASSERT(impl->init);

    ext = V3_Malloc(sizeof(struct v3_extension));
    
    if (!ext) {
	PrintError("Could not allocate extension\n");
	return -1;
    }

    ext->impl = impl;

    if (impl->init(vm, cfg, &(ext->priv_data)) == -1) {
	PrintError("Error initializing Extension (%s)\n", name);
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

    list_for_each_entry(ext, &(core->vm_info->extensions.extensions), node) {
	if ((ext->impl) && (ext->impl->core_init)) {
	    if (ext->impl->core_init(core, ext->priv_data) == -1) {
		PrintError("Error configuring per core extension %s on core %d\n", 
			   ext->impl->name, core->vcpu_id);
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
