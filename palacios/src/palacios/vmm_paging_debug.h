
#ifdef USE_VMM_PAGING_DEBUG

/* 
 * 
 *  This is an implementation file that gets included only in vmm_paging.c
 * 
 */


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
    PrintPageTree_64(virtual_addr, CR3_TO_PML4E64_VA(cr3));
    break;
  default:
    PrintError("Unsupported CPU MODE %d\n", cpu_mode);
    break;
  }
}


#endif
