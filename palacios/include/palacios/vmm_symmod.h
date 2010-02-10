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

#ifndef __VMM_SYM_MOD_H__
#define __VMM_SYM_MOD_H__

#ifdef __V3VEE__

#include <palacios/vmm_config.h>
#include <palacios/vmm_hashtable.h>

struct v3_symmod_loader_ops {

    int (*load_module)(struct v3_vm_info * vm, char * name, int len, void * priv_data);
};


struct v3_symmod_state {

    struct v3_symmod_loader_ops * loader_ops;
    void * loader_data;


    struct hashtable * module_table;
};


struct v3_sym_module {
    char name[32];
    uint16_t num_bytes;
    char * data;
};


int v3_set_symmod_loader(struct v3_vm_info * vm, struct v3_symmod_loader_ops * ops, void * priv_data);

int v3_load_sym_module(struct v3_vm_info * vm, char * mod_name);


int v3_init_symmod_vm(struct v3_vm_info * vm, v3_cfg_tree_t * cfg);


struct v3_sym_module * v3_get_sym_module(struct v3_vm_info * vm, char * name);


int V3_init_symmod();







#endif

#endif
