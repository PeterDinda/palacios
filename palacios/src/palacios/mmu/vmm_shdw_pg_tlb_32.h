/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2014, Chunxiao Diao <chunxiaodiao2012@u.northwestern.edu> 
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 * Author: Chunxiao Diao <chunxiaodiao2012@u.northwestern.edu> 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#define GET_BUDDY(x) (((ullong_t)x) ^ 0x1)
#define MARK_LAST_ZERO(x) (((ullong_t)x) & 0x0)
#define CR3_PAGE_BASE_ADDR(x) ((x) >> 5)
#define V3_SHADOW_LARGE_PAGE 0x3


static inline int activate_shadow_pt_32( struct guest_info *info) 
{
	struct cr3_32_PAE * shadow_cr3 = (struct cr3_32_PAE *) &(info->ctrl_regs.cr3);
	struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);
       	struct cr4_32 * shadow_cr4 = (struct cr4_32 *) &(info->ctrl_regs.cr4);

#ifdef V3_CONFIG_DEBUG_SHADOW_PAGING
	struct cr4_32 * guest_cr4 = (struct cr4_32 *)&(info->shdw_pg_state.guest_cr4);
#endif
	
	struct shadow_page_data * shadow_pt = create_new_shadow_pt(info);
	addr_t shadow_pt_addr = shadow_pt->page_pa;		
	shadow_pt->cr3 = shadow_pt->page_pa;      
	PrintDebug(info->vm_info, info, "Top level ShadowPAE pdp page pa=%p\n", (void *)shadow_pt_addr);
	PrintDebug(info->vm_info,info,"Guest CR4 =%x and Shadow CR4 =%x\n", *(uint_t *)guest_cr4, *(uint_t*)shadow_cr4);
	//shadow cr3 points to the new page, which is PML4T
    shadow_cr3->pdpt_base_addr = CR3_PAGE_BASE_ADDR(shadow_pt_addr); // x >> 5
    PrintDebug(info->vm_info, info, "Creating new shadow page table %p\n", (void *)BASE_TO_PAGE_ADDR(shadow_cr3->pdpt_base_addr));

	shadow_cr3->pwt = guest_cr3->pwt;  
	shadow_cr3->pcd = guest_cr3->pcd;	
 	shadow_cr4->pae = 1;
	//shadow_cr4->pse = 1;
	/*	shadow_efer->lme = 1;
	shadow_efer->lma = 1; */
	
        return 0;
}

/*
*
* shadowPAE page fault handlers
*
*/

static int handle_pdpe_shadow_pagefault_32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code,pdpe32pae_t * shadow_pdp) ;
static int handle_pde_shadow_pagefault_32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code, pde32pae_t * shadow_pd);
static int handle_pte_shadow_pagefault_32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code,
					    pte32pae_t * shadow_pt, pte32_t * guest_pt) ;
static int handle_4MB_shadow_pagefault_pte_32(struct guest_info * info, 
					      addr_t fault_addr, pf_error_t error_code, 
					      pte32pae_t * shadow_pt, pde32_4MB_t * large_guest_pde) ;
static int handle_4MB_shadow_pagefault_pde_32(struct guest_info * info, 
				     addr_t fault_addr, pf_error_t error_code, 
				     pt_access_status_t shadow_pde_access,
				     pde32pae_2MB_t * large_shadow_pde, pde32pae_2MB_t *large_shadow_pde_bd,
						pde32_4MB_t * large_guest_pde)	;

static inline int handle_shadow_pagefault_32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code)
{
	// pointer to pml4t
	pdpe32pae_t * shadow_pdp = CR3_TO_PDPE32PAE_VA(info->ctrl_regs.cr3);
	PrintDebug(info->vm_info, info, "32 bit ShadowPAE page fault handler : %p----------------------------------------\n", (void*)fault_addr);
	if (handle_pdpe_shadow_pagefault_32(info, fault_addr, error_code, shadow_pdp) == -1) {
		PrintError(info->vm_info, info, "Error handling Page fault caused by PDPE\n");
		return -1;
        }	
	return 0;
}

//first 4 entries of shadow pdpe should be present and accessible
static int handle_pdpe_shadow_pagefault_32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code, pdpe32pae_t * shadow_pdp) 
{
	pt_access_status_t shadow_pdpe_access;	
	
	//fault address error
	if ( (PDPE32PAE_INDEX(fault_addr) != 0) && (PDPE32PAE_INDEX(fault_addr) != 1)
	    && (PDPE32PAE_INDEX(fault_addr) != 2) && (PDPE32PAE_INDEX(fault_addr) != 3))
	{
		PrintDebug(info->vm_info, info, "Fault pdpe index is 0x%x, out of range\n", PDPE32PAE_INDEX(fault_addr));
		if (v3_inject_guest_pf(info, fault_addr, error_code) == -1) {
			PrintError(info->vm_info, info, "Could not inject guest page fault\n");
			return -1;
		}	
		return 0;
	}
	
	pdpe32pae_t * shadow_pdpe = (pdpe32pae_t *)&(shadow_pdp[PDPE32PAE_INDEX(fault_addr)]);
    
	PrintDebug(info->vm_info, info, "Handling PDP fault\n");
	
	
    if (fault_addr==0) { 
		PrintDebug(info->vm_info, info, "Guest Page Tree for guest virtual address zero fault\n");
		PrintGuestPageTree(info,fault_addr,(addr_t)(info->shdw_pg_state.guest_cr3));
		PrintDebug(info->vm_info, info, "Host Page Tree for guest virtual address zero fault\n");
		PrintHostPageTree(info,fault_addr,(addr_t)(info->ctrl_regs.cr3));
    }	
  	
    PrintDebug(info->vm_info, info, "Checking shadow_pdp_access %p\n", (void *)shadow_pdp);	
    // Check the shadow page permissions
    shadow_pdpe_access = v3_can_access_pdpe32pae(shadow_pdp, fault_addr, error_code);	
	
   if (shadow_pdpe_access == PT_ACCESS_USER_ERROR || shadow_pdpe_access == PT_ACCESS_WRITE_ERROR) 
   {
		//
		// PML4 Entry marked non-user
		//      
		PrintDebug(info->vm_info, info, "Shadow Paging User or Write access error (shadow_pdpe_access=0x%x). Ignore it.\n", shadow_pdpe_access);
		//shadow_pdpe->user_page = 1;
		//return 0;
    } 
   else if ((shadow_pdpe_access != PT_ACCESS_NOT_PRESENT ) && 
	       (shadow_pdpe_access != PT_ACCESS_OK)) 
	{
		// inject page fault in guest
		//
		// unknown error
		//
		if (v3_inject_guest_pf(info, fault_addr, error_code) == -1) {
			PrintError(info->vm_info, info, "Could not inject guest page fault\n");
			return -1;
		}
		PrintDebug(info->vm_info, info, "Unknown Error occurred (shadow_pde_access=%d)\n", shadow_pdpe_access);
		PrintDebug(info->vm_info, info, "Manual Says to inject page fault into guest\n");
		return 0;
    }	

	pde32pae_t * shadow_pd = NULL;
	//get to page directory table level, allocate if not present
    if (shadow_pdpe_access == PT_ACCESS_NOT_PRESENT) {
		struct shadow_page_data * shdw_page = create_new_shadow_pt(info);
		shadow_pd = (pde32pae_t *)V3_VAddr((void *)shdw_page->page_pa);
               	PrintDebug(info->vm_info, info, "Creating new shadow PDE table: %p\n",shadow_pd);        
		//values should be 1
		shadow_pdpe->present = 1;
		//shadow_pdpe->user_page = 1;
		//shadow_pdpe->writable = 1;
		// when these values set to 0, the next levels have freedom to change them
		shadow_pdpe->write_through = 0;
		shadow_pdpe->cache_disable = 0;

		shadow_pdpe->pd_base_addr = PAGE_BASE_ADDR(shdw_page->page_pa);
    } 
	else 
    {
		shadow_pd = (pde32pae_t *)V3_VAddr((void *)(addr_t)BASE_TO_PAGE_ADDR(shadow_pdpe->pd_base_addr));
    }	
	


    if (handle_pde_shadow_pagefault_32(info, fault_addr, error_code, shadow_pd) == -1) {
		PrintError(info->vm_info, info, "Error handling Page fault caused by PDE\n");
		return -1;
    }
    return 0;
}	

//to handle pde fault
static int handle_pde_shadow_pagefault_32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code, pde32pae_t * shadow_pd)
{
    pt_access_status_t guest_pde_access;
    pt_access_status_t shadow_pde_access;
	
	pde32_t * guest_pd = NULL;
	pde32_t * guest_pde = NULL;
	addr_t guest_cr3 = CR3_TO_PDE32_PA(info->shdw_pg_state.guest_cr3);
    if (v3_gpa_to_hva(info, guest_cr3, (addr_t*)&guest_pd) == -1) {
		PrintError(info->vm_info, info, "Invalid Guest PDE Address: 0x%p\n",  (void *)guest_cr3);
		return -1;
    }
    guest_pde = (pde32_t *)&(guest_pd[PDE32_INDEX(fault_addr)]);
	
    pde32pae_t * shadow_pde = (pde32pae_t *)&(shadow_pd[PDE32PAE_INDEX(fault_addr)]);
    pde32pae_t * shadow_pde_bd = (pde32pae_t *)&(shadow_pd[GET_BUDDY(PDE32PAE_INDEX(fault_addr))]);
    pde32pae_t * shadow_pde_sd = (pde32pae_t *)&(shadow_pd[MARK_LAST_ZERO(PDE32PAE_INDEX(fault_addr))]);  
    PrintDebug(info->vm_info, info, "Handling PDE fault\n");	

    PrintDebug(info->vm_info, info, "Checking guest_pde_access %p\n", (void *)guest_pd);	
    // Check the guest page permissions
    guest_pde_access = v3_can_access_pde32(guest_pd, fault_addr, error_code);	
    // Check the shadow page permissions
    PrintDebug(info->vm_info, info, "Checking shadow_pde_access %p\n", (void *)shadow_pd);
    shadow_pde_access = v3_can_access_pde32pae(shadow_pd, fault_addr, error_code);
	
    /* Was the page fault caused by the Guest PDE */
    if (v3_is_guest_pf(guest_pde_access, shadow_pde_access) == 1) 
	{
		PrintDebug(info->vm_info, info, "Injecting PDE pf to guest: (guest access error=%d) (shdw access error=%d)  (pf error code=%d)\n", 
		   *(uint_t *)&guest_pde_access, *(uint_t *)&shadow_pde_access, *(uint_t *)&error_code);
		if (v3_inject_guest_pf(info, fault_addr, error_code) == -1) 
		{
			PrintError(info->vm_info, info, "Could not inject guest page fault for vaddr %p\n", (void *)fault_addr);
			return -1;
		}
		return 0;
    }
	
	//Guest PDE ok
    if (shadow_pde_access == PT_ACCESS_USER_ERROR) 
	{
		//
		// PDE Entry marked non-user
		//      
		PrintDebug(info->vm_info, info, "Shadow Paging User access error (shadow_pde_access=0x%x, guest_pde_access=0x%x)\n", 
			shadow_pde_access, guest_pde_access);
		if (v3_inject_guest_pf(info, fault_addr, error_code) == -1) 
		{
			PrintError(info->vm_info, info, "Could not inject guest page fault\n");
			return -1;
		}
		return 0;
    } else if ((shadow_pde_access == PT_ACCESS_WRITE_ERROR) && 
	       (guest_pde->large_page == 1)) 
	{

		((pde32_4MB_t *)guest_pde)->dirty = 1;
		shadow_pde->writable = guest_pde->writable;
		shadow_pde_bd->writable = guest_pde->writable;
		return 0;
    } else if ((shadow_pde_access != PT_ACCESS_NOT_PRESENT) &&
	       (shadow_pde_access != PT_ACCESS_OK)) 
	{
		// inject page fault in guest
		//
		//unknown error
		//
		if (v3_inject_guest_pf(info, fault_addr, error_code) == -1) {
			PrintError(info->vm_info, info, "Could not inject guest page fault\n");
			return -1;
		}
		PrintDebug(info->vm_info, info, "Unknown Error occurred (shadow_pde_access=%d)\n", shadow_pde_access);
		PrintDebug(info->vm_info, info, "Manual Says to inject page fault into guest\n");
		return 0;
    }

	pte32pae_t * shadow_pt = NULL;
	pte32pae_t * shadow_pt_bd = NULL;
	pte32_t * guest_pt = NULL;
	
    // get the next shadow page level (page table) , allocate 2 PDEs (buddies) if not present
    if (shadow_pde_access == PT_ACCESS_NOT_PRESENT) 
    {
        // Check if  we can use large pages and the guest memory is properly aligned
        // to potentially use a large page

        if ((info->use_large_pages == 1) && (guest_pde->large_page == 1)) 
		{
			addr_t guest_pa = BASE_TO_PAGE_ADDR_4MB(((pde32_4MB_t *)guest_pde)->page_base_addr);
 			uint32_t page_size = v3_get_max_page_size(info, guest_pa, PROTECTED);
	    
			if (page_size == PAGE_SIZE_4MB) 
			{

				if (shadow_pde !=  shadow_pde_sd) // when handling page fault, we pass through the buddy with last bit as 0
				{
					pde32pae_t * tmp_addr = shadow_pde;
					shadow_pde = shadow_pde_bd;  
					shadow_pde_bd = tmp_addr;
				}
				if (handle_4MB_shadow_pagefault_pde_32(info, fault_addr, error_code, shadow_pde_access,
						       (pde32pae_2MB_t *)shadow_pde,(pde32pae_2MB_t *)shadow_pde_bd, (pde32_4MB_t *)guest_pde) == -1) 
				{
					PrintError(info->vm_info, info, "Error handling large pagefault with large page\n");
					return -1;
				}
				return 0;
			} 
	    // Fallthrough to handle the region with small pages
		}	
		
		struct shadow_page_data * shdw_page = create_new_shadow_pt(info);
		struct shadow_page_data * shdw_page_bd = create_new_shadow_pt(info);
		shadow_pt = (pte32pae_t *)V3_VAddr((void *)shdw_page->page_pa);
		shadow_pt_bd = (pte32pae_t *)V3_VAddr((void *)shdw_page_bd->page_pa);
		PrintDebug(info->vm_info, info, "Creating new shadow PTs: %p and %p\n", shadow_pt, shadow_pt_bd);

		shadow_pde->present = 1;
		shadow_pde_bd->present = 1;
		shadow_pde->user_page = guest_pde->user_page;	
		shadow_pde_bd->user_page = guest_pde->user_page;

		if (guest_pde->large_page == 0) {
			shadow_pde->writable = guest_pde->writable;
			shadow_pde_bd->writable = guest_pde->writable;
		} 
		else {
 			((pde32pae_2MB_t *)guest_pde)->vmm_info = V3_LARGE_PG;

			if (error_code.write) {
				shadow_pde->writable = guest_pde->writable;
				shadow_pde_bd->writable = guest_pde->writable;
				((pde32pae_2MB_t *)guest_pde)->dirty = 1;	
			} 
			else {
				shadow_pde->writable = 0;
				shadow_pde_bd->writable = 0;
				((pde32pae_2MB_t *)guest_pde)->dirty = 0;
			} 
		}		
	
		// VMM Specific options
		shadow_pde->write_through = guest_pde->write_through;
		shadow_pde->cache_disable = guest_pde->cache_disable;
		shadow_pde->global_page = guest_pde->global_page;
		
		shadow_pde_bd->write_through = guest_pde->write_through;
		shadow_pde_bd->cache_disable = guest_pde->cache_disable;
		shadow_pde_bd->global_page = guest_pde->global_page;
		//
		guest_pde->accessed = 1;
		
		shadow_pde->pt_base_addr = PAGE_BASE_ADDR(shdw_page->page_pa);
		shadow_pde_bd->pt_base_addr = PAGE_BASE_ADDR(shdw_page_bd->page_pa);
    } 
	else {
	  if ((info->use_large_pages == 1) && (guest_pde->large_page == 1) && (guest_pde->vmm_info == V3_SHADOW_LARGE_PAGE)) 
		{
			addr_t guest_pa = BASE_TO_PAGE_ADDR_4MB(((pde32_4MB_t *)guest_pde)->page_base_addr);
 			uint32_t page_size = v3_get_max_page_size(info, guest_pa, PROTECTED);   
			if (page_size == PAGE_SIZE_4MB) 
			{
				if (shadow_pde_access == PT_ACCESS_OK) {
					// Inconsistent state...
					// Guest Re-Entry will flush tables and everything should now workd
					PrintDebug(info->vm_info, info, "Inconsistent state PDE... Guest re-entry should flush tlb\n");
                    //PrintDebug(info->vm_info, info, "Bug here: shadow_pde_access is %d page_size is %d\n",
					//	   (uint_t)shadow_pde_access,(uint_t)page_size);
					return 0;
				}
			} 
		}
		shadow_pt = (pte32pae_t *)V3_VAddr((void *)BASE_TO_PAGE_ADDR(shadow_pde->pt_base_addr));
    }
	
    if (guest_pde->large_page == 0) 
	{
		if (v3_gpa_to_hva(info, BASE_TO_PAGE_ADDR(guest_pde->pt_base_addr), (addr_t*)&guest_pt) == -1) 
		{
			// Machine check the guest
			PrintDebug(info->vm_info, info, "Invalid Guest PTE Address: 0x%p\n", (void *)BASE_TO_PAGE_ADDR(guest_pde->pt_base_addr));
			v3_raise_exception(info, MC_EXCEPTION);
			return 0;
		}	
		if (handle_pte_shadow_pagefault_32(info, fault_addr, error_code, shadow_pt, guest_pt) == -1) 
		{
			PrintError(info->vm_info, info, "Error handling Page fault caused by PDE\n");
			return -1;
		}
	}
	else {
		//
		//use 4K pages to implement large page; ignore for now
		//
 		if (handle_4MB_shadow_pagefault_pte_32(info, fault_addr, error_code, shadow_pt, (pde32_4MB_t *)guest_pde) == -1) 
		{
			PrintError(info->vm_info, info, "Error handling large pagefault\n");
			return -1;
		}	 
    }	
	
	return 0;
}
	
	
static int handle_pte_shadow_pagefault_32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code,
					  pte32pae_t * shadow_pt, pte32_t * guest_pt) 
{
    pt_access_status_t guest_pte_access;
    pt_access_status_t shadow_pte_access;
    pte32_t * guest_pte = (pte32_t *)&(guest_pt[PTE32_INDEX(fault_addr)]);;
    pte32pae_t * shadow_pte = (pte32pae_t *)&(shadow_pt[PTE32PAE_INDEX(fault_addr)]);
    addr_t guest_pa = BASE_TO_PAGE_ADDR((addr_t)(guest_pte->page_base_addr)) +  PAGE_OFFSET(fault_addr);

     PrintDebug(info->vm_info, info, "Handling PTE fault\n");

    struct v3_mem_region * shdw_reg =  v3_get_mem_region(info->vm_info, info->vcpu_id, guest_pa);

    if (shdw_reg == NULL) {
		// Inject a machine check in the guest
		PrintDebug(info->vm_info, info, "Invalid Guest Address in page table (0x%p)\n", (void *)guest_pa);
		v3_raise_exception(info, MC_EXCEPTION);
		return 0;
    }

    // Check the guest page permissions
    guest_pte_access = v3_can_access_pte32(guest_pt, fault_addr, error_code);

    // Check the shadow page permissions
    shadow_pte_access = v3_can_access_pte32pae(shadow_pt, fault_addr, error_code);
  
  
    /* Was the page fault caused by the Guest's page tables? */
    if (v3_is_guest_pf(guest_pte_access, shadow_pte_access) == 1) 
	{

		PrintDebug(info->vm_info, info, "Access error injecting pf to guest (guest access error=%d) (pf error code=%d)\n", 
		   guest_pte_access, *(uint_t*)&error_code);
	

		//   inject:
		if (v3_inject_guest_pf(info, fault_addr, error_code) == -1) {
			PrintError(info->vm_info, info, "Could not inject guest page fault for vaddr %p\n", (void *)fault_addr);
			return -1;
		}	

		return 0; 
    }

  
  
    if (shadow_pte_access == PT_ACCESS_OK) 
	{
		// Inconsistent state...
		// Guest Re-Entry will flush page tables and everything should now work
		PrintDebug(info->vm_info, info, "Inconsistent state PTE... Guest re-entry should flush tlb\n");
		PrintDebug(info->vm_info, info, "guest_pte_access is %d and shadow_pte_access is %d\n", (uint_t)guest_pte_access, 
			   (uint_t)shadow_pte_access);
		PrintDebug(info->vm_info, info, "Error_code: write 0x%x, present 0x%x, user 0x%x, rsvd_access 0x%x, ifetch 0x%x \n",  error_code.write,error_code.present,error_code.user,error_code.rsvd_access,error_code.ifetch);
		PrintHostPageTree(info, fault_addr, info->ctrl_regs.cr3);
		PrintGuestPageTree(info,fault_addr,(addr_t)(info->shdw_pg_state.guest_cr3));
		return 0;
    }


    if (shadow_pte_access == PT_ACCESS_NOT_PRESENT) 
	{
		// Page Table Entry Not Present
		PrintDebug(info->vm_info, info, "guest_pa =%p\n", (void *)guest_pa);

		if ((shdw_reg->flags.alloced == 1) && (shdw_reg->flags.read == 1)) 
		{
			addr_t shadow_pa = 0;

			if (v3_gpa_to_hpa(info, guest_pa, &shadow_pa) == -1) 
			{
				PrintError(info->vm_info, info, "could not translate page fault address (%p)\n", (void *)guest_pa);
				return -1;
			}

			shadow_pte->page_base_addr = PAGE_BASE_ADDR(shadow_pa);

			PrintDebug(info->vm_info, info, "\tMapping shadow page (%p)\n", (void *)BASE_TO_PAGE_ADDR(shadow_pte->page_base_addr));
      
			shadow_pte->present = guest_pte->present;
			shadow_pte->user_page = guest_pte->user_page;
      
			//set according to VMM policy
			shadow_pte->write_through = guest_pte->write_through;
			shadow_pte->cache_disable = guest_pte->cache_disable;
			shadow_pte->global_page = guest_pte->global_page;
			//
      
			guest_pte->accessed = 1;
      
			if (guest_pte->dirty == 1) {
				shadow_pte->writable = guest_pte->writable;
			} 
			else if ((guest_pte->dirty == 0) && (error_code.write == 1)) {
				shadow_pte->writable = guest_pte->writable;
				guest_pte->dirty = 1;
			} 
			else if ((guest_pte->dirty == 0) && (error_code.write == 0)) {
				shadow_pte->writable = 0;
			}

			// Write hooks trump all, and are set Read Only
			if (shdw_reg->flags.write == 0) {
				shadow_pte->writable = 0;
			}	

		} 
		else {
			// Page fault on unhandled memory region
	    
			if (shdw_reg->unhandled(info, fault_addr, guest_pa, shdw_reg, error_code) == -1) {
				PrintError(info->vm_info, info, "Special Page fault handler returned error for address: %p\n",  (void *)fault_addr);
				return -1;
			}
		}
    } 
	else if (shadow_pte_access == PT_ACCESS_WRITE_ERROR) 
	{
		guest_pte->dirty = 1;

		if (shdw_reg->flags.write == 1) {
			PrintDebug(info->vm_info, info, "Shadow PTE Write Error\n");
			shadow_pte->writable = guest_pte->writable;
		} 
		else {
			if (shdw_reg->unhandled(info, fault_addr, guest_pa, shdw_reg, error_code) == -1) {
				PrintError(info->vm_info, info, "Special Page fault handler returned error for address: %p\n",  (void *)fault_addr);
				return -1;
			}
		}
		return 0;
    } 
	else {
		// Inject page fault into the guest	
		if (v3_inject_guest_pf(info, fault_addr, error_code) == -1) {
			PrintError(info->vm_info, info, "Could not inject guest page fault for vaddr %p\n", (void *)fault_addr);
			return -1;
		}

		PrintError(info->vm_info, info, "PTE Page fault fell through... Not sure if this should ever happen\n");
		PrintError(info->vm_info, info, "Manual Says to inject page fault into guest\n");
		return -1;
    }

    return 0;
}	
	
	
// Handle a 4MB page fault with 2 2MB page in the PDE
static int handle_4MB_shadow_pagefault_pde_32(struct guest_info * info, 
				     addr_t fault_addr, pf_error_t error_code, 
				     pt_access_status_t shadow_pde_access,
				     pde32pae_2MB_t * large_shadow_pde, pde32pae_2MB_t * large_shadow_pde_bd,
					 pde32_4MB_t * large_guest_pde)	
{
	addr_t guest_fault_pa = BASE_TO_PAGE_ADDR_4MB(large_guest_pde->page_base_addr) + PAGE_OFFSET_4MB(fault_addr);  

    PrintDebug(info->vm_info, info, "Handling 4MB fault with large page (guest_fault_pa=%p) (error_code=%x)\n", (void *)guest_fault_pa, *(uint_t*)&error_code);
    PrintDebug(info->vm_info, info, "LargeShadowPDE=%p, LargeGuestPDE=%p\n", large_shadow_pde, large_guest_pde);

    struct v3_mem_region * shdw_reg = v3_get_mem_region(info->vm_info, info->vcpu_id, guest_fault_pa);

 
    if (shdw_reg == NULL) {
		// Inject a machine check in the guest
		PrintDebug(info->vm_info, info, "Invalid Guest Address in page table (0x%p)\n", (void *)guest_fault_pa);
		v3_raise_exception(info, MC_EXCEPTION);
		return -1;
    }
	
	//dead bug
    if (shadow_pde_access == PT_ACCESS_OK) {
		// Inconsistent state...
		// Guest Re-Entry will flush tables and everything should now workd
		PrintDebug(info->vm_info, info, "Inconsistent state 4MB pde... Guest re-entry should flush tlb\n");
		return 0;
    }

  
    if (shadow_pde_access == PT_ACCESS_NOT_PRESENT) 
	{
		// Get the guest physical address of the fault

		if ((shdw_reg->flags.alloced == 1) && 
			(shdw_reg->flags.read  == 1)) 
		{
			addr_t shadow_pa = 0;


			if (v3_gpa_to_hpa(info, guest_fault_pa, &shadow_pa) == -1) 
			{
				PrintError(info->vm_info, info, "could not translate page fault address (%p)\n", (void *)guest_fault_pa);
				return -1;
			}

			PrintDebug(info->vm_info, info, "shadow PA = %p\n", (void *)shadow_pa);


              large_guest_pde->vmm_info = V3_SHADOW_LARGE_PAGE; /* For invalidations */
	      //shadow pde (last bit 0) gets the half with smaller address and its buddy gets the rest
            large_shadow_pde->page_base_addr = PAGE_BASE_ADDR_4MB(shadow_pa)<<1;
			large_shadow_pde_bd->page_base_addr =(PAGE_BASE_ADDR_4MB(shadow_pa)<<1)|1; 
			
			// large_shadow_pde->large_page = 1;
            large_shadow_pde->present = 1;
            large_shadow_pde->user_page = 1;
			
	    //		large_shadow_pde_bd->large_page = 1;
            large_shadow_pde_bd->present = 1;
            large_shadow_pde_bd->user_page = 1;

	    PrintDebug(info->vm_info, info, "\tMapping shadow pages (%p) and (%p)\n", 
		                                (void *)BASE_TO_PAGE_ADDR_2MB(large_shadow_pde->page_base_addr),
						(void *)BASE_TO_PAGE_ADDR_2MB(large_shadow_pde_bd->page_base_addr));

            if (shdw_reg->flags.write == 0) {
                large_shadow_pde->writable = 0;
				large_shadow_pde_bd->writable = 0;
            } else {
                large_shadow_pde_bd->writable = 1;
				large_shadow_pde->writable = 1;
            }

			//set according to VMM policy
			large_shadow_pde->write_through = large_guest_pde->write_through;
			large_shadow_pde->cache_disable = large_guest_pde->cache_disable;
			large_shadow_pde->global_page = large_guest_pde->global_page;

			large_shadow_pde_bd->write_through = large_guest_pde->write_through;
			large_shadow_pde_bd->cache_disable = large_guest_pde->cache_disable;
			large_shadow_pde_bd->global_page = large_guest_pde->global_page;			
		} 
		else {
			if (shdw_reg->unhandled(info, fault_addr, guest_fault_pa, shdw_reg, error_code) == -1) {
				PrintError(info->vm_info, info, "Special Page Fault handler returned error for address: %p\n", (void *)fault_addr);
				return -1;
			}
		}
	} 
	else if (shadow_pde_access == PT_ACCESS_WRITE_ERROR) 
	{

		if (shdw_reg->flags.write == 0) {
			if (shdw_reg->unhandled(info, fault_addr, guest_fault_pa, shdw_reg, error_code) == -1) {
				PrintError(info->vm_info, info, "Special Page Fault handler returned error for address: %p\n", (void *)fault_addr);
				return -1;
			}	
		}

    } 
	else {
		PrintError(info->vm_info, info, "Error in large page fault handler...\n");
		PrintError(info->vm_info, info, "This case should have been handled at the top level handler\n");
		return -1;
    }

	PrintDebug(info->vm_info, info, "Returning from large page->large page fault handler\n");
	return 0;
}	
	
static int handle_4MB_shadow_pagefault_pte_32(struct guest_info * info, 
					      addr_t fault_addr, pf_error_t error_code, 
					      pte32pae_t * shadow_pt, pde32_4MB_t * large_guest_pde) 
{
    pt_access_status_t shadow_pte_access = v3_can_access_pte32pae(shadow_pt, fault_addr, error_code);
    pte32pae_t * shadow_pte = (pte32pae_t *)&(shadow_pt[PTE32PAE_INDEX(fault_addr)]);
    addr_t guest_fault_pa = BASE_TO_PAGE_ADDR_4MB(large_guest_pde->page_base_addr) + PAGE_OFFSET_4MB(fault_addr);
    //  struct shadow_page_state * state = &(info->shdw_pg_state);

    PrintDebug(info->vm_info, info, "Handling 4MB PTE fault (guest_fault_pa=%p) (error_code=%x)\n", (void *)guest_fault_pa, *(uint_t*)&error_code);
    PrintDebug(info->vm_info, info, "ShadowPT=%p, LargeGuestPDE=%p\n", shadow_pt, large_guest_pde);

    struct v3_mem_region * shdw_reg = v3_get_mem_region(info->vm_info, info->vcpu_id, guest_fault_pa);

 
    if (shdw_reg == NULL) {
		// Inject a machine check in the guest
		PrintError(info->vm_info, info, "Invalid Guest Address in page table (0x%p)\n", (void *)guest_fault_pa);
		v3_raise_exception(info, MC_EXCEPTION);
		return 0;
    }

    if (shadow_pte_access == PT_ACCESS_OK) {
		// Inconsistent state...
		// Guest Re-Entry will flush tables and everything should now workd
		PrintDebug(info->vm_info, info, "Inconsistent state 4MB PTE... Guest re-entry should flush tlb\n");
		//PrintHostPageTree(info, fault_addr, info->ctrl_regs.cr3);
		return 0;
    }

  
    if (shadow_pte_access == PT_ACCESS_NOT_PRESENT) {
	// Get the guest physical address of the fault

		if ((shdw_reg->flags.alloced == 1) || 
			(shdw_reg->flags.read == 1)) {
			addr_t shadow_pa = 0;

			if (v3_gpa_to_hpa(info, guest_fault_pa, &shadow_pa) == -1) {
				PrintError(info->vm_info, info, "could not translate page fault address (%p)\n", (void *)guest_fault_pa);
				return -1;
			}

			shadow_pte->page_base_addr = PAGE_BASE_ADDR(shadow_pa);

			shadow_pte->present = 1;

			/* We are assuming that the PDE entry has precedence
			* so the Shadow PDE will mirror the guest PDE settings, 
			* and we don't have to worry about them here
			* Allow everything
			*/
			shadow_pte->user_page = 1;

			if (shdw_reg->flags.write == 0) {
				shadow_pte->writable = 0;
			} else {
				shadow_pte->writable = 1;
			}

			//set according to VMM policy
			shadow_pte->write_through = large_guest_pde->write_through;
			shadow_pte->cache_disable = large_guest_pde->cache_disable;
			shadow_pte->global_page = large_guest_pde->global_page;
			//
      
		} else {
			if (shdw_reg->unhandled(info, fault_addr, guest_fault_pa, shdw_reg, error_code) == -1) {
				PrintError(info->vm_info, info, "Special Page Fault handler returned error for address: %p\n", (void *)fault_addr);
				return -1;
			}
		}
    } else if (shadow_pte_access == PT_ACCESS_WRITE_ERROR) {
	    if (shdw_reg->unhandled(info, fault_addr, guest_fault_pa, shdw_reg, error_code) == -1) {
			PrintError(info->vm_info, info, "Special Page Fault handler returned error for address: %p\n", (void *)fault_addr);
			return -1;
		}
    } else {
		PrintError(info->vm_info, info, "Error in large page fault handler...\n");
		PrintError(info->vm_info, info, "This case should have been handled at the top level handler\n");
		return -1;
    }

    //  PrintHostPageTree(info, fault_addr, info->ctrl_regs.cr3);
    PrintDebug(info->vm_info, info, "Returning from large page->small page fault handler\n");
    return 0;
}	
	
static int invalidation_cb32_64(struct guest_info * info, page_type_t type, 
			      addr_t vaddr, addr_t page_ptr, addr_t page_pa, 
			      void * private_data) {

    switch (type) {
	case PAGE_PDP32PAE:
	    {
			pdpe32pae_t * pdp = (pdpe32pae_t *)page_ptr;
			pdpe32pae_t * pdpe = &(pdp[PDPE32PAE_INDEX(vaddr)]);

			if (pdpe->present == 0) {
				return 1;
			}
     
			if (pdpe->vmm_info == V3_LARGE_PG) {
				PrintError(info->vm_info, info, "1 Gigabyte pages not supported\n");
				return -1;
			}

			return 0;
	    }
	case PAGE_PD32PAE:
	    {
			pde32pae_t * pd = (pde32pae_t *)page_ptr;
			pde32pae_t * pde = &(pd[PDE32PAE_INDEX(vaddr)]);
			pde32pae_t * pde_bd = &(pd[GET_BUDDY(PDE32PAE_INDEX(vaddr))]);
			if (pde->present == 0) {
				return 1;
			}
      
			if (pde->vmm_info == V3_LARGE_PG || pde->vmm_info == V3_SHADOW_LARGE_PAGE) {
				pde->present = 0;
				pde_bd->present = 0;
				return 1;
			}

			return 0;
	    }
	case PAGE_PT32PAE:
	    {
			pte32pae_t * pt = (pte32pae_t *)page_ptr;

			pt[PTE32PAE_INDEX(vaddr)].present = 0;

			return 1;
	    }
	default:
	    PrintError(info->vm_info, info, "Invalid Page Type\n");
	    return -1;

    }

    // should not get here
    PrintError(info->vm_info, info, "Should not get here....\n");
    return -1;
}	

static inline int handle_shadow_invlpg_32(struct guest_info * info, addr_t vaddr) {
    PrintDebug(info->vm_info, info, "INVLPG32PAE - %p\n",(void*)vaddr);

    int ret =  v3_drill_host_pt_32pae(info, info->ctrl_regs.cr3, vaddr, invalidation_cb32_64, NULL);
    if (ret == -1) {
		PrintError(info->vm_info, info, "Page table drill returned error.... \n");
		PrintHostPageTree(info, vaddr, info->ctrl_regs.cr3);
    }

    return (ret == -1) ? -1 : 0; 
}
	
