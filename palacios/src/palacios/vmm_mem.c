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

#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_direct_paging.h>


#define MEM_OFFSET_HCALL 0x1000


static inline
struct v3_shadow_region * insert_shadow_region(struct guest_info * info, 
 					       struct v3_shadow_region * region);


static int mem_offset_hypercall(struct guest_info * info, uint_t hcall_id, void * private_data) {
    info->vm_regs.rbx = info->mem_map.base_region.host_addr;

    return 0;
}


void v3_init_shadow_map(struct guest_info * info) {
    v3_shdw_map_t * map = &(info->mem_map);
    addr_t mem_pages = info->mem_size >> 12;

    map->shdw_regions.rb_node = NULL;
    map->hook_hva = (addr_t)V3_VAddr(V3_AllocPages(1));

    // There is an underlying region that contains all of the guest memory
    // PrintDebug("Mapping %d pages of memory (%u bytes)\n", (int)mem_pages, (uint_t)info->mem_size);

    map->base_region.guest_start = 0;
    map->base_region.guest_end = mem_pages * PAGE_SIZE_4KB;
    map->base_region.host_type = SHDW_REGION_ALLOCATED;
    map->base_region.host_addr = (addr_t)V3_AllocPages(mem_pages);

    //memset(V3_VAddr((void *)map->base_region.host_addr), 0xffffffff, map->base_region.guest_end);

    v3_register_hypercall(info, MEM_OFFSET_HCALL, mem_offset_hypercall, NULL);
}

void v3_delete_shadow_map(struct guest_info * info) {
    struct rb_node * node = v3_rb_first(&(info->mem_map.shdw_regions));
    struct v3_shadow_region * reg;
    struct rb_node * tmp_node = NULL;
  
    while (node) {
	reg = rb_entry(node, struct v3_shadow_region, tree_node);
	tmp_node = node;
	node = v3_rb_next(node);

	v3_delete_shadow_region(info, reg);
    }

    V3_FreePage((void *)(info->mem_map.base_region.host_addr));
    V3_FreePage(V3_PAddr((void *)(info->mem_map.hook_hva)));
}




int v3_add_shadow_mem( struct guest_info *  info,
		       addr_t               guest_addr_start,
		       addr_t               guest_addr_end,
		       addr_t               host_addr)
{
    struct v3_shadow_region * entry = (struct v3_shadow_region *)V3_Malloc(sizeof(struct v3_shadow_region));

    entry->guest_start = guest_addr_start;
    entry->guest_end = guest_addr_end;
    entry->host_type = SHDW_REGION_ALLOCATED;
    entry->host_addr = host_addr;
    entry->write_hook = NULL;
    entry->read_hook = NULL;
    entry->priv_data = NULL;

    if (insert_shadow_region(info, entry)) {
	V3_Free(entry);
	return -1;
    }

    return 0;
}



int v3_hook_write_mem(struct guest_info * info, addr_t guest_addr_start, addr_t guest_addr_end, 
		      addr_t host_addr,
		      int (*write)(addr_t guest_addr, void * src, uint_t length, void * priv_data),
		      void * priv_data) {

    struct v3_shadow_region * entry = (struct v3_shadow_region *)V3_Malloc(sizeof(struct v3_shadow_region));


    entry->guest_start = guest_addr_start;
    entry->guest_end = guest_addr_end;
    entry->host_type = SHDW_REGION_WRITE_HOOK;
    entry->host_addr = host_addr;
    entry->write_hook = write;
    entry->read_hook = NULL;
    entry->priv_data = priv_data;

    if (insert_shadow_region(info, entry)) {
	V3_Free(entry);
	return -1;
    }

    return 0;  
}

int v3_hook_full_mem(struct guest_info * info, addr_t guest_addr_start, addr_t guest_addr_end,
		     int (*read)(addr_t guest_addr, void * dst, uint_t length, void * priv_data),
		     int (*write)(addr_t guest_addr, void * src, uint_t length, void * priv_data),
		     void * priv_data) {
  
    struct v3_shadow_region * entry = (struct v3_shadow_region *)V3_Malloc(sizeof(struct v3_shadow_region));

    entry->guest_start = guest_addr_start;
    entry->guest_end = guest_addr_end;
    entry->host_type = SHDW_REGION_FULL_HOOK;
    entry->host_addr = (addr_t)NULL;
    entry->write_hook = write;
    entry->read_hook = read;
    entry->priv_data = priv_data;
  
    if (insert_shadow_region(info, entry)) {
	V3_Free(entry);
	return -1;
    }

    return 0;
}


// This will unhook the memory hook registered at start address
// We do not support unhooking subregions
int v3_unhook_mem(struct guest_info * info, addr_t guest_addr_start) {
    struct v3_shadow_region * reg = v3_get_shadow_region(info, guest_addr_start);

    if ((reg->host_type != SHDW_REGION_FULL_HOOK) || 
	(reg->host_type != SHDW_REGION_WRITE_HOOK)) {
	PrintError("Trying to unhook a non hooked memory region (addr=%p)\n", (void *)guest_addr_start);
	return -1;
    }

    v3_delete_shadow_region(info, reg);

    return 0;
}



static inline 
struct v3_shadow_region * __insert_shadow_region(struct guest_info * info, 
						 struct v3_shadow_region * region) {
    struct rb_node ** p = &(info->mem_map.shdw_regions.rb_node);
    struct rb_node * parent = NULL;
    struct v3_shadow_region * tmp_region;

    while (*p) {
	parent = *p;
	tmp_region = rb_entry(parent, struct v3_shadow_region, tree_node);

	if (region->guest_end <= tmp_region->guest_start) {
	    p = &(*p)->rb_left;
	} else if (region->guest_start >= tmp_region->guest_end) {
	    p = &(*p)->rb_right;
	} else {
	    return tmp_region;
	}
    }

    rb_link_node(&(region->tree_node), parent, p);
  
    return NULL;
}


static inline
struct v3_shadow_region * insert_shadow_region(struct guest_info * info, 
 					       struct v3_shadow_region * region) {
    struct v3_shadow_region * ret;

    if ((ret = __insert_shadow_region(info, region))) {
	return ret;
    }
  
    v3_rb_insert_color(&(region->tree_node), &(info->mem_map.shdw_regions));



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

    return NULL;
}
						 



int handle_special_page_fault(struct guest_info * info, 
			      addr_t fault_gva, addr_t fault_gpa, 
			      pf_error_t access_info) 
{
    struct v3_shadow_region * reg = v3_get_shadow_region(info, fault_gpa);

    PrintDebug("Handling Special Page Fault\n");

    switch (reg->host_type) {
	case SHDW_REGION_WRITE_HOOK:
	    return v3_handle_mem_wr_hook(info, fault_gva, fault_gpa, reg, access_info);
	case SHDW_REGION_FULL_HOOK:
	    return v3_handle_mem_full_hook(info, fault_gva, fault_gpa, reg, access_info);
	default:
	    return -1;
    }

    return 0;

}

int v3_handle_mem_wr_hook(struct guest_info * info, addr_t guest_va, addr_t guest_pa, 
			  struct v3_shadow_region * reg, pf_error_t access_info) {

    addr_t dst_addr = (addr_t)V3_VAddr((void *)v3_get_shadow_addr(reg, guest_pa));

    if (v3_emulate_write_op(info, guest_va, guest_pa, dst_addr, 
			    reg->write_hook, reg->priv_data) == -1) {
	PrintError("Write hook emulation failed\n");
	return -1;
    }

    return 0;
}

int v3_handle_mem_full_hook(struct guest_info * info, addr_t guest_va, addr_t guest_pa, 
			    struct v3_shadow_region * reg, pf_error_t access_info) {
  
    addr_t op_addr = info->mem_map.hook_hva;

    if (access_info.write == 1) {
	if (v3_emulate_write_op(info, guest_va, guest_pa, op_addr, 
				reg->write_hook, reg->priv_data) == -1) {
	    PrintError("Write Full Hook emulation failed\n");
	    return -1;
	}
    } else {
	if (v3_emulate_read_op(info, guest_va, guest_pa, op_addr, 
			       reg->read_hook, reg->write_hook, 
			       reg->priv_data) == -1) {
	    PrintError("Read Full Hook emulation failed\n");
	    return -1;
	}
    }

    return 0;
}



struct v3_shadow_region * v3_get_shadow_region(struct guest_info * info, addr_t guest_addr) {
    struct rb_node * n = info->mem_map.shdw_regions.rb_node;
    struct v3_shadow_region * reg = NULL;

    while (n) {
	reg = rb_entry(n, struct v3_shadow_region, tree_node);

	if (guest_addr < reg->guest_start) {
	    n = n->rb_left;
	} else if (guest_addr >= reg->guest_end) {
	    n = n->rb_right;
	} else {
	    return reg;
	}
    }


    // There is not registered region, so we check if its a valid address in the base region

    if (guest_addr > info->mem_map.base_region.guest_end) {
	PrintError("Guest Address Exceeds Base Memory Size (ga=%p), (limit=%p)\n", 
		   (void *)guest_addr, (void *)info->mem_map.base_region.guest_end);
	return NULL;
    }
    
    return &(info->mem_map.base_region);
}


void v3_delete_shadow_region(struct guest_info * info, struct v3_shadow_region * reg) {
    if (reg == NULL) {
	return;
    }

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


    v3_rb_erase(&(reg->tree_node), &(info->mem_map.shdw_regions));

    V3_Free(reg);

    // flush virtual page tables 
    // 3 cases shadow, shadow passthrough, and nested

}




addr_t v3_get_shadow_addr(struct v3_shadow_region * reg, addr_t guest_addr) {
    if ( (reg) && 
         (reg->host_type != SHDW_REGION_FULL_HOOK)) {
        return (guest_addr - reg->guest_start) + reg->host_addr;
    } else {
        PrintError("MEM Region Invalid\n");
        return 0;
    }

}



void print_shadow_map(struct guest_info * info) {
    struct rb_node * node = v3_rb_first(&(info->mem_map.shdw_regions));
    struct v3_shadow_region * reg = &(info->mem_map.base_region);
    int i = 0;

    PrintDebug("Memory Layout:\n");
    

    PrintDebug("Base Region:  0x%p - 0x%p -> 0x%p\n", 
	       (void *)(reg->guest_start), 
	       (void *)(reg->guest_end - 1), 
	       (void *)(reg->host_addr));
    
    do {
	reg = rb_entry(node, struct v3_shadow_region, tree_node);

	PrintDebug("%d:  0x%p - 0x%p -> 0x%p\n", i, 
		   (void *)(reg->guest_start), 
		   (void *)(reg->guest_end - 1), 
		   (void *)(reg->host_addr));

	PrintDebug("\t(%s) (WriteHook = 0x%p) (ReadHook = 0x%p)\n", 
		   v3_shdw_region_type_to_str(reg->host_type),
		   (void *)(reg->write_hook), 
		   (void *)(reg->read_hook));
    
	i++;
    } while ((node = v3_rb_next(node)));
}


static const uchar_t  SHDW_REGION_WRITE_HOOK_STR[] = "SHDW_REGION_WRITE_HOOK";
static const uchar_t  SHDW_REGION_FULL_HOOK_STR[] = "SHDW_REGION_FULL_HOOK";
static const uchar_t  SHDW_REGION_ALLOCATED_STR[] = "SHDW_REGION_ALLOCATED";

const uchar_t * v3_shdw_region_type_to_str(v3_shdw_region_type_t type) {
    switch (type) {
	case SHDW_REGION_WRITE_HOOK:
	    return SHDW_REGION_WRITE_HOOK_STR;
	case SHDW_REGION_FULL_HOOK:
	    return SHDW_REGION_FULL_HOOK_STR;
	case SHDW_REGION_ALLOCATED:
	    return SHDW_REGION_ALLOCATED_STR;
	default:
	    return (uchar_t *)"SHDW_REGION_INVALID";
    }
}

