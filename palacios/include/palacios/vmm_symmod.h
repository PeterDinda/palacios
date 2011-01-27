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






struct v3_sym_capsule {
    char * name;
    void * start_addr;
    uint32_t size;
    uint32_t guest_size;

    union {
	uint32_t flags;
	struct {
#define V3_SYMMOD_INV (0x00)
#define V3_SYMMOD_LNX (0x01)
#define V3_SYMMOD_MOD (0x02)
#define V3_SYMMOD_SEC (0x03)
	    uint8_t type;

#define V3_SYMMOD_ARCH_INV     (0x00)
#define V3_SYMMOD_ARCH_i386    (0x01)
#define V3_SYMMOD_ARCH_x86_64  (0x02)
	    uint8_t arch;
	    uint16_t rsvd;
	} __attribute__((packed));
    } __attribute__((packed));
    
    void * capsule_data;

    struct list_head node;
};



struct v3_symmod_loader_ops {

    int (*load_capsule)(struct v3_vm_info * vm, struct v3_sym_capsule * mod,  void * priv_data);
};


struct v3_symmod_state {

    struct v3_symmod_loader_ops * loader_ops;
    void * loader_data;

    uint32_t num_avail_capsules;
    uint32_t num_loaded_capsules;

    struct hashtable * capsule_table;

    /* List containing V3 symbols */
    /* (defined in vmm_symmod.c)  */
    struct list_head v3_sym_list;
};



int v3_set_symmod_loader(struct v3_vm_info * vm, struct v3_symmod_loader_ops * ops, void * priv_data);

int v3_load_sym_capsule(struct v3_vm_info * vm, char * mod_name);

int v3_init_symmod_vm(struct v3_vm_info * vm, v3_cfg_tree_t * cfg);
int v3_deinit_symmod_vm(struct v3_vm_info * vm);

struct v3_sym_capsule * v3_get_sym_capsule(struct v3_vm_info * vm, char * name);



int V3_init_symmod();
int V3_deinit_symmod();






#endif

#endif
