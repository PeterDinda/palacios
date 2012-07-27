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


static pt_entry_type_t pde32_lookup(pde32_t * pd, addr_t addr, addr_t * entry);
static pt_entry_type_t pte32_lookup(pte32_t * pt, addr_t addr, addr_t * entry);

static pt_entry_type_t pdpe32pae_lookup(pdpe32pae_t * pdp, addr_t addr, addr_t * entry);
static pt_entry_type_t pde32pae_lookup(pde32pae_t * pd, addr_t addr, addr_t * entry);
static pt_entry_type_t pte32pae_lookup(pte32pae_t * pt, addr_t addr, addr_t * entry);

static pt_entry_type_t pml4e64_lookup(pml4e64_t * pml, addr_t addr, addr_t * entry);
static pt_entry_type_t pdpe64_lookup(pdpe64_t * pdp, addr_t addr, addr_t * entry);
static pt_entry_type_t pde64_lookup(pde64_t * pd, addr_t addr, addr_t * entry);
static pt_entry_type_t pte64_lookup(pte64_t * pt, addr_t addr, addr_t * entry);




#define USE_VMM_PAGING_DEBUG
// All of the debug functions defined in vmm_paging.h are implemented in this file
#include "vmm_paging_debug.h"
#undef USE_VMM_PAGING_DEBUG



#ifndef V3_CONFIG_DEBUG_SHADOW_PAGING
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif



void delete_page_tables_32(pde32_t * pde) {
    int i;

    if (pde == NULL) { 
	return;
    }

    PrintDebug("Deleting Page Tables (32) -- PDE (%p)\n", pde);

    for (i = 0; i < MAX_PDE32_ENTRIES; i++) {
	if ((pde[i].present == 1) && (pde[i].large_page == 0)) {
	    // We double cast, first to an addr_t to handle 64 bit issues, then to the pointer
      
	    PrintDebug("Deleting PT Page %d (%p)\n", i, (void *)(addr_t)BASE_TO_PAGE_ADDR_4KB(pde[i].pt_base_addr));
	    V3_FreePages((void *)(addr_t)BASE_TO_PAGE_ADDR_4KB(pde[i].pt_base_addr), 1);
	}
    }

    V3_FreePages(V3_PAddr(pde), 1);
}

void delete_page_tables_32pae(pdpe32pae_t * pdpe) {
    int i, j;

    if (pdpe == NULL) {
	return;
    }

    PrintDebug("Deleting Page Tables (32 PAE) -- PDPE (%p)\n", pdpe);
    
    for (i = 0; i < MAX_PDPE32PAE_ENTRIES; i++) {
	if (pdpe[i].present == 0) {
	    continue;
	}

	pde32pae_t * pde = (pde32pae_t *)V3_VAddr((void *)(addr_t)BASE_TO_PAGE_ADDR_4KB(pdpe[i].pd_base_addr));

	for (j = 0; j < MAX_PDE32PAE_ENTRIES; j++) {

	    if ((pde[j].present == 0) || (pde[j].large_page == 1)) {
		continue;
	    }

	    V3_FreePages((void *)(addr_t)BASE_TO_PAGE_ADDR_4KB(pde[j].pt_base_addr), 1);
	}

	V3_FreePages(V3_PAddr(pde), 1);
    }

    V3_FreePages(V3_PAddr(pdpe), 1);
}

void delete_page_tables_64(pml4e64_t * pml4) {
    int i, j, k;

    if (pml4 == NULL) {
	return;
    }

    PrintDebug("Deleting Page Tables (64) -- PML4 (%p)\n", pml4);

    for (i = 0; i < MAX_PML4E64_ENTRIES; i++) {
	if (pml4[i].present == 0) {
	    continue;
	}

	pdpe64_t * pdpe = (pdpe64_t *)V3_VAddr((void *)(addr_t)BASE_TO_PAGE_ADDR_4KB(pml4[i].pdp_base_addr));

	for (j = 0; j < MAX_PDPE64_ENTRIES; j++) {
	    if ((pdpe[j].present == 0) || (pdpe[j].large_page == 1)) {
		continue;
	    }

	    pde64_t * pde = (pde64_t *)V3_VAddr((void *)(addr_t)BASE_TO_PAGE_ADDR_4KB(pdpe[j].pd_base_addr));

	    for (k = 0; k < MAX_PDE64_ENTRIES; k++) {
		if ((pde[k].present == 0) || (pde[k].large_page == 1)) {
		    continue;
		}

		V3_FreePages((void *)(addr_t)BASE_TO_PAGE_ADDR_4KB(pde[k].pt_base_addr), 1);
	    }
	    
	    V3_FreePages(V3_PAddr(pde), 1);
	}

	V3_FreePages(V3_PAddr(pdpe), 1);
    }

    V3_FreePages(V3_PAddr(pml4), 1);
}




static int translate_pt_32_cb(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data) {
    addr_t * paddr = (addr_t *)private_data;

    switch (type) {
	case PAGE_PD32:
	case PAGE_PT32:
	    return 0;
	case PAGE_4MB:
	    *paddr = page_pa + PAGE_OFFSET_4MB(vaddr);
	    return 0;
	case PAGE_4KB:
	    *paddr = page_pa + PAGE_OFFSET_4KB(vaddr);
	    return 0;
	default: 
	    PrintError("Inavlid page type (%s) in tranlate pt 32 callback\n", v3_page_type_to_str(type));
	    return -1;
    }
}

static int translate_pt_32pae_cb(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data) {
    addr_t * paddr = (addr_t *)private_data;
  
    switch (type) {
	case PAGE_PDP32PAE:
	case PAGE_PD32PAE:
	case PAGE_PT32PAE:
	    return 0;
	case PAGE_2MB:
	    *paddr = page_pa + PAGE_OFFSET_2MB(vaddr);
	    return 0;
	case PAGE_4KB:
	    *paddr = page_pa + PAGE_OFFSET_4KB(vaddr);
	    return 0;
	default:
	    PrintError("Inavlid page type (%s) in translate pt 32pae callback\n", v3_page_type_to_str(type));
	    return -1;
    }
}

static int translate_pt_64_cb(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data) {
    addr_t * paddr = (addr_t *)private_data;

    switch (type) {
	case PAGE_PML464:
	case PAGE_PDP64:
	case PAGE_PD64:
	case PAGE_PT64:
	    return 0;
	case PAGE_1GB:
	    *paddr = page_pa + PAGE_OFFSET_1GB(vaddr);
	    return 0;
	case PAGE_2MB:
	    *paddr = page_pa + PAGE_OFFSET_2MB(vaddr);
	    return 0;
	case PAGE_4KB:
	    *paddr = page_pa + PAGE_OFFSET_4KB(vaddr);
	    return 0;
	default:
	    PrintError("Inavlid page type (%s) in translate pt 64 callback\n", v3_page_type_to_str(type));
	    return -1;
    }
}


int v3_translate_host_pt_32(struct guest_info * info, v3_reg_t host_cr3, addr_t vaddr, addr_t * paddr) {
    return v3_drill_host_pt_32(info, host_cr3, vaddr, translate_pt_32_cb, paddr);
}
int v3_translate_guest_pt_32(struct guest_info * info, v3_reg_t guest_cr3, addr_t vaddr, addr_t * paddr) {
    return v3_drill_guest_pt_32(info, guest_cr3, vaddr, translate_pt_32_cb, paddr);
}


int v3_translate_host_pt_32pae(struct guest_info * info, v3_reg_t host_cr3, addr_t vaddr, addr_t * paddr) {
    return v3_drill_host_pt_32pae(info, host_cr3, vaddr, translate_pt_32pae_cb, paddr);
}
int v3_translate_guest_pt_32pae(struct guest_info * info, v3_reg_t guest_cr3, addr_t vaddr, addr_t * paddr) {
    return v3_drill_guest_pt_32pae(info, guest_cr3, vaddr, translate_pt_32pae_cb, paddr);
}


int v3_translate_host_pt_64(struct guest_info * info, v3_reg_t host_cr3, addr_t vaddr, addr_t * paddr) {
    return v3_drill_host_pt_64(info, host_cr3, vaddr, translate_pt_64_cb, paddr);
}
int v3_translate_guest_pt_64(struct guest_info * info, v3_reg_t guest_cr3, addr_t vaddr, addr_t * paddr) {
    return v3_drill_guest_pt_64(info, guest_cr3, vaddr, translate_pt_64_cb, paddr);
}



struct pt_find_data {
    page_type_t type;
    addr_t * pt_page_ptr;
    addr_t * pt_page_pa;
};

static int find_pt_cb(struct guest_info * info, page_type_t type, addr_t vaddr, 
		      addr_t page_ptr, addr_t page_pa, void * private_data) {
    struct pt_find_data * pt_data = (struct pt_find_data *)private_data;

    PrintDebug("FIND_PT Type=%s, page_pa = %p\n", 	     
	       v3_page_type_to_str(type),
	       (void *)page_pa);

    if (type == pt_data->type) {
	*(pt_data->pt_page_ptr) = page_ptr;
	*(pt_data->pt_page_pa) = page_pa;
	return 1;
    }

    return 0;
}


int v3_find_host_pt_32_page(struct guest_info * info, v3_reg_t host_cr3, page_type_t type, addr_t vaddr, 
			    addr_t * page_ptr, addr_t * page_pa) {
    struct pt_find_data data;

    data.type = type;
    data.pt_page_ptr = page_ptr;
    data.pt_page_pa = page_pa;
  
    return v3_drill_host_pt_32(info, host_cr3, vaddr, find_pt_cb, &data);
}

int v3_find_host_pt_32pae_page(struct guest_info * info, v3_reg_t host_cr3, page_type_t type, addr_t vaddr, 
			       addr_t * page_ptr, addr_t * page_pa) {
    struct pt_find_data data;

    data.type = type;
    data.pt_page_ptr = page_ptr;
    data.pt_page_pa = page_pa;  

    return v3_drill_host_pt_32pae(info, host_cr3, vaddr, find_pt_cb, &data);
}

int v3_find_host_pt_64_page(struct guest_info * info, v3_reg_t host_cr3, page_type_t type, addr_t vaddr, 
			    addr_t * page_ptr, addr_t * page_pa) {
    struct pt_find_data data;

    data.type = type;
    data.pt_page_ptr = page_ptr;
    data.pt_page_pa = page_pa;

    return v3_drill_host_pt_64(info, host_cr3, vaddr, find_pt_cb, &data);
}
int v3_find_guest_pt_32_page(struct guest_info * info, v3_reg_t guest_cr3, page_type_t type, addr_t vaddr, 
			     addr_t * page_ptr, addr_t * page_pa) {
    struct pt_find_data data;

    data.type = type;
    data.pt_page_ptr = page_ptr;
    data.pt_page_pa = page_pa;
  
    return v3_drill_guest_pt_32(info, guest_cr3, vaddr, find_pt_cb, &data);
}

int v3_find_guest_pt_32pae_page(struct guest_info * info, v3_reg_t guest_cr3, page_type_t type, addr_t vaddr, 
				addr_t * page_ptr, addr_t * page_pa) {
    struct pt_find_data data;

    data.type = type;
    data.pt_page_ptr = page_ptr;
    data.pt_page_pa = page_pa;
  
    return v3_drill_guest_pt_32pae(info, guest_cr3, vaddr, find_pt_cb, &data);
}

int v3_find_guest_pt_64_page(struct guest_info * info, v3_reg_t guest_cr3, page_type_t type, addr_t vaddr, 
			     addr_t * page_ptr, addr_t * page_pa) {
    struct pt_find_data data;

    data.type = type;
    data.pt_page_ptr = page_ptr;
    data.pt_page_pa = page_pa;
  
    return v3_drill_guest_pt_64(info, guest_cr3, vaddr, find_pt_cb, &data);
}


/* 
 *
 * Page Table Access Checks
 *
 */


struct pt_check_data {
    pf_error_t access_type;
    pt_access_status_t * access_status;
};

static int check_pt_32_cb(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data) {
    struct pt_check_data * chk_data = (struct pt_check_data *)private_data;

    switch (type) {
	case PAGE_PD32:
	    *(chk_data->access_status) = v3_can_access_pde32((pde32_t *)page_ptr, vaddr, chk_data->access_type);
	    break;
	case PAGE_PT32:
	    *(chk_data->access_status) = v3_can_access_pte32((pte32_t *)page_ptr, vaddr, chk_data->access_type);
	    break;
	case PAGE_4MB:
	case PAGE_4KB:
	    return 0;
	default: 
	    PrintError("Inavlid page type (%s) in check pt 32 callback\n", v3_page_type_to_str(type));
	    return -1;
    }

    if (chk_data->access_status != PT_ACCESS_OK) {
	return 1;
    }

    return 0;
}


static int check_pt_32pae_cb(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data) {
    struct pt_check_data * chk_data = (struct pt_check_data *)private_data;

    switch (type) {
	case PAGE_PDP32PAE:
	    *(chk_data->access_status) = v3_can_access_pdpe32pae((pdpe32pae_t *)page_ptr, vaddr, chk_data->access_type);
	    break;
	case PAGE_PD32PAE:
	    *(chk_data->access_status) = v3_can_access_pde32pae((pde32pae_t *)page_ptr, vaddr, chk_data->access_type);
	    break;
	case PAGE_PT32PAE:
	    *(chk_data->access_status) = v3_can_access_pte32pae((pte32pae_t *)page_ptr, vaddr, chk_data->access_type);
	    break;
	case PAGE_2MB:
	case PAGE_4KB:
	    return 0;
	default: 
	    PrintError("Inavlid page type (%s) in check pt 32pae callback\n", v3_page_type_to_str(type));
	    return -1;
    }

    if (chk_data->access_status != PT_ACCESS_OK) {
	return 1;
    }

    return 0;
}


static int check_pt_64_cb(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data) {
    struct pt_check_data * chk_data = (struct pt_check_data *)private_data;

    switch (type) {
	case PAGE_PML464:
	    *(chk_data->access_status) = v3_can_access_pml4e64((pml4e64_t *)page_ptr, vaddr, chk_data->access_type);
	    break;
	case PAGE_PDP64:
	    *(chk_data->access_status) = v3_can_access_pdpe64((pdpe64_t *)page_ptr, vaddr, chk_data->access_type);
	    break;
	case PAGE_PD64:
	    *(chk_data->access_status) = v3_can_access_pde64((pde64_t *)page_ptr, vaddr, chk_data->access_type);
	    break;
	case PAGE_PT64:
	    *(chk_data->access_status) = v3_can_access_pte64((pte64_t *)page_ptr, vaddr, chk_data->access_type);
	    break;
	case PAGE_1GB:
	case PAGE_2MB:
	case PAGE_4KB:
	    return 0;
	default: 
	    PrintError("Inavlid page type (%s) in check pt 64 callback\n", v3_page_type_to_str(type));
	    return -1;
    }

    if (chk_data->access_status != PT_ACCESS_OK) {
	return 1;
    }

    return 0;
}



int v3_check_host_pt_32(struct guest_info * info, v3_reg_t host_cr3, addr_t vaddr, pf_error_t access_type, pt_access_status_t * access_status) {
    struct pt_check_data access_data;

    access_data.access_type = access_type;
    access_data.access_status = access_status;

    return v3_drill_host_pt_32(info, host_cr3, vaddr, check_pt_32_cb, &access_data);
}

int v3_check_host_pt_32pae(struct guest_info * info, v3_reg_t host_cr3, addr_t vaddr, pf_error_t access_type, pt_access_status_t * access_status) {
    struct pt_check_data access_data;

    access_data.access_type = access_type;
    access_data.access_status = access_status;

    return v3_drill_host_pt_32pae(info, host_cr3, vaddr, check_pt_32pae_cb, &access_data);
}



int v3_check_host_pt_64(struct guest_info * info, v3_reg_t host_cr3, addr_t vaddr, pf_error_t access_type, pt_access_status_t * access_status) {
    struct pt_check_data access_data;

    access_data.access_type = access_type;
    access_data.access_status = access_status;

    return v3_drill_host_pt_64(info, host_cr3, vaddr, check_pt_64_cb, &access_data);
}



int v3_check_guest_pt_32(struct guest_info * info, v3_reg_t guest_cr3, addr_t vaddr, 
			 pf_error_t access_type, pt_access_status_t * access_status) {
    struct pt_check_data access_data;

    access_data.access_type = access_type;
    access_data.access_status = access_status;

    return v3_drill_guest_pt_32(info, guest_cr3, vaddr, check_pt_32_cb, &access_data);
}





int v3_check_guest_pt_32pae(struct guest_info * info, v3_reg_t guest_cr3, addr_t vaddr, 
			    pf_error_t access_type, pt_access_status_t * access_status) {
    struct pt_check_data access_data;

    access_data.access_type = access_type;
    access_data.access_status = access_status;

    return v3_drill_guest_pt_32pae(info, guest_cr3, vaddr, check_pt_32pae_cb, &access_data);
}



int v3_check_guest_pt_64(struct guest_info * info, v3_reg_t guest_cr3, addr_t vaddr, 
			 pf_error_t access_type, pt_access_status_t * access_status) {
    struct pt_check_data access_data;

    access_data.access_type = access_type;
    access_data.access_status = access_status;

    return v3_drill_guest_pt_64(info, guest_cr3, vaddr, check_pt_64_cb, &access_data);
}


static int get_data_page_type_cb(struct guest_info * info, page_type_t type, 
				 addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data) {
    switch (type) {
	case PAGE_4KB:
	case PAGE_2MB:
	case PAGE_4MB:
	case PAGE_1GB:
	    return 1;
	default:
	    return 0;
    }
}



page_type_t v3_get_guest_data_page_type_32(struct guest_info * info, v3_reg_t cr3, addr_t vaddr) {
    return v3_drill_guest_pt_32(info, cr3, vaddr, get_data_page_type_cb, NULL);
}
page_type_t v3_get_guest_data_page_type_32pae(struct guest_info * info, v3_reg_t cr3, addr_t vaddr) {
    return v3_drill_guest_pt_32pae(info, cr3, vaddr, get_data_page_type_cb, NULL);
}
page_type_t v3_get_guest_data_page_type_64(struct guest_info * info, v3_reg_t cr3, addr_t vaddr) {
    return v3_drill_guest_pt_64(info, cr3, vaddr, get_data_page_type_cb, NULL);
}
page_type_t v3_get_host_data_page_type_32(struct guest_info * info, v3_reg_t cr3, addr_t vaddr) {
    return v3_drill_host_pt_32(info, cr3, vaddr, get_data_page_type_cb, NULL);
}
page_type_t v3_get_host_data_page_type_32pae(struct guest_info * info, v3_reg_t cr3, addr_t vaddr) {
    return v3_drill_host_pt_32pae(info, cr3, vaddr, get_data_page_type_cb, NULL);
}
page_type_t v3_get_host_data_page_type_64(struct guest_info * info, v3_reg_t cr3, addr_t vaddr) {
    return v3_drill_host_pt_64(info, cr3, vaddr, get_data_page_type_cb, NULL);
}


/*
 * PAGE TABLE LOOKUP FUNCTIONS
 *
 * The value of entry is a return type:
 * Page not present: *entry = 0
 */

/**
 * 
 *  32 bit Page Table lookup functions
 *
 **/

static pt_entry_type_t pde32_lookup(pde32_t * pd, addr_t addr, addr_t * entry) {
    pde32_t * pde_entry = &(pd[PDE32_INDEX(addr)]);

    if (!pde_entry->present) {
	*entry = 0;
	return PT_ENTRY_NOT_PRESENT;
    } else if (pde_entry->large_page) {
	pde32_4MB_t * large_pde = (pde32_4MB_t *)pde_entry;

	*entry = BASE_TO_PAGE_ADDR_4MB(large_pde->page_base_addr);

	return PT_ENTRY_LARGE_PAGE;
    } else {
	*entry = BASE_TO_PAGE_ADDR(pde_entry->pt_base_addr);
	return PT_ENTRY_PAGE;
    }
}



/* Takes a virtual addr (addr) and returns the physical addr (entry) as defined in the page table
 */
static pt_entry_type_t pte32_lookup(pte32_t * pt, addr_t addr, addr_t * entry) {
    pte32_t * pte_entry = &(pt[PTE32_INDEX(addr)]);

    if (!pte_entry->present) {
	*entry = 0;
	//    PrintDebug("Lookup at non present page (index=%d)\n", PTE32_INDEX(addr));
	return PT_ENTRY_NOT_PRESENT;
    } else {
	*entry = BASE_TO_PAGE_ADDR(pte_entry->page_base_addr);

	return PT_ENTRY_PAGE;
    }

}



/**
 * 
 *  32 bit PAE Page Table lookup functions
 *
 **/
static pt_entry_type_t pdpe32pae_lookup(pdpe32pae_t * pdp, addr_t addr, addr_t * entry) {
    pdpe32pae_t * pdpe_entry = &(pdp[PDPE32PAE_INDEX(addr)]);
  
    if (!pdpe_entry->present) {
	*entry = 0;
	return PT_ENTRY_NOT_PRESENT;
    } else {
	*entry = BASE_TO_PAGE_ADDR(pdpe_entry->pd_base_addr);
	return PT_ENTRY_PAGE;
    }
}

static pt_entry_type_t pde32pae_lookup(pde32pae_t * pd, addr_t addr, addr_t * entry) {
    pde32pae_t * pde_entry = &(pd[PDE32PAE_INDEX(addr)]);

    if (!pde_entry->present) {
	*entry = 0;
	return PT_ENTRY_NOT_PRESENT;
    } else if (pde_entry->large_page) {
	pde32pae_2MB_t * large_pde = (pde32pae_2MB_t *)pde_entry;

	*entry = BASE_TO_PAGE_ADDR_2MB(large_pde->page_base_addr);

	return PT_ENTRY_LARGE_PAGE;
    } else {
	*entry = BASE_TO_PAGE_ADDR(pde_entry->pt_base_addr);
	return PT_ENTRY_PAGE;
    }
}

static pt_entry_type_t pte32pae_lookup(pte32pae_t * pt, addr_t addr, addr_t * entry) {
    pte32pae_t * pte_entry = &(pt[PTE32PAE_INDEX(addr)]);

    if (!pte_entry->present) {
	*entry = 0;
	return PT_ENTRY_NOT_PRESENT;
    } else {
	*entry = BASE_TO_PAGE_ADDR(pte_entry->page_base_addr);
	return PT_ENTRY_PAGE;
    }
}



/**
 * 
 *  64 bit Page Table lookup functions
 *
 **/
static pt_entry_type_t pml4e64_lookup(pml4e64_t * pml, addr_t addr, addr_t * entry) {
    pml4e64_t * pml_entry = &(pml[PML4E64_INDEX(addr)]);

    if (!pml_entry->present) {
	*entry = 0;
	return PT_ENTRY_NOT_PRESENT;
    } else {
	*entry = BASE_TO_PAGE_ADDR(pml_entry->pdp_base_addr);
	return PT_ENTRY_PAGE;
    }
}

static pt_entry_type_t pdpe64_lookup(pdpe64_t * pdp, addr_t addr, addr_t * entry) {
    pdpe64_t * pdpe_entry = &(pdp[PDPE64_INDEX(addr)]);
  
    if (!pdpe_entry->present) {
	*entry = 0;
	return PT_ENTRY_NOT_PRESENT;
    } else if (pdpe_entry->large_page) {
	pdpe64_1GB_t * large_pdp = (pdpe64_1GB_t *)pdpe_entry;

	*entry = BASE_TO_PAGE_ADDR_1GB(large_pdp->page_base_addr);

	return PT_ENTRY_LARGE_PAGE;
    } else {
	*entry = BASE_TO_PAGE_ADDR(pdpe_entry->pd_base_addr);
	return PT_ENTRY_PAGE;
    }
}

static pt_entry_type_t pde64_lookup(pde64_t * pd, addr_t addr, addr_t * entry) {
    pde64_t * pde_entry = &(pd[PDE64_INDEX(addr)]);

    if (!pde_entry->present) {
	*entry = 0;
	return PT_ENTRY_NOT_PRESENT;
    } else if (pde_entry->large_page) {
	pde64_2MB_t * large_pde = (pde64_2MB_t *)pde_entry;

	*entry = BASE_TO_PAGE_ADDR_2MB(large_pde->page_base_addr);

	return PT_ENTRY_LARGE_PAGE;
    } else {
	*entry = BASE_TO_PAGE_ADDR(pde_entry->pt_base_addr);
	return PT_ENTRY_PAGE;
    }
}

static pt_entry_type_t pte64_lookup(pte64_t * pt, addr_t addr, addr_t * entry) {
    pte64_t * pte_entry = &(pt[PTE64_INDEX(addr)]);

    if (!pte_entry->present) {
	*entry = 0;
	return PT_ENTRY_NOT_PRESENT;
    } else {
	*entry = BASE_TO_PAGE_ADDR(pte_entry->page_base_addr);
	return PT_ENTRY_PAGE;
    }
}




static pt_access_status_t can_access_pt_entry(gen_pt_t * pt, pf_error_t access_type) {
    if (pt->present == 0) {
	return PT_ACCESS_NOT_PRESENT;
    } else if ((pt->writable == 0) && (access_type.write == 1)) {
	return PT_ACCESS_WRITE_ERROR;
    } else if ((pt->user_page == 0) && (access_type.user == 1)) {
	// Check CR0.WP?
	return PT_ACCESS_USER_ERROR;
    }

    return PT_ACCESS_OK;
}



/*
 *   32 bit access checks
 */
pt_access_status_t inline v3_can_access_pde32(pde32_t * pde, addr_t addr, pf_error_t access_type) {
    gen_pt_t * entry = (gen_pt_t *)&pde[PDE32_INDEX(addr)];
    return can_access_pt_entry(entry, access_type);
}

pt_access_status_t inline v3_can_access_pte32(pte32_t * pte, addr_t addr, pf_error_t access_type) {
    gen_pt_t * entry = (gen_pt_t *)&pte[PTE32_INDEX(addr)];
    return can_access_pt_entry(entry, access_type);
}


/*
 *  32 bit PAE access checks
 */
pt_access_status_t inline v3_can_access_pdpe32pae(pdpe32pae_t * pdpe, addr_t addr, pf_error_t access_type) {
    gen_pt_t * entry = (gen_pt_t *)&pdpe[PDPE32PAE_INDEX(addr)];
    return can_access_pt_entry(entry, access_type);
}

pt_access_status_t inline v3_can_access_pde32pae(pde32pae_t * pde, addr_t addr, pf_error_t access_type) {
    gen_pt_t * entry = (gen_pt_t *)&pde[PDE32PAE_INDEX(addr)];
    return can_access_pt_entry(entry, access_type);
}

pt_access_status_t inline v3_can_access_pte32pae(pte32pae_t * pte, addr_t addr, pf_error_t access_type) {
    gen_pt_t * entry = (gen_pt_t *)&pte[PTE32PAE_INDEX(addr)];
    return can_access_pt_entry(entry, access_type);
}

/*
 *   64 Bit access checks
 */
pt_access_status_t inline v3_can_access_pml4e64(pml4e64_t * pmle, addr_t addr, pf_error_t access_type) {
    gen_pt_t * entry = (gen_pt_t *)&pmle[PML4E64_INDEX(addr)];
    return can_access_pt_entry(entry, access_type);
}

pt_access_status_t inline v3_can_access_pdpe64(pdpe64_t * pdpe, addr_t addr, pf_error_t access_type) {
    gen_pt_t * entry = (gen_pt_t *)&pdpe[PDPE64_INDEX(addr)];
    return can_access_pt_entry(entry, access_type);
}

pt_access_status_t inline v3_can_access_pde64(pde64_t * pde, addr_t addr, pf_error_t access_type) {
    gen_pt_t * entry = (gen_pt_t *)&pde[PDE64_INDEX(addr)];
    return can_access_pt_entry(entry, access_type);
}

pt_access_status_t inline v3_can_access_pte64(pte64_t * pte, addr_t addr, pf_error_t access_type) {
    gen_pt_t * entry = (gen_pt_t *)&pte[PTE64_INDEX(addr)];
    return can_access_pt_entry(entry, access_type);
}






int v3_drill_host_pt_32(struct guest_info * info, v3_reg_t host_cr3, addr_t vaddr, 
			int (*callback)(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			void * private_data) {
    pde32_t * host_pde = (pde32_t *)CR3_TO_PDE32_VA(host_cr3);
    addr_t host_pde_pa = CR3_TO_PDE32_PA(host_cr3);
    addr_t host_pte_pa = 0;
    addr_t page_pa = 0;
    int ret;

    if ((ret = callback(info, PAGE_PD32, vaddr, (addr_t)host_pde, host_pde_pa, private_data)) != 0) {
	return (ret == -1) ? -1 : PAGE_PD32;
    }

    switch (pde32_lookup(host_pde, vaddr, &host_pte_pa)) {
	case PT_ENTRY_NOT_PRESENT:
	    return -1;
	case PT_ENTRY_LARGE_PAGE:
	    if ((ret == callback(info, PAGE_4MB, vaddr, (addr_t)V3_VAddr((void *)host_pte_pa), host_pte_pa, private_data)) != 0) {
		return (ret == -1) ? -1 : PAGE_4MB;
	    }
	    return 0;
	case PT_ENTRY_PAGE:
	    if ((ret = callback(info, PAGE_PT32, vaddr, (addr_t)V3_VAddr((void *)host_pte_pa), host_pte_pa, private_data) != 0)) {
		return (ret == -1) ? -1 : PAGE_PT32;
	    }
    
	    if (pte32_lookup(V3_VAddr((void *)host_pte_pa), vaddr, &page_pa) == PT_ENTRY_NOT_PRESENT) {
		return -1;
	    } else {
		if ((ret = callback(info, PAGE_4KB, vaddr, (addr_t)V3_VAddr((void *)PAGE_BASE_ADDR(page_pa)), page_pa, private_data)) != 0) {
		    return (ret == -1) ? -1 : PAGE_4KB;
		}
		return 0;
	    }
    }
    return -1;
}



int v3_drill_host_pt_32pae(struct guest_info * info, v3_reg_t host_cr3, addr_t vaddr,
			   int (*callback)(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			   void * private_data) {
    pdpe32pae_t * host_pdpe = (pdpe32pae_t *)CR3_TO_PDPE32PAE_VA(host_cr3);
    addr_t host_pdpe_pa = CR3_TO_PDPE32PAE_PA(host_cr3);
    addr_t host_pde_pa = 0;
    addr_t host_pte_pa = 0;
    addr_t page_pa = 0;
    int ret;

    if ((ret = callback(info, PAGE_PDP32PAE, vaddr, (addr_t)host_pdpe, host_pdpe_pa, private_data)) != 0) {
	return (ret == -1) ? -1 : PAGE_PDP32PAE;
    }

    switch (pdpe32pae_lookup(host_pdpe, vaddr, &host_pde_pa)) {
	case PT_ENTRY_NOT_PRESENT:
	    return -1;
	case PT_ENTRY_PAGE:

	    if ((ret = callback(info, PAGE_PD32PAE, vaddr, (addr_t)V3_VAddr((void *)host_pde_pa), host_pde_pa, private_data) != 0)) {
		return (ret == -1) ? -1 : PAGE_PD32PAE;
	    }
      
	    switch (pde32pae_lookup(V3_VAddr((void *)host_pde_pa), vaddr, &host_pte_pa)) {
		case PT_ENTRY_NOT_PRESENT:
		    return -1;
		case PT_ENTRY_LARGE_PAGE:
		    if ((ret == callback(info, PAGE_2MB, vaddr, (addr_t)V3_VAddr((void *)host_pte_pa), host_pte_pa, private_data)) != 0) {
			return (ret == -1) ? -1 : PAGE_2MB;
		    }
		    return 0;
		case PT_ENTRY_PAGE:
		    if ((ret = callback(info, PAGE_PT32PAE, vaddr, (addr_t)V3_VAddr((void *)host_pte_pa), host_pte_pa, private_data) != 0)) {
			return (ret == -1) ? -1 : PAGE_PT32PAE;
		    }

		    if (pte32pae_lookup(V3_VAddr((void *)host_pte_pa), vaddr, &page_pa) == PT_ENTRY_NOT_PRESENT) {
			return -1;
		    } else {
			if ((ret = callback(info, PAGE_4KB, vaddr, (addr_t)V3_VAddr((void *)PAGE_BASE_ADDR(page_pa)), page_pa, private_data)) != 0) {
			    return (ret == -1) ? -1 : PAGE_4KB;
			}
			return 0;
		    } 
	    }
	default:
	    return -1;
    }

    // should never get here
    return -1;
}


int v3_drill_host_pt_64(struct guest_info * info, v3_reg_t host_cr3, addr_t vaddr,
			int (*callback)(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			void * private_data) {
    pml4e64_t * host_pmle = (pml4e64_t *)CR3_TO_PML4E64_VA(host_cr3);
    addr_t host_pmle_pa = CR3_TO_PML4E64_PA(host_cr3);
    addr_t host_pdpe_pa = 0;
    addr_t host_pde_pa = 0;
    addr_t host_pte_pa = 0;
    addr_t page_pa = 0;
    int ret;

    if ((ret = callback(info, PAGE_PML464, vaddr, (addr_t)host_pmle, host_pmle_pa, private_data)) != 0) {
	return (ret == -1) ? -1 : PAGE_PML464;
    }

    switch(pml4e64_lookup(host_pmle, vaddr, &host_pdpe_pa)) {
	case PT_ENTRY_NOT_PRESENT:
	    return -1;
	case PT_ENTRY_PAGE:

	    if ((ret = callback(info, PAGE_PDP64, vaddr, (addr_t)V3_VAddr((void *)host_pdpe_pa), host_pdpe_pa, private_data)) != 0) {
		return (ret == -1) ? -1 : PAGE_PDP64;
	    }

	    switch(pdpe64_lookup(V3_VAddr((void *)host_pdpe_pa), vaddr, &host_pde_pa)) {
		case PT_ENTRY_NOT_PRESENT:
		    return -1;
		case PT_ENTRY_LARGE_PAGE:
		    if ((ret == callback(info, PAGE_1GB, vaddr, (addr_t)V3_VAddr((void *)host_pde_pa), host_pde_pa, private_data)) != 0) {
			return (ret == -1) ? -1 : PAGE_1GB;
		    }
		    PrintError("1 Gigabyte Pages not supported\n");
		    return 0;
		case PT_ENTRY_PAGE:

		    if ((ret = callback(info, PAGE_PD64, vaddr, (addr_t)V3_VAddr((void *)host_pde_pa), host_pde_pa, private_data) != 0)) {
			return (ret == -1) ? -1 : PAGE_PD64;
		    }

		    switch (pde64_lookup(V3_VAddr((void *)host_pde_pa), vaddr, &host_pte_pa)) {
			case PT_ENTRY_NOT_PRESENT:
			    return -1;
			case PT_ENTRY_LARGE_PAGE:
			    if ((ret == callback(info, PAGE_2MB, vaddr, (addr_t)V3_VAddr((void *)host_pte_pa), host_pte_pa, private_data)) != 0) {
				return (ret == -1) ? -1 : PAGE_2MB;
			    }
			    return 0;
			case PT_ENTRY_PAGE:

			    if ((ret = callback(info, PAGE_PT64, vaddr, (addr_t)V3_VAddr((void *)host_pte_pa), host_pte_pa, private_data) != 0)) {
				return (ret == -1) ? -1 : PAGE_PT64;
			    }

			    if (pte64_lookup(V3_VAddr((void *)host_pte_pa), vaddr, &page_pa) == PT_ENTRY_NOT_PRESENT) {
				return -1;
			    } else {
				if ((ret = callback(info, PAGE_4KB, vaddr, (addr_t)V3_VAddr((void *)PAGE_BASE_ADDR(page_pa)), page_pa, private_data)) != 0) {
				    return (ret == -1) ? -1 : PAGE_4KB;
				}
				return 0;
			    }
		    }
	    }
	default:
	    return -1;
    }
    // should never get here
    return -1;
}







int v3_drill_guest_pt_32(struct guest_info * info, v3_reg_t guest_cr3, addr_t vaddr, 
			 int (*callback)(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			 void * private_data) {
    addr_t guest_pde_pa = CR3_TO_PDE32_PA(guest_cr3);
    pde32_t * guest_pde = NULL;
    addr_t guest_pte_pa = 0;
    int ret; 
  

    if (v3_gpa_to_hva(info, guest_pde_pa, (addr_t *)&guest_pde) == -1) {
	PrintError("Could not get virtual address of Guest PDE32 (PA=%p)\n", 
		   (void *)guest_pde_pa);
	return -1;
    }
  
    if ((ret = callback(info, PAGE_PD32, vaddr, (addr_t)guest_pde, guest_pde_pa, private_data)) != 0) {
	return (ret == -1) ? -1 : PAGE_PD32;
    }
  
    switch (pde32_lookup(guest_pde, vaddr, &guest_pte_pa)) {
	case PT_ENTRY_NOT_PRESENT:
	    return -1;
	case PT_ENTRY_LARGE_PAGE:
	    {
		addr_t large_page_pa = (addr_t)guest_pte_pa;
		addr_t large_page_va = 0;
      
		if (v3_gpa_to_hva(info, large_page_pa, &large_page_va) == -1) {
		    large_page_va = 0 ;
		}


		if ((ret == callback(info, PAGE_4MB, vaddr, large_page_va, large_page_pa, private_data)) != 0) {
		    return (ret == -1) ? -1 : PAGE_4MB;
		}
		return 0;
	    }
	case PT_ENTRY_PAGE:
	    {
		pte32_t * guest_pte = NULL;
		addr_t page_pa;

		if (v3_gpa_to_hva(info, guest_pte_pa, (addr_t*)&guest_pte) == -1) {
		    PrintError("Could not get virtual address of Guest PTE32 (PA=%p)\n", 
			       (void *)guest_pte_pa);
		    return -1;
		}

		if ((ret = callback(info, PAGE_PT32, vaddr, (addr_t)guest_pte, guest_pte_pa, private_data) != 0)) {
		    return (ret == -1) ? -1 : PAGE_PT32;
		}

		if (pte32_lookup(guest_pte, vaddr, &page_pa) == PT_ENTRY_NOT_PRESENT) {
		    return -1;
		} else {
		    addr_t page_va;

		    if (v3_gpa_to_hva(info, page_pa, &page_va) == -1) {
			page_va = 0;
		    }

		    if ((ret = callback(info, PAGE_4KB, vaddr, page_va, page_pa, private_data)) != 0) {
			return (ret == -1) ? -1 : PAGE_4KB;
		    }
		    return 0;
		}
	    }
    }

    // should never get here
    PrintError("End of drill function (guest 32)... Should never have gotten here...\n");
    return -1;
}



int v3_drill_guest_pt_32pae(struct guest_info * info, v3_reg_t guest_cr3, addr_t vaddr,
			    int (*callback)(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			    void * private_data) {			
    addr_t guest_pdpe_pa = CR3_TO_PDPE32PAE_PA(guest_cr3);
    pdpe32pae_t * guest_pdpe = 0;
    addr_t guest_pde_pa = 0;
    int ret = 0;

    if (v3_gpa_to_hva(info, guest_pdpe_pa, (addr_t*)&guest_pdpe) == -1) {
	PrintError("Could not get virtual address of Guest PDPE32PAE (PA=%p)\n",
		   (void *)guest_pdpe_pa);
	return -1;
    }

    if ((ret = callback(info, PAGE_PDP32PAE, vaddr, (addr_t)guest_pdpe, guest_pdpe_pa, private_data)) != 0) {
	return (ret == -1) ? -1 : PAGE_PDP32PAE;
    }

    switch (pdpe32pae_lookup(guest_pdpe, vaddr, &guest_pde_pa)) 
	{
	    case PT_ENTRY_NOT_PRESENT:
		return -1;
	    case PT_ENTRY_PAGE:
		{
		    pde32pae_t * guest_pde = NULL;
		    addr_t guest_pte_pa = 0;
	
		    if (v3_gpa_to_hva(info, guest_pde_pa, (addr_t *)&guest_pde) == -1) {
			PrintError("Could not get virtual Address of Guest PDE32PAE (PA=%p)\n", 
				   (void *)guest_pde_pa);
			return -1;
		    }

		    if ((ret = callback(info, PAGE_PD32PAE, vaddr, (addr_t)guest_pde, guest_pde_pa, private_data)) != 0) {
			return (ret == -1) ? -1 : PAGE_PD32PAE;
		    }
	
		    switch (pde32pae_lookup(guest_pde, vaddr, &guest_pte_pa)) 
			{
			    case PT_ENTRY_NOT_PRESENT:
				return -1;
			    case PT_ENTRY_LARGE_PAGE:
				{
				    addr_t large_page_pa = (addr_t)guest_pte_pa;
				    addr_t large_page_va = 0;
	      
				    if (v3_gpa_to_hva(info, large_page_pa, &large_page_va) == -1) {
					large_page_va = 0;
				    }
	      
				    if ((ret == callback(info, PAGE_2MB, vaddr, large_page_va, large_page_pa, private_data)) != 0) {
					return (ret == -1) ? -1 : PAGE_2MB;
				    }
				    return 0;
				}
			    case PT_ENTRY_PAGE:
				{
				    pte32pae_t * guest_pte = NULL;
				    addr_t page_pa;

				    if (v3_gpa_to_hva(info, guest_pte_pa, (addr_t *)&guest_pte) == -1) {
					PrintError("Could not get virtual Address of Guest PTE32PAE (PA=%p)\n", 
						   (void *)guest_pte_pa);
					return -1;
				    }

				    if ((ret = callback(info, PAGE_PT32PAE, vaddr, (addr_t)guest_pte, guest_pte_pa, private_data) != 0)) {
					return (ret == -1) ? -1 : PAGE_PT32PAE;
				    }

				    if (pte32pae_lookup(guest_pte, vaddr, &page_pa) == PT_ENTRY_NOT_PRESENT) {
					return -1;
				    } else {
					addr_t page_va;
		
					if (v3_gpa_to_hva(info, page_pa, &page_va) == -1) {
					    page_va = 0;
					}
		
					if ((ret = callback(info, PAGE_4KB, vaddr, page_va, page_pa, private_data)) != 0) {
					    return (ret == -1) ? -1 : PAGE_4KB;
					}
					return 0;
				    }
				}
			}
		}
	    default:
		PrintError("Invalid page type for PD32PAE\n");
		return -1;
	}

    // should never get here
    PrintError("End of drill function (guest 32pae)... Should never have gotten here...\n");
    return -1;
}

int v3_drill_guest_pt_64(struct guest_info * info, v3_reg_t guest_cr3, addr_t vaddr, 
			 int (*callback)(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			 void * private_data) {	
    addr_t guest_pml4_pa = CR3_TO_PML4E64_PA(guest_cr3);
    pml4e64_t * guest_pmle = 0;
    addr_t guest_pdpe_pa = 0;
    int ret = 0;

    if (v3_gpa_to_hva(info, guest_pml4_pa, (addr_t*)&guest_pmle) == -1) {
	PrintError("Could not get virtual address of Guest PML4E64 (PA=%p)\n", 
		   (void *)guest_pml4_pa);
	return -1;
    }
  
    if ((ret = callback(info, PAGE_PML464, vaddr, (addr_t)guest_pmle, guest_pml4_pa, private_data)) != 0) {
	return (ret == -1) ? -1 : PAGE_PML464;
    }

    switch (pml4e64_lookup(guest_pmle, vaddr, &guest_pdpe_pa)) {
	case PT_ENTRY_NOT_PRESENT:
	    return -1;
	case PT_ENTRY_PAGE:
	    {
		pdpe64_t * guest_pdp = NULL;
		addr_t guest_pde_pa = 0;

		if (v3_gpa_to_hva(info, guest_pdpe_pa, (addr_t *)&guest_pdp) == -1) {
		    PrintError("Could not get virtual address of Guest PDPE64 (PA=%p)\n", 
			       (void *)guest_pdpe_pa);
		    return -1;
		}

		if ((ret = callback(info, PAGE_PDP64, vaddr, (addr_t)guest_pdp, guest_pdpe_pa, private_data)) != 0) {
		    return (ret == -1) ? -1 : PAGE_PDP64;
		}

		switch (pdpe64_lookup(guest_pdp, vaddr, &guest_pde_pa)) {
		    case PT_ENTRY_NOT_PRESENT:
			return -1;
		    case PT_ENTRY_LARGE_PAGE:
			{
			    addr_t large_page_pa = (addr_t)guest_pde_pa;
			    addr_t large_page_va = 0;
	  
			    if (v3_gpa_to_hva(info, large_page_pa, &large_page_va) == -1) {
				large_page_va = 0;
			    }
	  
			    if ((ret == callback(info, PAGE_1GB, vaddr, large_page_va, large_page_pa, private_data)) != 0) {
				return (ret == -1) ? -1 : PAGE_1GB;
			    }
			    PrintError("1 Gigabyte Pages not supported\n");
			    return 0;
			}
		    case PT_ENTRY_PAGE:
			{
			    pde64_t * guest_pde = NULL;
			    addr_t guest_pte_pa = 0;

			    if (v3_gpa_to_hva(info, guest_pde_pa, (addr_t *)&guest_pde) == -1) {
				PrintError("Could not get virtual address of guest PDE64 (PA=%p)\n", 
					   (void *)guest_pde_pa);
				return -1;
			    }
	
			    if ((ret = callback(info, PAGE_PD64, vaddr, (addr_t)guest_pde, guest_pde_pa, private_data)) != 0) {
				return (ret == -1) ? -1 : PAGE_PD64;
			    }

			    switch (pde64_lookup(guest_pde, vaddr, &guest_pte_pa)) {
				case PT_ENTRY_NOT_PRESENT:
				    return -1;
				case PT_ENTRY_LARGE_PAGE:
				    {
					addr_t large_page_pa = (addr_t)guest_pte_pa;
					addr_t large_page_va = 0;
	      
					if (v3_gpa_to_hva(info, large_page_pa, &large_page_va) == -1) {
					    large_page_va = 0;
					}
	      
					if ((ret == callback(info, PAGE_2MB, vaddr, large_page_va, large_page_pa, private_data)) != 0) {
					    return (ret == -1) ? -1 : PAGE_2MB;
					}
					return 0;
				    }
				case PT_ENTRY_PAGE:
				    {
					pte64_t * guest_pte = NULL;
					addr_t page_pa;
	      
					if (v3_gpa_to_hva(info, guest_pte_pa, (addr_t *)&guest_pte) == -1) {
					    PrintError("Could not get virtual address of guest PTE64 (PA=%p)\n", 
						       (void *)guest_pte_pa);
					    return -1;
					}

					if ((ret = callback(info, PAGE_PT64, vaddr, (addr_t)guest_pte, guest_pte_pa, private_data) != 0)) {
					    return (ret == -1) ? -1 : PAGE_PT64;
					}
		
					if (pte64_lookup(guest_pte, vaddr, &page_pa) == PT_ENTRY_NOT_PRESENT) {
					    return -1;
					} else {
					    addr_t page_va;
		
					    if (v3_gpa_to_hva(info, page_pa, &page_va) == -1) {
						page_va = 0;
					    }
		
					    if ((ret = callback(info, PAGE_4KB, vaddr, page_va, page_pa, private_data)) != 0) {
						return (ret == -1) ? -1 : PAGE_4KB;
					    }

					    return 0;
					}
				    }
			    }
			}
		}
	    }
	default:
	    return -1;
    }

    // should never get here
    PrintError("End of drill function (guest 64)... Should never have gotten here...\n");
    return -1;
}




int v3_walk_guest_pt_32(struct guest_info * info,  v3_reg_t guest_cr3,
			int (*callback)(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			void * private_data) {
    addr_t guest_pde_pa = CR3_TO_PDE32_PA(guest_cr3);
    pde32_t * guest_pde = NULL;
    int i, j;
    addr_t vaddr = 0;
    int ret = 0;

    if (!callback) {
	PrintError("Call back was not specified\n");
	return -1;
    }

    if (v3_gpa_to_hva(info, guest_pde_pa, (addr_t *)&guest_pde) == -1) {
	PrintError("Could not get virtual address of Guest PDE32 (PA=%p)\n", 
		   (void *)guest_pde_pa);
	return -1;
    }

    if ((ret = callback(info, PAGE_PD32, vaddr, (addr_t)guest_pde, guest_pde_pa, private_data)) != 0) {
	return ret;
    }

    for (i = 0; i < MAX_PDE32_ENTRIES; i++) {
	if (guest_pde[i].present) {
	    if (guest_pde[i].large_page) {
		pde32_4MB_t * large_pde = (pde32_4MB_t *)&(guest_pde[i]);
		addr_t large_page_pa = BASE_TO_PAGE_ADDR_4MB(large_pde->page_base_addr);
		addr_t large_page_va = 0;

		if (v3_gpa_to_hva(info, large_page_pa, &large_page_va) == -1) {
		    PrintDebug("Could not get virtual address of Guest 4MB Page (PA=%p)\n", 
			       (void *)large_page_pa);
		    // We'll let it through for data pages because they may be unmapped or hooked
		    large_page_va = 0;
		}

		if ((ret = callback(info, PAGE_4MB, vaddr, large_page_va, large_page_pa, private_data)) != 0) {
		    return ret;
		}

		vaddr += PAGE_SIZE_4MB;
	    } else {
		addr_t pte_pa = BASE_TO_PAGE_ADDR(guest_pde[i].pt_base_addr);
		pte32_t * tmp_pte = NULL;

		if (v3_gpa_to_hva(info, pte_pa, (addr_t *)&tmp_pte) == -1) {
		    PrintError("Could not get virtual address of Guest PTE32 (PA=%p)\n", 
			       (void *)pte_pa);
		    return -1;
		}

		if ((ret = callback(info, PAGE_PT32, vaddr, (addr_t)tmp_pte, pte_pa, private_data)) != 0) {
		    return ret;
		}

		for (j = 0; j < MAX_PTE32_ENTRIES; j++) {
		    if (tmp_pte[j].present) {
			addr_t page_pa = BASE_TO_PAGE_ADDR(tmp_pte[j].page_base_addr);
			addr_t page_va = 0;

			if (v3_gpa_to_hva(info, page_pa, &page_va) == -1) {
			    PrintDebug("Could not get virtual address of Guest 4KB Page (PA=%p)\n", 
				       (void *)page_pa);
			    // We'll let it through for data pages because they may be unmapped or hooked
			    page_va = 0;
			}
	    
			if ((ret = callback(info, PAGE_4KB, vaddr, page_va, page_pa, private_data)) != 0) {
			    return ret;
			}
		    }

		    vaddr += PAGE_SIZE_4KB;
		}
	    }
	} else {
	    vaddr += PAGE_SIZE_4MB;
	}
    }
    return 0;
}


int v3_walk_guest_pt_32pae(struct guest_info * info,  v3_reg_t guest_cr3,
			   int (*callback)(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			   void * private_data) {
    addr_t guest_pdpe_pa = CR3_TO_PDPE32PAE_PA(guest_cr3);
    pdpe32pae_t * guest_pdpe = NULL;
    int i, j, k;
    addr_t vaddr = 0;
    int ret = 0;

    if (!callback) {
	PrintError("Call back was not specified\n");
	return -1;
    }

    if (v3_gpa_to_hva(info, guest_pdpe_pa, (addr_t *)&guest_pdpe) == -1) {
	PrintError("Could not get virtual address of Guest PDPE32PAE (PA=%p)\n", 
		   (void *)guest_pdpe_pa);
	return -1;
    }

  
    if ((ret = callback(info, PAGE_PDP32PAE, vaddr, (addr_t)guest_pdpe, guest_pdpe_pa, private_data)) != 0) {
	return ret;
    }

    for (i = 0; i < MAX_PDPE32PAE_ENTRIES; i++) {
	if (guest_pdpe[i].present) {
	    addr_t pde_pa = BASE_TO_PAGE_ADDR(guest_pdpe[i].pd_base_addr);
	    pde32pae_t * tmp_pde = NULL;

	    if (v3_gpa_to_hva(info, pde_pa, (addr_t *)&tmp_pde) == -1) {
		PrintError("Could not get virtual address of Guest PDE32PAE (PA=%p)\n", 
			   (void *)pde_pa);
		return -1;
	    }

	    if ((ret = callback(info, PAGE_PD32PAE, vaddr, (addr_t)tmp_pde, pde_pa, private_data)) != 0) {
		return ret;
	    }
      
	    for (j = 0; j < MAX_PDE32PAE_ENTRIES; j++) {
		if (tmp_pde[j].present) {
		    if (tmp_pde[j].large_page) {
			pde32pae_2MB_t * large_pde = (pde32pae_2MB_t *)&(tmp_pde[j]);
			addr_t large_page_pa = BASE_TO_PAGE_ADDR_2MB(large_pde->page_base_addr);
			addr_t large_page_va = 0;
	    
			if (v3_gpa_to_hva(info, large_page_pa, &large_page_va) == -1) {
			    PrintDebug("Could not get virtual address of Guest 2MB Page (PA=%p)\n", 
				       (void *)large_page_pa);
			    // We'll let it through for data pages because they may be unmapped or hooked
			    large_page_va = 0;
			}
	    
			if ((ret = callback(info, PAGE_2MB, vaddr, large_page_va, large_page_pa, private_data)) != 0) {
			    return ret;
			}

			vaddr += PAGE_SIZE_2MB;
		    } else {
			addr_t pte_pa = BASE_TO_PAGE_ADDR(tmp_pde[j].pt_base_addr);
			pte32pae_t * tmp_pte = NULL;
	    
			if (v3_gpa_to_hva(info, pte_pa, (addr_t *)&tmp_pte) == -1) {
			    PrintError("Could not get virtual address of Guest PTE32PAE (PA=%p)\n", 
				       (void *)pte_pa);
			    return -1;
			}
	    
			if ((ret = callback(info, PAGE_PT32PAE, vaddr, (addr_t)tmp_pte, pte_pa, private_data)) != 0) {
			    return ret;
			}
	    
			for (k = 0; k < MAX_PTE32PAE_ENTRIES; k++) {
			    if (tmp_pte[k].present) {
				addr_t page_pa = BASE_TO_PAGE_ADDR(tmp_pte[k].page_base_addr);
				addr_t page_va = 0;
		
				if (v3_gpa_to_hva(info, page_pa, &page_va) == -1) {
				    PrintDebug("Could not get virtual address of Guest 4KB Page (PA=%p)\n", 
					       (void *)page_pa);
				    // We'll let it through for data pages because they may be unmapped or hooked
				    page_va = 0;
				}
		
				if ((ret = callback(info, PAGE_4KB, vaddr, page_va, page_pa, private_data)) != 0) {
				    return ret;
				}
			    }
	      
			    vaddr += PAGE_SIZE_4KB;
			}
		    }
		} else {
		    vaddr += PAGE_SIZE_2MB;
		}
	    }
	} else {
	    vaddr += PAGE_SIZE_2MB * MAX_PDE32PAE_ENTRIES;
	}
    }
    return 0;
}




int v3_walk_guest_pt_64(struct guest_info * info,  v3_reg_t guest_cr3,
			int (*callback)(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			void * private_data) {
    addr_t guest_pml_pa = CR3_TO_PML4E64_PA(guest_cr3);
    pml4e64_t * guest_pml = NULL;
    int i, j, k, m;
    addr_t vaddr = 0;
    int ret = 0;

    if (!callback) {
	PrintError("Call back was not specified\n");
	return -1;
    }

    if (v3_gpa_to_hva(info, guest_pml_pa, (addr_t *)&guest_pml) == -1) {
	PrintError("Could not get virtual address of Guest PML464 (PA=%p)\n", 
		   (void *)guest_pml);
	return -1;
    }


    if ((ret = callback(info, PAGE_PML464, vaddr, (addr_t)guest_pml, guest_pml_pa, private_data)) != 0) {
	return ret;
    }

    for (i = 0; i < MAX_PML4E64_ENTRIES; i++) {
	if (guest_pml[i].present) {
	    addr_t pdpe_pa = BASE_TO_PAGE_ADDR(guest_pml[i].pdp_base_addr);
	    pdpe64_t * tmp_pdpe = NULL;
      
      
	    if (v3_gpa_to_hva(info, pdpe_pa, (addr_t *)&tmp_pdpe) == -1) {
		PrintError("Could not get virtual address of Guest PDPE64 (PA=%p)\n", 
			   (void *)pdpe_pa);
		return -1;
	    }
      
	    if ((ret = callback(info, PAGE_PDP64, vaddr, (addr_t)tmp_pdpe, pdpe_pa, private_data)) != 0) {
		return ret;
	    }
      
	    for (j = 0; j < MAX_PDPE64_ENTRIES; j++) {
		if (tmp_pdpe[j].present) {
		    if (tmp_pdpe[j].large_page) {
			pdpe64_1GB_t * large_pdpe = (pdpe64_1GB_t *)&(tmp_pdpe[j]);
			addr_t large_page_pa = BASE_TO_PAGE_ADDR_1GB(large_pdpe->page_base_addr);
			addr_t large_page_va = 0;

			if (v3_gpa_to_hva(info, large_page_pa, &large_page_va) == -1) {
			    PrintDebug("Could not get virtual address of Guest 1GB page (PA=%p)\n", 
				       (void *)large_page_pa);
			    // We'll let it through for data pages because they may be unmapped or hooked
			    large_page_va = 0;
			}

			if ((ret = callback(info, PAGE_1GB, vaddr, (addr_t)large_page_va, large_page_pa, private_data)) != 0) {
			    return ret;
			}

			vaddr += PAGE_SIZE_1GB;
		    } else {
			addr_t pde_pa = BASE_TO_PAGE_ADDR(tmp_pdpe[j].pd_base_addr);
			pde64_t * tmp_pde = NULL;
	    
			if (v3_gpa_to_hva(info, pde_pa, (addr_t *)&tmp_pde) == -1) {
			    PrintError("Could not get virtual address of Guest PDE64 (PA=%p)\n", 
				       (void *)pde_pa);
			    return -1;
			}
	    
			if ((ret = callback(info, PAGE_PD64, vaddr, (addr_t)tmp_pde, pde_pa, private_data)) != 0) {
			    return ret;
			}
	    
			for (k = 0; k < MAX_PDE64_ENTRIES; k++) {
			    if (tmp_pde[k].present) {
				if (tmp_pde[k].large_page) {
				    pde64_2MB_t * large_pde = (pde64_2MB_t *)&(tmp_pde[k]);
				    addr_t large_page_pa = BASE_TO_PAGE_ADDR_2MB(large_pde->page_base_addr);
				    addr_t large_page_va = 0;
		  
				    if (v3_gpa_to_hva(info, large_page_pa, &large_page_va) == -1) {
					PrintDebug("Could not get virtual address of Guest 2MB page (PA=%p)\n", 
						   (void *)large_page_pa);
					// We'll let it through for data pages because they may be unmapped or hooked
					large_page_va = 0;
				    }
		  
				    if ((ret = callback(info, PAGE_2MB, vaddr, large_page_va, large_page_pa, private_data)) != 0) {
					return ret;
				    }

				    vaddr += PAGE_SIZE_2MB;
				} else {
				    addr_t pte_pa = BASE_TO_PAGE_ADDR(tmp_pde[k].pt_base_addr);
				    pte64_t * tmp_pte = NULL;
		  
				    if (v3_gpa_to_hva(info, pte_pa, (addr_t *)&tmp_pte) == -1) {
					PrintError("Could not get virtual address of Guest PTE64 (PA=%p)\n", 
						   (void *)pte_pa);
					return -1;
				    }
		  
				    if ((ret = callback(info, PAGE_PT64, vaddr, (addr_t)tmp_pte, pte_pa, private_data)) != 0) {
					return ret;
				    }
		  
				    for (m = 0; m < MAX_PTE64_ENTRIES; m++) {
					if (tmp_pte[m].present) {
					    addr_t page_pa = BASE_TO_PAGE_ADDR(tmp_pte[m].page_base_addr);
					    addr_t page_va = 0;
		      
					    if (v3_gpa_to_hva(info, page_pa, &page_va) == -1) {
						PrintDebug("Could not get virtual address of Guest 4KB Page (PA=%p)\n", 
							   (void *)page_pa);
						// We'll let it through for data pages because they may be unmapped or hooked
						page_va = 0;
					    }
		      
					    if ((ret = callback(info, PAGE_4KB, vaddr, page_va, page_pa, private_data)) != 0) {
						return ret;
					    }
					}

					vaddr += PAGE_SIZE_4KB;
				    }
				}
			    } else {
				vaddr += PAGE_SIZE_2MB;
			    }
			}
		    }
		} else {
		    vaddr += PAGE_SIZE_1GB;
		}
	    }
	} else {
	    vaddr += ((ullong_t)PAGE_SIZE_1GB * (ullong_t)MAX_PDPE64_ENTRIES);
	}
    }
    return 0;
}

int v3_walk_host_pt_32(struct guest_info * info, v3_reg_t host_cr3,
		       int (*callback)(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
		       void * private_data) {
    pde32_t * host_pde = (pde32_t *)CR3_TO_PDE32_VA(host_cr3);
    addr_t pde_pa = CR3_TO_PDE32_PA(host_cr3);
    int i, j;
    addr_t vaddr = 0;
    int ret = 0;

    if (!callback) {
	PrintError("Call back was not specified\n");
	return -1;
    }

    if ((ret = callback(info, PAGE_PD32, vaddr, (addr_t)host_pde, pde_pa, private_data)) != 0) {
	return ret;
    }

    for (i = 0; i < MAX_PDE32_ENTRIES; i++) {
	if (host_pde[i].present) {
	    if (host_pde[i].large_page) {
		pde32_4MB_t * large_pde = (pde32_4MB_t *)&(host_pde[i]);
		addr_t large_page_pa = BASE_TO_PAGE_ADDR_4MB(large_pde->page_base_addr);

		if ((ret = callback(info, PAGE_4MB, vaddr, (addr_t)V3_VAddr((void *)large_page_pa), large_page_pa, private_data)) != 0) {
		    return ret;
		}

		vaddr += PAGE_SIZE_4MB;
	    } else {
		addr_t pte_pa = BASE_TO_PAGE_ADDR(host_pde[i].pt_base_addr);
		pte32_t * tmp_pte = (pte32_t *)V3_VAddr((void *)pte_pa);

		if ((ret = callback(info, PAGE_PT32, vaddr, (addr_t)tmp_pte, pte_pa, private_data)) != 0) {
		    return ret;
		}

		for (j = 0; j < MAX_PTE32_ENTRIES; j++) {
		    if (tmp_pte[j].present) {
			addr_t page_pa = BASE_TO_PAGE_ADDR(tmp_pte[j].page_base_addr);
			if ((ret = callback(info, PAGE_4KB, vaddr, (addr_t)V3_VAddr((void *)page_pa), page_pa, private_data)) != 0) {
			    return ret;
			}
		    }

		    vaddr += PAGE_SIZE_4KB;
		}
	    }
	} else {
	    vaddr += PAGE_SIZE_4MB;
	}
    }
    return 0;
}





int v3_walk_host_pt_32pae(struct guest_info * info, v3_reg_t host_cr3,
			  int (*callback)(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
			  void * private_data) {
    pdpe32pae_t * host_pdpe = (pdpe32pae_t *)CR3_TO_PDPE32PAE_VA(host_cr3);
    addr_t pdpe_pa = CR3_TO_PDPE32PAE_PA(host_cr3);
    int i, j, k;
    addr_t vaddr = 0;
    int ret = 0;

    if (!callback) {
	PrintError("Callback was not specified\n");
	return -1;
    }
  
    if ((ret = callback(info, PAGE_PDP32PAE, vaddr, (addr_t)host_pdpe, pdpe_pa, private_data)) != 0) {
	return ret;
    }
  
    for (i = 0; i < MAX_PDPE32PAE_ENTRIES; i++) {
	if (host_pdpe[i].present) {	
	    addr_t pde_pa = BASE_TO_PAGE_ADDR(host_pdpe[i].pd_base_addr);
	    pde32pae_t * tmp_pde = (pde32pae_t *)V3_VAddr((void *)pde_pa);
      
	    if ((ret = callback(info, PAGE_PD32PAE, vaddr, (addr_t)tmp_pde, pde_pa, private_data)) != 0) {
		return ret;
	    }
      
	    for (j = 0; j < MAX_PDE32PAE_ENTRIES; j++) {
		if (tmp_pde[j].present) {
	  
		    if (tmp_pde[j].large_page) {
			pde32pae_2MB_t * large_pde = (pde32pae_2MB_t *)&(tmp_pde[j]);
			addr_t large_page_pa = BASE_TO_PAGE_ADDR_2MB(large_pde->page_base_addr);

			if ((ret = callback(info, PAGE_2MB, vaddr, (addr_t)V3_VAddr((void *)large_page_pa), large_page_pa, private_data)) != 0) {
			    return ret;
			}

			vaddr += PAGE_SIZE_2MB;
		    } else {
			addr_t pte_pa = BASE_TO_PAGE_ADDR(tmp_pde[j].pt_base_addr);
			pte32pae_t * tmp_pte = (pte32pae_t *)V3_VAddr((void *)pte_pa);
	    
			if ((ret = callback(info, PAGE_PT32PAE, vaddr, (addr_t)tmp_pte, pte_pa, private_data)) != 0) {
			    return ret;
			}
	    
			for (k = 0; k < MAX_PTE32PAE_ENTRIES; k++) {
			    if (tmp_pte[k].present) {
				addr_t page_pa = BASE_TO_PAGE_ADDR(tmp_pte[k].page_base_addr);
				if ((ret = callback(info, PAGE_4KB, vaddr, (addr_t)V3_VAddr((void *)page_pa), page_pa, private_data)) != 0) {
				    return ret;
				}
			    }

			    vaddr += PAGE_SIZE_4KB;
			}
		    }
		} else {
		    vaddr += PAGE_SIZE_2MB;
		}
	    }
	} else {
	    vaddr += PAGE_SIZE_2MB * MAX_PDE32PAE_ENTRIES;
	}
    }
    return 0;
}
			

int v3_walk_host_pt_64(struct guest_info * info, v3_reg_t host_cr3,
		       int (*callback)(struct guest_info * info, page_type_t type, addr_t vaddr, addr_t page_ptr, addr_t page_pa, void * private_data),
		       void * private_data) {
    pml4e64_t * host_pml = (pml4e64_t *)CR3_TO_PML4E64_VA(host_cr3);
    addr_t pml_pa = CR3_TO_PML4E64_PA(host_cr3);
    int i, j, k, m;
    addr_t vaddr = 0;
    int ret = 0;

    if (!callback) {
	PrintError("Callback was not specified\n");
	return -1;
    }

    if ((ret = callback(info, PAGE_PML464, vaddr, (addr_t)host_pml, pml_pa, private_data)) != 0) {
	return ret;
    }

    for (i = 0; i < MAX_PML4E64_ENTRIES; i++) {
	if (host_pml[i].present) {
	    addr_t pdpe_pa = BASE_TO_PAGE_ADDR(host_pml[i].pdp_base_addr);
	    pdpe64_t * tmp_pdpe = (pdpe64_t *)V3_VAddr((void *)pdpe_pa);

	    if ((ret = callback(info, PAGE_PDP64, vaddr, (addr_t)tmp_pdpe, pdpe_pa, private_data)) != 0) {
		return ret;
	    }

	    for (j = 0; j < MAX_PDPE64_ENTRIES; j++) {
		if (tmp_pdpe[j].present) {
		    if (tmp_pdpe[j].large_page) {
			pdpe64_1GB_t * large_pdp = (pdpe64_1GB_t *)&(tmp_pdpe[j]);
			addr_t large_page_pa = BASE_TO_PAGE_ADDR_1GB(large_pdp->page_base_addr);

			if ((ret = callback(info, PAGE_1GB, vaddr, (addr_t)V3_VAddr((void *)large_page_pa), large_page_pa, private_data)) != 0) {
			    return ret;
			}

			vaddr += PAGE_SIZE_1GB;
		    } else {
			addr_t pde_pa = BASE_TO_PAGE_ADDR(tmp_pdpe[j].pd_base_addr);
			pde64_t * tmp_pde = (pde64_t *)V3_VAddr((void *)pde_pa);

			if ((ret = callback(info, PAGE_PD64, vaddr, (addr_t)tmp_pde, pde_pa, private_data)) != 0) {
			    return ret;
			}

			for (k = 0; k < MAX_PDE64_ENTRIES; k++) {
			    if (tmp_pde[k].present) {
				if (tmp_pde[k].large_page) {
				    pde64_2MB_t * large_pde = (pde64_2MB_t *)&(tmp_pde[k]);
				    addr_t large_page_pa = BASE_TO_PAGE_ADDR_2MB(large_pde->page_base_addr);
		  
				    if ((ret = callback(info, PAGE_2MB, vaddr, (addr_t)V3_VAddr((void *)large_page_pa), large_page_pa, private_data)) != 0) {
					return ret;
				    }
		  
				    vaddr += PAGE_SIZE_2MB;
				} else {
				    addr_t pte_pa = BASE_TO_PAGE_ADDR(tmp_pde[k].pt_base_addr);
				    pte64_t * tmp_pte = (pte64_t *)V3_VAddr((void *)pte_pa);

				    if ((ret = callback(info, PAGE_PT64, vaddr, (addr_t)tmp_pte, pte_pa, private_data)) != 0) {
					return ret;
				    }

				    for (m = 0; m < MAX_PTE64_ENTRIES; m++) {
					if (tmp_pte[m].present) {
					    addr_t page_pa = BASE_TO_PAGE_ADDR(tmp_pte[m].page_base_addr);
					    if ((ret = callback(info, PAGE_4KB, vaddr, (addr_t)V3_VAddr((void *)page_pa), page_pa, private_data)) != 0) {
						return ret;
					    }
					}
					vaddr += PAGE_SIZE_4KB;
				    }
				}
			    } else {
				vaddr += PAGE_SIZE_2MB;
			    }
			}
		    }
		} else {
		    vaddr += PAGE_SIZE_1GB;
		}
	    }
	} else {
	    vaddr += (ullong_t)PAGE_SIZE_1GB * (ullong_t)MAX_PDPE64_ENTRIES;
	}
    }
    return 0;
}



static const uchar_t PAGE_4KB_STR[] = "4KB_PAGE";
static const uchar_t PAGE_2MB_STR[] = "2MB_PAGE";
static const uchar_t PAGE_4MB_STR[] = "4MB_PAGE";
static const uchar_t PAGE_1GB_STR[] = "1GB_PAGE";
static const uchar_t PAGE_PT32_STR[] = "32 Bit PT";
static const uchar_t PAGE_PD32_STR[] = "32 Bit PD";
static const uchar_t PAGE_PDP32PAE_STR[] = "32 Bit PAE PDP";
static const uchar_t PAGE_PD32PAE_STR[] = "32 Bit PAE PD";
static const uchar_t PAGE_PT32PAE_STR[] = "32 Bit PAE PT";
static const uchar_t PAGE_PML464_STR[] = "64 Bit PML4";
static const uchar_t PAGE_PDP64_STR[] = "64 Bit PDP";
static const uchar_t PAGE_PD64_STR[] = "64 Bit PD";
static const uchar_t PAGE_PT64_STR[] = "64 Bit PT";


const uchar_t * v3_page_type_to_str(page_type_t type) {
    switch (type) {
	case PAGE_4KB:
	    return PAGE_4KB_STR;
	case PAGE_2MB:
	    return PAGE_2MB_STR;
	case PAGE_4MB:
	    return PAGE_4MB_STR;
	case PAGE_1GB:
	    return PAGE_1GB_STR;
	case PAGE_PT32:
	    return PAGE_PT32_STR;
	case PAGE_PD32:
	    return PAGE_PD32_STR;
	case PAGE_PDP32PAE:
	    return PAGE_PDP32PAE_STR;
	case PAGE_PD32PAE:
	    return PAGE_PD32PAE_STR;
	case PAGE_PT32PAE:
	    return PAGE_PT32PAE_STR;
	case PAGE_PML464:
	    return PAGE_PML464_STR;
	case PAGE_PDP64:
	    return PAGE_PDP64_STR;
	case PAGE_PD64:
	    return PAGE_PD64_STR;
	case PAGE_PT64:
	    return PAGE_PT64_STR;
	default:
	    return NULL;
    }
}
