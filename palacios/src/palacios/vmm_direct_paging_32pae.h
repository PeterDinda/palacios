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

#ifndef __VMM_DIRECT_PAGING_32PAE_H__
#define __VMM_DIRECT_PAGING_32PAE_H__

#include <palacios/vmm_mem.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vm_guest.h>

/* This always builds 3 level page tables - no large pages */

static inline int handle_passthrough_pagefault_32pae(struct guest_info * info, 
						     addr_t fault_addr, 
						     pf_error_t error_code,
						     addr_t *actual_start, addr_t *actual_end) {
    pdpe32pae_t * pdpe = NULL;
    pde32pae_t * pde = NULL;
    pte32pae_t * pte = NULL;
    addr_t host_addr = 0;

    int pdpe_index = PDPE32PAE_INDEX(fault_addr);
    int pde_index = PDE32PAE_INDEX(fault_addr);
    int pte_index = PTE32PAE_INDEX(fault_addr);
    

    struct v3_mem_region * region =  v3_get_mem_region(info->vm_info, info->vcpu_id, fault_addr);
  
    if (region == NULL) {
	PrintError(info->vm_info, info, "Invalid region in passthrough page fault 32PAE, addr=%p\n", 
		   (void *)fault_addr);
	return -1;
    }

    PrintDebug(info->vm_info, info, "Direct Paging 32PAE page fault handler=%p\n", (void *)fault_addr);

    // Lookup the correct PDPE address based on the PAGING MODE
    if (info->shdw_pg_mode == SHADOW_PAGING) {
	pdpe = CR3_TO_PDPE32PAE_VA(info->ctrl_regs.cr3);
    } else {
	pdpe = CR3_TO_PDPE32PAE_VA(info->direct_map_pt);
    }
 
    PrintDebug(info->vm_info, info, "Top level pdpe error pdp address=%p\n", (void *)pdpe);
    // Fix up the PDPE entry
    if (pdpe[pdpe_index].present == 0) {
	pde = (pde32pae_t *)create_generic_pt_page(info);
        PrintDebug(info->vm_info, info, "Creating a new pd page=%p\n", (void *)pde);
	pdpe[pdpe_index].present = 1;
	// Set default PDPE Flags...
	pdpe[pdpe_index].pd_base_addr = PAGE_BASE_ADDR((addr_t)V3_PAddr(pde));    
    } else {
	pde = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pdpe[pdpe_index].pd_base_addr));
    }
    PrintDebug(info->vm_info, info, "Handling pde error pd base address =%p\n", (void *)pde);

    *actual_start = BASE_TO_PAGE_ADDR_4KB(PAGE_BASE_ADDR_4KB(fault_addr));
    *actual_end = BASE_TO_PAGE_ADDR_4KB(PAGE_BASE_ADDR_4KB(fault_addr)+1)-1;

    // Fix up the PDE entry
    if (pde[pde_index].present == 0) {
	pte = (pte32pae_t *)create_generic_pt_page(info);
        PrintDebug(info->vm_info, info, "Creating a new pt page=%p\n", (void *)pte);
	pde[pde_index].present = 1;
	pde[pde_index].writable = 1;
	pde[pde_index].user_page = 1;

	pde[pde_index].pt_base_addr = PAGE_BASE_ADDR((addr_t)V3_PAddr(pte));
    } else {
	pte = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pde[pde_index].pt_base_addr));
    }

    PrintDebug(info->vm_info, info, "Handling pte error pt base address=%p\n", (void *)pte);


    // Fix up the PTE entry
    if (pte[pte_index].present == 0) {
	pte[pte_index].user_page = 1;

	if ((region->flags.alloced == 1) && 
	    (region->flags.read == 1)) {

	    pte[pte_index].present = 1;

	    if (region->flags.write == 1) {
		pte[pte_index].writable = 1;
	    } else {
		pte[pte_index].writable = 0;
	    }

	    if (v3_gpa_to_hpa(info, fault_addr, &host_addr) == -1) {
		PrintError(info->vm_info, info, "Could not translate fault address (%p)\n", (void *)fault_addr);
		return -1;
	    }

	    pte[pte_index].page_base_addr = PAGE_BASE_ADDR(host_addr);
            PrintDebug(info->vm_info, info, "PTE mapped to =%p\n", (void *)host_addr);
            PrintDebug(info->vm_info, info, "PTE is =%llx\n", *(uint64_t *)&(pte[pte_index]));
	} else {
	    return region->unhandled(info, fault_addr, fault_addr, region, error_code);
	}
    } else {
	return region->unhandled(info, fault_addr, fault_addr, region, error_code);
    }
   
    PrintDebug(info->vm_info, info, "Handler ends with fault address=%p\n", (void *)fault_addr);

    return 0;
}


static inline int invalidate_addr_32pae_internal(struct guest_info * info, addr_t inv_addr,
						 addr_t *actual_start, uint64_t *actual_size) {
    pdpe32pae_t * pdpe = NULL;
    pde32pae_t * pde = NULL;
    pte32pae_t * pte = NULL;


    // TODO:
    // Call INVLPGA

    // clear the page table entry
    int pdpe_index = PDPE32PAE_INDEX(inv_addr);
    int pde_index = PDE32PAE_INDEX(inv_addr);
    int pte_index = PTE32PAE_INDEX(inv_addr);

    
    // Lookup the correct PDE address based on the PAGING MODE
    if (info->shdw_pg_mode == SHADOW_PAGING) {
	pdpe = CR3_TO_PDPE32PAE_VA(info->ctrl_regs.cr3);
    } else {
	pdpe = CR3_TO_PDPE32PAE_VA(info->direct_map_pt);
    }    


    if (pdpe[pdpe_index].present == 0) {
        *actual_start = BASE_TO_PAGE_ADDR_1GB(PAGE_BASE_ADDR_1GB(inv_addr));
        *actual_size = PAGE_SIZE_1GB;
	return 0;
    }

    pde = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pdpe[pdpe_index].pd_base_addr));

    if (pde[pde_index].present == 0) {
        *actual_start = BASE_TO_PAGE_ADDR_2MB(PAGE_BASE_ADDR_2MB(inv_addr));
        *actual_size = PAGE_SIZE_2MB;
	return 0;
    } else if (pde[pde_index].large_page) {
	pde[pde_index].present = 0;
        *actual_start = BASE_TO_PAGE_ADDR_2MB(PAGE_BASE_ADDR_2MB(inv_addr));
        *actual_size = PAGE_SIZE_2MB;
	return 0;
    }

    pte = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pde[pde_index].pt_base_addr));

    pte[pte_index].present = 0;

    *actual_start = BASE_TO_PAGE_ADDR_4KB(PAGE_BASE_ADDR_4KB(inv_addr));
    *actual_size = PAGE_SIZE_4KB;
    return 0;
}



static inline int invalidate_addr_32pae(struct guest_info * core, addr_t inv_addr,
					addr_t *actual_start, addr_t *actual_end)
{
  uint64_t len;
  int rc;
  
  rc = invalidate_addr_32pae_internal(core,inv_addr,actual_start,&len);

  *actual_end = *actual_start + len - 1;

  return rc;
    

}
   
static inline int invalidate_addr_32pae_range(struct guest_info * core, addr_t inv_addr_start, addr_t inv_addr_end,
					      addr_t *actual_start, addr_t *actual_end)
{
  addr_t next;
  addr_t start;
  uint64_t len;
  int rc;
  
  for (next=inv_addr_start; next<=inv_addr_end; ) {
    rc = invalidate_addr_32pae_internal(core,next,&start, &len);
    if (next==inv_addr_start) { 
      // first iteration, capture where we start invalidating
      *actual_start = start;
    }
    if (rc) { 
      return rc;
    }
    next = start + len;
    *actual_end = next;
  }
  // last iteration, actual_end is off by one
  (*actual_end)--;
  return 0;
}

#endif
