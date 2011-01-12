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
#include <palacios/vmm_cpuid.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vm_guest.h>


void v3_init_cpuid_map(struct v3_vm_info * vm) {
    vm->cpuid_map.map.rb_node = NULL;
}

int v3_deinit_cpuid_map(struct v3_vm_info * vm) {
    struct rb_node * node = v3_rb_first(&(vm->cpuid_map.map));
    struct v3_cpuid_hook * hook = NULL;
    struct rb_node * tmp_node = NULL;
    

    while (node) {
	hook = rb_entry(node, struct v3_cpuid_hook, tree_node);
	tmp_node = node;
	node = v3_rb_next(node);

	v3_rb_erase(&(hook->tree_node), &(vm->cpuid_map.map));
	V3_Free(hook);
	
    }

    return 0;
}


static inline struct v3_cpuid_hook * __insert_cpuid_hook(struct v3_vm_info * vm, struct v3_cpuid_hook * hook) {
  struct rb_node ** p = &(vm->cpuid_map.map.rb_node);
  struct rb_node * parent = NULL;
  struct v3_cpuid_hook * tmp_hook = NULL;

  while (*p) {
    parent = *p;
    tmp_hook = rb_entry(parent, struct v3_cpuid_hook, tree_node);

    if (hook->cpuid < tmp_hook->cpuid) {
      p = &(*p)->rb_left;
    } else if (hook->cpuid > tmp_hook->cpuid) {
      p = &(*p)->rb_right;
    } else {
      return tmp_hook;
    }
  }
  rb_link_node(&(hook->tree_node), parent, p);

  return NULL;
}


static inline struct v3_cpuid_hook * insert_cpuid_hook(struct v3_vm_info * vm, struct v3_cpuid_hook * hook) {
  struct v3_cpuid_hook * ret;

  if ((ret = __insert_cpuid_hook(vm, hook))) {
    return ret;
  }

  v3_rb_insert_color(&(hook->tree_node), &(vm->cpuid_map.map));

  return NULL;
}



static struct v3_cpuid_hook * get_cpuid_hook(struct v3_vm_info * vm, uint32_t cpuid) {
  struct rb_node * n = vm->cpuid_map.map.rb_node;
  struct v3_cpuid_hook * hook = NULL;

  while (n) {
    hook = rb_entry(n, struct v3_cpuid_hook, tree_node);
    
    if (cpuid < hook->cpuid) {
      n = n->rb_left;
    } else if (cpuid > hook->cpuid) {
      n = n->rb_right;
    } else {
      return hook;
    }
  }

  return NULL;
}


int v3_unhook_cpuid(struct v3_vm_info * vm, uint32_t cpuid) {
    struct v3_cpuid_hook * hook = get_cpuid_hook(vm, cpuid);

    if (hook == NULL) {
	PrintError("Could not find cpuid to unhook (0x%x)\n", cpuid);
	return -1;
    }

    v3_rb_erase(&(hook->tree_node), &(vm->cpuid_map.map));

    V3_Free(hook);

    return 0;
}

int v3_hook_cpuid(struct v3_vm_info * vm, uint32_t cpuid, 
		  int (*hook_fn)(struct guest_info * info, uint32_t cpuid, \
				 uint32_t * eax, uint32_t * ebx, \
				 uint32_t * ecx, uint32_t * edx, \
				 void * private_data), 
		  void * private_data) {
    struct v3_cpuid_hook * hook = NULL;

    if (hook_fn == NULL) {
	PrintError("CPUID hook requested with null handler\n");
	return -1;
    }

    hook = (struct v3_cpuid_hook *)V3_Malloc(sizeof(struct v3_cpuid_hook));
    hook->cpuid = cpuid;
    hook->private_data = private_data;
    hook->hook_fn = hook_fn;

    if (insert_cpuid_hook(vm, hook)) {
	PrintError("Could not hook cpuid 0x%x (already hooked)\n", cpuid);
	V3_Free(hook);
	return -1;
    }

    return 0;
}

int v3_handle_cpuid(struct guest_info * info) {
    uint32_t cpuid = info->vm_regs.rax;
    struct v3_cpuid_hook * hook = get_cpuid_hook(info->vm_info, cpuid);

    //PrintDebug("CPUID called for 0x%x\n", cpuid);

    if (hook == NULL) {
	//PrintDebug("Calling passthrough handler\n");
	// call the passthrough handler
	v3_cpuid(cpuid, 
		 (uint32_t *)&(info->vm_regs.rax), 
		 (uint32_t *)&(info->vm_regs.rbx), 
		 (uint32_t *)&(info->vm_regs.rcx), 
		 (uint32_t *)&(info->vm_regs.rdx));
    } else {
	//	PrintDebug("Calling hook function\n");

	if (hook->hook_fn(info, cpuid, 
			  (uint32_t *)&(info->vm_regs.rax), 
			  (uint32_t *)&(info->vm_regs.rbx), 
			  (uint32_t *)&(info->vm_regs.rcx), 
			  (uint32_t *)&(info->vm_regs.rdx), 
			  hook->private_data) == -1) {
	    PrintError("Error in cpuid handler for 0x%x\n", cpuid);
	    return -1;
	}
    }

    //    PrintDebug("Cleaning up register contents\n");

    info->vm_regs.rax &= 0x00000000ffffffffLL;
    info->vm_regs.rbx &= 0x00000000ffffffffLL;
    info->vm_regs.rcx &= 0x00000000ffffffffLL;
    info->vm_regs.rdx &= 0x00000000ffffffffLL;

    info->rip += 2;

    return 0;
}
