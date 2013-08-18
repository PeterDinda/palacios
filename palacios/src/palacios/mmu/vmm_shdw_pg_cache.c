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

#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_swapbypass.h>
#include <palacios/vmm_ctrl_regs.h>

#include <palacios/vm_guest.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm_hashtable.h>
#include <palacios/vmm_list.h>

#define DEFAULT_CACHE_SIZE ((32 * 1024 * 1024) / 4096)

#define V3_CACHED_PG 0x1

#ifndef V3_CONFIG_DEBUG_SHDW_PG_CACHE
#undef PrintDebug
#define PrintDebug(fmt, ...)
#endif


struct shdw_back_ptr {
    addr_t gva;
    struct shdw_pg_data * pg_data;
    struct list_head back_ptr_node;
};

struct guest_pg_tuple {
    addr_t gpa;
    page_type_t pt_type;    
} __attribute__((packed));



struct rmap_entry {
    addr_t gva;
    addr_t gpa;
    page_type_t pt_type;
    struct list_head rmap_node;
};

struct shdw_pg_data {
    struct guest_pg_tuple tuple;

    addr_t hpa;
    void * hva;

    struct list_head back_ptrs;
    struct list_head pg_queue_node;

};



struct cache_core_state {


};


struct cache_vm_state {
    
    v3_lock_t cache_lock;

    struct hashtable * page_htable; // GPA to shdw_pg_data
    struct hashtable * reverse_map;


    int max_cache_pgs;
    int pgs_in_cache;

    struct list_head pg_queue;

    int pgs_in_free_list;
    struct list_head free_list;
};



static  inline int evict_pt(void * pt, addr_t va, page_type_t pt_type) {
    
    switch (pt_type) {
	case PAGE_PD32: {
	    pde32_t * pde = pt;
	    pde[PDE32_INDEX(va)].present = 0;
	    break;
	}
	case PAGE_4MB: {
	    pde32_4MB_t * pde = pt;
	    pde[PDE32_INDEX(va)].present = 0;
	    break;
	}
	case PAGE_PT32: {
	    pte32_t * pte = pt;
	    pte[PTE32_INDEX(va)].present = 0;
	    break;
	}
	case PAGE_PML464: {
	    pml4e64_t * pml = pt;
	    pml[PML4E64_INDEX(va)].present = 0;
	    break;
	}
	case PAGE_PDP64: {
	    pdpe64_t * pdp = pt;
	    pdp[PDPE64_INDEX(va)].present = 0;
	    break;
	}
	case PAGE_PD64: {
	    pde64_t * pde = pt;
	    pde[PDE64_INDEX(va)].present = 0;
	    break;
	}
	case PAGE_PT64: {
	    pte64_t * pte = pt;
	    pte[PTE64_INDEX(va)].present = 0;
	    break;
	}
	default:
	    PrintError(VM_NONE, VCORE_NONE, "Invalid page type: %d\n", pt_type);
	    return -1;
    }

    return 0;
}



static  inline int grab_pt(void * pt, addr_t va, page_type_t pt_type) {
    
    switch (pt_type) {
	case PAGE_PD32: {
	    pde32_t * pde = pt;
	    pde[PDE32_INDEX(va)].writable = 0;
	    break;
	}
	case PAGE_4MB: {
	    pde32_4MB_t * pde = pt;
	    pde[PDE32_INDEX(va)].writable = 0;
	    break;
	}
	case PAGE_PT32: {
	    pte32_t * pte = pt;
	    pte[PTE32_INDEX(va)].writable = 0;
	    break;
	}
	case PAGE_PML464: {
	    pml4e64_t * pml = pt;
	    pml[PML4E64_INDEX(va)].writable = 0;
	    break;
	}
	case PAGE_PDP64: {
	    pdpe64_t * pdp = pt;
	    pdp[PDPE64_INDEX(va)].writable = 0;
	    break;
	}
	case PAGE_PD64: {
	    pde64_t * pde = pt;
	    pde[PDE64_INDEX(va)].writable = 0;
	    break;
	}
	case PAGE_PT64: {
	    pte64_t * pte = pt;
	    pte[PTE64_INDEX(va)].writable = 0;
	    break;
	}
	default:
	    PrintError(VM_NONE, VCORE_NONE, "Invalid page type: %d\n", pt_type);
	    return -1;
    }

    return 0;
}


static int unlink_shdw_pg(struct shdw_pg_data * pg_data) {
    struct shdw_back_ptr * back_ptr = NULL;
    struct shdw_back_ptr * tmp_ptr = NULL;

    PrintError(VM_NONE, VCORE_NONE, "Unlinking gpa=%p, type=%d\n", (void *)pg_data->tuple.gpa, pg_data->tuple.pt_type);

    list_for_each_entry_safe(back_ptr, tmp_ptr, &(pg_data->back_ptrs), back_ptr_node) {
	struct shdw_pg_data * parent = back_ptr->pg_data;
	
	evict_pt(parent->hva, back_ptr->gva, parent->tuple.pt_type);
	list_del(&(back_ptr->back_ptr_node));
	V3_Free(back_ptr);
    }
    


    return 0;
}


static int add_rmap(struct v3_vm_info * vm, struct shdw_pg_data * pg_data, addr_t gpa, addr_t gva) {
    struct cache_vm_state * cache_state = vm->shdw_impl.impl_data;
    struct list_head * rmap_list = NULL;
    struct rmap_entry * entry = NULL;


    rmap_list = (struct list_head *)v3_htable_search(cache_state->reverse_map, gpa);

    if (rmap_list == NULL) {
	rmap_list = V3_Malloc(sizeof(struct list_head));

	if (!rmap_list) {
	    PrintError(vm, VCORE_NONE, "Cannot allocate\n");
	    return -1;
	}

	INIT_LIST_HEAD(rmap_list);

	v3_htable_insert(cache_state->reverse_map, gpa, (addr_t)rmap_list);
    }
    
    entry = V3_Malloc(sizeof(struct rmap_entry));

    if (!entry) {
	PrintError(vm, VCORE_NONE,  "Cannot allocate\n");
	return -1;
    }

    entry->gva = gva;
    entry->gpa = pg_data->tuple.gpa;
    entry->pt_type = pg_data->tuple.pt_type;

    list_add(&(entry->rmap_node), rmap_list);

    return 0;
}



static int update_rmap_entries(struct v3_vm_info * vm, addr_t gpa) {
    struct cache_vm_state * cache_state = vm->shdw_impl.impl_data;
    struct list_head * rmap_list = NULL;
    struct rmap_entry * entry = NULL;
    int i = 0;

    rmap_list = (struct list_head *)v3_htable_search(cache_state->reverse_map, gpa);

    if (rmap_list == NULL) {
	return 0;
    }

    PrintError(vm, VCORE_NONE, "Updating rmap entries\n\t");

    list_for_each_entry(entry, rmap_list, rmap_node) {
	struct shdw_pg_data * pg_data = NULL;
	struct guest_pg_tuple tuple = {entry->gpa, entry->pt_type};

	V3_Print(vm, VCORE_NONE,  "%d \n", i);

	pg_data = (struct shdw_pg_data *)v3_htable_search(cache_state->page_htable, (addr_t)&tuple);

	if (!pg_data) {
	    PrintError(vm, VCORE_NONE, "Invalid PTE reference... Should Delete rmap entry\n");
	    continue;
	}

	if (grab_pt(pg_data->hva, entry->gva, entry->pt_type) == -1) {
	    PrintError(vm, VCORE_NONE, "Could not invalidate reverse map entry\n");
	    return -1;
	}

	i++;
	
    }

    return 0;
}




static int link_shdw_pg(struct shdw_pg_data * child_pg, struct shdw_pg_data * parent_pg, addr_t gva) {
    struct shdw_back_ptr * back_ptr = V3_Malloc(sizeof(struct shdw_back_ptr));

    if (!back_ptr) {
	PrintError(VM_NONE, VCORE_NONE,  "Cannot allocate\n");
	return -1;
    }

    memset(back_ptr, 0, sizeof(struct shdw_back_ptr));

    back_ptr->pg_data = parent_pg;
    back_ptr->gva = gva;

    list_add(&(back_ptr->back_ptr_node), &(child_pg->back_ptrs));
   
    return 0;
}



static struct shdw_pg_data * find_shdw_pt(struct v3_vm_info * vm, addr_t gpa, page_type_t pt_type) {
    struct cache_vm_state * cache_state = vm->shdw_impl.impl_data;
    struct shdw_pg_data * pg_data = NULL;
    struct guest_pg_tuple tuple = {gpa, pt_type};
    
    pg_data = (struct shdw_pg_data *)v3_htable_search(cache_state->page_htable, (addr_t)&tuple);

    if (pg_data != NULL) {
	// move pg_data to head of queue, for LRU policy
	list_move(&(pg_data->pg_queue_node), &(cache_state->pg_queue));
    }

    return pg_data;
}


static int evict_shdw_pg(struct v3_vm_info * vm, addr_t gpa, page_type_t pt_type) {
    struct cache_vm_state * cache_state = vm->shdw_impl.impl_data;
    struct shdw_pg_data * pg_data = NULL;

    pg_data = find_shdw_pt(vm, gpa, pt_type);

    PrintError(vm, VCORE_NONE,  "Evicting GPA: %p, type=%d\n", (void *)gpa, pt_type);

    if (pg_data != NULL) {
	if (unlink_shdw_pg(pg_data) == -1) {
	    PrintError(vm, VCORE_NONE,  "Error unlinking page...\n");
	    return -1;
	}
	
	v3_htable_remove(cache_state->page_htable, (addr_t)&(pg_data->tuple), 0);
	

	// Move Page to free list
	list_move(&(pg_data->pg_queue_node), &(cache_state->free_list));
	cache_state->pgs_in_free_list++;
	cache_state->pgs_in_cache--;
    }

    return 0;
}


static struct shdw_pg_data * pop_queue_pg(struct v3_vm_info * vm, 
					  struct cache_vm_state * cache_state) {
    struct shdw_pg_data * pg_data = NULL;

    PrintError(vm, VCORE_NONE, "popping page from queue\n");

    pg_data = list_tail_entry(&(cache_state->pg_queue), struct shdw_pg_data, pg_queue_node);


    if (unlink_shdw_pg(pg_data) == -1) {
        PrintError(vm, VCORE_NONE, "Error unlinking cached page\n");
	return NULL;
    }

    v3_htable_remove(cache_state->page_htable, (addr_t)&(pg_data->tuple), 0);
    list_del(&(pg_data->pg_queue_node));
    
    cache_state->pgs_in_cache--;

    return pg_data;
}

static struct shdw_pg_data * create_shdw_pt(struct v3_vm_info * vm, addr_t gpa, page_type_t pt_type) {
    struct cache_vm_state * cache_state = vm->shdw_impl.impl_data;
    struct shdw_pg_data * pg_data = NULL;


    PrintError(vm, VCORE_NONE, "Creating shdw page: gpa=%p, type=%d\n", (void *)gpa, pt_type);

    if (cache_state->pgs_in_cache < cache_state->max_cache_pgs) {
	pg_data = V3_Malloc(sizeof(struct shdw_pg_data));

	if (!pg_data) {
	    PrintError(vm, VCORE_NONE,  "Cannot allocate\n");
	    return NULL;
	}

	pg_data->hpa = (addr_t)V3_AllocPagesExtended(1,PAGE_SIZE_4KB,-1,
						     V3_ALLOC_PAGES_CONSTRAINT_4GB);


	if (!pg_data->hpa) {
	    PrintError(vm, VCORE_NONE,  "Cannot allocate page for shadow page table\n");
	    return NULL;
	}

	pg_data->hva = (void *)V3_VAddr((void *)pg_data->hpa);

    } else if (cache_state->pgs_in_free_list) {

	PrintError(vm, VCORE_NONE,  "pulling page from free list\n");
	// pull from free list
	pg_data = list_tail_entry(&(cache_state->free_list), struct shdw_pg_data, pg_queue_node);
	
	list_del(&(pg_data->pg_queue_node));
	cache_state->pgs_in_free_list--;

    } else {
	// pull from queue
	pg_data = pop_queue_pg(vm, cache_state);
    }


    if (pg_data == NULL) {
	PrintError(vm, VCORE_NONE,  "Error creating Shadow Page table page\n");
	return NULL;
    }

    memset(pg_data->hva, 0, PAGE_SIZE_4KB);

    pg_data->tuple.gpa = gpa;
    pg_data->tuple.pt_type = pt_type;

    INIT_LIST_HEAD(&(pg_data->back_ptrs));

    v3_htable_insert(cache_state->page_htable, (addr_t)&(pg_data->tuple), (addr_t)pg_data);

    list_add(&(pg_data->pg_queue_node), &(cache_state->pg_queue));
    cache_state->pgs_in_cache++;

    return pg_data;

}


#include "vmm_shdw_pg_cache_32.h"
//#include "vmm_shdw_pg_cache_32pae.h"
//#include "vmm_shdw_pg_cache_64.h"


static uint_t cache_hash_fn(addr_t key) {
    struct guest_pg_tuple * tuple = (struct guest_pg_tuple *)key;

    return v3_hash_buffer((uint8_t *)tuple, sizeof(struct guest_pg_tuple));
}

static int cache_eq_fn(addr_t key1, addr_t key2) {
    struct guest_pg_tuple * tuple1 = (struct guest_pg_tuple *)key1;
    struct guest_pg_tuple * tuple2 = (struct guest_pg_tuple *)key2;
	
    return ((tuple1->gpa == tuple2->gpa) && (tuple1->pt_type == tuple2->pt_type));
}

static uint_t rmap_hash_fn(addr_t key) {
    return v3_hash_long(key, sizeof(addr_t) * 8);
}

static int rmap_eq_fn(addr_t key1, addr_t key2) {
    return (key1 == key2);
}


static int cache_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct v3_shdw_impl_state * vm_state = &(vm->shdw_impl);
    struct cache_vm_state * cache_state = NULL;
    int cache_size = DEFAULT_CACHE_SIZE;
    char * cache_sz_str = v3_cfg_val(cfg, "cache_size");

    if (cache_sz_str != NULL) {
	cache_size = ((atoi(cache_sz_str) * 1024 * 1024) / 4096);
    }

    V3_Print(vm, VCORE_NONE, "Shadow Page Cache initialization\n");

    cache_state = V3_Malloc(sizeof(struct cache_vm_state));

    if (!cache_state) {
	PrintError(vm, VCORE_NONE, "Cannot allocate\n");
	return -1;
    }

    memset(cache_state, 0, sizeof(struct cache_vm_state));

    cache_state->page_htable = v3_create_htable(0, cache_hash_fn, cache_eq_fn);
    cache_state->reverse_map = v3_create_htable(0, rmap_hash_fn, rmap_eq_fn);
    v3_lock_init(&(cache_state->cache_lock));
    INIT_LIST_HEAD(&(cache_state->pg_queue));
    INIT_LIST_HEAD(&(cache_state->free_list));
    cache_state->max_cache_pgs = cache_size;

    vm_state->impl_data = cache_state;

    return 0;
}


static int cache_deinit(struct v3_vm_info * vm) {
    return -1;
}


static int cache_local_init(struct guest_info * core) {
    //    struct v3_shdw_pg_state * core_state = &(vm->shdw_pg_state);


    return 0;
}

static int cache_activate_shdw_pt(struct guest_info * core) {
    switch (v3_get_vm_cpu_mode(core)) {

	case PROTECTED:
	    PrintError(core->vm_info, core, "Calling 32 bit cache activation\n");
	    return activate_shadow_pt_32(core);
	case PROTECTED_PAE:
	    //	    return activate_shadow_pt_32pae(core);
	case LONG:
	case LONG_32_COMPAT:
	case LONG_16_COMPAT:
	    //	    return activate_shadow_pt_64(core);
	default:
	    PrintError(core->vm_info, core, "Invalid CPU mode: %s\n", v3_cpu_mode_to_str(v3_get_vm_cpu_mode(core)));
	    return -1;
    }

    return 0;
}

static int cache_invalidate_shdw_pt(struct guest_info * core) {
    // wipe everything...
    V3_Print(core->vm_info, core, "Cache invalidation called\n");
    
    return cache_activate_shdw_pt(core);
}



static int cache_handle_pf(struct guest_info * core, addr_t fault_addr, pf_error_t error_code) {

	switch (v3_get_vm_cpu_mode(core)) {
	    case PROTECTED:
		return handle_shadow_pagefault_32(core, fault_addr, error_code);
		break;
	    case PROTECTED_PAE:
		//	return handle_shadow_pagefault_32pae(core, fault_addr, error_code);
	    case LONG:
	    case LONG_32_COMPAT:
	    case LONG_16_COMPAT:
		//	return handle_shadow_pagefault_64(core, fault_addr, error_code);
	    default:
		PrintError(core->vm_info, core, "Unhandled CPU Mode: %s\n", v3_cpu_mode_to_str(v3_get_vm_cpu_mode(core)));
		return -1;
	}
}


static int cache_handle_invlpg(struct guest_info * core, addr_t vaddr) {
    PrintError(core->vm_info, core, "INVLPG called for %p\n", (void *)vaddr);

    switch (v3_get_vm_cpu_mode(core)) {
	case PROTECTED:
	    return handle_shadow_invlpg_32(core, vaddr);
	case PROTECTED_PAE:
	    //    return handle_shadow_invlpg_32pae(core, vaddr);
	case LONG:
	case LONG_32_COMPAT:
	case LONG_16_COMPAT:
	    //    return handle_shadow_invlpg_64(core, vaddr);
	default:
	    PrintError(core->vm_info, core, "Invalid CPU mode: %s\n", v3_cpu_mode_to_str(v3_get_vm_cpu_mode(core)));
	    return -1;
    }
}






static struct v3_shdw_pg_impl cache_impl = {
    .name = "SHADOW_CACHE",
    .init = cache_init, 
    .deinit = cache_deinit, 
    .local_init = cache_local_init, 
    .handle_pagefault = cache_handle_pf, 
    .handle_invlpg = cache_handle_invlpg,
    .activate_shdw_pt = cache_activate_shdw_pt, 
    .invalidate_shdw_pt = cache_invalidate_shdw_pt
};



register_shdw_pg_impl(&cache_impl);
