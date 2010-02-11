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
#include <palacios/vmm_symmod.h>
#include <palacios/vmm_symbiotic.h>
#include <palacios/vm_guest.h>

static struct v3_sym_module  test_module;

int V3_init_symmod() {

    return -1;
}



int v3_init_symmod_vm(struct v3_vm_info * info, v3_cfg_tree_t * cfg) {
    return 0;
}



int v3_set_symmod_loader(struct v3_vm_info * vm, struct v3_symmod_loader_ops * ops, void * priv_data) {
    struct v3_symmod_state * symmod_state = &(vm->sym_vm_state.symmod_state);
    extern uint8_t symmod_start[];
    extern uint8_t symmod_end[];




    struct v3_sym_module tmp_mod = {
	.name = "test",
	.num_bytes = symmod_end - symmod_start,
	.data = symmod_start,
    };
    
    
    test_module = tmp_mod;


    symmod_state->loader_ops = ops;
    symmod_state->loader_data = priv_data;

    
    return 0;

}





int v3_load_sym_module(struct v3_vm_info * vm, char * mod_name) {
    struct v3_symmod_state * symmod_state = &(vm->sym_vm_state.symmod_state);

    PrintDebug("Loading Module (%s)\n", mod_name);

    return symmod_state->loader_ops->load_module(vm, test_module.name, test_module.num_bytes, symmod_state->loader_data);
}




struct v3_sym_module * v3_get_sym_module(struct v3_vm_info * vm, char * name) {
    return &test_module;
}
