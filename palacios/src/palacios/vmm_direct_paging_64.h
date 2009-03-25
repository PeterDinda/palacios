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

#ifndef __VMM_DIRECT_PAGING_64_H__
#define __VMM_DIRECT_PAGING_64_H__

#include <palacios/vmm_mem.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vm_guest.h>

#ifndef DEBUG_NESTED_PAGING
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static inline int handle_passthrough_pagefault_64(struct guest_info * info, 
						     addr_t fault_addr, 
						     pf_error_t error_code) {
    pml4e64_t * pml = NULL;
    pdpe64_t * pdpe = NULL;
    pde64_t * pde = NULL;
    pte64_t * pte = NULL;
    addr_t host_addr = 0;

    int pml_index = PML4E64_INDEX(fault_addr);
    int pdpe_index = PDPE64_INDEX(fault_addr);
    int pde_index = PDE64_INDEX(fault_addr);
    int pte_index = PTE64_INDEX(fault_addr);


    

    struct v3_shadow_region * region =  v3_get_shadow_region(info, fault_addr);
  
    if ((region == NULL) || 
	(region->host_type == SHDW_REGION_INVALID)) {
	PrintError("Invalid region in passthrough page fault 64, addr=%p\n", 
		   (void *)fault_addr);
	return -1;
    }

    host_addr = v3_get_shadow_addr(region, fault_addr);
    //

    // Lookup the correct PML address based on the PAGING MODE
    if (info->shdw_pg_mode == SHADOW_PAGING) {
	pml = CR3_TO_PML4E64_VA(info->ctrl_regs.cr3);
    } else {
	pml = CR3_TO_PML4E64_VA(info->direct_map_pt);
    }

    //Fix up the PML entry
    if (pml[pml_index].present == 0) {
	pdpe = (pdpe64_t *)create_generic_pt_page();
   
	// Set default PML Flags...
	pml[pml_index].present = 1;
        pml[pml_index].writable = 1;
        pml[pml_index].user_page = 1;

	pml[pml_index].pdp_base_addr = PAGE_BASE_ADDR((addr_t)V3_PAddr(pdpe));    
    } else {
	pdpe = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pml[pml_index].pdp_base_addr));
    }

    // Fix up the PDPE entry
    if (pdpe[pdpe_index].present == 0) {
	pde = (pde64_t *)create_generic_pt_page();
	
	// Set default PDPE Flags...
	pdpe[pdpe_index].present = 1;
	pdpe[pdpe_index].writable = 1;
	pdpe[pdpe_index].user_page = 1;

	pdpe[pdpe_index].pd_base_addr = PAGE_BASE_ADDR((addr_t)V3_PAddr(pde));    
    } else {
	pde = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pdpe[pdpe_index].pd_base_addr));
    }


    // Fix up the PDE entry
    if (pde[pde_index].present == 0) {
	pte = (pte64_t *)create_generic_pt_page();
	
	pde[pde_index].present = 1;
	pde[pde_index].writable = 1;
	pde[pde_index].user_page = 1;
	
	pde[pde_index].pt_base_addr = PAGE_BASE_ADDR((addr_t)V3_PAddr(pte));
    } else {
	pte = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pde[pde_index].pt_base_addr));
    }


    // Fix up the PTE entry
    if (pte[pte_index].present == 0) {
	pte[pte_index].user_page = 1;
	
	if (region->host_type == SHDW_REGION_ALLOCATED) {
	    // Full access
	    pte[pte_index].present = 1;
	    pte[pte_index].writable = 1;

	    pte[pte_index].page_base_addr = PAGE_BASE_ADDR(host_addr);
	    
	} else if (region->host_type == SHDW_REGION_WRITE_HOOK) {
	    // Only trap writes
	    pte[pte_index].present = 1; 
	    pte[pte_index].writable = 0;

	    pte[pte_index].page_base_addr = PAGE_BASE_ADDR(host_addr);

	} else if (region->host_type == SHDW_REGION_FULL_HOOK) {
	    // trap all accesses
	    return v3_handle_mem_full_hook(info, fault_addr, fault_addr, region, error_code);

	} else {
	    PrintError("Unknown Region Type...\n");
	    return -1;
	}
    }
   
    if ( (region->host_type == SHDW_REGION_WRITE_HOOK) && 
	 (error_code.write == 1) ) {
	return v3_handle_mem_wr_hook(info, fault_addr, fault_addr, region, error_code);
    }

    return 0;
}


#endif
