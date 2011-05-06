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

// Reference: AMD Software Developer Manual Vol.2 Ch.5 "Page Translation and Protection"

static inline int handle_passthrough_pagefault_64(struct guest_info * core, addr_t fault_addr, pf_error_t error_code) {
    pml4e64_t * pml      = NULL;
    pdpe64_t * pdpe      = NULL;
    pde64_t * pde        = NULL;
    pde64_2MB_t * pde2mb = NULL;
    pte64_t * pte        = NULL;
    addr_t host_addr     = 0;

    int pml_index  = PML4E64_INDEX(fault_addr);
    int pdpe_index = PDPE64_INDEX(fault_addr);
    int pde_index  = PDE64_INDEX(fault_addr);
    int pte_index  = PTE64_INDEX(fault_addr);

    struct v3_mem_region * region =  v3_get_mem_region(core->vm_info, core->vcpu_id, fault_addr);
    int page_size = PAGE_SIZE_4KB;

    if (region == NULL) {
	PrintError("%s: invalid region, addr=%p\n", __FUNCTION__, (void *)fault_addr);
	return -1;
    }

    /*  Check if:
     *  1. the guest is configured to use large pages and 
     * 	2. the memory regions can be referenced by a large page
     */
    if ((core->use_large_pages == 1) || (core->use_giant_pages == 1)) {
	page_size = v3_get_max_page_size(core, fault_addr, LONG);
    }

    PrintDebug("Using page size of %dKB\n", page_size / 1024);

 
    // Lookup the correct PML address based on the PAGING MODE
    if (core->shdw_pg_mode == SHADOW_PAGING) {
	pml = CR3_TO_PML4E64_VA(core->ctrl_regs.cr3);
    } else {
	pml = CR3_TO_PML4E64_VA(core->direct_map_pt);
    }

    //Fix up the PML entry
    if (pml[pml_index].present == 0) {
	pdpe = (pdpe64_t *)create_generic_pt_page();
   
	// Set default PML Flags...
	pml[pml_index].present = 1;
        pml[pml_index].writable = 1;
        pml[pml_index].user_page = 1;

	pml[pml_index].pdp_base_addr = PAGE_BASE_ADDR_4KB((addr_t)V3_PAddr(pdpe));    
    } else {
	pdpe = V3_VAddr((void*)BASE_TO_PAGE_ADDR_4KB(pml[pml_index].pdp_base_addr));
    }

    // Fix up the PDPE entry
    if (pdpe[pdpe_index].present == 0) {
	pde = (pde64_t *)create_generic_pt_page();
	
	// Set default PDPE Flags...
	pdpe[pdpe_index].present = 1;
	pdpe[pdpe_index].writable = 1;
	pdpe[pdpe_index].user_page = 1;

	pdpe[pdpe_index].pd_base_addr = PAGE_BASE_ADDR_4KB((addr_t)V3_PAddr(pde));    
    } else {
	pde = V3_VAddr((void*)BASE_TO_PAGE_ADDR_4KB(pdpe[pdpe_index].pd_base_addr));
    }

    // Fix up the 2MiB PDE and exit here
    if (page_size == PAGE_SIZE_2MB) {
	pde2mb = (pde64_2MB_t *)pde; // all but these two lines are the same for PTE
	pde2mb[pde_index].large_page = 1;

	if (pde2mb[pde_index].present == 0) {
	    pde2mb[pde_index].user_page = 1;

	    if ( (region->flags.alloced == 1) && 
		 (region->flags.read == 1)) {
		// Full access
		pde2mb[pde_index].present = 1;

		if (region->flags.write == 1) {
		    pde2mb[pde_index].writable = 1;
		} else {
		    pde2mb[pde_index].writable = 0;
		}

		if (v3_gpa_to_hpa(core, fault_addr, &host_addr) == -1) {
		    PrintError("Error Could not translate fault addr (%p)\n", (void *)fault_addr);
		    return -1;
		}

		pde2mb[pde_index].page_base_addr = PAGE_BASE_ADDR_2MB(host_addr);
	    } else {
		return region->unhandled(core, fault_addr, fault_addr, region, error_code);
	    }
	} else {
	    // We fix all permissions on the first pass, 
	    // so we only get here if its an unhandled exception

	    return region->unhandled(core, fault_addr, fault_addr, region, error_code);
	}

	// All done
	return 0;
    } 

    // Continue with the 4KiB page heirarchy
    
    // Fix up the PDE entry
    if (pde[pde_index].present == 0) {
	pte = (pte64_t *)create_generic_pt_page();
	
	pde[pde_index].present = 1;
	pde[pde_index].writable = 1;
	pde[pde_index].user_page = 1;
	
	pde[pde_index].pt_base_addr = PAGE_BASE_ADDR_4KB((addr_t)V3_PAddr(pte));
    } else {
	pte = V3_VAddr((void*)BASE_TO_PAGE_ADDR_4KB(pde[pde_index].pt_base_addr));
    }

    // Fix up the PTE entry
    if (pte[pte_index].present == 0) {
	pte[pte_index].user_page = 1;
	
	if ((region->flags.alloced == 1) && 
	    (region->flags.read == 1)) {
	    // Full access
	    pte[pte_index].present = 1;

	    if (region->flags.write == 1) {
		pte[pte_index].writable = 1;
	    } else {
		pte[pte_index].writable = 0;
	    }

    	    if (v3_gpa_to_hpa(core, fault_addr, &host_addr) == -1) {
		PrintError("Error Could not translate fault addr (%p)\n", (void *)fault_addr);
		return -1;
   	    }

	    pte[pte_index].page_base_addr = PAGE_BASE_ADDR_4KB(host_addr);
	} else {
	    return region->unhandled(core, fault_addr, fault_addr, region, error_code);
	}
    } else {
	// We fix all permissions on the first pass, 
	// so we only get here if its an unhandled exception

	return region->unhandled(core, fault_addr, fault_addr, region, error_code);
    }

    return 0;
}

static inline int invalidate_addr_64(struct guest_info * core, addr_t inv_addr) {
    pml4e64_t * pml = NULL;
    pdpe64_t * pdpe = NULL;
    pde64_t * pde = NULL;
    pte64_t * pte = NULL;


    // TODO:
    // Call INVLPGA

    // clear the page table entry
    int pml_index = PML4E64_INDEX(inv_addr);
    int pdpe_index = PDPE64_INDEX(inv_addr);
    int pde_index = PDE64_INDEX(inv_addr);
    int pte_index = PTE64_INDEX(inv_addr);

    
    // Lookup the correct PDE address based on the PAGING MODE
    if (core->shdw_pg_mode == SHADOW_PAGING) {
	pml = CR3_TO_PML4E64_VA(core->ctrl_regs.cr3);
    } else {
	pml = CR3_TO_PML4E64_VA(core->direct_map_pt);
    }

    if (pml[pml_index].present == 0) {
	return 0;
    }

    pdpe = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pml[pml_index].pdp_base_addr));

    if (pdpe[pdpe_index].present == 0) {
	return 0;
    } else if (pdpe[pdpe_index].large_page == 1) { // 1GiB
	pdpe[pdpe_index].present = 0;
	pdpe[pdpe_index].writable = 0;
	pdpe[pdpe_index].user_page = 0;
	return 0;
    }

    pde = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pdpe[pdpe_index].pd_base_addr));

    if (pde[pde_index].present == 0) {
	return 0;
    } else if (pde[pde_index].large_page == 1) { // 2MiB
	pde[pde_index].present = 0;
	pde[pde_index].writable = 0;
	pde[pde_index].user_page = 0;
	return 0;
    }

    pte = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pde[pde_index].pt_base_addr));

    pte[pte_index].present = 0; // 4KiB
    pte[pte_index].writable = 0;
    pte[pte_index].user_page = 0;

    return 0;
}



#endif
