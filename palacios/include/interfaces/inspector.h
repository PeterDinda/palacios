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


#ifndef __VMM_INSPECTOR_H__
#define __VMM_INSPECTOR_H__

#include <palacios/vmm.h>

typedef void v3_inspect_node_t;

#ifdef __V3VEE__

#include <palacios/vmm_multitree.h>



// Flags
#define SERIALIZABLE 1
#define READ_ONLY 2
#define HOOKED 4


int v3_init_inspector(struct v3_vm_info * vm);
int v3_init_inspector_core(struct guest_info * core);


int v3_inspect_8(v3_inspect_node_t * node, char * name, uint8_t * val);
int v3_inspect_16(v3_inspect_node_t * node, char * name, uint16_t * val);
int v3_inspect_32(v3_inspect_node_t * node, char * name, uint32_t * val);
int v3_inspect_64(v3_inspect_node_t * node, char * name, uint64_t * val);
int v3_inspect_addr(v3_inspect_node_t * node, char * name, addr_t * val);
int v3_inspect_buf(v3_inspect_node_t * node, char * name, uint8_t * buf, uint64_t size);

v3_inspect_node_t * v3_inspect_add_subtree(v3_inspect_node_t * root, char * name);





#endif


struct v3_inspection_value {
    char * name;
    unsigned char * value;
    unsigned long long size; // Size of 0 means this is a subtree root
    unsigned char flags;
};




int v3_find_inspection_value(v3_inspect_node_t * node, char * name, 
			    struct v3_inspection_value * value);

struct v3_inspection_value v3_inspection_value(v3_inspect_node_t * node);



v3_inspect_node_t * v3_get_inspection_root(struct v3_vm_info * vm);
v3_inspect_node_t * v3_get_inspection_subtree(v3_inspect_node_t * root, char * name);

v3_inspect_node_t * v3_inspection_node_next(v3_inspect_node_t * node);
v3_inspect_node_t * v3_inspection_first_child(v3_inspect_node_t * root);

#endif
