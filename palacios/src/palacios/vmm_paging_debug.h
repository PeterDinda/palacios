
#ifdef USE_VMM_PAGING_DEBUG

/* 
 * 
 *  This is an implementation file that gets included only in vmm_paging.c
 * 
 */


static void PrintPDE32(addr_t virtual_address, pde32_t * pde)
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

  
static void PrintPTE32(addr_t virtual_address, pte32_t * pte)
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








static void PrintPDPE32PAE(addr_t virtual_address, pdpe32pae_t * pdpe)
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

static void PrintPDE32PAE(addr_t virtual_address, pde32pae_t * pde)
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

  
static void PrintPTE32PAE(addr_t virtual_address, pte32pae_t * pte)
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








static void PrintPML4e64(addr_t virtual_address, pml4e64_t * pml)
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

static void PrintPDPE64(addr_t virtual_address, pdpe64_t * pdpe)
{
  PrintDebug("PDPE64 %p -> %p : present=%x, writable=%x, user=%x, wt=%x, cd=%x, accessed=%x, reserved=%x, largePages=%x, globalPage/zero=%x, kernelInfo=%x\n",
	     (void *)virtual_address,
	     (void *)(addr_t) (BASE_TO_PAGE_ADDR(pdpe->pd_base_addr)),
	     pdpe->present,
	     pdpe->writable,
	     pdpe->user_page, 
	     pdpe->write_through,
	     pdpe->cache_disable,
	     pdpe->accessed,
	     pdpe->avail,
	     pdpe->large_page,
	     pdpe->zero,
	     pdpe->vmm_info);
}



static void PrintPDE64(addr_t virtual_address, pde64_t * pde)
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
	     pde->avail,
	     pde->large_page,
	     pde->global_page,
	     pde->vmm_info);
}

  
static void PrintPTE64(addr_t virtual_address, pte64_t * pte)
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

  












static int print_page_tree_cb(page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data) {

  switch (type) {

    /* 64 Bit */

  case PAGE_PML464:
    {
      pml4e64_t * pml = (pml4e64_t *)page_ptr;
      PrintPML4e64(vaddr, &(pml[PML4E64_INDEX(vaddr)]));
      break;
    }
  case PAGE_PDP64:
    {
      pdpe64_t * pdp = (pdpe64_t *)page_ptr;
      PrintPDPE64(vaddr, &(pdp[PDPE64_INDEX(vaddr)]));
      break;
    }
  case PAGE_PD64:
    {
      pde64_t * pd = (pde64_t *)page_ptr;
      PrintPDE64(vaddr, &(pd[PDE64_INDEX(vaddr)]));
      break;
    }
  case PAGE_PT64:
    {
      pte64_t * pt = (pte64_t *)page_ptr;
      PrintPTE64(vaddr, &(pt[PTE64_INDEX(vaddr)]));
      break;
    }

    /* 32 BIT PAE */
    
  case PAGE_PDP32PAE:
    {
      pdpe32pae_t * pdp = (pdpe32pae_t *)page_ptr;
      PrintPDPE32PAE(vaddr, &(pdp[PDPE32PAE_INDEX(vaddr)]));
      break;
    }
  case PAGE_PD32PAE:
    {
      pde32pae_t * pd = (pde32pae_t *)page_ptr;
      PrintPDE32PAE(vaddr, &(pd[PDE32PAE_INDEX(vaddr)]));
      break;
    }
  case PAGE_PT32PAE:
    {
      pte32pae_t * pt = (pte32pae_t *)page_ptr;
      PrintPTE32PAE(vaddr, &(pt[PTE32PAE_INDEX(vaddr)]));
      break;
    }

    /* 32 Bit */

  case PAGE_PD32:
    {
      pde32_t * pd = (pde32_t *)page_ptr;
      PrintPDE32(vaddr, &(pd[PDE32_INDEX(vaddr)]));
      break;
    }
  case PAGE_PT32:
    {
      pte32_t * pt = (pte32_t *)page_ptr;
      PrintPTE32(vaddr, &(pt[PTE32_INDEX(vaddr)]));
      break;
    }
  default:
    PrintDebug("%s %p->%p \n", v3_page_type_to_str(type), (void *)vaddr, (void *)page_pa);
    break;
  }

  return 0;
}



void PrintPTEntry(page_type_t type, addr_t vaddr, void * entry) {
  print_page_tree_cb(type, vaddr, PAGE_ADDR_4KB((addr_t)entry), 0, NULL);
}


void PrintHostPageTables(v3_vm_cpu_mode_t cpu_mode, addr_t cr3) {
  switch (cpu_mode) {
  case PROTECTED:
    v3_walk_host_pt_32(cr3, print_page_tree_cb, NULL);
  case PROTECTED_PAE:
    v3_walk_host_pt_32pae(cr3, print_page_tree_cb, NULL);
  case LONG:
  case LONG_32_COMPAT:
  case LONG_16_COMPAT:
    v3_walk_host_pt_64(cr3, print_page_tree_cb, NULL);
    break;
  default:
    PrintError("Unsupported CPU MODE %s\n", v3_cpu_mode_to_str(cpu_mode));
    break;
  }
}


void PrintGuestPageTables(struct guest_info * info, addr_t cr3) {
  switch (info->cpu_mode) {
  case PROTECTED:
    v3_walk_guest_pt_32(info, cr3, print_page_tree_cb, NULL);
  case PROTECTED_PAE:
    v3_walk_guest_pt_32pae(info, cr3, print_page_tree_cb, NULL);
  case LONG:
  case LONG_32_COMPAT:
  case LONG_16_COMPAT:
    v3_walk_guest_pt_64(info, cr3, print_page_tree_cb, NULL);
    break;
  default:
    PrintError("Unsupported CPU MODE %s\n", v3_cpu_mode_to_str(info->cpu_mode));
    break;
  }
}

void PrintHostPageTree(v3_vm_cpu_mode_t cpu_mode, addr_t virtual_addr, addr_t cr3) {
  switch (cpu_mode) {
  case PROTECTED:
    v3_drill_host_pt_32(cr3, virtual_addr, print_page_tree_cb, NULL);
  case PROTECTED_PAE:
    v3_drill_host_pt_32pae(cr3, virtual_addr, print_page_tree_cb, NULL);
  case LONG:
  case LONG_32_COMPAT:
  case LONG_16_COMPAT:
    v3_drill_host_pt_64(cr3, virtual_addr, print_page_tree_cb, NULL);
    break;
  default:
    PrintError("Unsupported CPU MODE %s\n", v3_cpu_mode_to_str(cpu_mode));
    break;
  }
}

void PrintGuestPageTree(struct guest_info * info, addr_t virtual_addr, addr_t cr3) {
  switch (info->cpu_mode) {
  case PROTECTED:
    v3_drill_guest_pt_32(info, cr3, virtual_addr, print_page_tree_cb, NULL);
  case PROTECTED_PAE:
    v3_drill_guest_pt_32pae(info, cr3, virtual_addr, print_page_tree_cb, NULL);
  case LONG:
  case LONG_32_COMPAT:
  case LONG_16_COMPAT:
    v3_drill_guest_pt_64(info, cr3, virtual_addr, print_page_tree_cb, NULL);
    break;
  default:
    PrintError("Unsupported CPU MODE %s\n", v3_cpu_mode_to_str(info->cpu_mode));
    break;
  }
}


#endif
