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


#ifdef V3_CONFIG_SWAPBYPASS_TELEMETRY
#include <palacios/vmm_telemetry.h>
#endif






// This is a hack and 32 bit linux specific.... need to fix...
struct swap_pte {
    uint32_t present    : 1;
    uint32_t dev_index  : 8;
    uint32_t pg_index   : 23;
};


struct shadow_pointer {
    uint32_t pg_index;
    uint32_t dev_index;

    pte32_t * shadow_pte;
    
    addr_t guest_pte;
    
    struct list_head node;
};






struct shadow_page_data {
    v3_reg_t cr3;
    addr_t page_pa;
  
    struct list_head page_list_node;
};


struct swapbypass_local_state {
 
    struct list_head page_list;

};

struct v3_swap_dev {
    uint8_t present;

    struct v3_swap_ops * ops;

    void * private_data;
};


struct swapbypass_vm_state {
    struct v3_swap_dev devs[256];

#ifdef V3_CONFIG_SWAPBYPASS_TELEMETRY
    uint32_t read_faults;
    uint32_t write_faults;
    uint32_t flushes;
    uint32_t mapped_pages;
    uint32_t list_size;
#endif

    // shadow pointers
    struct hashtable * shdw_ptr_ht;
};




static uint_t swap_hash_fn(addr_t key) {
    return v3_hash_long(key, 32);
}


static int swap_eq_fn(addr_t key1, addr_t key2) {
    return (key1 == key2);
}



static inline uint32_t get_pg_index(pte32_t * pte) {
    return ((struct swap_pte *)pte)->pg_index;
}


static inline uint32_t get_dev_index(pte32_t * pte) {
    return ((struct swap_pte *)pte)->dev_index;
}


// Present = 0 and Dirty = 0
// fixme
static inline int is_swapped_pte32(pte32_t * pte) {
    return ((pte->present == 0) && (*(uint32_t *)pte != 0));
}




static struct shadow_page_data * create_new_shadow_pt(struct guest_info * core);



#ifdef V3_CONFIG_SWAPBYPASS_TELEMETRY
static void telemetry_cb(struct v3_vm_info * vm, void * private_data, char * hdr) {
    struct swapbypass_vm_state * swap_state = (struct swapbypass_vm_state *)(vm->shdw_impl.impl_data);

    V3_Print("%sSymbiotic Swap:\n", hdr);
    V3_Print("%s\tRead faults=%d\n", hdr, swap_state->read_faults);
    V3_Print("%s\tWrite faults=%d\n", hdr, swap_state->write_faults);
    V3_Print("%s\tMapped Pages=%d\n", hdr, swap_state->mapped_pages);
    V3_Print("%s\tFlushes=%d\n", hdr, swap_state->flushes);
    V3_Print("%s\tlist size=%d\n", hdr, swap_state->list_size);
}
#endif









static int get_vaddr_perms(struct guest_info * info, addr_t vaddr, pte32_t * guest_pte, pf_error_t * page_perms) {
    uint64_t pte_val = (uint64_t)*(uint32_t *)guest_pte;

    // symcall to check if page is in cache or on swap disk
    if (v3_sym_call3(info, SYMCALL_MEM_LOOKUP, (uint64_t *)&vaddr, (uint64_t *)&pte_val, (uint64_t *)page_perms) == -1) {
	PrintError("Sym call error?? that's weird... \n");
	return -1;
    }

    //    V3_Print("page perms = %x\n", *(uint32_t *)page_perms);

    if (vaddr == 0) {
	return 1;
    }

    return 0;
}


static addr_t get_swapped_pg_addr(struct v3_vm_info * vm, pte32_t * guest_pte) {
   struct swapbypass_vm_state * swap_state = (struct swapbypass_vm_state *)(vm->shdw_impl.impl_data);
    int dev_index = get_dev_index(guest_pte);
    struct v3_swap_dev * swp_dev = &(swap_state->devs[dev_index]);


    if (! swp_dev->present ) {
	return 0;
    }

    return (addr_t)swp_dev->ops->get_swap_entry(get_pg_index(guest_pte), swp_dev->private_data);
}



static addr_t map_swp_page(struct v3_vm_info * vm, pte32_t * shadow_pte, pte32_t * guest_pte, void * swp_page_ptr) {
   struct swapbypass_vm_state * swap_state = (struct swapbypass_vm_state *)(vm->shdw_impl.impl_data);
    struct list_head * shdw_ptr_list = NULL;
    struct shadow_pointer * shdw_ptr = NULL;



    if (swp_page_ptr == NULL) {
	//	PrintError("Swapped out page not found on swap device\n");
	return 0;
    }

    shdw_ptr_list = (struct list_head *)v3_htable_search(swap_state->shdw_ptr_ht, (addr_t)*(uint32_t *)guest_pte);

    if (shdw_ptr_list == NULL) {
	shdw_ptr_list = (struct list_head *)V3_Malloc(sizeof(struct list_head));
#ifdef V3_CONFIG_SWAPBYPASS_TELEMETRY
	swap_state->list_size++;
#endif
	INIT_LIST_HEAD(shdw_ptr_list);
	v3_htable_insert(swap_state->shdw_ptr_ht, (addr_t)*(uint32_t *)guest_pte, (addr_t)shdw_ptr_list);
    }

    shdw_ptr = (struct shadow_pointer *)V3_Malloc(sizeof(struct shadow_pointer));

    if (shdw_ptr == NULL) {
	PrintError("MEMORY LEAK\n");
#ifdef V3_CONFIG_SWAPBYPASS_TELEMETRY
	telemetry_cb(vm, NULL, "");
#endif
	return 0;
    }

    shdw_ptr->shadow_pte = shadow_pte;
    shdw_ptr->guest_pte = *(uint32_t *)guest_pte;
    shdw_ptr->pg_index = get_pg_index(guest_pte);
    shdw_ptr->dev_index = get_dev_index(guest_pte);

    // We don't check for conflicts, because it should not happen...
    list_add(&(shdw_ptr->node), shdw_ptr_list);

    return PAGE_BASE_ADDR((addr_t)V3_PAddr(swp_page_ptr));
}





#include "vmm_shdw_pg_swapbypass_32.h"
#include "vmm_shdw_pg_swapbypass_32pae.h"
#include "vmm_shdw_pg_swapbypass_64.h"


static struct shadow_page_data * create_new_shadow_pt(struct guest_info * core) {
    struct v3_shdw_pg_state * state = &(core->shdw_pg_state);
    struct swapbypass_local_state * impl_state = (struct swapbypass_local_state *)(state->local_impl_data);
    v3_reg_t cur_cr3 = core->ctrl_regs.cr3;
    struct shadow_page_data * page_tail = NULL;
    addr_t shdw_page = 0;

    if (!list_empty(&(impl_state->page_list))) {
	page_tail = list_tail_entry(&(impl_state->page_list), struct shadow_page_data, page_list_node);


	if (page_tail->cr3 != cur_cr3) {
	    PrintDebug("Reusing old shadow Page: %p (cur_CR3=%p)(page_cr3=%p) \n",
		       (void *)(addr_t)page_tail->page_pa, 
		       (void *)(addr_t)cur_cr3, 
		       (void *)(addr_t)(page_tail->cr3));

	    list_move(&(page_tail->page_list_node), &(impl_state->page_list));

	    memset(V3_VAddr((void *)(page_tail->page_pa)), 0, PAGE_SIZE_4KB);


	    return page_tail;
	}
    }

    // else  

    page_tail = (struct shadow_page_data *)V3_Malloc(sizeof(struct shadow_page_data));
    page_tail->page_pa = (addr_t)V3_AllocPages(1);

    PrintDebug("Allocating new shadow Page: %p (cur_cr3=%p)\n", 
	       (void *)(addr_t)page_tail->page_pa, 
	       (void *)(addr_t)cur_cr3);

    page_tail->cr3 = cur_cr3;
    list_add(&(page_tail->page_list_node), &(impl_state->page_list));

    shdw_page = (addr_t)V3_VAddr((void *)(page_tail->page_pa));
    memset((void *)shdw_page, 0, PAGE_SIZE_4KB);

    return page_tail;
}






int v3_register_swap_disk(struct v3_vm_info * vm, int dev_index, 
			  struct v3_swap_ops * ops, void * private_data) {
    struct swapbypass_vm_state * swap_state = (struct swapbypass_vm_state *)(vm->shdw_impl.impl_data);

    swap_state->devs[dev_index].present = 1;
    swap_state->devs[dev_index].private_data = private_data;
    swap_state->devs[dev_index].ops = ops;

    return 0;
}




int v3_swap_in_notify(struct v3_vm_info * vm, int pg_index, int dev_index) {
    struct list_head * shdw_ptr_list = NULL;
    struct swapbypass_vm_state * swap_state = (struct swapbypass_vm_state *)(vm->shdw_impl.impl_data);
    struct shadow_pointer * tmp_shdw_ptr = NULL;
    struct shadow_pointer * shdw_ptr = NULL;
    struct swap_pte guest_pte = {0, dev_index, pg_index};

    shdw_ptr_list = (struct list_head * )v3_htable_search(swap_state->shdw_ptr_ht, *(addr_t *)&(guest_pte));

    if (shdw_ptr_list == NULL) {
	return 0;
    }

    list_for_each_entry_safe(shdw_ptr, tmp_shdw_ptr, shdw_ptr_list, node) {
	if ((shdw_ptr->pg_index == pg_index) &&
	    (shdw_ptr->dev_index == dev_index)) {

	    // Trigger faults for next shadow access
	    shdw_ptr->shadow_pte->present = 0;

	    // Delete entry from list
	    list_del(&(shdw_ptr->node));
	    V3_Free(shdw_ptr);
	}
    }

    return 0;
}



int v3_swap_flush(struct v3_vm_info * vm) {
    struct swapbypass_vm_state * swap_state = (struct swapbypass_vm_state *)(vm->shdw_impl.impl_data);
    struct hashtable_iter * ht_iter = v3_create_htable_iter(swap_state->shdw_ptr_ht);

    //    PrintDebug("Flushing Symbiotic Swap table\n");

#ifdef V3_CONFIG_SWAPBYPASS_TELEMETRY
    swap_state->flushes++;
#endif

    if (!ht_iter) {
	PrintError("NULL iterator in swap flush!! Probably will crash soon...\n");
    }

    while (ht_iter->entry) {
	struct shadow_pointer * tmp_shdw_ptr = NULL;
	struct shadow_pointer * shdw_ptr = NULL;
	struct list_head * shdw_ptr_list = (struct list_head *)v3_htable_get_iter_value(ht_iter);

	// delete all swapped entries
	// we can leave the list_head structures and reuse them for the next round
	
	list_for_each_entry_safe(shdw_ptr, tmp_shdw_ptr, shdw_ptr_list, node) {
	    if (shdw_ptr == NULL) {
		PrintError("Null shadow pointer in swap flush!! Probably crashing soon...\n");
	    }

	    // Trigger faults for next shadow access
	    shdw_ptr->shadow_pte->present = 0;
	    
	    // Delete entry from list
	    list_del(&(shdw_ptr->node));
	    V3_Free(shdw_ptr);
	}

	v3_htable_iter_advance(ht_iter);
    }

    V3_Free(ht_iter);

    return 0;
}







static int sb_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    struct v3_shdw_impl_state * impl_state = &(vm->shdw_impl);
    struct swapbypass_vm_state * sb_state = NULL;

    memset(sb_state, 0, sizeof(struct swapbypass_vm_state));
    sb_state->shdw_ptr_ht = v3_create_htable(0, swap_hash_fn, swap_eq_fn);

#ifdef V3_CONFIG_SWAPBYPASS_TELEMETRY
    if (vm->enable_telemetry) {
	v3_add_telemetry_cb(vm, telemetry_cb, NULL);
    }
#endif

    impl_state->impl_data = sb_state;

    PrintDebug("Initialized SwapBypass\n");


    return 0;
}

static int sb_deinit(struct v3_vm_info * vm) {
    return -1;
}

static int sb_local_init(struct guest_info * core) {
    struct v3_shdw_pg_state * state = &(core->shdw_pg_state);
    struct swapbypass_local_state * swapbypass_state = NULL;

    V3_Print("SWAPBYPASS local initialization\n");

    swapbypass_state = (struct swapbypass_local_state *)V3_Malloc(sizeof(struct swapbypass_local_state));

    INIT_LIST_HEAD(&(swapbypass_state->page_list));

    state->local_impl_data = swapbypass_state;

    return 0;
}


static int sb_activate_shdw_pt(struct guest_info * core) {
    switch (v3_get_vm_cpu_mode(core)) {

	case PROTECTED:
	    return activate_shadow_pt_32(core);
	case PROTECTED_PAE:
	    return activate_shadow_pt_32pae(core);
	case LONG:
	case LONG_32_COMPAT:
	case LONG_16_COMPAT:
	    return activate_shadow_pt_64(core);
	default:
	    PrintError("Invalid CPU mode: %s\n", v3_cpu_mode_to_str(v3_get_vm_cpu_mode(core)));
	    return -1;
    }

    return 0;
}

static int sb_invalidate_shdw_pt(struct guest_info * core) {
    return sb_activate_shdw_pt(core);
}


static int sb_handle_pf(struct guest_info * core, addr_t fault_addr, pf_error_t error_code) {

	switch (v3_get_vm_cpu_mode(core)) {
	    case PROTECTED:
		return handle_shadow_pagefault_32(core, fault_addr, error_code);
		break;
	    case PROTECTED_PAE:
		return handle_shadow_pagefault_32pae(core, fault_addr, error_code);
	    case LONG:
	    case LONG_32_COMPAT:
	    case LONG_16_COMPAT:
		return handle_shadow_pagefault_64(core, fault_addr, error_code);
		break;
	    default:
		PrintError("Unhandled CPU Mode: %s\n", v3_cpu_mode_to_str(v3_get_vm_cpu_mode(core)));
		return -1;
	}
}


static int sb_handle_invlpg(struct guest_info * core, addr_t vaddr) {

    switch (v3_get_vm_cpu_mode(core)) {
	case PROTECTED:
	    return handle_shadow_invlpg_32(core, vaddr);
	case PROTECTED_PAE:
	    return handle_shadow_invlpg_32pae(core, vaddr);
	case LONG:
	case LONG_32_COMPAT:
	case LONG_16_COMPAT:
	    return handle_shadow_invlpg_64(core, vaddr);
	default:
	    PrintError("Invalid CPU mode: %s\n", v3_cpu_mode_to_str(v3_get_vm_cpu_mode(core)));
	    return -1;
    }
}

static struct v3_shdw_pg_impl sb_impl =  {
    .name = "SWAPBYPASS",
    .init = sb_init,
    .deinit = sb_deinit,
    .local_init = sb_local_init,
    .handle_pagefault = sb_handle_pf,
    .handle_invlpg = sb_handle_invlpg,
    .activate_shdw_pt = sb_activate_shdw_pt,
    .invalidate_shdw_pt = sb_invalidate_shdw_pt
};





register_shdw_pg_impl(&sb_impl);
