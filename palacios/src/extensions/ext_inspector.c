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


//#include <palacios/vmm_inspector.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vmm_extensions.h>

#include <palacios/vmm_multitree.h>
#include <interfaces/inspector.h>

// Note that v3_inspect_node_t is actuall a struct v3_mtree
// Its set as void for opaque portability

struct v3_inspector_state {
    struct v3_mtree state_tree;

};


static int init_inspector(struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data) {
    struct v3_inspector_state * state = V3_Malloc(sizeof(struct v3_inspector_state));
    memset(state, 0, sizeof(struct v3_inspector_state));

    strncpy(state->state_tree.name, "vm->name", 50);
    state->state_tree.subtree = 1;

    *priv_data = state;

    return 0;
}


static int init_inspector_core(struct guest_info * core, void * priv_data) {
    struct v3_inspector_state * vm_state = priv_data;
    char core_name[50];

    snprintf(core_name, 50, "core.%d", core->vcpu_id);

    {
	struct v3_mtree * core_node = v3_mtree_create_subtree(&(vm_state->state_tree), core_name);
	v3_inspect_64(core_node, "RIP", (uint64_t *)&(core->rip));
	v3_inspect_64(core_node, "NUM_EXITS", (uint64_t *)&(core->num_exits));
	//	v3_inspect_buf(core_node, "EXEC_NAME", core->exec_name, sizeof(core->exec_name));


	struct v3_mtree * gpr_node = v3_mtree_create_subtree(core_node, "GPRS");
	v3_inspect_64(gpr_node, "RAX", (uint64_t *)&(core->vm_regs.rax));    
	v3_inspect_64(gpr_node, "RBX", (uint64_t *)&(core->vm_regs.rbx));    
	v3_inspect_64(gpr_node, "RCX", (uint64_t *)&(core->vm_regs.rcx));    
	v3_inspect_64(gpr_node, "RDX", (uint64_t *)&(core->vm_regs.rdx));    
	v3_inspect_64(gpr_node, "RSP", (uint64_t *)&(core->vm_regs.rsp));    
	v3_inspect_64(gpr_node, "RBP", (uint64_t *)&(core->vm_regs.rbp));    
	v3_inspect_64(gpr_node, "RSI", (uint64_t *)&(core->vm_regs.rsi));    
	v3_inspect_64(gpr_node, "RDI", (uint64_t *)&(core->vm_regs.rdi));    


	struct v3_mtree * cr_node = v3_mtree_create_subtree(core_node, "CTRL_REGS");
	v3_inspect_64(cr_node, "CR0", (uint64_t *)&(core->ctrl_regs.cr0));    
	v3_inspect_64(cr_node, "CR2", (uint64_t *)&(core->ctrl_regs.cr2));    
	v3_inspect_64(cr_node, "CR3", (uint64_t *)&(core->ctrl_regs.cr3));    
	v3_inspect_64(cr_node, "CR4", (uint64_t *)&(core->ctrl_regs.cr4));    
	v3_inspect_64(cr_node, "RFLAGS", (uint64_t *)&(core->ctrl_regs.rflags));    
	v3_inspect_64(cr_node, "EFER", (uint64_t *)&(core->ctrl_regs.efer));    


	//struct v3_mtree * seg_node = v3_mtree_create_subtree(core_node, "SEGMENTS");
	


    }

    return 0;
}





static struct v3_extension_impl inspector_impl = {
    .name = "inspector",
    .init = init_inspector,
    .deinit = NULL,
    .core_init = init_inspector_core,
    .core_deinit = NULL,
    .on_entry = NULL,
    .on_exit = NULL
};


register_extension(&inspector_impl);


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



int v3_find_inspection_value(v3_inspect_node_t * node, char * name, 
			   struct v3_inspection_value * value) {
    struct v3_mtree * mt_node = v3_mtree_find_node(node, name);
    
    if (!mt_node) {
	return -1;
    }
    
    *value = v3_inspection_value(mt_node);

    return 0;
}

struct v3_inspection_value v3_inspection_value(v3_inspect_node_t * node) {
    struct v3_mtree * mt_node = node;
    struct v3_inspection_value value;

    value.value = mt_node->value;
    value.size = mt_node->size;
    value.flags = mt_node->user_flags;
    value.name = mt_node->name;

    return value;
}



v3_inspect_node_t * v3_get_inspection_root(struct v3_vm_info * vm) {
    struct v3_inspector_state * inspector = v3_get_extension_state(vm, inspector_impl.name);

    if (inspector == NULL) {
	return NULL;
    }

    return &(inspector->state_tree);
}

v3_inspect_node_t * v3_get_inspection_subtree(v3_inspect_node_t * root, char * name) {
    return v3_mtree_find_subtree(root, name);
}


v3_inspect_node_t * v3_inspection_node_next(v3_inspect_node_t * node) {
    return v3_mtree_next_node(node);
}

v3_inspect_node_t * v3_inspection_first_child(v3_inspect_node_t * root) {
    return v3_mtree_first_child(root);
}




