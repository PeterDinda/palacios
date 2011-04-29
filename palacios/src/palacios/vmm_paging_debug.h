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

  






static int print_page_walk_cb(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data) {
    int i = 0;
    addr_t tmp_vaddr = 0;
    switch (type) {

	/* 64 Bit */

	case PAGE_PML464:
	    {
		pml4e64_t * pml = (pml4e64_t *)page_ptr;
		PrintDebug("PML4E64 Page\n");
		for (i = 0; i < MAX_PML4E64_ENTRIES; i++) {
		    tmp_vaddr = (4096 * MAX_PTE64_ENTRIES);
		    tmp_vaddr *= (MAX_PDE64_ENTRIES * MAX_PDPE64_ENTRIES * i); // break apart to avoid int overflow compile errors
		    tmp_vaddr += vaddr;
		    if (pml[i].present) 
			PrintPML4e64(tmp_vaddr, &(pml[i]));
		}
		break;
	    }
	case PAGE_PDP64:
	    {
		pdpe64_t * pdp = (pdpe64_t *)page_ptr;
		PrintDebug("PDPE64 Page\n");
		for (i = 0; i < MAX_PDPE64_ENTRIES; i++) {
		    tmp_vaddr = 4096 * MAX_PTE64_ENTRIES * MAX_PDE64_ENTRIES * i; 
		    tmp_vaddr += vaddr;
		    if (pdp[i].present)
			PrintPDPE64(tmp_vaddr, &(pdp[i]));
		}
		break;
	    }
	case PAGE_PD64:
	    {
		pde64_t * pd = (pde64_t *)page_ptr;
		PrintDebug("PDE64 Page\n");
		for (i = 0; i < MAX_PDE64_ENTRIES; i++) {
		    tmp_vaddr = 4096 * MAX_PTE64_ENTRIES * i; 
		    tmp_vaddr += vaddr;
		    if (pd[i].present)
			PrintPDE64(tmp_vaddr, &(pd[i]));
		}
		break;
	    }
	case PAGE_PT64:
	    {
		pte64_t * pt = (pte64_t *)page_ptr;
		PrintDebug("PTE64 Page\n");
		for (i = 0; i < MAX_PTE64_ENTRIES; i++) {
		    tmp_vaddr = 4096 * i; 
		    tmp_vaddr += vaddr;
		    if (pt[i].present)
			PrintPTE64(tmp_vaddr, &(pt[i]));
		}
		break;
	    }

	    /* 32 BIT PAE */
    
	case PAGE_PDP32PAE:
	    {
		pdpe32pae_t * pdp = (pdpe32pae_t *)page_ptr;
		PrintDebug("PDPE32PAE Page\n");
		for (i = 0; i < MAX_PDPE32PAE_ENTRIES; i++) {
		    tmp_vaddr = 4096 * MAX_PTE32PAE_ENTRIES * MAX_PDE32PAE_ENTRIES * i; 
		    tmp_vaddr += vaddr;
		    if (pdp[i].present) 
			PrintPDPE32PAE(tmp_vaddr, &(pdp[i]));
		}
		break;
	    }
	case PAGE_PD32PAE:
	    {
		pde32pae_t * pd = (pde32pae_t *)page_ptr;
		PrintDebug("PDE32PAE Page\n");
		for (i = 0; i < MAX_PDE32PAE_ENTRIES; i++) {
		    tmp_vaddr = 4096 * MAX_PTE32PAE_ENTRIES * i; 
		    tmp_vaddr += vaddr;
		    if (pd[i].present)
			PrintPDE32PAE(tmp_vaddr, &(pd[i]));
		}
		break;
	    }
	case PAGE_PT32PAE:
	    {
		pte32pae_t * pt = (pte32pae_t *)page_ptr;
		PrintDebug("PTE32PAE Page\n");
		for (i = 0; i < MAX_PTE32PAE_ENTRIES; i++) {
		    tmp_vaddr = 4096 * i; 
		    tmp_vaddr += vaddr;
		    if (pt[i].present) 
			PrintPTE32PAE(tmp_vaddr, &(pt[i]));
		}
		break;
	    }

	    /* 32 Bit */

	case PAGE_PD32:
	    {
		pde32_t * pd = (pde32_t *)page_ptr;
		PrintDebug("PDE32 Page\n");
		for (i = 0; i < MAX_PTE32_ENTRIES; i++) {
		    tmp_vaddr = 4096 * MAX_PTE32_ENTRIES * i; 
		    tmp_vaddr += vaddr;
		    if (pd[i].present)
			PrintPDE32(tmp_vaddr, &(pd[i]));
		}
		break;
	    }
	case PAGE_PT32:
	    {
		pte32_t * pt = (pte32_t *)page_ptr;
		PrintDebug("PTE32 Page\n");
		for (i = 0; i < MAX_PTE32_ENTRIES; i++) {
		    tmp_vaddr = 4096 * i; 
		    tmp_vaddr += vaddr;
		    if (pt[i].present)
			PrintPTE32(tmp_vaddr, &(pt[i]));
		}
		break;
	    }
	default:
	    break;
    }

    return 0;
}





static int print_page_tree_cb(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data) {
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



void PrintPTEntry(struct guest_info * info, page_type_t type, addr_t vaddr, void * entry) {
    print_page_tree_cb(info, type, vaddr, PAGE_ADDR_4KB((addr_t)entry), 0, NULL);
}


void PrintHostPageTables(struct guest_info * info, v3_cpu_mode_t cpu_mode, addr_t cr3) {
    PrintDebug("CR3: %p\n", (void *)cr3);
    switch (cpu_mode) {
	case PROTECTED:
	    v3_walk_host_pt_32(info, cr3, print_page_walk_cb, NULL);
	    break;
	case PROTECTED_PAE:
	    v3_walk_host_pt_32pae(info, cr3, print_page_walk_cb, NULL);
	    break;
	case LONG:
	case LONG_32_COMPAT:
	case LONG_16_COMPAT:
	    v3_walk_host_pt_64(info, cr3, print_page_walk_cb, NULL);
	    break;
	default:
	    PrintError("Unsupported CPU MODE %s\n", v3_cpu_mode_to_str(info->cpu_mode));
	    break;
    }
}


void PrintGuestPageTables(struct guest_info * info, addr_t cr3) {
    PrintDebug("CR3: %p\n", (void *)cr3);
    switch (info->cpu_mode) {
	case REAL:
	case PROTECTED:
	    v3_walk_guest_pt_32(info, cr3, print_page_walk_cb, NULL);
	    break;
	case PROTECTED_PAE:
	    v3_walk_guest_pt_32pae(info, cr3, print_page_walk_cb, NULL);
	    break;
	case LONG:
	case LONG_32_COMPAT:
	case LONG_16_COMPAT:
	    v3_walk_guest_pt_64(info, cr3, print_page_walk_cb, NULL);
	    break;
	default:
	    PrintError("Unsupported CPU MODE %s\n", v3_cpu_mode_to_str(info->cpu_mode));
	    break;
    }
}

void PrintHostPageTree(struct guest_info * info,  addr_t virtual_addr, addr_t cr3) {
    PrintDebug("CR3: %p\n", (void *)cr3);
    switch (info->cpu_mode) {
	case PROTECTED:
	    v3_drill_host_pt_32(info, cr3, virtual_addr, print_page_tree_cb, NULL);
	    break;
	case PROTECTED_PAE:
	    v3_drill_host_pt_32pae(info, cr3, virtual_addr, print_page_tree_cb, NULL);
	    break;
	case LONG:
	case LONG_32_COMPAT:
	case LONG_16_COMPAT:
	    v3_drill_host_pt_64(info, cr3, virtual_addr, print_page_tree_cb, NULL);
	    break;
	default:
	    PrintError("Unsupported CPU MODE %s\n", v3_cpu_mode_to_str(info->cpu_mode));
	    break;
    }
}

void PrintGuestPageTree(struct guest_info * info, addr_t virtual_addr, addr_t cr3) {
    PrintDebug("CR3: %p\n", (void *)cr3);
    switch (info->cpu_mode) {
	case PROTECTED:
	    v3_drill_guest_pt_32(info, cr3, virtual_addr, print_page_tree_cb, NULL);
	    break;
	case PROTECTED_PAE:
	    v3_drill_guest_pt_32pae(info, cr3, virtual_addr, print_page_tree_cb, NULL);
	    break;
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
