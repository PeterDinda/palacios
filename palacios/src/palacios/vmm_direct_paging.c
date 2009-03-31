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

#include <palacios/vmm_direct_paging.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vm_guest.h>


#ifndef DEBUG_NESTED_PAGING
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static addr_t create_generic_pt_page() {
    void * page = 0;
    page = V3_VAddr(V3_AllocPages(1));
    memset(page, 0, PAGE_SIZE);

    return (addr_t)page;
}

// Inline handler functions for each cpu mode
#include "vmm_direct_paging_32.h"
#include "vmm_direct_paging_32pae.h"
#include "vmm_direct_paging_64.h"


addr_t v3_create_direct_passthrough_pts(struct guest_info * info) {
    return create_generic_pt_page();
}

int v3_handle_passthrough_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
    v3_vm_cpu_mode_t mode = v3_get_cpu_mode(info);

    switch(mode) {
	case REAL:
	case PROTECTED:
	    return handle_passthrough_pagefault_32(info, fault_addr, error_code);

	case PROTECTED_PAE:
	case LONG:
	case LONG_32_COMPAT:
	    // Long mode will only use 32PAE page tables...
	    return handle_passthrough_pagefault_32pae(info, fault_addr, error_code);

	default:
	    PrintError("Unknown CPU Mode\n");
	    break;
    }
    return -1;
}



int v3_handle_nested_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
    // THIS IS VERY BAD
    v3_vm_cpu_mode_t mode = LONG;


    PrintDebug("Nested PageFault: fault_addr=%p, error_code=%u\n",(void*)fault_addr, *(uint_t *)&error_code);

    switch(mode) {
	case REAL:
	case PROTECTED:
	    return handle_passthrough_pagefault_32(info, fault_addr, error_code);

	case PROTECTED_PAE:
	    return handle_passthrough_pagefault_32pae(info, fault_addr, error_code);

	case LONG:
	case LONG_32_COMPAT:
	    return handle_passthrough_pagefault_64(info, fault_addr, error_code);	    
	
	default:
	    PrintError("Unknown CPU Mode\n");
	    break;
    }
    return -1;
}

