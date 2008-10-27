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

#include <palacios/vmm_paging.h>

#include <palacios/vmm.h>

#include <palacios/vm_guest_mem.h>




  


void delete_page_tables_32(pde32_t * pde) {
  int i;

  if (pde == NULL) { 
    return;
  }

  for (i = 0; (i < MAX_PDE32_ENTRIES); i++) {
    if (pde[i].present) {
      // We double cast, first to an addr_t to handle 64 bit issues, then to the pointer
      PrintDebug("PTE base addr %x \n", pde[i].pt_base_addr);
      pte32_t * pte = (pte32_t *)((addr_t)(uint_t)(pde[i].pt_base_addr << PAGE_POWER));

      PrintDebug("Deleting PTE %d (%p)\n", i, pte);
      V3_FreePage(pte);
    }
  }

  PrintDebug("Deleting PDE (%p)\n", pde);
  V3_FreePage(V3_PAddr(pde));
}

void delete_page_tables_32PAE(pdpe32pae_t * pdpe) { 
  PrintError("Unimplemented function\n");
}

void delete_page_tables_64(pml4e64_t * pml4) {
  PrintError("Unimplemented function\n");
}





int pt32_lookup(pde32_t * pd, addr_t vaddr, addr_t * paddr) {
  addr_t pde_entry;
  pde32_entry_type_t pde_entry_type;

  if (pd == 0) {
    return -1;
  }

  pde_entry_type = pde32_lookup(pd, vaddr, &pde_entry);

  if (pde_entry_type == PDE32_ENTRY_PTE32) {
    return pte32_lookup((pte32_t *)pde_entry, vaddr, paddr);
  } else if (pde_entry_type == PDE32_ENTRY_LARGE_PAGE) {
    *paddr = pde_entry;
    return 0;
  }

  return -1;
}



/* We can't do a full lookup because we don't know what context the page tables are in...
 * The entry addresses could be pointing to either guest physical memory or host physical memory
 * Instead we just return the entry address, and a flag to show if it points to a pte or a large page...
 */
pde32_entry_type_t pde32_lookup(pde32_t * pd, addr_t addr, addr_t * entry) {
  pde32_t * pde_entry = &(pd[PDE32_INDEX(addr)]);

  if (!pde_entry->present) {
    *entry = 0;
    return PDE32_ENTRY_NOT_PRESENT;
  } else  {

    if (pde_entry->large_page) {
      pde32_4MB_t * large_pde = (pde32_4MB_t *)pde_entry;

      *entry = PDE32_4MB_T_ADDR(*large_pde);
      *entry += PD32_4MB_PAGE_OFFSET(addr);
      return PDE32_ENTRY_LARGE_PAGE;
    } else {
      *entry = PDE32_T_ADDR(*pde_entry);
      return PDE32_ENTRY_PTE32;
    }
  }  
  return PDE32_ENTRY_NOT_PRESENT;
}



/* Takes a virtual addr (addr) and returns the physical addr (entry) as defined in the page table
 */
int pte32_lookup(pte32_t * pt, addr_t addr, addr_t * entry) {
  pte32_t * pte_entry = &(pt[PTE32_INDEX(addr)]);

  if (!pte_entry->present) {
    *entry = 0;
    PrintDebug("Lookup at non present page (index=%d)\n", PTE32_INDEX(addr));
    return -1;
  } else {
    *entry = PTE32_T_ADDR(*pte_entry) + PT32_PAGE_OFFSET(addr);
    return 0;
  }

  return -1;
}



pt_access_status_t can_access_pde32(pde32_t * pde, addr_t addr, pf_error_t access_type) {
  pde32_t * entry = &pde[PDE32_INDEX(addr)];

  if (entry->present == 0) {
    return PT_ENTRY_NOT_PRESENT;
  } else if ((entry->writable == 0) && (access_type.write == 1)) {
    return PT_WRITE_ERROR;
  } else if ((entry->user_page == 0) && (access_type.user == 1)) {
    // Check CR0.WP?
    return PT_USER_ERROR;
  }

  return PT_ACCESS_OK;
}


pt_access_status_t can_access_pte32(pte32_t * pte, addr_t addr, pf_error_t access_type) {
  pte32_t * entry = &pte[PTE32_INDEX(addr)];

  if (entry->present == 0) {
    return PT_ENTRY_NOT_PRESENT;
  } else if ((entry->writable == 0) && (access_type.write == 1)) {
    return PT_WRITE_ERROR;
  } else if ((entry->user_page == 0) && (access_type.user == 1)) {
    // Check CR0.WP?
    return PT_USER_ERROR;
  }

  return PT_ACCESS_OK;
}




/* We generate a page table to correspond to a given memory layout
 * pulling pages from the mem_list when necessary
 * If there are any gaps in the layout, we add them as unmapped pages
 */
pde32_t * create_passthrough_pts_32(struct guest_info * guest_info) {
  addr_t current_page_addr = 0;
  int i, j;
  struct shadow_map * map = &(guest_info->mem_map);

  pde32_t * pde = V3_VAddr(V3_AllocPages(1));

  for (i = 0; i < MAX_PDE32_ENTRIES; i++) {
    int pte_present = 0;
    pte32_t * pte = V3_VAddr(V3_AllocPages(1));
    

    for (j = 0; j < MAX_PTE32_ENTRIES; j++) {
      struct shadow_region * region = get_shadow_region_by_addr(map, current_page_addr);

      if (!region || 
	  (region->host_type == HOST_REGION_HOOK) || 
	  (region->host_type == HOST_REGION_UNALLOCATED) || 
	  (region->host_type == HOST_REGION_MEMORY_MAPPED_DEVICE) || 
	  (region->host_type == HOST_REGION_REMOTE) ||
	  (region->host_type == HOST_REGION_SWAPPED)) {
	pte[j].present = 0;
	pte[j].writable = 0;
	pte[j].user_page = 0;
	pte[j].write_through = 0;
	pte[j].cache_disable = 0;
	pte[j].accessed = 0;
	pte[j].dirty = 0;
	pte[j].pte_attr = 0;
	pte[j].global_page = 0;
	pte[j].vmm_info = 0;
	pte[j].page_base_addr = 0;
      } else {
	addr_t host_addr;
	pte[j].present = 1;
	pte[j].writable = 1;
	pte[j].user_page = 1;
	pte[j].write_through = 0;
	pte[j].cache_disable = 0;
	pte[j].accessed = 0;
	pte[j].dirty = 0;
	pte[j].pte_attr = 0;
	pte[j].global_page = 0;
	pte[j].vmm_info = 0;

	if (guest_pa_to_host_pa(guest_info, current_page_addr, &host_addr) == -1) {
	  // BIG ERROR
	  // PANIC
	  return NULL;
	}
	
	pte[j].page_base_addr = host_addr >> 12;
	
	pte_present = 1;
      }

      current_page_addr += PAGE_SIZE;
    }

    if (pte_present == 0) { 
      V3_FreePage(V3_PAddr(pte));

      pde[i].present = 0;
      pde[i].writable = 0;
      pde[i].user_page = 0;
      pde[i].write_through = 0;
      pde[i].cache_disable = 0;
      pde[i].accessed = 0;
      pde[i].reserved = 0;
      pde[i].large_page = 0;
      pde[i].global_page = 0;
      pde[i].vmm_info = 0;
      pde[i].pt_base_addr = 0;
    } else {
      pde[i].present = 1;
      pde[i].writable = 1;
      pde[i].user_page = 1;
      pde[i].write_through = 0;
      pde[i].cache_disable = 0;
      pde[i].accessed = 0;
      pde[i].reserved = 0;
      pde[i].large_page = 0;
      pde[i].global_page = 0;
      pde[i].vmm_info = 0;
      pde[i].pt_base_addr = PAGE_ALIGNED_ADDR((addr_t)V3_PAddr(pte));
    }

  }

  return pde;
}


/* We generate a page table to correspond to a given memory layout
 * pulling pages from the mem_list when necessary
 * If there are any gaps in the layout, we add them as unmapped pages
 */
pdpe32pae_t * create_passthrough_pts_32PAE(struct guest_info * guest_info) {
  addr_t current_page_addr = 0;
  int i, j, k;
  struct shadow_map * map = &(guest_info->mem_map);

  pdpe32pae_t * pdpe = V3_VAddr(V3_AllocPages(1));
  memset(pdpe, 0, PAGE_SIZE);

  for (i = 0; i < MAX_PDPE32PAE_ENTRIES; i++) {
    int pde_present = 0;
    pde32pae_t * pde = V3_VAddr(V3_AllocPages(1));

    for (j = 0; j < MAX_PDE32PAE_ENTRIES; j++) {


      int pte_present = 0;
      pte32pae_t * pte = V3_VAddr(V3_AllocPages(1));
      
      
      for (k = 0; k < MAX_PTE32PAE_ENTRIES; k++) {
	struct shadow_region * region = get_shadow_region_by_addr(map, current_page_addr);
	
	if (!region || 
	    (region->host_type == HOST_REGION_HOOK) || 
	    (region->host_type == HOST_REGION_UNALLOCATED) || 
	    (region->host_type == HOST_REGION_MEMORY_MAPPED_DEVICE) || 
	    (region->host_type == HOST_REGION_REMOTE) ||
	    (region->host_type == HOST_REGION_SWAPPED)) {
	  pte[k].present = 0;
	  pte[k].writable = 0;
	  pte[k].user_page = 0;
	  pte[k].write_through = 0;
	  pte[k].cache_disable = 0;
	  pte[k].accessed = 0;
	  pte[k].dirty = 0;
	  pte[k].pte_attr = 0;
	  pte[k].global_page = 0;
	  pte[k].vmm_info = 0;
	  pte[k].page_base_addr = 0;
	  pte[k].rsvd = 0;
	} else {
	  addr_t host_addr;
	  pte[k].present = 1;
	  pte[k].writable = 1;
	  pte[k].user_page = 1;
	  pte[k].write_through = 0;
	  pte[k].cache_disable = 0;
	  pte[k].accessed = 0;
	  pte[k].dirty = 0;
	  pte[k].pte_attr = 0;
	  pte[k].global_page = 0;
	  pte[k].vmm_info = 0;
	  
	  if (guest_pa_to_host_pa(guest_info, current_page_addr, &host_addr) == -1) {
	    // BIG ERROR
	    // PANIC
	    return NULL;
	  }
	  
	  pte[k].page_base_addr = host_addr >> 12;
	  pte[k].rsvd = 0;

	  pte_present = 1;
	}
	
	current_page_addr += PAGE_SIZE;
      }
      
      if (pte_present == 0) { 
	V3_FreePage(V3_PAddr(pte));
	
	pde[j].present = 0;
	pde[j].writable = 0;
	pde[j].user_page = 0;
	pde[j].write_through = 0;
	pde[j].cache_disable = 0;
	pde[j].accessed = 0;
	pde[j].avail = 0;
	pde[j].large_page = 0;
	pde[j].global_page = 0;
	pde[j].vmm_info = 0;
	pde[j].pt_base_addr = 0;
	pde[j].rsvd = 0;
      } else {
	pde[j].present = 1;
	pde[j].writable = 1;
	pde[j].user_page = 1;
	pde[j].write_through = 0;
	pde[j].cache_disable = 0;
	pde[j].accessed = 0;
	pde[j].avail = 0;
	pde[j].large_page = 0;
	pde[j].global_page = 0;
	pde[j].vmm_info = 0;
	pde[j].pt_base_addr = PAGE_ALIGNED_ADDR((addr_t)V3_PAddr(pte));
	pde[j].rsvd = 0;

	pde_present = 1;
      }
      
    }
    
    if (pde_present == 0) { 
      V3_FreePage(V3_PAddr(pde));
      
      pdpe[i].present = 0;
      pdpe[i].rsvd = 0;
      pdpe[i].write_through = 0;
      pdpe[i].cache_disable = 0;
      pdpe[i].accessed = 0;
      pdpe[i].avail = 0;
      pdpe[i].rsvd2 = 0;
      pdpe[i].vmm_info = 0;
      pdpe[i].pd_base_addr = 0;
      pdpe[i].rsvd3 = 0;
    } else {
      pdpe[i].present = 1;
      pdpe[i].rsvd = 0;
      pdpe[i].write_through = 0;
      pdpe[i].cache_disable = 0;
      pdpe[i].accessed = 0;
      pdpe[i].avail = 0;
      pdpe[i].rsvd2 = 0;
      pdpe[i].vmm_info = 0;
      pdpe[i].pd_base_addr = PAGE_ALIGNED_ADDR((addr_t)V3_PAddr(pde));
      pdpe[i].rsvd3 = 0;
    }
    
  }


  return pdpe;
}






pml4e64_t * create_passthrough_pts_64(struct guest_info * info) {
  addr_t current_page_addr = 0;
  int i, j, k, m;
  struct shadow_map * map = &(info->mem_map);
  
  pml4e64_t * pml = V3_VAddr(V3_AllocPages(1));

  for (i = 0; i < 1; i++) {
    int pdpe_present = 0;
    pdpe64_t * pdpe = V3_VAddr(V3_AllocPages(1));

    for (j = 0; j < 20; j++) {
      int pde_present = 0;
      pde64_t * pde = V3_VAddr(V3_AllocPages(1));

      for (k = 0; k < MAX_PDE64_ENTRIES; k++) {
	int pte_present = 0;
	pte64_t * pte = V3_VAddr(V3_AllocPages(1));


	for (m = 0; m < MAX_PTE64_ENTRIES; m++) {
	  struct shadow_region * region = get_shadow_region_by_addr(map, current_page_addr);
	  

	  
	  if (!region || 
	      (region->host_type == HOST_REGION_HOOK) || 
	      (region->host_type == HOST_REGION_UNALLOCATED) || 
	      (region->host_type == HOST_REGION_MEMORY_MAPPED_DEVICE) || 
	      (region->host_type == HOST_REGION_REMOTE) ||
	      (region->host_type == HOST_REGION_SWAPPED)) {
	    pte[m].present = 0;
	    pte[m].writable = 0;
	    pte[m].user_page = 0;
	    pte[m].write_through = 0;
	    pte[m].cache_disable = 0;
	    pte[m].accessed = 0;
	    pte[m].dirty = 0;
	    pte[m].pte_attr = 0;
	    pte[m].global_page = 0;
	    pte[m].vmm_info = 0;
	    pte[m].page_base_addr = 0;
	  } else {
	    addr_t host_addr;
	    pte[m].present = 1;
	    pte[m].writable = 1;
	    pte[m].user_page = 1;
	    pte[m].write_through = 0;
	    pte[m].cache_disable = 0;
	    pte[m].accessed = 0;
	    pte[m].dirty = 0;
	    pte[m].pte_attr = 0;
	    pte[m].global_page = 0;
	    pte[m].vmm_info = 0;
	    
	    if (guest_pa_to_host_pa(info, current_page_addr, &host_addr) == -1) {
	      // BIG ERROR
	      // PANIC
	      return NULL;
	    }

	    pte[m].page_base_addr = PTE64_BASE_ADDR(host_addr);

	    //PrintPTE64(current_page_addr, &(pte[m]));

	    pte_present = 1;	  
	  }




	  current_page_addr += PAGE_SIZE;
	}
	
	if (pte_present == 0) {
	  V3_FreePage(V3_PAddr(pte));

	  pde[k].present = 0;
	  pde[k].writable = 0;
	  pde[k].user_page = 0;
	  pde[k].write_through = 0;
	  pde[k].cache_disable = 0;
	  pde[k].accessed = 0;
	  pde[k].reserved = 0;
	  pde[k].large_page = 0;
	  //pde[k].global_page = 0;
	  pde[k].vmm_info = 0;
	  pde[k].pt_base_addr = 0;
	} else {
	  pde[k].present = 1;
	  pde[k].writable = 1;
	  pde[k].user_page = 1;
	  pde[k].write_through = 0;
	  pde[k].cache_disable = 0;
	  pde[k].accessed = 0;
	  pde[k].reserved = 0;
	  pde[k].large_page = 0;
	  //pde[k].global_page = 0;
	  pde[k].vmm_info = 0;
	  pde[k].pt_base_addr = PAGE_ALIGNED_ADDR((addr_t)V3_PAddr(pte));

	  pde_present = 1;
	}
      }

      if (pde_present == 0) {
	V3_FreePage(V3_PAddr(pde));
	
	pdpe[j].present = 0;
	pdpe[j].writable = 0;
	pdpe[j].user_page = 0;
	pdpe[j].write_through = 0;
	pdpe[j].cache_disable = 0;
	pdpe[j].accessed = 0;
	pdpe[j].reserved = 0;
	pdpe[j].large_page = 0;
	//pdpe[j].global_page = 0;
	pdpe[j].vmm_info = 0;
	pdpe[j].pd_base_addr = 0;
      } else {
	pdpe[j].present = 1;
	pdpe[j].writable = 1;
	pdpe[j].user_page = 1;
	pdpe[j].write_through = 0;
	pdpe[j].cache_disable = 0;
	pdpe[j].accessed = 0;
	pdpe[j].reserved = 0;
	pdpe[j].large_page = 0;
	//pdpe[j].global_page = 0;
	pdpe[j].vmm_info = 0;
	pdpe[j].pd_base_addr = PAGE_ALIGNED_ADDR((addr_t)V3_PAddr(pde));


	pdpe_present = 1;
      }

    }

    PrintDebug("PML index=%d\n", i);

    if (pdpe_present == 0) {
      V3_FreePage(V3_PAddr(pdpe));
      
      pml[i].present = 0;
      pml[i].writable = 0;
      pml[i].user_page = 0;
      pml[i].write_through = 0;
      pml[i].cache_disable = 0;
      pml[i].accessed = 0;
      pml[i].reserved = 0;
      //pml[i].large_page = 0;
      //pml[i].global_page = 0;
      pml[i].vmm_info = 0;
      pml[i].pdp_base_addr = 0;
    } else {
      pml[i].present = 1;
      pml[i].writable = 1;
      pml[i].user_page = 1;
      pml[i].write_through = 0;
      pml[i].cache_disable = 0;
      pml[i].accessed = 0;
      pml[i].reserved = 0;
      //pml[i].large_page = 0;
      //pml[i].global_page = 0;
      pml[i].vmm_info = 0;
      pml[i].pdp_base_addr = PAGE_ALIGNED_ADDR((addr_t)V3_PAddr(pdpe));
    }
  }

  return pml;
}








void PrintPDE32(addr_t virtual_address, pde32_t * pde)
{
  PrintDebug("PDE %p -> %p : present=%x, writable=%x, user=%x, wt=%x, cd=%x, accessed=%x, reserved=%x, largePages=%x, globalPage=%x, kernelInfo=%x\n",
	     (void *)virtual_address,
	     (void *)(addr_t) (pde->pt_base_addr << PAGE_POWER),
	     pde->present,
	     pde->writable,
	     pde->user_page, 
	     pde->write_through,
	     pde->cache_disable,
	     pde->accessed,
	     pde->reserved,
	     pde->large_page,
	     pde->global_page,
	     pde->vmm_info);
}

  
void PrintPTE32(addr_t virtual_address, pte32_t * pte)
{
  PrintDebug("PTE %p -> %p : present=%x, writable=%x, user=%x, wt=%x, cd=%x, accessed=%x, dirty=%x, pteAttribute=%x, globalPage=%x, vmm_info=%x\n",
	     (void *)virtual_address,
	     (void*)(addr_t)(pte->page_base_addr << PAGE_POWER),
	     pte->present,
	     pte->writable,
	     pte->user_page,
	     pte->write_through,
	     pte->cache_disable,
	     pte->accessed,
	     pte->dirty,
	     pte->pte_attr,
	     pte->global_page,
	     pte->vmm_info);
}










void PrintPD32(pde32_t * pde)
{
  int i;

  PrintDebug("Page Directory at %p:\n", pde);
  for (i = 0; (i < MAX_PDE32_ENTRIES); i++) { 
    if ( pde[i].present) {
      PrintPDE32((addr_t)(PAGE_SIZE * MAX_PTE32_ENTRIES * i), &(pde[i]));
    }
  }
}

void PrintPT32(addr_t starting_address, pte32_t * pte) 
{
  int i;

  PrintDebug("Page Table at %p:\n", pte);
  for (i = 0; (i < MAX_PTE32_ENTRIES) ; i++) { 
    if (pte[i].present) {
      PrintPTE32(starting_address + (PAGE_SIZE * i), &(pte[i]));
    }
  }
}







void PrintDebugPageTables(pde32_t * pde)
{
  int i;
  
  PrintDebug("Dumping the pages starting with the pde page at %p\n", pde);

  for (i = 0; (i < MAX_PDE32_ENTRIES); i++) { 
    if (pde[i].present) {
      PrintPDE32((addr_t)(PAGE_SIZE * MAX_PTE32_ENTRIES * i), &(pde[i]));
      PrintPT32((addr_t)(PAGE_SIZE * MAX_PTE32_ENTRIES * i), (pte32_t *)V3_VAddr((void *)(addr_t)(pde[i].pt_base_addr << PAGE_POWER)));
    }
  }
}
    







void PrintPDPE32PAE(addr_t virtual_address, pdpe32pae_t * pdpe)
{
  PrintDebug("PDPE %p -> %p : present=%x, wt=%x, cd=%x, accessed=%x, kernelInfo=%x\n",
	     (void *)virtual_address,
	     (void *)(addr_t) (pdpe->pd_base_addr << PAGE_POWER),
	     pdpe->present,
	     pdpe->write_through,
	     pdpe->cache_disable,
	     pdpe->accessed,
	     pdpe->vmm_info);
}

void PrintPDE32PAE(addr_t virtual_address, pde32pae_t * pde)
{
  PrintDebug("PDE %p -> %p : present=%x, writable=%x, user=%x, wt=%x, cd=%x, accessed=%x, largePages=%x, globalPage=%x, kernelInfo=%x\n",
	     (void *)virtual_address,
	     (void *)(addr_t) (pde->pt_base_addr << PAGE_POWER),
	     pde->present,
	     pde->writable,
	     pde->user_page, 
	     pde->write_through,
	     pde->cache_disable,
	     pde->accessed,
	     pde->large_page,
	     pde->global_page,
	     pde->vmm_info);
}

  
void PrintPTE32PAE(addr_t virtual_address, pte32pae_t * pte)
{
  PrintDebug("PTE %p -> %p : present=%x, writable=%x, user=%x, wt=%x, cd=%x, accessed=%x, dirty=%x, pteAttribute=%x, globalPage=%x, vmm_info=%x\n",
	     (void *)virtual_address,
	     (void*)(addr_t)(pte->page_base_addr << PAGE_POWER),
	     pte->present,
	     pte->writable,
	     pte->user_page,
	     pte->write_through,
	     pte->cache_disable,
	     pte->accessed,
	     pte->dirty,
	     pte->pte_attr,
	     pte->global_page,
	     pte->vmm_info);
}






void PrintDebugPageTables32PAE(pdpe32pae_t * pdpe)
{
  int i, j, k;
  pde32pae_t * pde;
  pte32pae_t * pte;
  addr_t virtual_addr = 0;

  PrintDebug("Dumping the pages starting with the pde page at %p\n", pdpe);

  for (i = 0; (i < MAX_PDPE32PAE_ENTRIES); i++) { 

    if (pdpe[i].present) {
      pde = (pde32pae_t *)V3_VAddr((void *)(addr_t)BASE_TO_PAGE_ADDR(pdpe[i].pd_base_addr));

      PrintPDPE32PAE(virtual_addr, &(pdpe[i]));

      for (j = 0; j < MAX_PDE32PAE_ENTRIES; j++) {

	if (pde[j].present) {
	  pte = (pte32pae_t *)V3_VAddr((void *)(addr_t)BASE_TO_PAGE_ADDR(pde[j].pt_base_addr));

	  PrintPDE32PAE(virtual_addr, &(pde[j]));

	  for (k = 0; k < MAX_PTE32PAE_ENTRIES; k++) {
	    if (pte[k].present) {
	      PrintPTE32PAE(virtual_addr, &(pte[k]));
	    }

	    virtual_addr += PAGE_SIZE;
	  }
	} else {
	  virtual_addr += PAGE_SIZE * MAX_PTE32PAE_ENTRIES;
	}
      }
    } else {
      virtual_addr += PAGE_SIZE * MAX_PDE32PAE_ENTRIES * MAX_PTE32PAE_ENTRIES;
    }
  }
}
    


void PrintPML4e64(addr_t virtual_address, pml4e64_t * pml)
{
  PrintDebug("PML4e64 %p -> %p : present=%x, writable=%x, user=%x, wt=%x, cd=%x, accessed=%x, reserved=%x, kernelInfo=%x\n",
	     (void *)virtual_address,
	     (void *)(addr_t) (BASE_TO_PAGE_ADDR(pml->pdp_base_addr)),
	     pml->present,
	     pml->writable,
	     pml->user_page, 
	     pml->write_through,
	     pml->cache_disable,
	     pml->accessed,
	     pml->reserved,
	     pml->vmm_info);
}

void PrintPDPE64(addr_t virtual_address, pdpe64_t * pdpe)
{
  PrintDebug("PDPE64 %p -> %p : present=%x, writable=%x, user=%x, wt=%x, cd=%x, accessed=%x, reserved=%x, largePages=%x, globalPage=%x, kernelInfo=%x\n",
	     (void *)virtual_address,
	     (void *)(addr_t) (BASE_TO_PAGE_ADDR(pdpe->pd_base_addr)),
	     pdpe->present,
	     pdpe->writable,
	     pdpe->user_page, 
	     pdpe->write_through,
	     pdpe->cache_disable,
	     pdpe->accessed,
	     pdpe->reserved,
	     pdpe->large_page,
	     0,//pdpe->global_page,
	     pdpe->vmm_info);
}



void PrintPDE64(addr_t virtual_address, pde64_t * pde)
{
  PrintDebug("PDE64 %p -> %p : present=%x, writable=%x, user=%x, wt=%x, cd=%x, accessed=%x, reserved=%x, largePages=%x, globalPage=%x, kernelInfo=%x\n",
	     (void *)virtual_address,
	     (void *)(addr_t) (BASE_TO_PAGE_ADDR(pde->pt_base_addr)),
	     pde->present,
	     pde->writable,
	     pde->user_page, 
	     pde->write_through,
	     pde->cache_disable,
	     pde->accessed,
	     pde->reserved,
	     pde->large_page,
	     0,//pde->global_page,
	     pde->vmm_info);
}

  
void PrintPTE64(addr_t virtual_address, pte64_t * pte)
{
  PrintDebug("PTE64 %p -> %p : present=%x, writable=%x, user=%x, wt=%x, cd=%x, accessed=%x, dirty=%x, pteAttribute=%x, globalPage=%x, vmm_info=%x\n",
	     (void *)virtual_address,
	     (void*)(addr_t)(BASE_TO_PAGE_ADDR(pte->page_base_addr)),
	     pte->present,
	     pte->writable,
	     pte->user_page,
	     pte->write_through,
	     pte->cache_disable,
	     pte->accessed,
	     pte->dirty,
	     pte->pte_attr,
	     pte->global_page,
	     pte->vmm_info);
}

  



void PrintPageTree_64(addr_t virtual_addr, pml4e64_t * pml) {
  uint_t pml4_index = PML4E64_INDEX(virtual_addr);
  uint_t pdpe_index = PDPE64_INDEX(virtual_addr);
  uint_t pde_index = PDE64_INDEX(virtual_addr);
  uint_t pte_index = PTE64_INDEX(virtual_addr);

  PrintPML4e64(virtual_addr, &(pml[pml4_index]));
  if (pml[pml4_index].present) {
    pdpe64_t * pdpe = (pdpe64_t *)V3_VAddr((void *)(addr_t)BASE_TO_PAGE_ADDR(pml[pml4_index].pdp_base_addr));
    PrintPDPE64(virtual_addr, &(pdpe[pdpe_index]));

    if (pdpe[pdpe_index].present) {
      pde64_t * pde = (pde64_t *)V3_VAddr((void *)(addr_t)BASE_TO_PAGE_ADDR(pdpe[pdpe_index].pd_base_addr));
      PrintPDE64(virtual_addr, &(pde[pde_index]));
      
      if (pde[pde_index].present) {
	pte64_t * pte = (pte64_t *)V3_VAddr((void *)(addr_t)BASE_TO_PAGE_ADDR(pde[pde_index].pt_base_addr));
	PrintPTE64(virtual_addr, &(pte[pte_index]));
      }

    }

  }

}




void PrintPageTree(v3_vm_cpu_mode_t cpu_mode, addr_t virtual_addr, addr_t cr3) {
  switch (cpu_mode) {
  case LONG:
  case LONG_32_COMPAT:
  case LONG_16_COMPAT:
    PrintPageTree_64(virtual_addr, CR3_TO_PML4E64(cr3));
    break;
  default:
    PrintError("Unsupported CPU MODE %d\n", cpu_mode);
    break;
  }
}
