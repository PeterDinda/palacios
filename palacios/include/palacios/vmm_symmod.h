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







#define V3_SYMMOD_INV (0x00000000)
#define V3_SYMMOD_LNX (0x00000001)
#define V3_SYMMOD_MOD (0x00000002)
#define V3_SYMMOD_SEC (0x00000003)
union v3_symmod_flags {
    uint32_t flags;
    struct {
	uint8_t type;
    } __attribute__((packed));
} __attribute__((packed));


struct v3_sym_module {
    char * name;
    void * start_addr;
    void * end_addr;
    uint32_t flags; // see 'struct v3_symmod_flags'
} __attribute__((packed));



struct v3_symmod_loader_ops {

    int (*load_module)(struct v3_vm_info * vm, struct v3_sym_module * mod,  void * priv_data);
};


struct v3_symmod_state {

    struct v3_symmod_loader_ops * loader_ops;
    void * loader_data;

    struct hashtable * module_table;

    /* List containing V3 symbols */
    /* (defined in vmm_symmod.c)  */
    struct list_head v3_sym_list;
};



int v3_set_symmod_loader(struct v3_vm_info * vm, struct v3_symmod_loader_ops * ops, void * priv_data);

int v3_load_sym_module(struct v3_vm_info * vm, char * mod_name);

int v3_init_symmod_vm(struct v3_vm_info * vm, v3_cfg_tree_t * cfg);

struct v3_sym_module * v3_get_sym_module(struct v3_vm_info * vm, char * name);




#define register_module(name, start, end, flags)		\
    static char v3_module_name[] = name;			\
    static struct v3_sym_module _v3_module			\
    __attribute__((__used__))					\
	__attribute__((unused, __section__ ("_v3_modules"),	\
		       aligned(sizeof(addr_t))))		\
	= {v3_module_name, start, end, flags};


int V3_init_symmod();







#endif

#endif
