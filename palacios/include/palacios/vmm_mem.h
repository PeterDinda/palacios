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


#ifndef __VMM_MEM_H
#define __VMM_MEM_H


#ifdef __V3VEE__ 


#include <palacios/vmm_types.h>

#include <palacios/vmm_paging.h>
#include <palacios/vmm_rbtree.h>
#include <palacios/vmm_list.h>

struct guest_info;
struct v3_vm_info;

#ifdef V3_CONFIG_SWAPPING
#include <palacios/vmm_swapping.h>
#endif




#define V3_MEM_CORE_ANY ((uint16_t)-1)



typedef struct {
    union {
	uint16_t value;
	struct {
	    // These reflect the VMM's intent for the shadow or nested pts 
	    // that will implement the region.   The guest's intent is in
	    // its own page tables.
	    uint8_t read   : 1;
	    uint8_t write  : 1;
	    uint8_t exec   : 1;
	    uint8_t base   : 1;
	    uint8_t alloced : 1;
	    uint8_t limit32 : 1; // must be < 4GB in host
#ifdef V3_CONFIG_SWAPPING 
	    uint8_t swapped : 1;
	    uint8_t pinned : 1;
#endif 
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed)) v3_mem_flags_t;



struct v3_mem_region {
    addr_t                  guest_start; 
    addr_t                  guest_end; 

    v3_mem_flags_t          flags;

    addr_t                  host_addr; // This either points to a host address mapping

    int (*unhandled)(struct guest_info * info, addr_t guest_va, addr_t guest_pa, 
		     struct v3_mem_region * reg, pf_error_t access_info);

    void * priv_data;

    int core_id;     // The virtual core this region is assigned to (-1 means all cores)
    int numa_id;     // The NUMA node this region is allocated from

#ifdef V3_CONFIG_SWAPPING 
    struct v3_swap_region_state swap_state;
#endif

    struct rb_node tree_node; // This for memory regions mapped to the global map
};


struct v3_mem_map {

    struct rb_root mem_regions;
    
    uint32_t num_base_regions;
    struct v3_mem_region * base_regions;
};


int v3_init_mem_map(struct v3_vm_info * vm);
void v3_delete_mem_map(struct v3_vm_info * vm);





struct v3_mem_region * v3_create_mem_region(struct v3_vm_info * vm, uint16_t core_id, 
					       addr_t guest_addr_start, addr_t guest_addr_end);

int v3_insert_mem_region(struct v3_vm_info * vm, struct v3_mem_region * reg);

void v3_delete_mem_region(struct v3_vm_info * vm, struct v3_mem_region * reg);


/* This is a shortcut function for creating + inserting a memory region which redirects to host memory */
int v3_add_shadow_mem(struct v3_vm_info * vm, uint16_t core_id,
		      addr_t guest_addr_start, addr_t guest_addr_end, addr_t host_addr);



struct v3_mem_region * v3_get_mem_region(struct v3_vm_info * vm, uint16_t core_id, addr_t guest_addr);
struct v3_mem_region * v3_get_base_region(struct v3_vm_info * vm, addr_t gpa);


uint32_t v3_get_max_page_size(struct guest_info * core, addr_t fault_addr, v3_cpu_mode_t mode);


void v3_print_mem_map(struct v3_vm_info * vm);


void v3_init_mem();
void v3_deinit_mem();


#endif /* ! __V3VEE__ */


#endif
