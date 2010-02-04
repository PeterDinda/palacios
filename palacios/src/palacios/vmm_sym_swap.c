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


#include <palacios/vmm_sym_swap.h>
#include <palacios/vmm_list.h>
#include <palacios/vm_guest.h>

#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
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



#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
static void telemetry_cb(struct v3_vm_info * vm, void * private_data, char * hdr) {
    struct v3_sym_swap_state * swap_state = &(vm->swap_state);

    V3_Print("%sSymbiotic Swap:\n", hdr);
    V3_Print("%s\tRead faults=%d\n", hdr, swap_state->read_faults);
    V3_Print("%s\tWrite faults=%d\n", hdr, swap_state->write_faults);
    V3_Print("%s\tMapped Pages=%d\n", hdr, swap_state->mapped_pages);
    V3_Print("%s\tFlushes=%d\n", hdr, swap_state->flushes);
    V3_Print("%s\tlist size=%d\n", hdr, swap_state->list_size);
}
#endif


int v3_init_sym_swap(struct v3_vm_info * vm) {
    struct v3_sym_swap_state * swap_state = &(vm->swap_state);

    memset(swap_state, 0, sizeof(struct v3_sym_swap_state));
    swap_state->shdw_ptr_ht = v3_create_htable(0, swap_hash_fn, swap_eq_fn);

#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
    if (vm->enable_telemetry) {
	v3_add_telemetry_cb(vm, telemetry_cb, NULL);
    }
#endif

    PrintDebug("Initialized Symbiotic Swap\n");

    return 0;
}


int v3_register_swap_disk(struct v3_vm_info * vm, int dev_index, 
			  struct v3_swap_ops * ops, void * private_data) {
    struct v3_sym_swap_state * swap_state = &(vm->swap_state);

    swap_state->devs[dev_index].present = 1;
    swap_state->devs[dev_index].private_data = private_data;
    swap_state->devs[dev_index].ops = ops;

    return 0;
}




int v3_swap_in_notify(struct v3_vm_info * vm, int pg_index, int dev_index) {
    struct list_head * shdw_ptr_list = NULL;
    struct v3_sym_swap_state * swap_state = &(vm->swap_state);
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
    struct v3_sym_swap_state * swap_state = &(vm->swap_state);
    struct hashtable_iter * ht_iter = v3_create_htable_iter(swap_state->shdw_ptr_ht);

    //    PrintDebug("Flushing Symbiotic Swap table\n");

#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
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

int v3_get_vaddr_perms(struct guest_info * info, addr_t vaddr, pte32_t * guest_pte, pf_error_t * page_perms) {
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



addr_t v3_get_swapped_pg_addr(struct v3_vm_info * vm, pte32_t * guest_pte) {
    struct v3_sym_swap_state * swap_state = &(vm->swap_state);
    int dev_index = get_dev_index(guest_pte);
    struct v3_swap_dev * swp_dev = &(swap_state->devs[dev_index]);


    if (! swp_dev->present ) {
	return 0;
    }

    return (addr_t)swp_dev->ops->get_swap_entry(get_pg_index(guest_pte), swp_dev->private_data);
}


addr_t v3_map_swp_page(struct v3_vm_info * vm, pte32_t * shadow_pte, pte32_t * guest_pte, void * swp_page_ptr) {
    struct list_head * shdw_ptr_list = NULL;
    struct v3_sym_swap_state * swap_state = &(vm->swap_state);
    struct shadow_pointer * shdw_ptr = NULL;



    if (swp_page_ptr == NULL) {
	//	PrintError("Swapped out page not found on swap device\n");
	return 0;
    }

    shdw_ptr_list = (struct list_head *)v3_htable_search(swap_state->shdw_ptr_ht, (addr_t)*(uint32_t *)guest_pte);

    if (shdw_ptr_list == NULL) {
	shdw_ptr_list = (struct list_head *)V3_Malloc(sizeof(struct list_head *));
#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
	swap_state->list_size++;
#endif
	INIT_LIST_HEAD(shdw_ptr_list);
	v3_htable_insert(swap_state->shdw_ptr_ht, (addr_t)*(uint32_t *)guest_pte, (addr_t)shdw_ptr_list);
    }

    shdw_ptr = (struct shadow_pointer *)V3_Malloc(sizeof(struct shadow_pointer));

    if (shdw_ptr == NULL) {
	PrintError("MEMORY LEAK\n");
#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
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



/*
int v3_is_mapped_fault(struct guest_info * info, pte32_t * shadow_pte, pte32_t * guest_pte) {
    struct list_head * shdw_ptr_list = NULL;

    shdw_ptr_list = (struct list_head * )v3_htable_search(swap_state->shdw_ptr_ht, *(addr_t *)&(guest_pte));


    if (shdw_ptr_list != NULL) {
	PrintError("We faulted on a mapped in page....\n");
	return -1;
    }
    
    return 0;
}


*/
