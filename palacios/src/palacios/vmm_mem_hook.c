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
#include <palacios/vm_guest.h>
#include <palacios/vmm_mem_hook.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_hashtable.h>
#include <palacios/vmm_decoder.h>

struct mem_hook {
  
    // Called when data is read from a memory page
    int (*read)(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data);
    // Called when data is written to a memory page
    int (*write)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data);

    void * priv_data;
    struct v3_mem_region * region;


    struct list_head hook_node;
};



static int free_hook(struct v3_vm_info * vm, struct mem_hook * hook);

static uint_t mem_hash_fn(addr_t key) {
    return v3_hash_long(key, sizeof(void *) * 8);
}

static int mem_eq_fn(addr_t key1, addr_t key2) {
    return (key1 == key2);
}

int v3_init_mem_hooks(struct v3_vm_info * vm) {
    struct v3_mem_hooks * hooks = &(vm->mem_hooks);

    hooks->hook_hvas_1 = V3_VAddr(V3_AllocPages(vm->num_cores));
    hooks->hook_hvas_2 = V3_VAddr(V3_AllocPages(vm->num_cores));

    INIT_LIST_HEAD(&(hooks->hook_list));

    hooks->reg_table = v3_create_htable(0, mem_hash_fn, mem_eq_fn);

    return 0;
}


// We'll assume the actual hooks were either already cleared,
// or will be cleared by the memory map
int v3_deinit_mem_hooks(struct v3_vm_info * vm) {
    struct v3_mem_hooks * hooks = &(vm->mem_hooks);
    struct mem_hook * hook = NULL;
    struct mem_hook * tmp = NULL;


    // This is nasty...
    // We delete the hook info but leave its memory region intact
    // We rely on the memory map to clean up any orphaned regions as a result of this
    // This needs to be fixed at some point...
    list_for_each_entry_safe(hook, tmp, &(hooks->hook_list), hook_node) {
	free_hook(vm, hook);
    }


    v3_free_htable(hooks->reg_table, 0, 0);

    V3_FreePages(V3_PAddr(hooks->hook_hvas_1), vm->num_cores);
    V3_FreePages(V3_PAddr(hooks->hook_hvas_2), vm->num_cores);

    return 0;
}



static inline int get_op_length(struct x86_instr * instr, struct x86_operand * operand, addr_t tgt_addr) {

    if (instr->is_str_op) {
	if ((instr->str_op_length * operand->size)  < (0x1000 - PAGE_OFFSET_4KB(tgt_addr))) {
	    return (instr->str_op_length * operand->size);
	} else {
	    return (0x1000 - PAGE_OFFSET_4KB(tgt_addr));
	}
    } else {
	return instr->src_operand.size;
    }

}



static int handle_mem_hook(struct guest_info * core, addr_t guest_va, addr_t guest_pa, 
			   struct v3_mem_region * reg, pf_error_t access_info) {
    struct v3_mem_hooks * hooks = &(core->vm_info->mem_hooks);
    struct x86_instr instr;
    void * instr_ptr = NULL;
    int bytes_emulated = 0;
    int mem_op_size = 0;
    int ret = 0;

    struct mem_hook * src_hook = NULL;
    addr_t src_mem_op_hva = 0;
    addr_t src_mem_op_gpa = 0;
    int src_req_size = -1;

    struct mem_hook * dst_hook = NULL;
    addr_t dst_mem_op_hva = 0;
    addr_t dst_mem_op_gpa = 0;
    int dst_req_size = -1;

    /* Find and decode hooked instruction */
    if (core->mem_mode == PHYSICAL_MEM) { 
	ret = v3_gpa_to_hva(core, get_addr_linear(core, core->rip, &(core->segments.cs)), (addr_t *)&instr_ptr);
    } else { 
	ret = v3_gva_to_hva(core, get_addr_linear(core, core->rip, &(core->segments.cs)), (addr_t *)&instr_ptr);
    }

    if (ret == -1) {
      PrintError("Could not translate Instruction Address (%p)\n", (void *)(addr_t)core->rip);
	return -1;
    }

    if (v3_decode(core, (addr_t)instr_ptr, &instr) == -1) {
	PrintError("Decoding Error\n");
	return -1;
    }



    // Test source operand, if it's memory we need to do some translations, and handle a possible hook
    if (instr.src_operand.type == MEM_OPERAND) {
	struct v3_mem_region * src_reg = NULL;

	if (core->mem_mode == PHYSICAL_MEM) { 
	    src_mem_op_gpa = instr.src_operand.operand;
	} else { 
	    if (v3_gva_to_gpa(core, instr.src_operand.operand, &src_mem_op_gpa) == -1) {
		pf_error_t error = access_info;

		error.present = 0;
		v3_inject_guest_pf(core, instr.src_operand.operand, error);

		return 0;
	    }
	}

	if ((src_mem_op_gpa >= reg->guest_start) && 
	    (src_mem_op_gpa < reg->guest_end)) {   
	    // Src address corresponds to faulted region
	    src_reg = reg;
	} else {
	    // Note that this should only trigger for string operations
	    src_reg = v3_get_mem_region(core->vm_info, core->vcpu_id, src_mem_op_gpa);
	}

	if (src_reg == NULL) {
	    PrintError("Error finding Source region (addr=%p)\n", (void *)src_mem_op_gpa);
	    return -1;
	}

	src_hook = (struct mem_hook *)v3_htable_search(hooks->reg_table, (addr_t)src_reg);

	// We don't check whether the region is a hook here because it doesn't yet matter.
	// These hva calculations will be true regardless
	if (src_reg->flags.alloced == 0) {
	    src_mem_op_hva = (addr_t)(hooks->hook_hvas_1 + (PAGE_SIZE * core->vcpu_id));
	} else {
	    // We already have the region so we can do the conversion ourselves
	    src_mem_op_hva = (addr_t)V3_VAddr((void *)((src_mem_op_gpa - src_reg->guest_start) + src_reg->host_addr));
	}

	src_req_size = get_op_length(&instr, &(instr.src_operand), src_mem_op_hva);
    }

    // Now do the same translation / hook handling for the second operand
    if (instr.dst_operand.type == MEM_OPERAND) {
	struct v3_mem_region * dst_reg = NULL;


	if (core->mem_mode == PHYSICAL_MEM) { 
	    dst_mem_op_gpa = instr.dst_operand.operand;
	} else { 
	    if (v3_gva_to_gpa(core, instr.dst_operand.operand, &dst_mem_op_gpa) == -1) {
		pf_error_t error = access_info;

		error.present = 0;
		v3_inject_guest_pf(core, instr.dst_operand.operand, error);

		return 0;
	    }
	}

	if ((dst_mem_op_gpa >= reg->guest_start) && 
	    (dst_mem_op_gpa < reg->guest_end)) {
	    // Dst address corresponds to faulted region
	    dst_reg = reg;
	} else {
	    // Note that this should only trigger for string operations
	    dst_reg = v3_get_mem_region(core->vm_info, core->vcpu_id, dst_mem_op_gpa);
	}

	if (dst_reg == NULL) {
	    PrintError("Error finding Source region (addr=%p)\n", (void *)dst_mem_op_gpa);
	    return -1;
	}
	
	dst_hook = (struct mem_hook *)v3_htable_search(hooks->reg_table, (addr_t)dst_reg);

	// We don't check whether the region is a hook here because it doesn't yet matter.
	// These hva calculations will be true regardless
	if (dst_reg->flags.alloced == 0) {
	    dst_mem_op_hva = (addr_t)(hooks->hook_hvas_2 + (PAGE_SIZE * core->vcpu_id));
	} else {
	    // We already have the region so we can do the conversion ourselves
	    dst_mem_op_hva = (addr_t)V3_VAddr((void *)((dst_mem_op_gpa - dst_reg->guest_start) + dst_reg->host_addr));
	}

	dst_req_size = get_op_length(&instr, &(instr.dst_operand), dst_mem_op_hva);
    }


    mem_op_size = ((uint_t)src_req_size < (uint_t)dst_req_size) ? src_req_size : dst_req_size;


    /* Now handle the hooks if necessary */    
    if ( (src_hook != NULL)  && (src_hook->read != NULL) &&
	 (instr.src_operand.read == 1) ) {
	
	// Read in data from hook
	
	if (src_hook->read(core, src_mem_op_gpa, (void *)src_mem_op_hva, mem_op_size, src_hook->priv_data) == -1) {
	    PrintError("Read hook error at src_mem_op_gpa=%p\n", (void *)src_mem_op_gpa);
	    return -1;
	}
    }

    if ( (dst_hook != NULL)  && (dst_hook->read != NULL) &&
	 (instr.dst_operand.read == 1) ) {
	
	// Read in data from hook
	
	if (dst_hook->read(core, dst_mem_op_gpa, (void *)dst_mem_op_hva, mem_op_size, dst_hook->priv_data) == -1) {
	    PrintError("Read hook error at dst_mem_op_gpa=%p\n", (void *)dst_mem_op_gpa);
	    return -1;
	}
    }
    
    bytes_emulated = v3_emulate(core, &instr, mem_op_size, src_mem_op_hva, dst_mem_op_hva);

    if (bytes_emulated == -1) {
	PrintError("Error emulating instruction\n");
	return -1;
    }


    if ( (src_hook != NULL) && (src_hook->write != NULL) &&
	 (instr.src_operand.write == 1) ) {

	if (src_hook->write(core, src_mem_op_gpa, (void *)src_mem_op_hva, bytes_emulated, src_hook->priv_data) == -1) {
	    PrintError("Write hook error at src_mem_op_gpa=%p\n", (void *)src_mem_op_gpa);
	    return -1;
	}

    }


    if ( (dst_hook != NULL) && (dst_hook->write != NULL) &&
	 (instr.dst_operand.write == 1) ) {

	if (dst_hook->write(core, dst_mem_op_gpa, (void *)dst_mem_op_hva, bytes_emulated, dst_hook->priv_data) == -1) {
	    PrintError("Write hook error at dst_mem_op_gpa=%p\n", (void *)dst_mem_op_gpa);
	    return -1;
	}
    }


    if (instr.is_str_op == 0) {
	core->rip += instr.instr_length;
    }


    return 0;
}




int v3_hook_write_mem(struct v3_vm_info * vm, uint16_t core_id,
		      addr_t guest_addr_start, addr_t guest_addr_end, addr_t host_addr,
		      int (*write)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data),
		      void * priv_data) {
    struct v3_mem_region * entry = NULL;
    struct mem_hook * hook = V3_Malloc(sizeof(struct mem_hook));
    struct v3_mem_hooks * hooks = &(vm->mem_hooks);

    memset(hook, 0, sizeof(struct mem_hook));

    hook->write = write;
    hook->read = NULL;
    hook->priv_data = priv_data;

    entry = v3_create_mem_region(vm, core_id, guest_addr_start, guest_addr_end);

    hook->region = entry;

    entry->host_addr = host_addr;
    entry->unhandled = handle_mem_hook;
    entry->priv_data = hook;

    entry->flags.read = 1;
    entry->flags.exec = 1;
    entry->flags.alloced = 1;

    if (v3_insert_mem_region(vm, entry) == -1) {
	V3_Free(entry);
	V3_Free(hook);
	return -1;
    }

    v3_htable_insert(hooks->reg_table, (addr_t)entry, (addr_t)hook);
    list_add(&(hook->hook_node), &(hooks->hook_list));

    return 0;  
}



int v3_hook_full_mem(struct v3_vm_info * vm, uint16_t core_id, 
		     addr_t guest_addr_start, addr_t guest_addr_end,
		     int (*read)(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data),
		     int (*write)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data),
		     void * priv_data) {
  
    struct v3_mem_region * entry = NULL;
    struct mem_hook * hook = V3_Malloc(sizeof(struct mem_hook));
    struct v3_mem_hooks * hooks = &(vm->mem_hooks);

    memset(hook, 0, sizeof(struct mem_hook));

    hook->write = write;
    hook->read = read;
    hook->priv_data = priv_data;

    entry = v3_create_mem_region(vm, core_id, guest_addr_start, guest_addr_end);
    hook->region = entry;

    entry->unhandled = handle_mem_hook;
    entry->priv_data = hook;

    if (v3_insert_mem_region(vm, entry)) {
	V3_Free(entry);
	V3_Free(hook);
	return -1;
    }

    list_add(&(hook->hook_node), &(hooks->hook_list));
    v3_htable_insert(hooks->reg_table, (addr_t)entry, (addr_t)hook);


    return 0;
}


static int free_hook(struct v3_vm_info * vm, struct mem_hook * hook) {  
    v3_delete_mem_region(vm, hook->region);
    list_del(&(hook->hook_node));

    V3_Free(hook);

    return 0;
}


// This will unhook the memory hook registered at start address
// We do not support unhooking subregions
int v3_unhook_mem(struct v3_vm_info * vm, uint16_t core_id, addr_t guest_addr_start) {
    struct v3_mem_region * reg = v3_get_mem_region(vm, core_id, guest_addr_start);
    struct mem_hook * hook = NULL;

    if (reg == NULL) {
	PrintError("Could not find region at %p\n", (void *)guest_addr_start);
	return -1;
    }

    hook = reg->priv_data;

    if (hook == NULL) {
	PrintError("Trying to unhook region that is not a hook at %p\n", (void *)guest_addr_start);
	return -1;
    }


    free_hook(vm, hook);

    return 0;
}


