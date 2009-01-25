/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmm_shadow_paging.h>


#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_ctrl_regs.h>

#include <palacios/vmm_hashtable.h>

#ifndef DEBUG_SHADOW_PAGING
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


/*** 
 ***  There be dragons
 ***/


struct guest_table {
  addr_t cr3;
  struct list_head link;
};


struct backptr {
  addr_t ptr;
  struct list_head link;
};


struct shadow_page_data {
  addr_t ptr;
  addr_t guest_addr; 

  struct list_head backptrs;
  struct list_head guest_tables;
};




DEFINE_HASHTABLE_INSERT(add_cr3_to_cache, addr_t, struct hashtable *);
DEFINE_HASHTABLE_SEARCH(find_cr3_in_cache, addr_t, struct hashtable *);
DEFINE_HASHTABLE_REMOVE(del_cr3_from_cache, addr_t, struct hashtable *, 0);


DEFINE_HASHTABLE_INSERT(add_pte_map, addr_t, addr_t);
DEFINE_HASHTABLE_SEARCH(find_pte_map, addr_t, addr_t);
DEFINE_HASHTABLE_REMOVE(del_pte_map, addr_t, addr_t, 0);



static uint_t pte_hash_fn(addr_t key) {
  return hash_long(key, 32);
}

static int pte_equals(addr_t key1, addr_t key2) {
  return (key1 == key2);
}

static uint_t cr3_hash_fn(addr_t key) {
  return hash_long(key, 32);
}

static int cr3_equals(addr_t key1, addr_t key2) {
  return (key1 == key2);
}



static int activate_shadow_pt_32(struct guest_info * info);
static int activate_shadow_pt_32pae(struct guest_info * info);
static int activate_shadow_pt_64(struct guest_info * info);


static int handle_shadow_pagefault_32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code);
static int handle_shadow_pagefault_32pae(struct guest_info * info, addr_t fault_addr, pf_error_t error_code);
static int handle_shadow_pagefault_64(struct guest_info * info, addr_t fault_addr, pf_error_t error_code);


static int cache_page_tables_32(struct guest_info * info, addr_t pde);
static int cache_page_tables_64(struct guest_info * info, addr_t pde);

int v3_init_shadow_page_state(struct guest_info * info) {
  struct shadow_page_state * state = &(info->shdw_pg_state);
  
  state->guest_cr3 = 0;
  state->guest_cr0 = 0;

  state->cr3_cache = create_hashtable(0, &cr3_hash_fn, &cr3_equals);

  state->cached_cr3 = 0;
  state->cached_ptes = NULL;

  return 0;
}





int v3_cache_page_tables(struct guest_info * info, addr_t cr3) {
  switch(v3_get_cpu_mode(info)) {
  case PROTECTED:
    return cache_page_tables_32(info, CR3_TO_PDE32_PA(cr3));
  default:
    return -1;
  }
}

static int cache_page_tables_32(struct guest_info * info, addr_t pde) {
  struct shadow_page_state * state = &(info->shdw_pg_state);
  addr_t pde_host_addr;
  pde32_t * tmp_pde;
  struct hashtable * pte_cache = NULL;
  int i = 0;

  if (pde == state->cached_cr3) {
    return 1;
  }

  if (state->cached_ptes != NULL) {
    hashtable_destroy(state->cached_ptes, 0, 0);
    state->cached_ptes = NULL;
  }

  state->cached_cr3 = pde;

  pte_cache = create_hashtable(0, &pte_hash_fn, &pte_equals);
  state->cached_ptes = pte_cache;

  if (guest_pa_to_host_va(info, pde, &pde_host_addr) == -1) {
    PrintError("Could not lookup host address of guest PDE\n");
    return -1;
  }

  tmp_pde = (pde32_t *)pde_host_addr;

  add_pte_map(pte_cache, pde, pde_host_addr);


  for (i = 0; i < MAX_PDE32_ENTRIES; i++) {
    if ((tmp_pde[i].present) && (tmp_pde[i].large_page == 0)) {
      addr_t pte_host_addr;

      if (guest_pa_to_host_va(info, (addr_t)(BASE_TO_PAGE_ADDR(tmp_pde[i].pt_base_addr)), &pte_host_addr) == -1) {
	PrintError("Could not lookup host address of guest PDE\n");
	return -1;
      }

      add_pte_map(pte_cache, (addr_t)(BASE_TO_PAGE_ADDR(tmp_pde[i].pt_base_addr)), pte_host_addr); 
    }
  }

  return 0;

}


static int cache_page_tables_64(struct guest_info * info, addr_t pde) {
  return -1;
}


int v3_replace_shdw_page32(struct guest_info * info, addr_t location, pte32_t * new_page, pte32_t * old_page) {
  pde32_t * shadow_pd = (pde32_t *)CR3_TO_PDE32_VA(info->ctrl_regs.cr3);
  pde32_t * shadow_pde =  (pde32_t *)&(shadow_pd[PDE32_INDEX(location)]);

  if (shadow_pde->large_page == 0) {
    pte32_t * shadow_pt = (pte32_t *)(addr_t)BASE_TO_PAGE_ADDR(shadow_pde->pt_base_addr);
    pte32_t * shadow_pte = (pte32_t *)&(shadow_pt[PTE32_INDEX(location)]);

    //if (shadow_pte->present == 1) {
    *(uint_t *)old_page = *(uint_t *)shadow_pte;
    //}

    *(uint_t *)shadow_pte = *(uint_t *)new_page;

  } else {
    // currently unhandled
    PrintError("Replacing large shadow pages not implemented\n");
    return -1;
  }
  
  return 0;
}





// We assume that shdw_pg_state.guest_cr3 is pointing to the page tables we want to activate
// We also assume that the CPU mode has not changed during this page table transition
static int activate_shadow_pt_32(struct guest_info * info) {
  struct cr3_32 * shadow_cr3 = (struct cr3_32 *)&(info->ctrl_regs.cr3);
  struct cr3_32 * guest_cr3 = (struct cr3_32 *)&(info->shdw_pg_state.guest_cr3);
  int cached = 0;
  
  // Check if shadow page tables are in the cache
  cached = cache_page_tables_32(info, CR3_TO_PDE32_PA(*(addr_t *)guest_cr3));
  
  if (cached == -1) {
    PrintError("CR3 Cache failed\n");
    return -1;
  } else if (cached == 0) {
    addr_t shadow_pt;
    
    PrintDebug("New CR3 is different - flushing shadow page table %p\n", shadow_cr3 );
    delete_page_tables_32(CR3_TO_PDE32_VA(*(uint_t*)shadow_cr3));
    
    shadow_pt = v3_create_new_shadow_pt();
    
    shadow_cr3->pdt_base_addr = (addr_t)V3_PAddr((void *)(addr_t)PAGE_BASE_ADDR(shadow_pt));
    PrintDebug( "Created new shadow page table %p\n", (void *)(addr_t)shadow_cr3->pdt_base_addr );
  } else {
    PrintDebug("Reusing cached shadow Page table\n");
  }
  
  shadow_cr3->pwt = guest_cr3->pwt;
  shadow_cr3->pcd = guest_cr3->pcd;
  
  return 0;
}

static int activate_shadow_pt_32pae(struct guest_info * info) {
  PrintError("Activating 32 bit PAE page tables not implemented\n");
  return -1;
}

static int activate_shadow_pt_64_cb(page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data) {
  PrintDebug("CB: Page: %p->%p (host_ptr=%p), Type: %s\n", (void *)vaddr, (void *)page_pa, (void *)page_ptr, v3_page_type_to_str(type));
  return 0;
}


static int activate_shadow_pt_64(struct guest_info * info) {
  //  struct cr3_64 * shadow_cr3 = (struct cr3_64 *)&(info->ctrl_regs.cr3);
  struct cr3_64 * guest_cr3 = (struct cr3_64 *)&(info->shdw_pg_state.guest_cr3);
  int cached = 0;
  
  v3_walk_guest_pt_64(info, info->shdw_pg_state.guest_cr3, activate_shadow_pt_64_cb, NULL);

  

  return -1;


  // Check if shadow page tables are in the cache
  cached = cache_page_tables_64(info, CR3_TO_PDE32_PA(*(addr_t *)guest_cr3));
	      /*  
  if (cached == -1) {
    PrintError("CR3 Cache failed\n");
    return -1;
  } else if (cached == 0) {
    addr_t shadow_pt;
    
    PrintDebug("New CR3 is different - flushing shadow page table %p\n", shadow_cr3 );
    delete_page_tables_32(CR3_TO_PDE32_VA(*(uint_t*)shadow_cr3));
    
    shadow_pt = v3_create_new_shadow_pt();
    
    shadow_cr3->pml4t_base_addr = (addr_t)V3_PAddr((void *)(addr_t)PAGE_BASE_ADDR(shadow_pt));
    PrintDebug( "Created new shadow page table %p\n", (void *)(addr_t)shadow_cr3->pml4t_base_addr );
  } else {
    PrintDebug("Reusing cached shadow Page table\n");
  }
  
  shadow_cr3->pwt = guest_cr3->pwt;
  shadow_cr3->pcd = guest_cr3->pcd;
  
  return 0;
		      */
}


// Reads the guest CR3 register
// creates new shadow page tables
// updates the shadow CR3 register to point to the new pts
int v3_activate_shadow_pt(struct guest_info * info) {
  switch (info->cpu_mode) {

  case PROTECTED:
    return activate_shadow_pt_32(info);
  case PROTECTED_PAE:
    return activate_shadow_pt_32pae(info);
  case LONG:
  case LONG_32_COMPAT:
  case LONG_16_COMPAT:
    return activate_shadow_pt_64(info);
  default:
    PrintError("Invalid CPU mode: %d\n", info->cpu_mode);
    return -1;
  }

  return 0;
}


int v3_activate_passthrough_pt(struct guest_info * info) {
  // For now... But we need to change this....
  // As soon as shadow paging becomes active the passthrough tables are hosed
  // So this will cause chaos if it is called at that time

  info->ctrl_regs.cr3 = *(addr_t*)&(info->direct_map_pt);
  //PrintError("Activate Passthrough Page tables not implemented\n");
  return 0;
}



int v3_handle_shadow_pagefault(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  
  if (info->mem_mode == PHYSICAL_MEM) {
    // If paging is not turned on we need to handle the special cases

#ifdef DEBUG_SHADOW_PAGING
    PrintHostPageTree(info->cpu_mode, fault_addr, info->ctrl_regs.cr3);
    PrintGuestPageTree(info, fault_addr, info->shdw_pg_state.guest_cr3);
#endif

    return handle_special_page_fault(info, fault_addr, fault_addr, error_code);
  } else if (info->mem_mode == VIRTUAL_MEM) {

    switch (info->cpu_mode) {
    case PROTECTED:
      return handle_shadow_pagefault_32(info, fault_addr, error_code);
      break;
    case PROTECTED_PAE:
      return handle_shadow_pagefault_32pae(info, fault_addr, error_code);
    case LONG:
      return handle_shadow_pagefault_64(info, fault_addr, error_code);
      break;
    default:
      PrintError("Unhandled CPU Mode\n");
      return -1;
    }
  } else {
    PrintError("Invalid Memory mode\n");
    return -1;
  }
}

addr_t v3_create_new_shadow_pt() {
  void * host_pde = 0;

  host_pde = V3_VAddr(V3_AllocPages(1));
  memset(host_pde, 0, PAGE_SIZE);

  return (addr_t)host_pde;
}


static void inject_guest_pf(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  info->ctrl_regs.cr2 = fault_addr;
  v3_raise_exception_with_error(info, PF_EXCEPTION, *(uint_t *)&error_code);
}


static int is_guest_pf(pt_access_status_t guest_access, pt_access_status_t shadow_access) {
  /* basically the reasoning is that there can be multiple reasons for a page fault:
     If there is a permissions failure for a page present in the guest _BUT_ 
     the reason for the fault was that the page is not present in the shadow, 
     _THEN_ we have to map the shadow page in and reexecute, this will generate 
     a permissions fault which is _THEN_ valid to send to the guest
     _UNLESS_ both the guest and shadow have marked the page as not present

     whew...
  */
  if (guest_access != PT_ACCESS_OK) {
    // Guest Access Error
    
    if ((shadow_access != PT_ACCESS_NOT_PRESENT) &&
	(guest_access != PT_ACCESS_NOT_PRESENT)) {
      // aka (guest permission error)
      return 1;
    }

    if ((shadow_access == PT_ACCESS_NOT_PRESENT) &&
	(guest_access == PT_ACCESS_NOT_PRESENT)) {      
      // Page tables completely blank, handle guest first
      return 1;
    }

    // Otherwise we'll handle the guest fault later...?
  }

  return 0;
}




/* 
 * *
 * * 
 * * 64 bit Page table fault handlers
 * *
 * *
 */

static int handle_shadow_pagefault_64(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  pt_access_status_t guest_access;
  pt_access_status_t shadow_access;
  int ret; 
  PrintDebug("64 bit shadow page fault\n");

  ret = v3_check_guest_pt_32(info, info->shdw_pg_state.guest_cr3, fault_addr, error_code, &guest_access);

  PrintDebug("Guest Access Check: %d (access=%d)\n", ret, guest_access);

  ret = v3_check_host_pt_32(info->ctrl_regs.cr3, fault_addr, error_code, &shadow_access);

  PrintDebug("Shadow Access Check: %d (access=%d)\n", ret, shadow_access);
  

  PrintError("64 bit shadow paging not implemented\n");
  return -1;
}


/* 
 * *
 * * 
 * * 32 bit PAE  Page table fault handlers
 * *
 * *
 */

static int handle_shadow_pagefault_32pae(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  PrintError("32 bit PAE shadow paging not implemented\n");
  return -1;
}







/* 
 * *
 * * 
 * * 32 bit Page table fault handlers
 * *
 * *
 */
static int handle_large_pagefault_32(struct guest_info * info, 
				    addr_t fault_addr, pf_error_t error_code, 
				     pte32_t * shadow_pt, pde32_4MB_t * large_guest_pde);

static int handle_shadow_pte32_fault(struct guest_info * info, 
				     addr_t fault_addr, 
				     pf_error_t error_code,
				     pte32_t * shadow_pt, 
				     pte32_t * guest_pt);


static int handle_shadow_pagefault_32(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
  pde32_t * guest_pd = NULL;
  pde32_t * shadow_pd = CR3_TO_PDE32_VA(info->ctrl_regs.cr3);
  addr_t guest_cr3 = CR3_TO_PDE32_PA(info->shdw_pg_state.guest_cr3);
  pt_access_status_t guest_pde_access;
  pt_access_status_t shadow_pde_access;
  pde32_t * guest_pde = NULL;
  pde32_t * shadow_pde = (pde32_t *)&(shadow_pd[PDE32_INDEX(fault_addr)]);

  PrintDebug("Shadow page fault handler: %p\n", (void*) fault_addr );

  if (guest_pa_to_host_va(info, guest_cr3, (addr_t*)&guest_pd) == -1) {
    PrintError("Invalid Guest PDE Address: 0x%p\n",  (void *)guest_cr3);
    return -1;
  } 

  guest_pde = (pde32_t *)&(guest_pd[PDE32_INDEX(fault_addr)]);


  // Check the guest page permissions
  guest_pde_access = v3_can_access_pde32(guest_pd, fault_addr, error_code);

  // Check the shadow page permissions
  shadow_pde_access = v3_can_access_pde32(shadow_pd, fault_addr, error_code);
  
  /* Was the page fault caused by the Guest's page tables? */
  if (is_guest_pf(guest_pde_access, shadow_pde_access) == 1) {
    PrintDebug("Injecting PDE pf to guest: (guest access error=%d) (pf error code=%d)\n", 
	       *(uint_t *)&guest_pde_access, *(uint_t *)&error_code);
    inject_guest_pf(info, fault_addr, error_code);
    return 0;
  }

  
  if (shadow_pde_access == PT_ACCESS_NOT_PRESENT) 
    {
      pte32_t * shadow_pt =  (pte32_t *)v3_create_new_shadow_pt();

      shadow_pde->present = 1;
      shadow_pde->user_page = guest_pde->user_page;
      //    shadow_pde->large_page = guest_pde->large_page;
      shadow_pde->large_page = 0;
      

      // VMM Specific options
      shadow_pde->write_through = 0;
      shadow_pde->cache_disable = 0;
      shadow_pde->global_page = 0;
      //
      
      guest_pde->accessed = 1;
      
      shadow_pde->pt_base_addr = PAGE_BASE_ADDR((addr_t)V3_PAddr(shadow_pt));
      
      if (guest_pde->large_page == 0) {
	shadow_pde->writable = guest_pde->writable;
      } else {
	// ??  What if guest pde is dirty a this point?
	((pde32_4MB_t *)guest_pde)->dirty = 0;
	shadow_pde->writable = 0;
      }
    }
  else if (shadow_pde_access == PT_ACCESS_OK) 
    {
      //
      // PTE fault
      //
      pte32_t * shadow_pt = (pte32_t *)V3_VAddr( (void*)(addr_t) BASE_TO_PAGE_ADDR(shadow_pde->pt_base_addr) );

      if (guest_pde->large_page == 0) {
	pte32_t * guest_pt = NULL;
	if (guest_pa_to_host_va(info, BASE_TO_PAGE_ADDR(guest_pde->pt_base_addr), (addr_t*)&guest_pt) == -1) {
	  // Machine check the guest
	  PrintDebug("Invalid Guest PTE Address: 0x%p\n", (void *)BASE_TO_PAGE_ADDR(guest_pde->pt_base_addr));
	  v3_raise_exception(info, MC_EXCEPTION);
	  return 0;
	}
	
	if (handle_shadow_pte32_fault(info, fault_addr, error_code, shadow_pt, guest_pt)  == -1) {
	  PrintError("Error handling Page fault caused by PTE\n");
	  return -1;
	}
      } else if (guest_pde->large_page == 1) {
	if (handle_large_pagefault_32(info, fault_addr, error_code, shadow_pt, (pde32_4MB_t *)guest_pde) == -1) {
	  PrintError("Error handling large pagefault\n");
	  return -1;
	}
      }
    }
  else if ((shadow_pde_access == PT_ACCESS_WRITE_ERROR) && 
	   (guest_pde->large_page == 1) && 
	   (((pde32_4MB_t *)guest_pde)->dirty == 0)) 
    {
      //
      // Page Directory Entry marked read-only
      // Its a large page and we need to update the dirty bit in the guest
      //

      PrintDebug("Large page write error... Setting dirty bit and returning\n");
      ((pde32_4MB_t *)guest_pde)->dirty = 1;
      shadow_pde->writable = guest_pde->writable;
      return 0;
      
    } 
  else if (shadow_pde_access == PT_ACCESS_USER_ERROR) 
    {
      //
      // Page Directory Entry marked non-user
      //      
      PrintDebug("Shadow Paging User access error (shadow_pde_access=0x%x, guest_pde_access=0x%x)\n", 
		 shadow_pde_access, guest_pde_access);
      inject_guest_pf(info, fault_addr, error_code);
      return 0;
    }
  else 
    {
      // inject page fault in guest
      inject_guest_pf(info, fault_addr, error_code);
      PrintDebug("Unknown Error occurred (shadow_pde_access=%d)\n", shadow_pde_access);
      PrintDebug("Manual Says to inject page fault into guest\n");
#ifdef DEBUG_SHADOW_PAGING
      PrintDebug("Guest PDE: (access=%d)\n\t", guest_pde_access);
      PrintPTEntry(PAGE_PD32, fault_addr, guest_pde);
      PrintDebug("Shadow PDE: (access=%d)\n\t", shadow_pde_access);
      PrintPTEntry(PAGE_PD32, fault_addr, shadow_pde);
#endif

      return 0; 
    }

  PrintDebug("Returning end of PDE function (rip=%p)\n", (void *)(addr_t)(info->rip));
  return 0;
}



/* The guest status checks have already been done,
 * only special case shadow checks remain
 */
static int handle_large_pagefault_32(struct guest_info * info, 
				    addr_t fault_addr, pf_error_t error_code, 
				    pte32_t * shadow_pt, pde32_4MB_t * large_guest_pde) 
{
  pt_access_status_t shadow_pte_access = v3_can_access_pte32(shadow_pt, fault_addr, error_code);
  pte32_t * shadow_pte = (pte32_t *)&(shadow_pt[PTE32_INDEX(fault_addr)]);
  addr_t guest_fault_pa = BASE_TO_PAGE_ADDR_4MB(large_guest_pde->page_base_addr) + PAGE_OFFSET_4MB(fault_addr);  

  if (shadow_pte_access == PT_ACCESS_OK) {
    // Inconsistent state...
    // Guest Re-Entry will flush tables and everything should now workd
    PrintDebug("Inconsistent state... Guest re-entry should flush tlb\n");
    return 0;
  }

  
  if (shadow_pte_access == PT_ACCESS_NOT_PRESENT) {
    // Get the guest physical address of the fault
    shdw_region_type_t host_page_type = get_shadow_addr_type(info, guest_fault_pa);
 

    if (host_page_type == SHDW_REGION_INVALID) {
      // Inject a machine check in the guest
      PrintDebug("Invalid Guest Address in page table (0x%p)\n", (void *)guest_fault_pa);
      v3_raise_exception(info, MC_EXCEPTION);
      return 0;
    }

    if ((host_page_type == SHDW_REGION_ALLOCATED) || 
	(host_page_type == SHDW_REGION_WRITE_HOOK)) {
      struct shadow_page_state * state = &(info->shdw_pg_state);
      addr_t shadow_pa = get_shadow_addr(info, guest_fault_pa);

      shadow_pte->page_base_addr = PAGE_BASE_ADDR(shadow_pa);

      shadow_pte->present = 1;

      /* We are assuming that the PDE entry has precedence
       * so the Shadow PDE will mirror the guest PDE settings, 
       * and we don't have to worry about them here
       * Allow everything
       */
      shadow_pte->user_page = 1;

      if (find_pte_map(state->cached_ptes, PAGE_ADDR(guest_fault_pa)) != NULL) {
	// Check if the entry is a page table...
	PrintDebug("Marking page as Guest Page Table (large page)\n");
	shadow_pte->vmm_info = PT32_GUEST_PT;
	shadow_pte->writable = 0;
      } else if (host_page_type == SHDW_REGION_WRITE_HOOK) {
	shadow_pte->writable = 0;
      } else {
	shadow_pte->writable = 1;
      }

      //set according to VMM policy
      shadow_pte->write_through = 0;
      shadow_pte->cache_disable = 0;
      shadow_pte->global_page = 0;
      //
      
    } else {
      // Handle hooked pages as well as other special pages
      //      if (handle_special_page_fault(info, fault_addr, guest_fault_pa, error_code) == -1) {
      struct shadow_region * reg = v3_get_shadow_region(info, guest_fault_pa);

      if (v3_handle_mem_full_hook(info, fault_addr, guest_fault_pa, reg, error_code) == -1) {
	PrintError("Special Page Fault handler returned error for address: %p\n", (void *)fault_addr);
	return -1;
      }
    }
  } else if (shadow_pte_access == PT_ACCESS_WRITE_ERROR) {
    shdw_region_type_t host_page_type = get_shadow_addr_type(info, guest_fault_pa);

    if (host_page_type == SHDW_REGION_WRITE_HOOK) {
      struct shadow_region * reg = v3_get_shadow_region(info, guest_fault_pa);

      if (v3_handle_mem_wr_hook(info, fault_addr, guest_fault_pa, reg, error_code) == -1) {
	PrintError("Special Page Fault handler returned error for address: %p\n", (void *)fault_addr);
	return -1;
      }
    } else if (shadow_pte->vmm_info == PT32_GUEST_PT) {
      struct shadow_page_state * state = &(info->shdw_pg_state);
      PrintDebug("Write operation on Guest PAge Table Page (large page)\n");
      state->cached_cr3 = 0;
      shadow_pte->writable = 1;
    }

  } else {
    PrintError("Error in large page fault handler...\n");
    PrintError("This case should have been handled at the top level handler\n");
    return -1;
  }

  PrintDebug("Returning from large page fault handler\n");
  return 0;
}




/* 
 * We assume the the guest pte pointer has already been translated to a host virtual address
 */
static int handle_shadow_pte32_fault(struct guest_info * info, 
				     addr_t fault_addr, 
				     pf_error_t error_code,
				     pte32_t * shadow_pt, 
				     pte32_t * guest_pt) {

  pt_access_status_t guest_pte_access;
  pt_access_status_t shadow_pte_access;
  pte32_t * guest_pte = (pte32_t *)&(guest_pt[PTE32_INDEX(fault_addr)]);;
  pte32_t * shadow_pte = (pte32_t *)&(shadow_pt[PTE32_INDEX(fault_addr)]);
  addr_t guest_pa = BASE_TO_PAGE_ADDR((addr_t)(guest_pte->page_base_addr)) +  PAGE_OFFSET(fault_addr);


  // Check the guest page permissions
  guest_pte_access = v3_can_access_pte32(guest_pt, fault_addr, error_code);

  // Check the shadow page permissions
  shadow_pte_access = v3_can_access_pte32(shadow_pt, fault_addr, error_code);
  
#ifdef DEBUG_SHADOW_PAGING
  PrintDebug("Guest PTE: (access=%d)\n\t", guest_pte_access);
  PrintPTEntry(PAGE_PT32, fault_addr, guest_pte);
  PrintDebug("Shadow PTE: (access=%d)\n\t", shadow_pte_access);
  PrintPTEntry(PAGE_PT32, fault_addr, shadow_pte);
#endif
  
  /* Was the page fault caused by the Guest's page tables? */
  if (is_guest_pf(guest_pte_access, shadow_pte_access) == 1) {
    PrintDebug("Access error injecting pf to guest (guest access error=%d) (pf error code=%d)\n", 
	       guest_pte_access, *(uint_t*)&error_code);    
    inject_guest_pf(info, fault_addr, error_code);
    return 0; 
  }
  
  
  if (shadow_pte_access == PT_ACCESS_OK) {
    // Inconsistent state...
    // Guest Re-Entry will flush page tables and everything should now work
    PrintDebug("Inconsistent state... Guest re-entry should flush tlb\n");
    return 0;
  }


  if (shadow_pte_access == PT_ACCESS_NOT_PRESENT) {


    // Page Table Entry Not Present
    PrintDebug("guest_pa =%p\n", (void *)guest_pa);

    shdw_region_type_t host_page_type = get_shadow_addr_type(info, guest_pa);

    if (host_page_type == SHDW_REGION_INVALID) {
      // Inject a machine check in the guest
      PrintDebug("Invalid Guest Address in page table (0x%p)\n", (void *)guest_pa);
      v3_raise_exception(info, MC_EXCEPTION);
      return 0;
    }

    // else...

    if ((host_page_type == SHDW_REGION_ALLOCATED) ||
	(host_page_type == SHDW_REGION_WRITE_HOOK)) {
      struct shadow_page_state * state = &(info->shdw_pg_state);
      addr_t shadow_pa = get_shadow_addr(info, guest_pa);
      
      shadow_pte->page_base_addr = PAGE_BASE_ADDR(shadow_pa);
      
      shadow_pte->present = guest_pte->present;
      shadow_pte->user_page = guest_pte->user_page;
      
      //set according to VMM policy
      shadow_pte->write_through = 0;
      shadow_pte->cache_disable = 0;
      shadow_pte->global_page = 0;
      //
      
      guest_pte->accessed = 1;
      
      if (find_pte_map(state->cached_ptes, PAGE_ADDR(guest_pa)) != NULL) {
	// Check if the entry is a page table...
	PrintDebug("Marking page as Guest Page Table %d\n", shadow_pte->writable);
	shadow_pte->vmm_info = PT32_GUEST_PT;
      }

      if (host_page_type == SHDW_REGION_WRITE_HOOK) {
	shadow_pte->writable = 0;
      } else if (guest_pte->dirty == 1) {
	shadow_pte->writable = guest_pte->writable;
      } else if ((guest_pte->dirty == 0) && (error_code.write == 1)) {
	shadow_pte->writable = guest_pte->writable;
	guest_pte->dirty = 1;
	
	if (shadow_pte->vmm_info == PT32_GUEST_PT) {
	  // Well that was quick...
	  struct shadow_page_state * state = &(info->shdw_pg_state);
	  PrintDebug("Immediate Write operation on Guest PAge Table Page\n");
	  state->cached_cr3 = 0;
	}

      } else if ((guest_pte->dirty == 0) && (error_code.write == 0)) {  // was =
	shadow_pte->writable = 0;
      }

    } else {
      // Page fault handled by hook functions
      struct shadow_region * reg = v3_get_shadow_region(info, guest_pa);

      if (v3_handle_mem_full_hook(info, fault_addr, guest_pa, reg, error_code) == -1) {
	PrintError("Special Page fault handler returned error for address: %p\n",  (void *)fault_addr);
	return -1;
      }
    }
    /*
  } else if ((shadow_pte_access == PT_ACCESS_WRITE_ERROR) &&
	     (guest_pte->dirty == 0)) {
    */
  } else if (shadow_pte_access == PT_ACCESS_WRITE_ERROR) {
    guest_pte->dirty = 1;

    shdw_region_type_t host_page_type = get_shadow_addr_type(info, guest_pa);

    if (host_page_type == SHDW_REGION_WRITE_HOOK) {
      struct shadow_region * reg = v3_get_shadow_region(info, guest_pa);

      if (v3_handle_mem_wr_hook(info, fault_addr, guest_pa, reg, error_code) == -1) {
	PrintError("Special Page fault handler returned error for address: %p\n",  (void *)fault_addr);
	return -1;
      }
    } else {
      PrintDebug("Shadow PTE Write Error\n");
      shadow_pte->writable = guest_pte->writable;
    }

    if (shadow_pte->vmm_info == PT32_GUEST_PT) {
      struct shadow_page_state * state = &(info->shdw_pg_state);
      PrintDebug("Write operation on Guest PAge Table Page\n");
      state->cached_cr3 = 0;
    }

    return 0;

  } else {
    // Inject page fault into the guest	
    inject_guest_pf(info, fault_addr, error_code);
    PrintError("PTE Page fault fell through... Not sure if this should ever happen\n");
    PrintError("Manual Says to inject page fault into guest\n");
    return -1;
  }

  PrintDebug("Returning end of function\n");
  return 0;
}






/* Currently Does not work with Segmentation!!! */
int v3_handle_shadow_invlpg(struct guest_info * info)
{
  if (info->mem_mode != VIRTUAL_MEM) {
    // Paging must be turned on...
    // should handle with some sort of fault I think
    PrintError("ERROR: INVLPG called in non paged mode\n");
    return -1;
  }
  
  
  if (info->cpu_mode != PROTECTED) {
    PrintError("Unsupported CPU mode (mode=%s)\n", v3_cpu_mode_to_str(info->cpu_mode));
    return -1;
  }
  
  uchar_t instr[15];
  int index = 0;
  
  int ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 15, instr);
  if (ret != 15) {
    PrintError("Could not read instruction 0x%p (ret=%d)\n",  (void *)(addr_t)(info->rip), ret);
    return -1;
  }
  
  
  /* Can INVLPG work with Segments?? */
  while (is_prefix_byte(instr[index])) {
    index++;
  }
    
    
  if( (instr[index + 0] != (uchar_t) 0x0f) ||  
      (instr[index + 1] != (uchar_t) 0x01) ) {
    PrintError("invalid Instruction Opcode\n");
    PrintTraceMemDump(instr, 15);
    return -1;
  }
  
  addr_t first_operand;
  addr_t second_operand;
  addr_t guest_cr3 =  CR3_TO_PDE32_PA(info->shdw_pg_state.guest_cr3);
  
  pde32_t * guest_pd = NULL;
  
  if (guest_pa_to_host_va(info, guest_cr3, (addr_t*)&guest_pd) == -1) {
    PrintError("Invalid Guest PDE Address: 0x%p\n",  (void *)guest_cr3);
    return -1;
  }
  
  index += 2;

  v3_operand_type_t addr_type = decode_operands32(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);
  
  if (addr_type != MEM_OPERAND) {
    PrintError("Invalid Operand type\n");
    return -1;
  }
  
  pde32_t * shadow_pd = (pde32_t *)CR3_TO_PDE32_VA(info->ctrl_regs.cr3);
  pde32_t * shadow_pde = (pde32_t *)&shadow_pd[PDE32_INDEX(first_operand)];
  pde32_t * guest_pde;
  
  //PrintDebug("PDE Index=%d\n", PDE32_INDEX(first_operand));
  //PrintDebug("FirstOperand = %x\n", first_operand);
  
  PrintDebug("Invalidating page for %p\n", (void *)first_operand);
  
  guest_pde = (pde32_t *)&(guest_pd[PDE32_INDEX(first_operand)]);
  
  if (guest_pde->large_page == 1) {
    shadow_pde->present = 0;
    PrintDebug("Invalidating Large Page\n");
  } else
    if (shadow_pde->present == 1) {
      pte32_t * shadow_pt = (pte32_t *)(addr_t)BASE_TO_PAGE_ADDR(shadow_pde->pt_base_addr);
      pte32_t * shadow_pte = (pte32_t *) V3_VAddr( (void*) &shadow_pt[PTE32_INDEX(first_operand)] );
      
#ifdef DEBUG_SHADOW_PAGING
      PrintDebug("Setting not present\n");
      PrintPTEntry(PAGE_PT32, first_operand, shadow_pte);
#endif
      
      shadow_pte->present = 0;
    }
  
  info->rip += index;
  
  return 0;
}


