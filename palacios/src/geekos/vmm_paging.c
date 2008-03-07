#include <geekos/vmm_paging.h>

#include <geekos/vmm.h>



extern struct vmm_os_hooks * os_hooks;




/* We generate a page table to correspond to a given memory layout
 * pulling pages from the mem_list when necessary
 * If there are any gaps in the layout, we add them as unmapped pages
 */
vmm_pde_t * generate_guest_page_tables(vmm_mem_layout_t * layout, vmm_mem_list_t * list) {
  ullong_t current_page_addr = 0;
  uint_t layout_index = 0;
  uint_t list_index = 0;
  ullong_t layout_addr = 0;
  int i, j;
  uint_t num_entries = layout->num_pages;  // The number of pages left in the layout


  

  vmm_pde_t * pde = os_hooks->allocate_pages(1);

  for (i = 0; i < MAX_PAGE_DIR_ENTRIES; i++) {
    if (num_entries == 0) { 
      pde[i].present = 0;
      pde[i].flags = 0;
      pde[i].accessed = 0;
      pde[i].reserved = 0;
      pde[i].large_pages = 0;
      pde[i].global_page = 0;
      pde[i].vmm_info = 0;
      pde[i].pt_base_addr = 0;
    } else {
      vmm_pte_t * pte = os_hooks->allocate_pages(1);

      pde[i].present = 1;
      pde[i].flags = VM_READ | VM_WRITE | VM_EXEC | VM_USER;
      pde[i].accessed = 0;
      pde[i].reserved = 0;
      pde[i].large_pages = 0;
      pde[i].global_page = 0;
      pde[i].vmm_info = 0;
      pde[i].pt_base_addr = PAGE_ALLIGNED_ADDR(pte);



      for (j = 0; j < MAX_PAGE_TABLE_ENTRIES; j++) {
	layout_addr = get_mem_layout_addr(layout, layout_index);
	
	if ((current_page_addr < layout_addr) || (num_entries == 0)) {
	  // We have a gap in the layout, fill with unmapped page
	  pte[j].present = 0;
	  pte[j].flags = 0;
	  pte[j].accessed = 0;
	  pte[j].dirty = 0;
	  pte[j].pte_attr = 0;
	  pte[j].global_page = 0;
	  pte[j].vmm_info = 0;
	  pte[j].page_base_addr = 0;

	  current_page_addr += PAGE_SIZE;
	} else if (current_page_addr == layout_addr) {
	  // Set up the Table entry to map correctly to the layout region
	  layout_region_t * page_region = get_mem_layout_region(layout, layout_addr);

	  if (page_region->type == UNMAPPED) {
	    pte[j].present = 0;
	    pte[j].flags = 0;
	  } else {
	    pte[j].present = 1;
	    pte[j].flags = VM_READ | VM_WRITE | VM_EXEC | VM_USER;
	  }	    

	  pte[j].accessed = 0;
	  pte[j].dirty = 0;
	  pte[j].pte_attr = 0;
	  pte[j].global_page = 0;
	  pte[j].vmm_info = 0;

	  if (page_region->type == UNMAPPED) {
	    pte[j].page_base_addr = 0;
	  } else if (page_region->type == SHARED) {
	    addr_t host_addr = page_region->host_addr + (layout_addr - page_region->start);

	    pte[j].page_base_addr = host_addr >> 12;
	    pte[j].vmm_info = SHARED_PAGE;
	  } else if (page_region->type == GUEST) {
	    addr_t list_addr =  get_mem_list_addr(list, list_index++);
	    
	    if (list_addr == -1) {
	      // error
	      // cleanup...
	      free_guest_page_tables(pde);
	      return NULL;
	    }
	    PrintDebug("Adding guest page (%x)\n", list_addr);
	    pte[j].page_base_addr = list_addr >> 12;
	    
	    // Reset this when we move over to dynamic page allocation
	    //	    pte[j].vmm_info = GUEST_PAGE;	    
	    pte[j].vmm_info = SHARED_PAGE;
	  }

	  num_entries--;
	  current_page_addr += PAGE_SIZE;
	  layout_index++;
	} else {
	  // error
	  PrintDebug("Error creating page table...\n");
	  // cleanup
	  free_guest_page_tables(pde);
	  return NULL;
	}
      }
    }
  }

  return pde;
}


void free_guest_page_tables(vmm_pde_t * pde) {
  int i, j;


  for (i = 0; (i < MAX_PAGE_DIR_ENTRIES); i++) {
    if (pde[i].present) {
      vmm_pte_t * pte = (vmm_pte_t *)(pde[i].pt_base_addr << PAGE_POWER);
      
      for (j = 0; (j < MAX_PAGE_TABLE_ENTRIES); j++) {
	if ((pte[j].present) && (pte[j].vmm_info & GUEST_PAGE)){
	  os_hooks->free_page((void *)(pte[j].page_base_addr  << PAGE_POWER));
	}
      }
      
      os_hooks->free_page(pte);
    }
  }

  os_hooks->free_page(pde);
}




void PrintPDE(void * virtual_address, vmm_pde_t * pde)
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
  
void PrintPTE(void * virtual_address, vmm_pte_t * pte)
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



void PrintPD(vmm_pde_t * pde)
{
  int i;

  PrintDebug("Page Directory at %p:\n", pde);
  for (i = 0; (i < MAX_PAGE_DIR_ENTRIES) && pde[i].present; i++) { 
    PrintPDE((void*)(PAGE_SIZE * MAX_PAGE_TABLE_ENTRIES * i), &(pde[i]));
  }
}

void PrintPT(void * starting_address, vmm_pte_t * pte) 
{
  int i;

  PrintDebug("Page Table at %p:\n", pte);
  for (i = 0; (i < MAX_PAGE_TABLE_ENTRIES) && pte[i].present; i++) { 
    PrintPTE(starting_address + (PAGE_SIZE * i), &(pte[i]));
  }
}





void PrintDebugPageTables(vmm_pde_t * pde)
{
  int i;
  
  PrintDebug("Dumping the pages starting with the pde page at %p\n", pde);

  for (i = 0; (i < MAX_PAGE_DIR_ENTRIES) && pde[i].present; i++) { 
    PrintPDE((void *)(PAGE_SIZE * MAX_PAGE_TABLE_ENTRIES * i), &(pde[i]));
    PrintPT((void *)(PAGE_SIZE * MAX_PAGE_TABLE_ENTRIES * i), (void *)(pde[i].pt_base_addr << PAGE_POWER));
  }
}
    
    
