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


#include <palacios/vmm_inspector.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_sprintf.h>

// Note that v3_inspect_node_t is actuall a struct v3_mtree
// Its set as void for opaque portability


int v3_init_inspector(struct v3_vm_info * vm) {
    struct v3_inspector_state * state = (struct v3_inspector_state *)&(vm->inspector);

    strncpy(state->state_tree.name, "vm->name", 50);
    state->state_tree.subtree = 1;

    return 0;
}


int  v3_init_inspector_core(struct guest_info * core) {
    struct v3_inspector_state * vm_state = &(core->vm_info->inspector);
    char core_name[50];

    snprintf(core_name, 50, "core.%d", core->cpu_id);

    {
	struct v3_mtree * core_node = v3_mtree_create_subtree(&(vm_state->state_tree), core_name);
	struct v3_mtree * gpr_node = v3_mtree_create_subtree(core_node, "GPRS");

	v3_inspect_64(gpr_node, "RAX", (uint64_t *)&(core->vm_regs.rax));    
    }

    return 0;
}


v3_inspect_node_t * v3_inspect_add_subtree(v3_inspect_node_t * root, char * name) {
    return v3_mtree_create_subtree(root, name);
}

int v3_inspect_8(v3_inspect_node_t * node, char * name, uint8_t * val) {
    v3_mtree_create_value(node, name, 1, val);
    return 0;
}


int v3_inspect_16(v3_inspect_node_t * node, char * name, uint16_t * val) {
    v3_mtree_create_value(node, name, 2, val);

    return 0;
}

int v3_inspect_32(v3_inspect_node_t * node, char * name, uint32_t * val) {
    v3_mtree_create_value(node, name, 4, val); 
    return 0;
}

int v3_inspect_64(v3_inspect_node_t * node, char * name, uint64_t * val) {
    v3_mtree_create_value(node, name, 8, val);
    return 0;
}

int v3_inspect_addr(v3_inspect_node_t * node, char * name, addr_t * val) {
    v3_mtree_create_value(node, name, sizeof(addr_t), val);
    return 0;
}

int v3_inspect_buf(v3_inspect_node_t * node, char * name, 
		   uint8_t * buf, uint64_t size) {
    v3_mtree_create_value(node, name, size, buf);

    return 0;
}





int v3_get_inspection_value(v3_inspect_node_t * node, char * name, 
			   struct v3_inspection_value * value) {
    struct v3_mtree * mt_node = v3_mtree_find_node(node, name);
    
    if (!mt_node) {
	return -1;
    }
    
    value->value = mt_node->value;
    value->size = mt_node->size;
    value->flags = mt_node->user_flags;
    value->name = mt_node->name;


    return 0;
}


v3_inspect_node_t * v3_get_inspection_root(struct v3_vm_info * vm) {
    return &(vm->inspector.state_tree);
}

v3_inspect_node_t * v3_get_inspection_subtree(v3_inspect_node_t * root, char * name) {
    return v3_mtree_find_subtree(root, name);
}


