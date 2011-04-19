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

#ifndef __VMM_EXTENSIONS_H__
#define __VMM_EXTENSIONS_H__

#ifdef __V3VEE__

#include <palacios/vmm.h>
#include <palacios/vmm_config.h>
#include <palacios/vmm_list.h>


struct v3_vm_info;
struct guest_info;

struct v3_extensions {
    struct list_head extensions;
    struct list_head on_exits;
    struct list_head on_entries;
};


struct v3_extension_impl {
    char * name;
    int (*init)(struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data);
    int (*deinit)(struct v3_vm_info * vm, void * priv_data);
    int (*core_init)(struct guest_info * core, void * priv_data);
    int (*core_deinit)(struct guest_info * core, void * priv_data);
    int (*on_entry)(struct guest_info * core, void * priv_data);
    int (*on_exit)(struct guest_info * core, void * priv_data);
};

struct v3_extension {
    struct v3_extension_impl * impl;
    void * priv_data;

    struct list_head node;
    struct list_head exit_node;
    struct list_head entry_node;
};



int V3_init_extensions();
int V3_deinit_extensions();


int v3_init_ext_manager(struct v3_vm_info * vm);
int v3_add_extension(struct v3_vm_info * vm, const char * name, v3_cfg_tree_t * cfg);
int v3_init_core_extensions(struct guest_info * core);

void * v3_get_extension_state(struct v3_vm_info * vm, const char * name);


#define register_extension(ext)					\
    static struct v3_extension_impl * _v3_ext			\
    __attribute__((used))					\
	__attribute__((unused, __section__("_v3_extensions"),	\
		       aligned(sizeof(addr_t))))		\
	= ext;



#endif

#endif
