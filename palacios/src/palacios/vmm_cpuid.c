/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Jack Lange <jacklange@cs.pitt.edu> 
 * All rights reserved.
 *
 * Author: Jack Lange <jacklange@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_cpuid.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vm_guest.h>

struct masked_cpuid {
    uint32_t rax_mask;
    uint32_t rbx_mask;
    uint32_t rcx_mask;
    uint32_t rdx_mask;

    uint32_t rax;
    uint32_t rbx;
    uint32_t rcx;
    uint32_t rdx;
};


void v3_init_cpuid_map(struct v3_vm_info * vm) {
    vm->cpuid_map.map.rb_node = NULL;

    // Setup default cpuid entries


    // Disable XSAVE (cpuid 0x01, ECX bit 26)
    v3_cpuid_add_fields(vm, 0x01, 0, 0, 0, 0, (1 << 26), 0, 0, 0);

    // Disable MONITOR/MWAIT (cpuid 0x01, ECX bit 3)
    v3_cpuid_add_fields(vm, 0x01, 0, 0, 0, 0, (1 << 3), 0, 0, 0);


    // disable MTRR
    v3_cpuid_add_fields(vm, 0x00000001, 0, 0, 0, 0, 0, 0, (1 << 12), 0);
    // disable PAT
    v3_cpuid_add_fields(vm, 0x00000001, 0, 0, 0, 0, 0, 0, (1 << 16), 0);
    // disable X2APIC
    v3_cpuid_add_fields(vm, 0x00000001, 0, 0, 0, 0, (1 << 21), 0, 0, 0);


    // Demarcate machine as a VM
    v3_cpuid_add_fields(vm, 0x00000001,
                        0, 0,
                        0, 0,
                        0x80000000, 0x80000000,
                        0, 0
                        );


    // disable ARAT
    v3_cpuid_add_fields(vm, 0x00000006, (1 << 2), 0, 0, 0, 0, 0, 0, 0);

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



static int mask_hook(struct guest_info * core, uint32_t cpuid, 
	      uint32_t * eax, uint32_t * ebx, 
	      uint32_t * ecx, uint32_t * edx,
	      void * priv_data) {
    struct masked_cpuid * mask = (struct masked_cpuid *)priv_data;

    v3_cpuid(cpuid, eax, ebx, ecx, edx);

    *eax &= ~(mask->rax_mask);
    *eax |= (mask->rax & mask->rax_mask);

    *ebx &= ~(mask->rbx_mask);
    *ebx |= (mask->rbx & mask->rbx_mask);

    *ecx &= ~(mask->rcx_mask);
    *ecx |= (mask->rcx & mask->rcx_mask);

    *edx &= ~(mask->rdx_mask);
    *edx |= (mask->rdx & mask->rdx_mask);

    return 0;
}



/* This function allows you to reserve a set of bits in a given cpuid value 
 * For each cpuid return register you specify which bits you want to reserve in the mask.
 * The value of those bits is set in the reg param.
 * The values of the reserved bits are  returned to the guest, when it reads the cpuid
 */ 
int v3_cpuid_add_fields(struct v3_vm_info * vm, uint32_t cpuid, 
			uint32_t rax_mask, uint32_t rax,
			uint32_t rbx_mask, uint32_t rbx, 
			uint32_t rcx_mask, uint32_t rcx, 
			uint32_t rdx_mask, uint32_t rdx) {
    struct v3_cpuid_hook * hook = get_cpuid_hook(vm, cpuid);


    if ((~rax_mask & rax) || (~rbx_mask & rbx) ||
	(~rcx_mask & rcx) || (~rdx_mask & rdx)) {
	PrintError("Invalid cpuid reg value (mask overrun)\n");
	return -1;
    }


    if (hook == NULL) {
	struct masked_cpuid * mask = V3_Malloc(sizeof(struct masked_cpuid));

	if (!mask) {
	    PrintError("Unable to alocate space for cpu id mask\n");
	    return -1;
	}

	memset(mask, 0, sizeof(struct masked_cpuid));
	
	mask->rax_mask = rax_mask;
	mask->rax = rax;
	mask->rbx_mask = rbx_mask;
	mask->rbx = rbx;
	mask->rcx_mask = rcx_mask;
	mask->rcx = rcx;
	mask->rdx_mask = rdx_mask;
	mask->rdx = rdx;

	if (v3_hook_cpuid(vm, cpuid, mask_hook, mask) == -1) {
	    PrintError("Error hooking cpuid %d\n", cpuid);
	    V3_Free(mask);
	    return -1;
	}
    } else {
	struct masked_cpuid * mask = NULL;
	uint32_t tmp_val = 0;

	if (hook->hook_fn != mask_hook) {
	    PrintError("trying to add fields to a fully hooked cpuid (%d)\n", cpuid);
	    return -1;
	}
	
	mask = (struct masked_cpuid *)(hook->private_data);

	if ((mask->rax_mask & rax_mask) ||
	    (mask->rbx_mask & rbx_mask) || 
	    (mask->rcx_mask & rcx_mask) || 
	    (mask->rdx_mask & rdx_mask)) {
	    PrintError("Trying to add fields that have already been masked\n");
	    return -1;
	}

	mask->rax_mask |= rax_mask;
	mask->rbx_mask |= rbx_mask;
	mask->rcx_mask |= rcx_mask;
	mask->rdx_mask |= rdx_mask;
	
	mask->rax |= rax;
	tmp_val = (~rax_mask | rax);
	mask->rax &= tmp_val;

	mask->rbx |= rbx;
	tmp_val = (~rbx_mask | rbx);
	mask->rbx &= tmp_val;

	mask->rcx |= rcx;
	tmp_val = (~rcx_mask | rcx);
	mask->rcx &= tmp_val;

	mask->rdx |= rdx;
	tmp_val = (~rdx_mask | rdx);
	mask->rdx &= tmp_val;

    }

    return 0;
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

    if (!hook) {
	PrintError("Cannot allocate memory to hook cpu id\n");
	return -1;
    }

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





