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

struct mem_hook {
  
    // Called when data is read from a memory page
    int (*read)(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data);
    // Called when data is written to a memory page
    int (*write)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data);

    void * priv_data;
    addr_t hook_hva;
    

};



int v3_init_mem_hooks(struct v3_vm_info * vm) {
    struct v3_mem_hooks * hooks = &(vm->mem_hooks);

    hooks->hook_hvas = V3_VAddr(V3_AllocPages(vm->num_cores));

    INIT_LIST_HEAD(&(hooks->hook_list));

    return 0;
}




static int handle_mem_hook(struct guest_info * info, addr_t guest_va, addr_t guest_pa, 
			   struct v3_shadow_region * reg, pf_error_t access_info) {
    struct mem_hook * hook = reg->priv_data;
    addr_t op_addr = 0;

    if (reg->flags.alloced == 0) {
	op_addr = hook->hook_hva;
    } else {
	op_addr = (addr_t)V3_VAddr((void *)v3_get_shadow_addr(reg, info->cpu_id, guest_pa));
    }

    
    if (access_info.write == 1) { 
	// Write Operation 

	if (v3_emulate_write_op(info, guest_va, guest_pa, op_addr, 
				hook->write, hook->priv_data) == -1) {
	    PrintError("Write Full Hook emulation failed\n");
	    return -1;
	}
    } else {
	// Read Operation
	
	if (reg->flags.read == 1) {
	    PrintError("Tried to emulate read for a guest Readable page\n");
	    return -1;
	}

	if (v3_emulate_read_op(info, guest_va, guest_pa, op_addr, 
			       hook->read, hook->write, 
			       hook->priv_data) == -1) {
	    PrintError("Read Full Hook emulation failed\n");
	    return -1;
	}

    }


    return 0;
}




int v3_hook_write_mem(struct v3_vm_info * vm, uint16_t core_id,
		      addr_t guest_addr_start, addr_t guest_addr_end, addr_t host_addr,
		      int (*write)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data),
		      void * priv_data) {
    struct v3_shadow_region * entry = NULL;
    struct mem_hook * hook = V3_Malloc(sizeof(struct mem_hook));
    //    struct v3_mem_hooks * hooks = &(vm->mem_hooks);

    memset(hook, 0, sizeof(struct mem_hook));

    hook->write = write;
    hook->read = NULL;
    hook->priv_data = priv_data;
    hook->hook_hva = (addr_t)V3_VAddr((void *)host_addr);

    entry = v3_create_mem_region(vm, core_id, guest_addr_start, guest_addr_end);
    
    entry->host_addr = host_addr;
    entry->unhandled = handle_mem_hook;
    entry->priv_data = hook;

    entry->flags.read = 1;
    entry->flags.exec = 1;
    entry->flags.alloced = 1;

    if (v3_insert_shadow_region(vm, entry) == -1) {
	V3_Free(entry);
	V3_Free(hook);
	return -1;
    }

    return 0;  
}



int v3_hook_full_mem(struct v3_vm_info * vm, uint16_t core_id, 
		     addr_t guest_addr_start, addr_t guest_addr_end,
		     int (*read)(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data),
		     int (*write)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data),
		     void * priv_data) {
  
    struct v3_shadow_region * entry = NULL;
    struct mem_hook * hook = V3_Malloc(sizeof(struct mem_hook));
    struct v3_mem_hooks * hooks = &(vm->mem_hooks);

    memset(hook, 0, sizeof(struct mem_hook));

    hook->write = write;
    hook->read = read;
    hook->priv_data = priv_data;
    hook->hook_hva = (addr_t)hooks->hook_hvas + (PAGE_SIZE_4KB * core_id);

    entry = v3_create_mem_region(vm, core_id, guest_addr_start, guest_addr_end);

    entry->unhandled = handle_mem_hook;
    entry->priv_data = hook;

    if (v3_insert_shadow_region(vm, entry)) {
	V3_Free(entry);
	V3_Free(hook);
	return -1;
    }

    return 0;
}



// This will unhook the memory hook registered at start address
// We do not support unhooking subregions
int v3_unhook_mem(struct v3_vm_info * vm, uint16_t core_id, addr_t guest_addr_start) {
    struct v3_shadow_region * reg = v3_get_shadow_region(vm, core_id, guest_addr_start);
    struct mem_hook * hook = reg->priv_data;

    V3_Free(hook);
  
    v3_delete_shadow_region(vm, reg);

    return 0;
}


