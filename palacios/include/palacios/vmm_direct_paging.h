/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Steven Jaconette <stevenjaconette2007@u.northwestern.edu> 
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Steven Jaconette <stevenjaconette2007@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_DIRECT_PAGING_H__
#define __VMM_DIRECT_PAGING_H__

#ifdef __V3VEE__

#include <palacios/vmm_mem.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm_list.h>
 

/**********************************
   PASSTHROUGH PAGING - CORE FUNC
 **********************************/


struct v3_passthrough_impl_state {
    // currently there is only a single implementation
    // that internally includes SVM and VMX support
    // The externally visible state is just the callbacks
    v3_rw_lock_t     event_callback_lock;
    struct list_head event_callback_list;
};


int v3_init_passthrough_paging(struct v3_vm_info *vm);
int v3_init_passthrough_paging_core(struct guest_info *core);
int v3_deinit_passthrough_paging(struct v3_vm_info *vm);
int v3_deinit_passthrough_paging_core(struct guest_info *core);

int v3_init_passthrough_pts(struct guest_info * guest_info);
int v3_free_passthrough_pts(struct guest_info * core);

int v3_reset_passthrough_pts(struct guest_info * guest_info);

// actual_start/end may be null if you don't want this info
// If non-null, these return the actual affected GPA range
int v3_handle_passthrough_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code,
				    addr_t *actual_start, addr_t *actual_end);

int v3_activate_passthrough_pt(struct guest_info * info);

int v3_invalidate_passthrough_addr(struct guest_info * info, addr_t inv_addr,
				   addr_t *actual_start, addr_t *actual_end);

// The range invalidated is minimally [start, end]
int v3_invalidate_passthrough_addr_range(struct guest_info * info, 
					 addr_t inv_addr_start, addr_t inv_addr_end,
					 addr_t *actual_start, addr_t *actual_end);

/**********************************
   PASSTHROUGH PAGING - EVENTS
 **********************************/

struct v3_passthrough_pg_event {
    enum {PASSTHROUGH_PAGEFAULT,PASSTHROUGH_INVALIDATE_RANGE,PASSTHROUGH_ACTIVATE} event_type;
    enum {PASSTHROUGH_PREIMPL, PASSTHROUGH_POSTIMPL} event_order;
    addr_t     gpa;        // for pf 
    pf_error_t error_code; // for pf
    addr_t     gpa_start;  // for invalidation of range or page fault
    addr_t     gpa_end;    // for invalidation of range or page fault (range is [start,end] )
                           // PREIMPL: start/end is the requested range
                           // POSTIMPL: start/end is the actual range invalidated
};



int v3_register_passthrough_paging_event_callback(struct v3_vm_info *vm,
						  int (*callback)(struct guest_info *core, 
								  struct v3_passthrough_pg_event *,
								  void      *priv_data),
						  void *priv_data);

int v3_unregister_passthrough_paging_event_callback(struct v3_vm_info *vm,
						    int (*callback)(struct guest_info *core, 
								    struct v3_passthrough_pg_event *,
								    void      *priv_data),
						    void *priv_data);



/*****************************
   NESTED PAGING - CORE FUNC
 *****************************/


struct v3_nested_impl_state {
    // currently there is only a single implementation
    // that internally includes SVM and VMX support
    // The externally visible state is just the callbacks
    v3_rw_lock_t     event_callback_lock;
    struct list_head event_callback_list;
};

int v3_init_nested_paging(struct v3_vm_info *vm);
int v3_init_nested_paging_core(struct guest_info *core, void *hwinfo);
int v3_deinit_nested_paging(struct v3_vm_info *vm);
int v3_deinit_nested_paging_core(struct guest_info *core);


// actual_start/end may be null if you don't want this info
// If non-null, these return the actual affected GPA range
int v3_handle_nested_pagefault(struct guest_info * info, addr_t fault_addr, void *pfinfo,
			       addr_t *actual_start, addr_t *actual_end);

int v3_invalidate_nested_addr(struct guest_info * info, addr_t inv_addr,
			      addr_t *actual_start, addr_t *actual_end);

// The range invalidated is minimally [start, end]
int v3_invalidate_nested_addr_range(struct guest_info * info, 
				    addr_t inv_addr_start, addr_t inv_addr_end,
				    addr_t *actual_start, addr_t *actual_end);



/*****************************
   NESTED PAGING - EVENTS
 *****************************/

struct v3_nested_pg_event {
    enum {NESTED_PAGEFAULT,NESTED_INVALIDATE_RANGE} event_type;
    enum {NESTED_PREIMPL, NESTED_POSTIMPL} event_order;
    addr_t     gpa;        // for pf 
    pf_error_t error_code; // for pf
    addr_t     gpa_start;  // for invalidation of range or page fault
    addr_t     gpa_end;    // for invalidation of range or page fault (range is [start,end] )
                           // PREIMPL: start/end is the requested range
                           // POSTIMPL: start/end is the actual range invalidated
};



int v3_register_nested_paging_event_callback(struct v3_vm_info *vm,
                                            int (*callback)(struct guest_info *core, 
                                                            struct v3_nested_pg_event *,
                                                            void      *priv_data),
                                            void *priv_data);

int v3_unregister_nested_paging_event_callback(struct v3_vm_info *vm,
                                              int (*callback)(struct guest_info *core, 
                                                              struct v3_nested_pg_event *,
                                                              void      *priv_data),
                                              void *priv_data);


#endif // ! __V3VEE__

#endif
