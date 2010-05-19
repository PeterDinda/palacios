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

#include <palacios/vmm_mem.h>
#include <palacios/vmm.h>
#include <palacios/vmm_util.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vm_guest.h>

#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_direct_paging.h>




static int mem_offset_hypercall(struct guest_info * info, uint_t hcall_id, void * private_data) {
    PrintDebug("V3Vee: Memory offset hypercall (offset=%p)\n", 
	       (void *)(info->vm_info->mem_map.base_region.host_addr));

    info->vm_regs.rbx = info->vm_info->mem_map.base_region.host_addr;

    return 0;
}

static int unhandled_err(struct guest_info * core, addr_t guest_va, addr_t guest_pa, 
			 struct v3_mem_region * reg, pf_error_t access_info) {

    PrintError("Unhandled memory access error\n");

    v3_print_mem_map(core->vm_info);

    v3_print_guest_state(core);

    return -1;
}



int v3_init_mem_map(struct v3_vm_info * vm) {
    struct v3_mem_map * map = &(vm->mem_map);
    addr_t mem_pages = vm->mem_size >> 12;

    memset(&(map->base_region), 0, sizeof(struct v3_mem_region));

    map->mem_regions.rb_node = NULL;


    // There is an underlying region that contains all of the guest memory
    // PrintDebug("Mapping %d pages of memory (%u bytes)\n", (int)mem_pages, (uint_t)info->mem_size);

    map->base_region.guest_start = 0;
    map->base_region.guest_end = mem_pages * PAGE_SIZE_4KB;
    map->base_region.host_addr = (addr_t)V3_AllocPages(mem_pages);

    map->base_region.flags.read = 1;
    map->base_region.flags.write = 1;
    map->base_region.flags.exec = 1;
    map->base_region.flags.base = 1;
    map->base_region.flags.alloced = 1;
    
    map->base_region.unhandled = unhandled_err;

    if ((void *)map->base_region.host_addr == NULL) {
	PrintError("Could not allocate Guest memory\n");
	return -1;
    }
	
    //memset(V3_VAddr((void *)map->base_region.host_addr), 0xffffffff, map->base_region.guest_end);

    v3_register_hypercall(vm, MEM_OFFSET_HCALL, mem_offset_hypercall, NULL);

    return 0;
}


void v3_delete_mem_map(struct v3_vm_info * vm) {
    struct rb_node * node = v3_rb_first(&(vm->mem_map.mem_regions));
    struct v3_mem_region * reg;
    struct rb_node * tmp_node = NULL;
  
    while (node) {
	reg = rb_entry(node, struct v3_mem_region, tree_node);
	tmp_node = node;
	node = v3_rb_next(node);

	v3_delete_mem_region(vm, reg);
    }

    V3_FreePage((void *)(vm->mem_map.base_region.host_addr));
}


struct v3_mem_region * v3_create_mem_region(struct v3_vm_info * vm, uint16_t core_id, 
					       addr_t guest_addr_start, addr_t guest_addr_end) {
    
    struct v3_mem_region * entry = (struct v3_mem_region *)V3_Malloc(sizeof(struct v3_mem_region));
    memset(entry, 0, sizeof(struct v3_mem_region));

    entry->guest_start = guest_addr_start;
    entry->guest_end = guest_addr_end;
    entry->core_id = core_id;
    entry->unhandled = unhandled_err;

    return entry;
}




int v3_add_shadow_mem( struct v3_vm_info * vm, uint16_t core_id,
		       addr_t               guest_addr_start,
		       addr_t               guest_addr_end,
		       addr_t               host_addr)
{
    struct v3_mem_region * entry = NULL;

    entry = v3_create_mem_region(vm, core_id, 
				 guest_addr_start, 
				 guest_addr_end);

    entry->host_addr = host_addr;


    entry->flags.read = 1;
    entry->flags.write = 1;
    entry->flags.exec = 1;
    entry->flags.alloced = 1;

    if (v3_insert_mem_region(vm, entry) == -1) {
	V3_Free(entry);
	return -1;
    }

    return 0;
}



static inline 
struct v3_mem_region * __insert_mem_region(struct v3_vm_info * vm, 
						 struct v3_mem_region * region) {
    struct rb_node ** p = &(vm->mem_map.mem_regions.rb_node);
    struct rb_node * parent = NULL;
    struct v3_mem_region * tmp_region;

    while (*p) {
	parent = *p;
	tmp_region = rb_entry(parent, struct v3_mem_region, tree_node);

	if (region->guest_end <= tmp_region->guest_start) {
	    p = &(*p)->rb_left;
	} else if (region->guest_start >= tmp_region->guest_end) {
	    p = &(*p)->rb_right;
	} else {
	    if ((region->guest_end != tmp_region->guest_end) ||
		(region->guest_start != tmp_region->guest_start)) {
		PrintError("Trying to map a partial overlapped core specific page...\n");
		return tmp_region; // This is ugly... 
	    } else if (region->core_id == tmp_region->core_id) {
		return tmp_region;
	    } else if (region->core_id < tmp_region->core_id) {
		p = &(*p)->rb_left;
	    } else {
		p = &(*p)->rb_right;
	    }
	}
    }

    rb_link_node(&(region->tree_node), parent, p);
  
    return NULL;
}



int v3_insert_mem_region(struct v3_vm_info * vm, 
			    struct v3_mem_region * region) {
    struct v3_mem_region * ret;
    int i = 0;

    if ((ret = __insert_mem_region(vm, region))) {
	return -1;
    }

    v3_rb_insert_color(&(region->tree_node), &(vm->mem_map.mem_regions));



    for (i = 0; i < vm->num_cores; i++) {
	struct guest_info * info = &(vm->cores[i]);

	// flush virtual page tables 
	// 3 cases shadow, shadow passthrough, and nested

	if (info->shdw_pg_mode == SHADOW_PAGING) {
	    v3_mem_mode_t mem_mode = v3_get_vm_mem_mode(info);
	    
	    if (mem_mode == PHYSICAL_MEM) {
		addr_t cur_addr;
		
		for (cur_addr = region->guest_start;
		     cur_addr < region->guest_end;
		     cur_addr += PAGE_SIZE_4KB) {
		    v3_invalidate_passthrough_addr(info, cur_addr);
		}
	    } else {
		v3_invalidate_shadow_pts(info);
	    }
	    
	} else if (info->shdw_pg_mode == NESTED_PAGING) {
	    addr_t cur_addr;
	    
	    for (cur_addr = region->guest_start;
		 cur_addr < region->guest_end;
		 cur_addr += PAGE_SIZE_4KB) {
		
		v3_invalidate_nested_addr(info, cur_addr);
	    }
	}
    }

    return 0;
}
						 



struct v3_mem_region * v3_get_mem_region(struct v3_vm_info * vm, uint16_t core_id, addr_t guest_addr) {
    struct rb_node * n = vm->mem_map.mem_regions.rb_node;
    struct v3_mem_region * reg = NULL;

    while (n) {
	reg = rb_entry(n, struct v3_mem_region, tree_node);

	if (guest_addr < reg->guest_start) {
	    n = n->rb_left;
	} else if (guest_addr >= reg->guest_end) {
	    n = n->rb_right;
	} else {
	    if ((core_id == reg->core_id) || 
		(reg->core_id == V3_MEM_CORE_ANY)) {
	    return reg;
	    } else {
		n = n->rb_right;
	    }
	}
    }


    // There is not registered region, so we check if its a valid address in the base region

    if (guest_addr > vm->mem_map.base_region.guest_end) {
	PrintError("Guest Address Exceeds Base Memory Size (ga=%p), (limit=%p)\n", 
		   (void *)guest_addr, (void *)vm->mem_map.base_region.guest_end);
	v3_print_mem_map(vm);

	return NULL;
    }
    
    return &(vm->mem_map.base_region);
}




void v3_delete_mem_region(struct v3_vm_info * vm, struct v3_mem_region * reg) {
    int i = 0;

    if (reg == NULL) {
	return;
    }

    for (i = 0; i < vm->num_cores; i++) {
	struct guest_info * info = &(vm->cores[i]);

	// flush virtual page tables 
	// 3 cases shadow, shadow passthrough, and nested

	if (info->shdw_pg_mode == SHADOW_PAGING) {
	    v3_mem_mode_t mem_mode = v3_get_vm_mem_mode(info);
	    
	    if (mem_mode == PHYSICAL_MEM) {
		addr_t cur_addr;
		
		for (cur_addr = reg->guest_start;
		     cur_addr < reg->guest_end;
		     cur_addr += PAGE_SIZE_4KB) {
		    v3_invalidate_passthrough_addr(info, cur_addr);
		}
	    } else {
		v3_invalidate_shadow_pts(info);
	    }
	    
	} else if (info->shdw_pg_mode == NESTED_PAGING) {
	    addr_t cur_addr;
	    
	    for (cur_addr = reg->guest_start;
		 cur_addr < reg->guest_end;
		 cur_addr += PAGE_SIZE_4KB) {
		
		v3_invalidate_nested_addr(info, cur_addr);
	    }
	}
    }

    v3_rb_erase(&(reg->tree_node), &(vm->mem_map.mem_regions));

    V3_Free(reg);

    // flush virtual page tables 
    // 3 cases shadow, shadow passthrough, and nested

}



void v3_print_mem_map(struct v3_vm_info * vm) {
    struct rb_node * node = v3_rb_first(&(vm->mem_map.mem_regions));
    struct v3_mem_region * reg = &(vm->mem_map.base_region);
    int i = 0;

    V3_Print("Memory Layout:\n");
    

    V3_Print("Base Region:  0x%p - 0x%p -> 0x%p\n", 
	       (void *)(reg->guest_start), 
	       (void *)(reg->guest_end - 1), 
	       (void *)(reg->host_addr));
    

    // If the memory map is empty, don't print it
    if (node == NULL) {
	return;
    }

    do {
	reg = rb_entry(node, struct v3_mem_region, tree_node);

	V3_Print("%d:  0x%p - 0x%p -> 0x%p\n", i, 
		   (void *)(reg->guest_start), 
		   (void *)(reg->guest_end - 1), 
		   (void *)(reg->host_addr));

	V3_Print("\t(flags=%x) (unhandled = 0x%p)\n", 
		   reg->flags.value,
		   reg->unhandled);
    
	i++;
    } while ((node = v3_rb_next(node)));
}

