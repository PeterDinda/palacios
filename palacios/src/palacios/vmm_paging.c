#include <palacios/vmm_paging.h>

#include <palacios/vmm.h>

#include <palacios/vm_guest_mem.h>


extern struct vmm_os_hooks * os_hooks;

void delete_page_tables_pde32(pde32_t * pde) {
  int i, j;

  if (pde == NULL) { 
    return;
  }

  for (i = 0; (i < MAX_PDE32_ENTRIES); i++) {
    if (pde[i].present) {
      pte32_t * pte = (pte32_t *)(pde[i].pt_base_addr << PAGE_POWER);
      
      for (j = 0; (j < MAX_PTE32_ENTRIES); j++) {
	if ((pte[j].present)) {
	  os_hooks->free_page((void *)(pte[j].page_base_addr << PAGE_POWER));
	}
      }
      
      os_hooks->free_page(pte);
    }
  }

  os_hooks->free_page(pde);
}







/* We can't do a full lookup because we don't know what context the page tables are in...
 * The entry addresses could be pointing to either guest physical memory or host physical memory
 * Instead we just return the entry address, and a flag to show if it points to a pte or a large page...
 */
pde32_entry_type_t pde32_lookup(pde32_t * pde, addr_t addr, addr_t * entry) {
  pde32_t * pde_entry = &(pde[PDE32_INDEX(addr)]);

  if (!pde_entry->present) {
    *entry = 0;
    return NOT_PRESENT;
  } else  {
    *entry = PAGE_ADDR(pde_entry->pt_base_addr);
    
    if (pde_entry->large_pages) {
      *entry += PAGE_OFFSET(addr);
      return LARGE_PAGE;
    } else {
      return PTE32;
    }
  }  
  return NOT_PRESENT;
}


int pte32_lookup(pte32_t * pte, addr_t addr, addr_t * entry) {
  pte32_t * pte_entry = &(pte[PTE32_INDEX(addr)]);

  if (!pte_entry->present) {
    *entry = 0;
    return -1;
  } else {
    *entry = PAGE_ADDR(pte_entry->page_base_addr);
    *entry += PAGE_OFFSET(addr);
    return 0;
  }

  return -1;
}








/* We generate a page table to correspond to a given memory layout
 * pulling pages from the mem_list when necessary
 * If there are any gaps in the layout, we add them as unmapped pages
 */
pde32_t * create_passthrough_pde32_pts(struct guest_info * guest_info) {
  ullong_t current_page_addr = 0;
  int i, j;
  shadow_map_t * map = &(guest_info->mem_map);


  pde32_t * pde = os_hooks->allocate_pages(1);

  for (i = 0; i < MAX_PDE32_ENTRIES; i++) {
    int pte_present = 0;
    pte32_t * pte = os_hooks->allocate_pages(1);
    

    for (j = 0; j < MAX_PTE32_ENTRIES; j++) {
      shadow_region_t * region = get_shadow_region_by_addr(map, current_page_addr);

      if (!region || 
	  (region->host_type == HOST_REGION_NOTHING) || 
	  (region->host_type == HOST_REGION_UNALLOCATED) || 
	  (region->host_type == HOST_REGION_MEMORY_MAPPED_DEVICE) || 
	  (region->host_type == HOST_REGION_REMOTE) ||
	  (region->host_type == HOST_REGION_SWAPPED)) {
	pte[j].present = 0;
	pte[j].flags = 0;
	pte[j].accessed = 0;
	pte[j].dirty = 0;
	pte[j].pte_attr = 0;
	pte[j].global_page = 0;
	pte[j].vmm_info = 0;
	pte[j].page_base_addr = 0;
      } else {
	addr_t host_addr;
	pte[j].present = 1;
	pte[j].flags = VM_READ | VM_WRITE | VM_EXEC | VM_USER;   
	
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
      os_hooks->free_page(pte);

      pde[i].present = 0;
      pde[i].flags = 0;
      pde[i].accessed = 0;
      pde[i].reserved = 0;
      pde[i].large_pages = 0;
      pde[i].global_page = 0;
      pde[i].vmm_info = 0;
      pde[i].pt_base_addr = 0;
    } else {
      pde[i].present = 1;
      pde[i].flags = VM_READ | VM_WRITE | VM_EXEC | VM_USER;
      pde[i].accessed = 0;
      pde[i].reserved = 0;
      pde[i].large_pages = 0;
      pde[i].global_page = 0;
      pde[i].vmm_info = 0;
      pde[i].pt_base_addr = PAGE_ALIGNED_ADDR(pte);
    }

  }

  return pde;
}






void PrintPDE32(void * virtual_address, pde32_t * pde)
{
  PrintDebug("PDE %p -> %p : present=%x, flags=%x, accessed=%x, reserved=%x, largePages=%x, globalPage=%x, kernelInfo=%x\n",
	      virtual_address,
	      (void *) (pde->pt_base_addr << PAGE_POWER),
	      pde->present,
	      pde->flags,
	      pde->accessed,
	      pde->reserved,
	      pde->large_pages,
	      pde->global_page,
	      pde->vmm_info);
}
  
void PrintPTE32(void * virtual_address, pte32_t * pte)
{
  PrintDebug("PTE %p -> %p : present=%x, flags=%x, accessed=%x, dirty=%x, pteAttribute=%x, globalPage=%x, vmm_info=%x\n",
	      virtual_address,
	      (void*)(pte->page_base_addr << PAGE_POWER),
	      pte->present,
	      pte->flags,
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
  for (i = 0; (i < MAX_PDE32_ENTRIES) && pde[i].present; i++) { 
    PrintPDE32((void*)(PAGE_SIZE * MAX_PTE32_ENTRIES * i), &(pde[i]));
  }
}

void PrintPT32(void * starting_address, pte32_t * pte) 
{
  int i;

  PrintDebug("Page Table at %p:\n", pte);
  for (i = 0; (i < MAX_PTE32_ENTRIES) && pte[i].present; i++) { 
    PrintPTE32(starting_address + (PAGE_SIZE * i), &(pte[i]));
  }
}





void PrintDebugPageTables(pde32_t * pde)
{
  int i;
  
  PrintDebug("Dumping the pages starting with the pde page at %p\n", pde);

  for (i = 0; (i < MAX_PDE32_ENTRIES) && pde[i].present; i++) { 
    PrintPDE32((void *)(PAGE_SIZE * MAX_PTE32_ENTRIES * i), &(pde[i]));
    PrintPT32((void *)(PAGE_SIZE * MAX_PTE32_ENTRIES * i), (void *)(pde[i].pt_base_addr << PAGE_POWER));
  }
}
    
