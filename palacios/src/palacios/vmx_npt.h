/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Jack Lange <jacklange@cs.pitt.edu> 
 * All rights reserved.
 *
 * Author: Jack Lange <jacklange@cs.pitt.edu>    (implementation)
 *         Peter Dinda <pdinda@northwestern.edu> (invalidation)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmx_ept.h>
#include <palacios/vmx_lowlevel.h>
#include <palacios/vmm_paging.h>
#include <palacios/vm_guest_mem.h>


/*

  Note that the Intel nested page table have a slightly different format
  than regular page tables.   Also note that our implementation
  uses only 64 bit (4 level) page tables.  This is unlike the SVM 
  nested paging implementation.


*/

#ifndef V3_CONFIG_VMX


static int handle_vmx_nested_pagefault(struct guest_info * info, addr_t fault_addr, void *info) 
{
    PrintError(info->vm_info, info, "Cannot do nested page fault as VMX is not enabled.\n");
    return -1;
}
static int handle_vmx_invalidate_nested_addr(struct guest_info * info, addr_t inv_addr) 
{
    PrintError(info->vm_info, info, "Cannot do invalidate nested addr as VMX is not enabled.\n");
    return -1;
}
static int handle_vmx_invalidate_nested_addr_range(struct guest_info * info, 
						   addr_t inv_addr_start, addr_t inv_addr_end) 
{
    PrintError(info->vm_info, info, "Cannot do invalidate nested addr range as VMX is not enabled.\n");
    return -1;
}

#else

static struct vmx_ept_msr * ept_info = NULL;


static addr_t create_ept_page() {
    void * temp;
    void * page = 0;
    
    temp = V3_AllocPages(1);  // need not be shadow-safe, not exposed to guest
    if (!temp) {
	PrintError(VM_NONE, VCORE_NONE, "Cannot allocate EPT page\n");
	return 0;
    }
    page = V3_VAddr(temp);
    memset(page, 0, PAGE_SIZE);

    return (addr_t)page;
}




static int init_ept(struct guest_info * core, struct vmx_hw_info * hw_info) {
    addr_t ept_pa = (addr_t)V3_PAddr((void *)create_ept_page());    
    vmx_eptp_t * ept_ptr = (vmx_eptp_t *)&(core->direct_map_pt);


    ept_info = &(hw_info->ept_info);

    /* TODO: Should we set this to WB?? */
    ept_ptr->psmt = 0;

    if (ept_info->pg_walk_len4) {
	ept_ptr->pwl1 = 3;
    } else {
	PrintError(core->vm_info, core, "Unsupported EPT Table depth\n");
	return -1;
    }

    ept_ptr->pml_base_addr = PAGE_BASE_ADDR(ept_pa);


    return 0;
}


static inline void ept_exit_qual_to_pf_error(struct ept_exit_qual *qual, pf_error_t *error)
{
    memset(error,0,sizeof(pf_error_t));
    error->present = qual->present;
    error->write = qual->write;
    error->ifetch = qual->ifetch;
}
    

/* We can use the default paging macros, since the formats are close enough to allow it */


static int handle_vmx_nested_pagefault(struct guest_info * core, addr_t fault_addr, void *pfinfo,
				       addr_t *actual_start, addr_t *actual_end )
{
    struct ept_exit_qual * ept_qual = (struct ept_exit_qual *) pfinfo;
    ept_pml4_t    * pml     = NULL;
    //    ept_pdp_1GB_t * pdpe1gb = NULL;
    ept_pdp_t     * pdpe    = NULL;
    ept_pde_2MB_t * pde2mb  = NULL;
    ept_pde_t     * pde     = NULL;
    ept_pte_t     * pte     = NULL;
    addr_t host_addr     = 0;

    int pml_index  = PML4E64_INDEX(fault_addr);
    int pdpe_index = PDPE64_INDEX(fault_addr);
    int pde_index  = PDE64_INDEX(fault_addr);
    int pte_index  = PTE64_INDEX(fault_addr);

    struct v3_mem_region * region = v3_get_mem_region(core->vm_info, core->vcpu_id, fault_addr);
    int page_size = PAGE_SIZE_4KB;


    pf_error_t error_code;
    
    ept_exit_qual_to_pf_error(ept_qual, &error_code);

    PrintDebug(info->vm_info, info, "Nested PageFault: fault_addr=%p, error_code=%u, exit_qual=0x%llx\n", (void *)fault_addr, *(uint_t *)&error_code, qual->value);

    
    if (region == NULL) {
	PrintError(core->vm_info, core, "invalid region, addr=%p\n", (void *)fault_addr);
	return -1;
    }

    if ((core->use_large_pages == 1) || (core->use_giant_pages == 1)) {
	page_size = v3_get_max_page_size(core, fault_addr, LONG);
    }



    pml = (ept_pml4_t *)CR3_TO_PML4E64_VA(core->direct_map_pt);



    //Fix up the PML entry
    if (pml[pml_index].read == 0) { 
	pdpe = (ept_pdp_t *)create_ept_page();
	
	// Set default PML Flags...
	pml[pml_index].read = 1;
	pml[pml_index].write = 1;
	pml[pml_index].exec = 1;

	pml[pml_index].pdp_base_addr = PAGE_BASE_ADDR_4KB((addr_t)V3_PAddr(pdpe));
    } else {
	pdpe = V3_VAddr((void *)BASE_TO_PAGE_ADDR_4KB(pml[pml_index].pdp_base_addr));
    }


    // Fix up the PDPE entry
    if (pdpe[pdpe_index].read == 0) {
	pde = (ept_pde_t *)create_ept_page();

	// Set default PDPE Flags...
	pdpe[pdpe_index].read = 1;
	pdpe[pdpe_index].write = 1;
	pdpe[pdpe_index].exec = 1;

	pdpe[pdpe_index].pd_base_addr = PAGE_BASE_ADDR_4KB((addr_t)V3_PAddr(pde));
    } else {
	pde = V3_VAddr((void *)BASE_TO_PAGE_ADDR_4KB(pdpe[pdpe_index].pd_base_addr));
    }



    // Fix up the 2MiB PDE and exit here
    if (page_size == PAGE_SIZE_2MB) {
	pde2mb = (ept_pde_2MB_t *)pde; // all but these two lines are the same for PTE
	pde2mb[pde_index].large_page = 1;

	*actual_start = BASE_TO_PAGE_ADDR_2MB(PAGE_BASE_ADDR_2MB(fault_addr));
	*actual_end = BASE_TO_PAGE_ADDR_2MB(PAGE_BASE_ADDR_2MB(fault_addr)+1)-1;

	if (pde2mb[pde_index].read == 0) {

	    if ( (region->flags.alloced == 1) && 
		 (region->flags.read == 1)) {
		// Full access
		pde2mb[pde_index].read = 1;
		pde2mb[pde_index].exec = 1;
		pde2mb[pde_index].ipat = 1;
		pde2mb[pde_index].mt = 6;

		if (region->flags.write == 1) {
		    pde2mb[pde_index].write = 1;
		} else {
		    pde2mb[pde_index].write = 0;
		}

		if (v3_gpa_to_hpa(core, fault_addr, &host_addr) == -1) {
		    PrintError(core->vm_info, core, "Error: Could not translate fault addr (%p)\n", (void *)fault_addr);
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

	return 0;
    }

    // Continue with the 4KiB page heirarchy
    

    // Fix up the PDE entry
    if (pde[pde_index].read == 0) {
	pte = (ept_pte_t *)create_ept_page();
	
	pde[pde_index].read = 1;
	pde[pde_index].write = 1;
	pde[pde_index].exec = 1;

	pde[pde_index].pt_base_addr = PAGE_BASE_ADDR_4KB((addr_t)V3_PAddr(pte));
    } else {
	pte = V3_VAddr((void *)BASE_TO_PAGE_ADDR_4KB(pde[pde_index].pt_base_addr));
    }


    *actual_start = BASE_TO_PAGE_ADDR_4KB(PAGE_BASE_ADDR_4KB(fault_addr));
    *actual_end = BASE_TO_PAGE_ADDR_4KB(PAGE_BASE_ADDR_4KB(fault_addr)+1)-1;


    // Fix up the PTE entry
    if (pte[pte_index].read == 0) {

	if ( (region->flags.alloced == 1) && 
	     (region->flags.read == 1)) {
	    // Full access
	    pte[pte_index].read = 1;
	    pte[pte_index].exec = 1;
	    pte[pte_index].ipat = 1;
	    pte[pte_index].mt = 6;

	    if (region->flags.write == 1) {
		pte[pte_index].write = 1;
	    } else {
		pte[pte_index].write = 0;
	    }

	    if (v3_gpa_to_hpa(core, fault_addr, &host_addr) == -1) {
		PrintError(core->vm_info, core, "Error Could not translate fault addr (%p)\n", (void *)fault_addr);
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


static int handle_vmx_invalidate_nested_addr_internal(struct guest_info *core, addr_t inv_addr,
						      addr_t *actual_start, uint64_t *actual_size) {
  ept_pml4_t    *pml = NULL;
  ept_pdp_t     *pdpe = NULL;
  ept_pde_t     *pde = NULL;
  ept_pte_t     *pte = NULL;


 
  // clear the page table entry
  
  int pml_index = PML4E64_INDEX(inv_addr);
  int pdpe_index = PDPE64_INDEX(inv_addr);
  int pde_index = PDE64_INDEX(inv_addr);
  int pte_index = PTE64_INDEX(inv_addr);

 
  pml = (ept_pml4_t *)CR3_TO_PML4E64_VA(core->direct_map_pt);
  

  // note that there are no present bits in EPT, so we 
  // use the read bit to signify this.
  // either an entry is read/write/exec or it is none of these
 
  if (pml[pml_index].read == 0) {
    // already invalidated
    *actual_start = BASE_TO_PAGE_ADDR_512GB(PAGE_BASE_ADDR_512GB(inv_addr));
    *actual_size = PAGE_SIZE_512GB;
    return 0;
  }

  pdpe = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pml[pml_index].pdp_base_addr));
  
  if (pdpe[pdpe_index].read == 0) {
    // already invalidated
    *actual_start = BASE_TO_PAGE_ADDR_1GB(PAGE_BASE_ADDR_1GB(inv_addr));
    *actual_size = PAGE_SIZE_1GB;
    return 0;
  } else if (pdpe[pdpe_index].large_page == 1) { // 1GiB
    pdpe[pdpe_index].read = 0;
    pdpe[pdpe_index].write = 0;
    pdpe[pdpe_index].exec = 0;
    *actual_start = BASE_TO_PAGE_ADDR_1GB(PAGE_BASE_ADDR_1GB(inv_addr));
    *actual_size = PAGE_SIZE_1GB;
    return 0;
  }

  pde = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pdpe[pdpe_index].pd_base_addr));

  if (pde[pde_index].read == 0) {
    // already invalidated
    *actual_start = BASE_TO_PAGE_ADDR_2MB(PAGE_BASE_ADDR_2MB(inv_addr));
    *actual_size = PAGE_SIZE_2MB;
    return 0;
  } else if (pde[pde_index].large_page == 1) { // 2MiB
    pde[pde_index].read = 0;
    pde[pde_index].write = 0;
    pde[pde_index].exec = 0;
    *actual_start = BASE_TO_PAGE_ADDR_2MB(PAGE_BASE_ADDR_2MB(inv_addr));
    *actual_size = PAGE_SIZE_2MB;
    return 0;
  }

  pte = V3_VAddr((void*)BASE_TO_PAGE_ADDR(pde[pde_index].pt_base_addr));
  
  pte[pte_index].read = 0; // 4KiB
  pte[pte_index].write = 0;
  pte[pte_index].exec = 0;
  
  *actual_start = BASE_TO_PAGE_ADDR_4KB(PAGE_BASE_ADDR_4KB(inv_addr));
  *actual_size = PAGE_SIZE_4KB;
  
  return 0;
}


static int handle_vmx_invalidate_nested_addr(struct guest_info *core, addr_t inv_addr, 
					     addr_t *actual_start, addr_t *actual_end) 
{
  uint64_t len;
  int rc;
  
  rc = handle_vmx_invalidate_nested_addr_internal(core,inv_addr,actual_start,&len);
  
  *actual_end = *actual_start + len - 1;

  return rc;
}


static int handle_vmx_invalidate_nested_addr_range(struct guest_info *core, 
						   addr_t inv_addr_start, addr_t inv_addr_end,
						   addr_t *actual_start, addr_t *actual_end) 
{
  addr_t next;
  addr_t start;
  uint64_t len;
  int rc;
  
  for (next=inv_addr_start; next<=inv_addr_end; ) {
    rc = handle_vmx_invalidate_nested_addr_internal(core,next,&start, &len);
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
