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
static void telemetry_cb(struct guest_info * info, void * private_data) {
    struct v3_sym_swap_state * swap_state = &(info->swap_state);

    V3_Print("Symbiotic Swap:\n");
    V3_Print("\tRead faults=%d\n", swap_state->read_faults);
    V3_Print("\tWrite faults=%d\n", swap_state->write_faults);
    V3_Print("\tFlushes=%d\n", swap_state->flushes);
}
#endif


int v3_init_sym_swap(struct guest_info * info) {
    struct v3_sym_swap_state * swap_state = &(info->swap_state);

    memset(swap_state, 0, sizeof(struct v3_sym_swap_state));
    swap_state->shdw_ptr_ht = v3_create_htable(0, swap_hash_fn, swap_eq_fn);

#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
    if (info->enable_telemetry) {
	v3_add_telemetry_cb(info, telemetry_cb, NULL);
    }
#endif

    PrintDebug("Initialized Symbiotic Swap\n");

    return 0;
}


int v3_register_swap_disk(struct guest_info * info, int dev_index, 
			  struct v3_swap_ops * ops, void * private_data) {
    struct v3_sym_swap_state * swap_state = &(info->swap_state);

    swap_state->devs[dev_index].present = 1;
    swap_state->devs[dev_index].private_data = private_data;
    swap_state->devs[dev_index].ops = ops;

    return 0;
}




int v3_swap_in_notify(struct guest_info * info, int pg_index, int dev_index) {
    struct list_head * shdw_ptr_list = NULL;
    struct v3_sym_swap_state * swap_state = &(info->swap_state);
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



int v3_swap_flush(struct guest_info * info) {
    struct v3_sym_swap_state * swap_state = &(info->swap_state);
    struct hashtable_iter * ht_iter = v3_create_htable_iter(swap_state->shdw_ptr_ht);

    PrintDebug("Flushing Symbiotic Swap table\n");

#ifdef CONFIG_SYMBIOTIC_SWAP_TELEMETRY
    swap_state->flushes++;
#endif

    while (ht_iter->entry) {
	struct shadow_pointer * tmp_shdw_ptr = NULL;
	struct shadow_pointer * shdw_ptr = NULL;
	struct list_head * shdw_ptr_list = (struct list_head *)v3_htable_get_iter_value(ht_iter);

	// delete all swapped entries
	// we can leave the list_head structures and reuse them for the next round
	
	list_for_each_entry_safe(shdw_ptr, tmp_shdw_ptr, shdw_ptr_list, node) {
	    // Trigger faults for next shadow access
	    shdw_ptr->shadow_pte->present = 0;
	    
	    // Delete entry from list
	    list_del(&(shdw_ptr->node));
	    V3_Free(shdw_ptr);	    
	}

	v3_htable_iter_advance(ht_iter);
    }

    return 0;
}


addr_t v3_get_swapped_pg_addr(struct guest_info * info, pte32_t * shadow_pte, pte32_t * guest_pte) {
    struct list_head * shdw_ptr_list = NULL;
    struct v3_sym_swap_state * swap_state = &(info->swap_state);
    struct shadow_pointer * shdw_ptr = NULL;
    void * swp_page_ptr = NULL;
    int dev_index = get_dev_index(guest_pte);
    struct v3_swap_dev * swp_dev = &(swap_state->devs[dev_index]);

    if (! swp_dev->present ) {
	return 0;
    }

    swp_page_ptr = swp_dev->ops->get_swap_entry(get_pg_index(guest_pte), swp_dev->private_data);

    if (swp_page_ptr == NULL) {
	PrintError("Swapped out page not found on swap device\n");
	return 0;
    }

    shdw_ptr_list = (struct list_head *)v3_htable_search(swap_state->shdw_ptr_ht, (addr_t)*(uint32_t *)guest_pte);

    if (shdw_ptr_list == NULL) {
	shdw_ptr_list = (struct list_head *)V3_Malloc(sizeof(struct list_head *));
	INIT_LIST_HEAD(shdw_ptr_list);
	v3_htable_insert(swap_state->shdw_ptr_ht, (addr_t)*(uint32_t *)guest_pte, (addr_t)shdw_ptr_list);
    }

    shdw_ptr = (struct shadow_pointer *)V3_Malloc(sizeof(struct shadow_pointer));

    shdw_ptr->shadow_pte = shadow_pte;
    shdw_ptr->guest_pte = *(uint32_t *)guest_pte;
    shdw_ptr->pg_index = get_pg_index(guest_pte);
    shdw_ptr->dev_index = get_dev_index(guest_pte);

    // We don't check for conflicts, because it should not happen...
    list_add(&(shdw_ptr->node), shdw_ptr_list);

    return PAGE_BASE_ADDR((addr_t)V3_PAddr(swp_page_ptr));
}
