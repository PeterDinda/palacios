

#ifdef V3_CONFIG_SHADOW_CACHE

#define PT64_NX_MASK (1ULL << 63)
//#define SHOW_ALL 

static inline int activate_shadow_pt_64(struct guest_info * core) {
    struct cr3_64 * shadow_cr3 = (struct cr3_64 *)&(core->ctrl_regs.cr3);
    struct cr3_64 * guest_cr3 = (struct cr3_64 *)&(core->shdw_pg_state.guest_cr3);

    struct shadow_page_cache_data *shadow_pt;

    if(core->n_free_shadow_pages < MIN_FREE_SHADOW_PAGES) {
	shadow_free_some_pages(core);
    }
    shadow_pt = shadow_page_get_page(core, (addr_t)(guest_cr3->pml4t_base_addr), 4, 0, 0, 0, 0);
    PrintDebug("Activate shadow_pt %p\n", (void *)BASE_TO_PAGE_ADDR(guest_cr3->pml4t_base_addr));

    struct shadow_page_cache_data * shadow_pt = create_new_shadow_pt(core);

    addr_t shadow_pt_addr = shadow_pt->page_pa;

    // Because this is a new CR3 load the allocated page is the new CR3 value
    shadow_pt->cr3 = shadow_pt->page_pa;

    PrintDebug("Top level Shadow page pa=%p\n", (void *)shadow_pt_addr);

    shadow_cr3->pml4t_base_addr = PAGE_BASE_ADDR_4KB(shadow_pt_addr);
    PrintDebug("Creating new 64 bit shadow page table %p\n", (void *)BASE_TO_PAGE_ADDR(shadow_cr3->pml4t_base_addr));
  
    shadow_cr3->pwt = guest_cr3->pwt;
    shadow_cr3->pcd = guest_cr3->pcd;

    shadow_topup_caches(core);

    return 0;
}






/* 
 * *
 * * 
 * * 64 bit Page table fault handlers
 * *
 * *
 */

static inline void burst_64 (struct guest_info * core) {
#ifdef SHOW_ALL
		struct shadow_page_cache_data * sp, *node;
		pte64_t * pt, *pte;
		int idx;
		list_for_each_entry_safe(sp, node, &core->active_shadow_pages, link) {
			pt = (pte64_t *)V3_VAddr((void *)sp->page_pa);
			PrintDebug("burst: pt %p\n",(void *)pt);
			for (idx = 0; idx < PT_ENT_PER_PAGE; ++idx) {				
				pte = (pte64_t *)&(pt[idx]);
				if(*((uint64_t*)pte)) PrintDebug("%d: s %p\n",idx, (void*)*((uint64_t*)pte));
			}
			
		}
#endif
}

static inline int fix_read_pf_64(pte64_t *shadow_pte, uint_t vmm_info) {

    PrintDebug("\tReadPf, start vmm_info %d\n", vmm_info);

    if ((vmm_info & PT_USER_MASK) && !(shadow_pte->user_page)) {
	shadow_pte->user_page = 1;
	shadow_pte->writable = 0;
	return 1;
    }
    return 0;		
}

static inline int fix_write_pf_64(struct guest_info *core, pte64_t *shadow_pte, pte64_t *guest_pte,
    int user, int *write_pt, addr_t guest_fn, uint_t vmm_info) {

    int writable_shadow;
    struct cr0_64 *guest_cr0;
    struct shadow_page_cache_data *page;
    *write_pt = 0;

    PrintDebug("\tWritePf, start vmm_info %d\n", vmm_info);

    if (shadow_pte->writable) {
	return 0;
    }

    PrintDebug("\tWritePf, pass writable\n");
    writable_shadow = vmm_info & PT_WRITABLE_MASK;
    PrintDebug("\tWritePf, writable_shadow %d\n", writable_shadow);

    if (user) {
	if (!(vmm_info & PT_USER_MASK) || !writable_shadow) {
	    PrintDebug("\tWritePf: 1st User Check\n");
	    return 0;
	}
    } else {
    	if (!writable_shadow) {
	   guest_cr0 = (struct cr0_64 *)&(core->shdw_pg_state.guest_cr0);
	   PrintDebug("\tWritePf: WP %d\n", guest_cr0->wp);

	   if (guest_cr0->wp) {
	       return 0;
	   }
	   shadow_pte->user_page = 0;
    	}
    }

    if (guest_pte->present == 0) {
	memset((void*)shadow_pte, 0, sizeof(struct pte64));
	PrintDebug("\tWritePf: Guest Not Present\n");
	return 0;
    }

    if (user) {
	while ((page = shadow_page_lookup_page(core, guest_fn, 0)) != NULL) {
	    shadow_zap_page(core, page);
	}
	
	PrintDebug("\tWritePf: Zap Page\n");
    } else if ((page = shadow_page_lookup_page(core, guest_fn, 0)) != NULL) {
	if ((page = shadow_page_lookup_page (core, guest_fn, 0)) != NULL) {
	    guest_pte->dirty = 1;
	    *write_pt = 1;
	    PrintDebug("\tWritePf: Write Needed\n");
	    return 0;
	}
    }

    shadow_pte->writable = 1;
    guest_pte->dirty = 1;

    rmap_add(core, (addr_t)shadow_pte);

    PrintDebug("\tWritePf: On Writable\n");
    return 1;

}


static int handle_2MB_shadow_pagefault_64(struct guest_info * core, addr_t fault_addr, pf_error_t error_code,
					  pte64_t * shadow_pt, pde64_2MB_t * large_guest_pde, uint32_t inherited_ar);

static int handle_pte_shadow_pagefault_64(struct guest_info * core, addr_t fault_addr, pf_error_t error_code,
					  pte64_t * shadow_pt, pte64_t * guest_pt, uint32_t inherited_ar);

static int handle_pde_shadow_pagefault_64(struct guest_info * core, addr_t fault_addr, pf_error_t error_code,
					  pde64_t * shadow_pd, pde64_t * guest_pd, uint32_t inherited_ar);

static int handle_pdpe_shadow_pagefault_64(struct guest_info * core, addr_t fault_addr, pf_error_t error_code,
					   pdpe64_t * shadow_pdp, pdpe64_t * guest_pdp, uint32_t inherited_ar);


static inline int handle_shadow_pagefault_64(struct guest_info * core, addr_t fault_addr, pf_error_t error_code) {
    pml4e64_t * guest_pml = NULL;
    pml4e64_t * shadow_pml = CR3_TO_PML4E64_VA(core->ctrl_regs.cr3);
    addr_t guest_cr3 = CR3_TO_PML4E64_PA(core->shdw_pg_state.guest_cr3);
    pt_access_status_t guest_pml4e_access;
    pt_access_status_t shadow_pml4e_access;
    pml4e64_t * guest_pml4e = NULL;
    pml4e64_t * shadow_pml4e = (pml4e64_t *)&(shadow_pml[PML4E64_INDEX(fault_addr)]);

    PrintDebug("64 bit Shadow page fault handler: %p\n", (void *)fault_addr);
    PrintDebug("Handling PML fault\n");

    int metaphysical = 0;
    unsigned hugepage_access = 0;
    addr_t pml4e_base_addr = 0;
    uint32_t inherited_ar = PT_USER_MASK | PT_WRITABLE_MASK;

    if (core->n_free_shadow_pages < MIN_FREE_SHADOW_PAGES) {
	shadow_free_some_pages(core);
    }
    shadow_topup_caches(core);

#ifdef SHOW_ALL
    //debug
    burst_64 (core);
#endif

    if (guest_pa_to_host_va(core, guest_cr3, (addr_t*)&guest_pml) == -1) {
	PrintError("Invalid Guest PML4E Address: 0x%p\n",  (void *)guest_cr3);
	return -1;
    } 

    guest_pml4e = (pml4e64_t *)&(guest_pml[PML4E64_INDEX(fault_addr)]);

    pml4e_base_addr = (addr_t)(guest_pml4e->pdp_base_addr);

    PrintDebug("Checking Guest %p\n", (void *)guest_pml);
    // Check the guest page permissions
    guest_pml4e_access = v3_can_access_pml4e64(guest_pml, fault_addr, error_code);

    PrintDebug("Checking shadow %p\n", (void *)shadow_pml);
    // Check the shadow page permissions
    shadow_pml4e_access = v3_can_access_pml4e64(shadow_pml, fault_addr, error_code);

    if (guest_pml4e_access == PT_ACCESS_NOT_PRESENT) {
	error_code.present = 0;
	goto pml4e_error;
    }

    if (guest_pml4e_access == PT_ACCESS_WRITE_ERROR) {
	struct cr0_64 *guest_cr0 = (struct cr0_64*)&(core->shdw_pg_state.guest_cr0);
	if (error_code.user || guest_cr0->wp) {
	    error_code.present = 1;
	    goto pml4e_error;
	}
    }

    if (guest_pml4e_access == PT_ACCESS_USER_ERROR) {
	error_code.present = 1;
	goto pml4e_error;
    }

    
    if (error_code.ifetch == 1 && ((*(uint64_t*)guest_pml4e) & PT64_NX_MASK)) {
	struct efer_64 *guest_efer = (struct efer_64*)&(core->shdw_pg_state.guest_efer);
	if (guest_efer->lma == 1) {
	    goto pml4e_error;
	}
    }	
	
    goto pml4e_noerror;

pml4e_error:

    PrintDebug("Injecting PML Pf to Guest: (Guest Access Error = %d) (SHDW Access Error = %d) (Pf Error Code = %d)\n",
	*(uint_t*)&guest_pml4e_access, *(uint_t*)&shadow_pml4e_access, *(uint_t*)&error_code);
    if (inject_guest_pf(core, fault_addr, error_code) == -1) {
	PrintError("Could Not Inject Guest Page Fault\n");
	return -1;
    }
    return 0;

pml4e_noerror:

    if (guest_pml4e->accessed == 0) {
	guest_pml4e->accessed = 1;		
    }

    inherited_ar &= *(uint64_t*)guest_pml4e;
    PrintDebug("PML: inherited %x\n", inherited_ar);

    pdpe64_t * shadow_pdp = NULL;
    pdpe64_t * guest_pdp = NULL;

    // Get the next shadow page level, allocate if not present

    if (shadow_pml4e_access == PT_ACCESS_NOT_PRESENT) {
	struct shadow_page_cache_data *shdw_page = shadow_page_get_page(core, pml4e_base_addr, 3, metaphysical, 
	    hugepage_access, (addr_t)shadow_pml4e, 0);
	shadow_pdp = (pdpe64_t *)V3_VAddr((void *)shdw_page->page_pa);

	shadow_pml4e->present =1;
	shadow_pml4e->accessed=1;
	shadow_pml4e->writable=1;
	shadow_pml4e->user_page=1;	
    
	shadow_pml4e->pdp_base_addr = PAGE_BASE_ADDR(shdw_page->page_pa);
    } else {
	shadow_pdp = (pdpe64_t *)V3_VAddr((void *)(addr_t)BASE_TO_PAGE_ADDR(shadow_pml4e->pdp_base_addr));
    }

    // Continue processing at the next level

    if (guest_pa_to_host_va(core, BASE_TO_PAGE_ADDR(guest_pml4e->pdp_base_addr), (addr_t *)&guest_pdp) == -1) {
	// Machine check the guest
	PrintError("Invalid Guest PDP Address: 0x%p\n", (void *)BASE_TO_PAGE_ADDR(guest_pml4e->pdp_base_addr));
	v3_raise_exception(core, MC_EXCEPTION);
	return 0;
    }
  
    if (handle_pdpe_shadow_pagefault_64(core, fault_addr, error_code, shadow_pdp, guest_pdp, inherited_ar) == -1) {
	PrintError("Error handling Page fault caused by PDPE\n");
	return -1;
    }

    return 0;
}



// For now we are not going to handle 1 Gigabyte pages
static int handle_pdpe_shadow_pagefault_64(struct guest_info * core, addr_t fault_addr, pf_error_t error_code,
					   pdpe64_t * shadow_pdp, pdpe64_t * guest_pdp, uint32_t inherited_ar) {
    pt_access_status_t guest_pdpe_access;
    pt_access_status_t shadow_pdpe_access;
    pdpe64_t * guest_pdpe = (pdpe64_t *)&(guest_pdp[PDPE64_INDEX(fault_addr)]);
    pdpe64_t * shadow_pdpe = (pdpe64_t *)&(shadow_pdp[PDPE64_INDEX(fault_addr)]);
 
    PrintDebug("Handling PDP fault\n");

    if (fault_addr==0) { 
	PrintDebug("Guest Page Tree for guest virtual address zero fault\n");
	PrintGuestPageTree(core,fault_addr,(addr_t)(core->shdw_pg_state.guest_cr3));
	PrintDebug("Host Page Tree for guest virtual address zero fault\n");
	PrintHostPageTree(core,fault_addr,(addr_t)(core->ctrl_regs.cr3));
    }

    int metaphysical = 0;
    unsigned hugepage_access = 0;

    addr_t pdpe_base_addr = (addr_t)(guest_pdpe->pd_base_addr);

    // Check the guest page permissions
    guest_pdpe_access = v3_can_access_pdpe64(guest_pdp, fault_addr, error_code);

    // Check the shadow page permissions
    shadow_pdpe_access = v3_can_access_pdpe64(shadow_pdp, fault_addr, error_code);

    if (guest_pdpe_access == PT_ACCESS_NOT_PRESENT) {
	PrintDebug("Guest Page Tree for guest virtual address zero fault\n");
	error_code.present = 0;
	goto pdpe_error;
    }

    if (guest_pdpe_access == PT_ACCESS_WRITE_ERROR) {
	struct cr0_64 *guest_cr0 = (struct cr0_64*)&(core->shdw_pg_state.guest_cr0);
	if (error_code.user || guest_cr0->wp) {
	    error_code.present = 1;
	    goto pdpe_error;
	}
    }

    if (guest_pdpe_access == PT_ACCESS_USER_ERROR) {
	error_code.present = 1;
	goto pdpe_error;
    }

    if (error_code.ifetch == 1 && ((*(uint64_t*)guest_pdpe) & PT64_NX_MASK)) {
	struct efer_64 *guest_efer = (struct efer_64*)&(core->shdw_pg_state.guest_efer);
	if (guest_efer->lma == 1) {
	    goto pdpe_error;
	}
    }	
	
    goto pdpe_noerror;

pdpe_error:

    PrintDebug("Injecting PML Pf to Guest: (Guest Access Error = %d) (SHDW Access Error = %d) (Pf Error Code = %d)\n",
	*(uint_t*)&guest_pdpe_access, *(uint_t*)&shadow_pdpe_access, *(uint_t*)&error_code);
    if (inject_guest_pf(core, fault_addr, error_code) == -1) {
	PrintError("Could Not Inject Guest Page Fault\n");
	return -1;
    }
    return 0;

pdpe_noerror:

    if (guest_pdpe->accessed == 0) {
	guest_pdpe->accessed = 1;		
    }

    inherited_ar &= *(uint64_t*)guest_pdpe;
    PrintDebug("PDPE: inherited %x\n", inherited_ar);
  
    pde64_t * shadow_pd = NULL;
    pde64_t * guest_pd = NULL;

    // Get the next shadow page level, allocate if not present

    if (shadow_pdpe_access == PT_ACCESS_NOT_PRESENT) {
	struct shadow_page_cache_data *shdw_page = shadow_page_get_page(core, pdpe_base_addr, 2, metaphysical, 
	    hugepage_access, (addr_t) shadow_pdpe, 0);

	shadow_pd = (pde64_t *)V3_VAddr((void *)shdw_page->page_pa);

	shadow_pdpe->present =1;
	shadow_pdpe->accessed=1;
	shadow_pdpe->writable=1;
	shadow_pdpe->user_page=1;	
    
	shadow_pdpe->pd_base_addr = PAGE_BASE_ADDR(shdw_page->page_pa);
    } else {
	shadow_pd = (pde64_t *)V3_VAddr((void *)(addr_t)BASE_TO_PAGE_ADDR(shadow_pdpe->pd_base_addr));
    }

    // Continue processing at the next level

    if (guest_pa_to_host_va(core, BASE_TO_PAGE_ADDR(guest_pdpe->pd_base_addr), (addr_t *)&guest_pd) == -1) {
	// Machine check the guest
	PrintError("Invalid Guest PTE Address: 0x%p\n", (void *)BASE_TO_PAGE_ADDR(guest_pdpe->pd_base_addr));
	v3_raise_exception(core, MC_EXCEPTION);
	return 0;
    }
  
    if (handle_pde_shadow_pagefault_64(core, fault_addr, error_code, shadow_pd, guest_pd, inherited_ar) == -1) {
	PrintError("Error handling Page fault caused by PDE\n");
	return -1;
    }

    return 0;
}


static int handle_pde_shadow_pagefault_64(struct guest_info * core, addr_t fault_addr, pf_error_t error_code,
					  pde64_t * shadow_pd, pde64_t * guest_pd, uint32_t inherited_ar) {
    pt_access_status_t guest_pde_access;
    pt_access_status_t shadow_pde_access;
    pde64_t * guest_pde = (pde64_t *)&(guest_pd[PDE64_INDEX(fault_addr)]);
    pde64_t * shadow_pde = (pde64_t *)&(shadow_pd[PDE64_INDEX(fault_addr)]);

    PrintDebug("Handling PDE fault\n");

    int metaphysical = 0;
    unsigned hugepage_access = 0;

    addr_t pde_base_addr = (addr_t)(guest_pde->pt_base_addr);

    if (guest_pde->large_page == 1) {
	pde_base_addr = (addr_t)PAGE_BASE_ADDR(BASE_TO_PAGE_ADDR_2MB(((pde64_2MB_t *) guest_pde)->page_base_addr));
	metaphysical = 1;
	hugepage_access = (((pde64_2MB_t*) guest_pde)->writable | (((pde64_2MB_t*) guest_pde)->user_page << 1));
    } 
 
    // Check the guest page permissions
    guest_pde_access = v3_can_access_pde64(guest_pd, fault_addr, error_code);

    // Check the shadow page permissions
    shadow_pde_access = v3_can_access_pde64(shadow_pd, fault_addr, error_code);

    if (guest_pde_access == PT_ACCESS_NOT_PRESENT) {
	error_code.present = 0;
	goto pde_error;
    }

    if (guest_pde_access == PT_ACCESS_WRITE_ERROR) {
	struct cr0_64 *guest_cr0 = (struct cr0_64*)&(core->shdw_pg_state.guest_cr0);
	if (error_code.user || guest_cr0->wp) {
	    error_code.present = 1;
	    goto pde_error;
	}
    }

    if (guest_pde_access == PT_ACCESS_USER_ERROR) {
	error_code.present = 1;
	goto pde_error;
    }

    if (error_code.ifetch == 1 && ((*(uint64_t*)guest_pde) & PT64_NX_MASK)) {
	struct efer_64 *guest_efer = (struct efer_64*)&(core->shdw_pg_state.guest_efer);
	if (guest_efer->lma == 1) {
	    goto pde_error;
	}
    }	
	
    goto pde_noerror;

pde_error:

    PrintDebug("Injecting PML Pf to Guest: (Guest Access Error = %d) (SHDW Access Error = %d) (Pf Error Code = %d)\n",
	*(uint_t*)&guest_pde_access, *(uint_t*)&shadow_pde_access, *(uint_t*)&error_code);
    if (inject_guest_pf(core, fault_addr, error_code) == -1) {
	PrintError("Could Not Inject Guest Page Fault\n");
	return -1;
    }
    return 0;

pde_noerror:

    if (guest_pde->accessed == 0) {
	guest_pde->accessed = 1;		
    }

    inherited_ar &= *(uint64_t*)guest_pde;
    PrintDebug("PDE: inherited %x\n", inherited_ar);
  
    pte64_t * shadow_pt = NULL;
    pte64_t * guest_pt = NULL;

    // Get the next shadow page level, allocate if not present

    if (shadow_pde_access == PT_ACCESS_NOT_PRESENT) {
	struct shadow_page_cache_data *shdw_page = shadow_page_get_page(core, pde_base_addr, 1, metaphysical, 
	    hugepage_access, (addr_t) shadow_pde, 0);
	shadow_pt = (pte64_t *)V3_VAddr((void *)shdw_page->page_pa);

	PrintDebug("Creating new shadow PT: %p\n", shadow_pt);

	shadow_pde->present =1;
	shadow_pde->accessed=1;
	shadow_pde->writable=1;
	shadow_pde->user_page=1;	

	shadow_pde->pt_base_addr = PAGE_BASE_ADDR(shdw_page->page_pa);
    } else {
	shadow_pt = (pte64_t *)V3_VAddr((void *)BASE_TO_PAGE_ADDR(shadow_pde->pt_base_addr));
    }

    // Continue processing at the next level
    if (guest_pde->large_page == 0) {
	if (guest_pa_to_host_va(core, BASE_TO_PAGE_ADDR(guest_pde->pt_base_addr), (addr_t *)&guest_pt) == -1) {
	    // Machine check the guest
	    PrintError("Invalid Guest PTE Address: 0x%p\n", (void *)BASE_TO_PAGE_ADDR(guest_pde->pt_base_addr));
	    v3_raise_exception(core, MC_EXCEPTION);
	    return 0;
	}
    
	if (handle_pte_shadow_pagefault_64(core, fault_addr, error_code, shadow_pt, guest_pt, inherited_ar) == -1) {
	    PrintError("Error handling Page fault caused by PDE\n");
	    return -1;
	}

    } else {
	if (handle_2MB_shadow_pagefault_64(core, fault_addr, error_code, shadow_pt, 
		(pde64_2MB_t *)guest_pde, inherited_ar) == -1) {
	    PrintError("Error handling large pagefault\n");
	    return -1;
	} 
    }

    return 0;
}


static int handle_pte_shadow_pagefault_64(struct guest_info * core, addr_t fault_addr, pf_error_t error_code,
					  pte64_t * shadow_pt, pte64_t * guest_pt, uint32_t inherited_ar)  {
    pt_access_status_t guest_pte_access;
    pt_access_status_t shadow_pte_access;
    pte64_t * guest_pte = (pte64_t *)&(guest_pt[PTE64_INDEX(fault_addr)]);;
    pte64_t * shadow_pte = (pte64_t *)&(shadow_pt[PTE64_INDEX(fault_addr)]);
    addr_t guest_pa = BASE_TO_PAGE_ADDR((addr_t)(guest_pte->page_base_addr)) +  PAGE_OFFSET(fault_addr);
    //  struct shadow_page_state * state = &(core->shdw_pg_state);

    PrintDebug("Handling PTE fault\n");

    struct v3_mem_region * shdw_reg =  v3_get_mem_region(core->vm_info, core->vcpu_id, guest_pa);



    if (shdw_reg == NULL) {
	// Inject a machine check in the guest
	PrintError("Invalid Guest Address in page table (0x%p)\n", (void *)guest_pa);
	v3_raise_exception(core, MC_EXCEPTION);
	return 0;
    }

    // Check the guest page permissions
    guest_pte_access = v3_can_access_pte64(guest_pt, fault_addr, error_code);

    // Check the shadow page permissions
    shadow_pte_access = v3_can_access_pte64(shadow_pt, fault_addr, error_code);

    if (guest_pte_access == PT_ACCESS_NOT_PRESENT) {
	error_code.present = 0;
	goto pte_error;
    }

    if (guest_pte_access == PT_ACCESS_WRITE_ERROR) {
	struct cr0_64 *guest_cr0 = (struct cr0_64*)&(core->shdw_pg_state.guest_cr0);
	if (error_code.user || guest_cr0->wp) {
	    error_code.present = 1;
	    goto pte_error;
	}
    }

    if (guest_pte_access == PT_ACCESS_USER_ERROR) {
	error_code.present = 1;
	goto pte_error;
    }

    if (error_code.ifetch == 1 && ((*(uint64_t*)guest_pte) & PT64_NX_MASK)) {
	struct efer_64 *guest_efer = (struct efer_64*)&(core->shdw_pg_state.guest_efer);
	if (guest_efer->lma == 1) {
	    goto pte_error;
	}
    }	
	
    goto pte_noerror;

pte_error:

    PrintDebug("Injecting PML Pf to Guest: (Guest Access Error = %d) (SHDW Access Error = %d) (Pf Error Code = %d)\n",
	*(uint_t*)&guest_pte_access, *(uint_t*)&shadow_pte_access, *(uint_t*)&error_code);
    if (inject_guest_pf(core, fault_addr, error_code) == -1) {
	PrintError("Could Not Inject Guest Page Fault\n");
	return -1;
    }
    return 0;

pte_noerror:

    if (guest_pte->accessed == 0) {
	guest_pte->accessed = 1;		
    }

    inherited_ar &= *(uint64_t*)guest_pte;
    PrintDebug("PTE: inherited %x\n", inherited_ar);

    if (shadow_pte_access == PT_ACCESS_NOT_PRESENT) {
	// Page Table Entry Not Present
	PrintDebug("guest_pa =%p\n", (void *)guest_pa);

	if ((shdw_reg->host_type == SHDW_REGION_ALLOCATED) ||
	    (shdw_reg->host_type == SHDW_REGION_WRITE_HOOK)) {

	    int inherited_ar_user = ((inherited_ar & PT_USER_MASK) == PT_USER_MASK) ? 1 : 0;
	    int inherited_ar_writable = ((inherited_ar & PT_WRITABLE_MASK) == PT_WRITABLE_MASK) ? 1 : 0;

	    addr_t shadow_pa = v3_get_shadow_addr(shdw_reg, core->vcpu_id, guest_pa);
      
	    shadow_pte->page_base_addr = PAGE_BASE_ADDR(shadow_pa);
      
	    shadow_pte->present = guest_pte->present;

	    shadow_pte->user_page = inherited_ar_user;
    	    PrintDebug("PTE: inheritied shdow_pte_user %d, guest_pte_user %d\n", shadow_pte->user_page, guest_pte->user_page);
      
	    //set according to VMM policy
	    shadow_pte->global_page = guest_pte->global_page;
	    //
      
	    shadow_pte->accessed = guest_pte->accessed;
	    shadow_pte->dirty = guest_pte->dirty;
	    shadow_pte->writable = inherited_ar_writable;

    	    PrintDebug("PTE: inheritied shdow_pte_writable %d, guest_pte_writable %d\n", shadow_pte->writable, guest_pte->writable);


	    // Write hooks trump all, and are set Read Only
	    if (shdw_reg->host_type == SHDW_REGION_WRITE_HOOK) {
		shadow_pte->writable = 0;
	    }

	    shadow_pte->vmm_info = (inherited_ar_writable << 1) | (inherited_ar_user << 2);

	    if (inherited_ar_writable & guest_pte->writable) {

		struct shadow_page_cache_data *shadow;
		shadow = shadow_page_lookup_page(core, PAGE_BASE_ADDR(guest_pa), 0);
		if (shadow) {
		    if (shadow_pte->writable) {
			shadow_pte->writable = 0;
		    }
		}
	    }

	    PrintDebug("PTE: Updated Shadow Present %d, Write %d, User %d, Dirty %d, Accessed %d, Global %d\n",
			shadow_pte->present, shadow_pte->writable, shadow_pte->user_page, shadow_pte->dirty, shadow_pte->accessed, 
			shadow_pte->global_page);	
	    PrintDebug("PTE: Shadow %p\n", (void*)*((addr_t*)shadow_pte));
	    rmap_add(core, (addr_t)shadow_pte);

	} else {
	    // Page fault handled by hook functions

	    if (v3_handle_mem_full_hook(core, fault_addr, guest_pa, shdw_reg, error_code) == -1) {
		PrintError("Special Page fault handler returned error for address: %p\n",  (void *)fault_addr);
		return -1;
	    }
	}
    } else if (shadow_pte_access == PT_ACCESS_WRITE_ERROR) {
	guest_pte->dirty = 1;

	if (shdw_reg->host_type == SHDW_REGION_WRITE_HOOK) {
	    if (v3_handle_mem_wr_hook(core, fault_addr, guest_pa, shdw_reg, error_code) == -1) {
		PrintError("Special Page fault handler returned error for address: %p\n",  (void *)fault_addr);
		return -1;
	    }
	} else {
	    PrintDebug("Shadow PTE Write Error\n");
	    shadow_pte->writable = guest_pte->writable;
	}

	return 0;
    }

    int fixed = 0;
    int write_pt = 0;
    uint_t vmm_info = shadow_pte->vmm_info;

    if (error_code.write == 1) {
	fixed = fix_write_pf_64(core, shadow_pte, guest_pte, (int)error_code.user, &write_pt, PAGE_BASE_ADDR(guest_pa), vmm_info);
    } else {
        fixed = fix_read_pf_64(shadow_pte, vmm_info);
    }

    PrintDebug("PTE: Fixed %d Write_Pt %d\n", fixed, write_pt);
    PrintDebug("PTE: Shadow Present %d, Write %d, User %d, Dirty %d, Accessed %d, Global %d\n", 
		shadow_pte->present, shadow_pte->writable, shadow_pte->user_page, shadow_pte->dirty, shadow_pte->accessed, 
		shadow_pte->global_page);
    PrintDebug("PTE: Shadow %p\n", (void*)*((addr_t*)shadow_pte));

    if (shdw_reg->host_type == SHDW_REGION_ALLOCATED && write_pt == 1) {
	PrintDebug("PTE: Emul\n");
	if (v3_handle_mem_wr_hook(core, fault_addr, guest_pa, shdw_reg, error_code) == -1) {
	    shadow_unprotect_page(core, (addr_t)guest_pte->page_base_addr);
	}
    }

    PrintDebug("PTE: PTE end\n");
    PrintDebug("PTE: Updated Shadow Present %d, Write %d, User %d, Dirty %d, Accessed %d, Global %d\n", 
		shadow_pte->present, shadow_pte->writable, shadow_pte->user_page, shadow_pte->dirty, shadow_pte->accessed, 
		shadow_pte->global_page);
    PrintDebug("PTE: Updated Shadow %p\n", (void*)*((addr_t*)shadow_pte));
    PrintDebug("PTE: Guest PA %p, Host PA %p\n",  (void*)guest_pa, (void*)BASE_TO_PAGE_ADDR(shadow_pte->page_base_addr));

    return 0;
}



static int handle_2MB_shadow_pagefault_64(struct guest_info * core, 
					  addr_t fault_addr, pf_error_t error_code, 
					  pte64_t * shadow_pt, pde64_2MB_t * large_guest_pde, uint32_t inherited_ar) 
{
    pt_access_status_t shadow_pte_access = v3_can_access_pte64(shadow_pt, fault_addr, error_code);
    pte64_t * shadow_pte = (pte64_t *)&(shadow_pt[PTE64_INDEX(fault_addr)]);
    addr_t guest_fault_pa = BASE_TO_PAGE_ADDR_2MB(large_guest_pde->page_base_addr) + PAGE_OFFSET_2MB(fault_addr);
    //  struct shadow_page_state * state = &(core->shdw_pg_state);

    PrintDebug("Handling 2MB fault (guest_fault_pa=%p) (error_code=%x)\n", (void *)guest_fault_pa, *(uint_t*)&error_code);
    PrintDebug("ShadowPT=%p, LargeGuestPDE=%p\n", shadow_pt, large_guest_pde);

    struct v3_mem_region * shdw_reg = v3_get_mem_region(core->vm_info, core->vcpu_id, guest_fault_pa);

    int fixed = 0;
    int write_pt = 0;
    addr_t guest_fn;
 
    if (shdw_reg == NULL) {
	// Inject a machine check in the guest
	PrintError("Invalid Guest Address in page table (0x%p)\n", (void *)guest_fault_pa);
	v3_raise_exception(core, MC_EXCEPTION);
	return 0;
    }

  
    if (shadow_pte_access == PT_ACCESS_NOT_PRESENT) {
	// Get the guest physical address of the fault
	int inherited_ar_user = ((inherited_ar & PT_USER_MASK) == PT_USER_MASK) ? 1 : 0;
	int inherited_ar_writable = ((inherited_ar & PT_WRITABLE_MASK) == PT_WRITABLE_MASK) ? 1 : 0;

	if ((shdw_reg->host_type == SHDW_REGION_ALLOCATED) || 
	    (shdw_reg->host_type == SHDW_REGION_WRITE_HOOK)) {
	    addr_t shadow_pa = v3_get_shadow_addr(shdw_reg, core->vcpu_id, guest_fault_pa);

	    shadow_pte->page_base_addr = PAGE_BASE_ADDR(shadow_pa);

    	    PrintDebug("LPTE: inherited_ar %d\n", inherited_ar);
	    shadow_pte->user_page = inherited_ar_user;
    	    PrintDebug("LPTE: inheritied shdow_pte_user %d\n", shadow_pte->user_page);

	    shadow_pte->present = large_guest_pde->present;
	    shadow_pte->dirty = large_guest_pde->dirty;

	    shadow_pte->present = 1;

	    /* We are assuming that the PDE entry has precedence
	     * so the Shadow PDE will mirror the guest PDE settings, 
	     * and we don't have to worry about them here
	     * Allow everything
	     */
	    shadow_pte->user_page = 1;

	    if (shdw_reg->host_type == SHDW_REGION_WRITE_HOOK) {
		shadow_pte->writable = 0;
	    } else {
		shadow_pte->writable = inherited_ar_writable;
    	       PrintDebug("LPTE: inheritied shdow_pte_writable %d, PT_WRITABLE_MASK %p, inherited_ar & PT_WRITABLE_MASK %p\n", 
			   	shadow_pte->writable, (void*)PT_WRITABLE_MASK, (void*)(inherited_ar & PT_WRITABLE_MASK));
	    }

	    //set according to VMM policy
	    shadow_pte->global_page = large_guest_pde->global_page;
	    //

	    shadow_pte->vmm_info = (inherited_ar_writable <<1) | (inherited_ar_user << 2);

	    if (large_guest_pde->writable) {
		struct shadow_page_cache_data *shadow;
		shadow = shadow_page_lookup_page(core, PAGE_BASE_ADDR(guest_fault_pa), 0);

		if (shadow) {
		    if (shadow_pte->writable) {
			shadow_pte->writable = 0;
		    }
		}	
	    }

	    rmap_add(core, (addr_t)shadow_pte);
      
	} else {
	    if (v3_handle_mem_full_hook(core, fault_addr, guest_fault_pa, shdw_reg, error_code) == -1) {
		PrintError("Special Page Fault handler returned error for address: %p\n", (void *)fault_addr);
		return -1;
	    }
	}
    } else if (shadow_pte_access == PT_ACCESS_WRITE_ERROR) {

	if (shdw_reg->host_type == SHDW_REGION_WRITE_HOOK) {

	    if (v3_handle_mem_wr_hook(core, fault_addr, guest_fault_pa, shdw_reg, error_code) == -1) {
		PrintError("Special Page Fault handler returned error for address: %p\n", (void *)fault_addr);
		return -1;
	    }
	}
	
    }

    struct cr3_64 *guest_cr3 = (struct cr3_64 *)&(core->shdw_pg_state.guest_cr3);
    guest_fn = (addr_t)guest_cr3->pml4t_base_addr;
    uint_t vmm_info = shadow_pte->vmm_info;

    if (error_code.write == 1) {
	fixed = fix_write_pf_64(core, shadow_pte, (pte64_t *)large_guest_pde, (int) error_code.user, 
		&write_pt, PAGE_BASE_ADDR(guest_fault_pa), vmm_info);
    } else {
        fixed = fix_read_pf_64(shadow_pte, vmm_info);
    }

    PrintDebug("LPTE: Fixed %d, Write_Pt %d\n", fixed, write_pt);
    PrintDebug("LPTE: Shadow Present %d, Write %d, User %d, Dirty %d, Accessed %d, Global %d\n",
		shadow_pte->present, shadow_pte->writable, shadow_pte->user_page, shadow_pte->dirty,
		shadow_pte->accessed, shadow_pte->global_page);
    PrintDebug("LPTE: Shadow %p\n", (void*)*((addr_t*)shadow_pte));

    if (shdw_reg->host_type == SHDW_REGION_ALLOCATED && write_pt == 1){
	if (v3_handle_mem_wr_hook(core, fault_addr, guest_fault_pa, shdw_reg, error_code) == -1) {
	    shadow_unprotect_page(core, (addr_t)PAGE_BASE_ADDR(guest_fault_pa));
	}
    }

    PrintDebug("Updated LPTE: Shadow Present %d, Write %d, User %d, Dirty %d, Accessed %d, Global %d\n",
		shadow_pte->present, shadow_pte->writable, shadow_pte->user_page, shadow_pte->dirty, shadow_pte->accessed, 
		shadow_pte->global_page);
    PrintDebug("LPTE: Updated Shadow %p\n", (void*)*((addr_t*)shadow_pte));
    PrintDebug("LPTE: Guest PA %p Host PA %p\n", 
		(void*)BASE_TO_PAGE_ADDR(PAGE_BASE_ADDR(guest_fault_pa)), 
		(void*)BASE_TO_PAGE_ADDR(shadow_pte->page_base_addr));
    PrintDebug("Returning from Large Page Fault Handler\n");

    //  PrintHostPageTree(core, fault_addr, info->ctrl_regs.cr3);
    PrintDebug("Returning from large page fault handler\n");
    return 0;
}




static int invalidation_cb_64(struct guest_info * core, page_type_t type, 
			      addr_t vaddr, addr_t page_ptr, addr_t page_pa, 
			      void * private_data) {

    switch (type) {
	case PAGE_PML464:
	    {    
		pml4e64_t * pml = (pml4e64_t *)page_ptr;

		if (pml[PML4E64_INDEX(vaddr)].present == 0) {
		    return 1;
		}
		return 0;
	    }
	case PAGE_PDP64:
	    {
		pdpe64_t * pdp = (pdpe64_t *)page_ptr;
		pdpe64_t * pdpe = &(pdp[PDPE64_INDEX(vaddr)]);

		if (pdpe->present == 0) {
		    return 1;
		}
     
		if (pdpe->vmm_info == V3_LARGE_PG) {
		    PrintError("1 Gigabyte pages not supported\n");
		    return -1;

		    pdpe->present = 0;
		    return 1;
		}

		return 0;
	    }
	case PAGE_PD64:
	    {
		pde64_t * pd = (pde64_t *)page_ptr;
		pde64_t * pde = &(pd[PDE64_INDEX(vaddr)]);

		if (pde->present == 0) {
		    return 1;
		}
      
		if (pde->vmm_info == V3_LARGE_PG) {
		    pde->present = 0;
		    return 1;
		}

		return 0;
	    }
	case PAGE_PT64:
	    {
		pte64_t * pt = (pte64_t *)page_ptr;

		pt[PTE64_INDEX(vaddr)].present = 0;

		return 1;
	    }
	default:
	    PrintError("Invalid Page Type\n");
	    return -1;

    }

    // should not get here
    PrintError("Should not get here....\n");
    return -1;
}


static inline int handle_shadow_invlpg_64(struct guest_info * core, addr_t vaddr) {
    PrintDebug("INVLPG64 - %p\n",(void*)vaddr);

    int ret =  v3_drill_host_pt_64(core, core->ctrl_regs.cr3, vaddr, invalidation_cb_64, NULL);
    if (ret == -1) {
	PrintError("Page table drill returned error.... \n");
	PrintHostPageTree(core, vaddr, core->ctrl_regs.cr3);
    }

    return (ret == -1) ? -1 : 0; 
}

#endif 
