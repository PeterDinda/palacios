

#ifdef V3_CONFIG_SHADOW_CACHE

static inline int activate_shadow_pt_32pae(struct guest_info * info) {
    PrintError("Activating 32 bit PAE page tables not implemented\n");
    return -1;
}






/* 
 * *
 * * 
 * * 32 bit PAE  Page table fault handlers
 * *
 * *
 */

static inline int handle_shadow_pagefault_32pae(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
    PrintError("32 bit PAE shadow paging not implemented\n");
    return -1;
}


static inline int handle_shadow_invlpg_32pae(struct guest_info * info, addr_t vaddr) {
    PrintError("32 bit PAE shadow paging not implemented\n");
    return -1;
}


#endif

