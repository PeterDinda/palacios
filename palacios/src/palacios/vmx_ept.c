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
 * Author: Jack Lange <jacklange@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmx_ept.h>
#include <palacios/vmx_lowlevel.h>
#include <palacios/vmm_paging.h>
#include <palacios/vm_guest_mem.h>


static struct vmx_ept_msr * ept_info = NULL;


static addr_t create_ept_page() {
    void * page = 0;
    page = V3_VAddr(V3_AllocPages(1));
    memset(page, 0, PAGE_SIZE);

    return (addr_t)page;
}




int v3_init_ept(struct guest_info * core, struct vmx_hw_info * hw_info) {
    addr_t ept_pa = (addr_t)V3_PAddr((void *)create_ept_page());    
    vmx_eptp_t * ept_ptr = (vmx_eptp_t *)&(core->direct_map_pt);


    ept_info = &(hw_info->ept_info);

    /* TODO: Should we set this to WB?? */
    ept_ptr->psmt = 0;

    if (ept_info->pg_walk_len4) {
	ept_ptr->pwl1 = 3;
    } else {
	PrintError("Unsupported EPT Table depth\n");
	return -1;
    }

    ept_ptr->pml_base_addr = PAGE_BASE_ADDR(ept_pa);


    return 0;
}


/* We can use the default paging macros, since the formats are close enough to allow it */

int v3_handle_ept_fault(struct guest_info * core, addr_t fault_addr, struct ept_exit_qual * ept_qual) {
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



    pf_error_t error_code = {0};
    error_code.present = ept_qual->present;
    error_code.write = ept_qual->write;
    
    if (region == NULL) {
	PrintError("invalid region, addr=%p\n", (void *)fault_addr);
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

	if (pde2mb[pde_index].read == 0) {

	    if ( (region->flags.alloced == 1) && 
		 (region->flags.read == 1)) {
		// Full access
		pde2mb[pde_index].read = 1;
		pde2mb[pde_index].exec = 1;
	       
		if (region->flags.write == 1) {
		    pde2mb[pde_index].write = 1;
		} else {
		    pde2mb[pde_index].write = 0;
		}

		if (v3_gpa_to_hpa(core, fault_addr, &host_addr) == -1) {
		    PrintError("Error: Could not translate fault addr (%p)\n", (void *)fault_addr);
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




    // Fix up the PTE entry
    if (pte[pte_index].read == 0) {

	if ( (region->flags.alloced == 1) && 
	     (region->flags.read == 1)) {
	    // Full access
	    pte[pte_index].read = 1;
	    pte[pte_index].exec = 1;

	    if (region->flags.write == 1) {
		pte[pte_index].write = 1;
	    } else {
		pte[pte_index].write = 0;
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
