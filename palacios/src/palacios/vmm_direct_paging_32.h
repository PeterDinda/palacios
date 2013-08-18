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

#ifndef __VMM_DIRECT_PAGING_32_H__
#define __VMM_DIRECT_PAGING_32_H__

#include <palacios/vmm_mem.h>
#include <palacios/vmm_paging.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vm_guest.h>


static inline int handle_passthrough_pagefault_32(struct guest_info * info, 
						  addr_t fault_addr, 
						  pf_error_t error_code) {
    // Check to see if pde and pte exist (create them if not)
    pde32_t * pde = NULL;
    pte32_t * pte = NULL;
    addr_t host_addr = 0;
    
    int pde_index = PDE32_INDEX(fault_addr);
    int pte_index = PTE32_INDEX(fault_addr);
    
    struct v3_mem_region * region = v3_get_mem_region(info->vm_info, info->vcpu_id, fault_addr);

    if (region == NULL) {
        PrintError(info->vm_info, info, "Invalid region in passthrough page fault 32, addr=%p\n", 
		   (void *)fault_addr);
	return -1;
    }
    
    // Lookup the correct PDE address based on the PAGING MODE
    if (info->shdw_pg_mode == SHADOW_PAGING) {
	pde = CR3_TO_PDE32_VA(info->ctrl_regs.cr3);
    } else {
	pde = CR3_TO_PDE32_VA(info->direct_map_pt);
    }


    // Fix up the PDE entry
    if (pde[pde_index].present == 0) {
	pte = (pte32_t *)create_generic_pt_page(info);
	
	pde[pde_index].present = 1;
	pde[pde_index].writable = 1;
	pde[pde_index].user_page = 1;
	pde[pde_index].pt_base_addr = PAGE_BASE_ADDR((addr_t)V3_PAddr(pte));
	
    } else {
	pte = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pde[pde_index].pt_base_addr));
    }
    
    // Fix up the PTE entry
    if (pte[pte_index].present == 0) {
	
	
	if ((region->flags.alloced == 1) && 
	    (region->flags.read == 1)) {

	    pte[pte_index].user_page = 1;

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
	} else {
	    return region->unhandled(info, fault_addr, fault_addr, region, error_code);
	}
    } else {
	// We fix all permissions on the first pass, 
	// so we only get here if its an unhandled exception
	return region->unhandled(info, fault_addr, fault_addr, region, error_code);	    
    }

    return 0;
}




static inline int invalidate_addr_32_internal(struct guest_info * info, addr_t inv_addr,
					      addr_t *actual_start, uint64_t *actual_size) {
    pde32_t * pde = NULL;
    pte32_t * pte = NULL;

    // TODO:
    // Call INVLPGA

    // clear the page table entry
    int pde_index = PDE32_INDEX(inv_addr);
    int pte_index = PTE32_INDEX(inv_addr);

    
    // Lookup the correct PDE address based on the PAGING MODE
    if (info->shdw_pg_mode == SHADOW_PAGING) {
	pde = CR3_TO_PDE32_VA(info->ctrl_regs.cr3);
    } else {
	pde = CR3_TO_PDE32_VA(info->direct_map_pt);
    }    

    if (pde[pde_index].present == 0) {
        *actual_start = BASE_TO_PAGE_ADDR_4MB(PAGE_BASE_ADDR_4MB(inv_addr));
        *actual_size = PAGE_SIZE_4MB;
	return 0;
    } else if (pde[pde_index].large_page) {
	pde[pde_index].present = 0;
	pde[pde_index].writable = 0;
	pde[pde_index].user_page = 0;
        *actual_start = BASE_TO_PAGE_ADDR_4MB(PAGE_BASE_ADDR_4MB(inv_addr));
        *actual_size = PAGE_SIZE_4MB;
	return 0;
    }

    pte = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pde[pde_index].pt_base_addr));

    pte[pte_index].present = 0;
    pte[pte_index].writable = 0;
    pte[pte_index].user_page = 0;

    *actual_start = BASE_TO_PAGE_ADDR_4KB(PAGE_BASE_ADDR_4KB(inv_addr));
    *actual_size = PAGE_SIZE_4KB;

    return 0;
}


static inline int invalidate_addr_32(struct guest_info * core, addr_t inv_addr)
{
  addr_t start;
  uint64_t len;
  
  return invalidate_addr_32_internal(core,inv_addr,&start,&len);
}
   
static inline int invalidate_addr_32_range(struct guest_info * core, addr_t inv_addr_start, addr_t inv_addr_end)
{
  addr_t next;
  addr_t start;
  uint64_t len;
  int rc;
  
  for (next=inv_addr_start; next<=inv_addr_end; ) {
    rc = invalidate_addr_32_internal(core,next,&start, &len);
    if (rc) { 
      return rc;
    }
    next = start + len;
  }
  return 0;
}



#endif
