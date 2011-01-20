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

#ifndef __VMM_SHADOW_PAGING_H__
#define __VMM_SHADOW_PAGING_H__


#ifdef __V3VEE__

#include <palacios/vmm_util.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm_hashtable.h>
#include <palacios/vmm_list.h>
#include <palacios/vmm_msr.h>

#include <palacios/vmm_config.h>


struct guest_info;

struct v3_shdw_pg_impl {
    char * name;
    int (*init)(struct v3_vm_info * vm, v3_cfg_tree_t * cfg);
    int (*deinit)(struct v3_vm_info * vm);
    int (*local_init)(struct guest_info * core);
    int (*local_deinit)(struct guest_info * core);
    int (*handle_pagefault)(struct guest_info * core, addr_t fault_addr, pf_error_t error_code);
    int (*handle_invlpg)(struct guest_info * core, addr_t vaddr);
    int (*activate_shdw_pt)(struct guest_info * core);
    int (*invalidate_shdw_pt)(struct guest_info * core);
};



struct v3_shdw_impl_state {
    
    struct v3_shdw_pg_impl * current_impl;
    void * impl_data;
};

struct v3_shdw_pg_state {

    // virtualized control registers
    v3_reg_t guest_cr3;
    v3_reg_t guest_cr0;
    v3_msr_t guest_efer;

    void * local_impl_data;

#ifdef CONFIG_SHADOW_PAGING_TELEMETRY
    uint_t guest_faults;
#endif

};






int v3_init_shdw_impl(struct v3_vm_info * vm);
int v3_deinit_shdw_impl(struct v3_vm_info * vm);

int v3_init_shdw_pg_state(struct guest_info * core);
int v3_deinit_shdw_pg_state(struct guest_info * core);


/* Handler implementations */
int v3_handle_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code);
int v3_handle_shadow_invlpg(struct guest_info * info);

/* Actions.. */
int v3_activate_shadow_pt(struct guest_info * info);
int v3_invalidate_shadow_pts(struct guest_info * info);


/* Utility functions for shadow paging implementations */
int v3_inject_guest_pf(struct guest_info * info, addr_t fault_addr, pf_error_t error_code);
int v3_is_guest_pf(pt_access_status_t guest_access, pt_access_status_t shadow_access);





int V3_init_shdw_paging();
int V3_deinit_shdw_paging();

#define register_shdw_pg_impl(impl)					\
    static struct v3_shdw_pg_impl * _v3_shdw_pg_impl			\
    __attribute__((used))						\
	__attribute__((unused, __section__ ("_v3_shdw_pg_impls"),	\
		       aligned(sizeof(addr_t))))			\
	= impl;




#endif // ! __V3VEE__

#endif
