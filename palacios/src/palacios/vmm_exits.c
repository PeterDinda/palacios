/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm.h>
#include <palacios/vmm_exits.h>
#include <palacios/vm_guest.h>


int v3_init_exit_hooks(struct v3_vm_info * vm) {
    struct v3_exit_map * map = &(vm->exit_map);
    
    map->exits = V3_Malloc(sizeof(struct v3_exit_hook) * V3_EXIT_INVALID);

    if (map->exits == NULL) {
	PrintError("Error allocating exit map\n");
	return -1;
    }
    
    memset(map->exits, 0, sizeof(struct v3_exit_hook) * V3_EXIT_INVALID);


    return 0;
}

int v3_deinit_exit_hooks(struct v3_vm_info * vm) {
    struct v3_exit_map * map = &(vm->exit_map);

    V3_Free(map->exits);

    return 0;
}




int v3_init_exit_hooks_core(struct guest_info * core) {
    struct v3_vm_info * vm = core->vm_info;
    struct v3_exit_map * map = &(vm->exit_map);	 
    struct v3_exit_hook * hook = NULL;
    int i = 0;

    for (i = 0; i < V3_EXIT_INVALID; i++) {
	hook = &(map->exits[i]);

	if (hook->hooked) {
	    if (hook->enable(core, i) != 0) {
		PrintError("Error could not enable exit hook %d on core %d\n", i, core->vcpu_id);
		return -1;
	    }
	}
    }

    return 0;
}

int v3_deinit_exit_hooks_core(struct guest_info * core) {

    return 0;
}



int v3_dispatch_exit_hook(struct guest_info * core, v3_exit_type_t exit_type, void * exit_data) {
    struct v3_exit_map * map = &(core->vm_info->exit_map);	 
    struct v3_exit_hook * hook = NULL;

    if (exit_type >= V3_EXIT_INVALID) {
	PrintError("Error: Tried to dispatch invalid exit type (%d)\n", exit_type);
	return -1;
    }
  
    hook = &(map->exits[exit_type]);

    if (hook->hooked == 0) {
	PrintError("Tried to dispatch an unhooked exit (%d)\n", exit_type);
	return -1;
    }

    return hook->handler(core, exit_type, hook->priv_data, exit_data);
   
}


int v3_register_exit(struct v3_vm_info * vm, v3_exit_type_t exit_type,
		     int (*enable)(struct guest_info * core, v3_exit_type_t exit_type),
		     int (*disable)(struct guest_info * core, v3_exit_type_t exit_type)) {
    struct v3_exit_map * map = &(vm->exit_map);	 
    struct v3_exit_hook * hook = NULL;

    if (exit_type >= V3_EXIT_INVALID) {
	PrintError("Error: Tried to register invalid exit type (%d)\n", exit_type);
	return -1;
    }
  
    hook = &(map->exits[exit_type]);

    if (hook->registered == 1) {
	PrintError("Tried to reregister an exit (%d)\n", exit_type);
	return -1;
    }

    hook->registered = 1;
    hook->enable = enable;
    hook->disable = disable;
    

    return 0;
}


int v3_hook_exit(struct v3_vm_info * vm, v3_exit_type_t exit_type,
		 int (*handler)(struct guest_info * core, v3_exit_type_t exit_type, 
			     void * priv_data, void * exit_data),
		 void * priv_data, 
		 struct guest_info * current_core) {
    struct v3_exit_map * map = &(vm->exit_map);	 
    struct v3_exit_hook * hook = NULL;
    
    
    if (exit_type >= V3_EXIT_INVALID) {
	PrintError("Error: Tried to hook invalid exit type (%d)\n", exit_type);
	return -1;
    }
  
    hook = &(map->exits[exit_type]);

    if (hook->registered == 0) {
	PrintError("Tried to hook unregistered exit (%d)\n", exit_type);
	return -1;
    } 

    if (hook->hooked != 0) {
	PrintError("Tried to rehook exit (%d)\n", exit_type);
	return -1;
    }


    hook->hooked = 1;
    hook->handler = handler;
    hook->priv_data = priv_data;

    if (vm->run_state != VM_INVALID) {
	int i = 0;

	while (v3_raise_barrier(vm, current_core) == -1);
	
	for (i = 0; i < vm->num_cores; i++) {
	    
	    if (hook->enable(&(vm->cores[i]), exit_type) != 0) {
		PrintError("Error could not enable exit hook %d on core %d\n", exit_type, i);
		v3_lower_barrier(vm);
		return -1;
	    }	
	}

	v3_lower_barrier(vm);
    }
    


    return 0;

}
