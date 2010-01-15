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

#include <palacios/vm_guest_mem.h>
#include <palacios/vmm.h>
#include <palacios/vmm_paging.h>

extern struct v3_os_hooks * os_hooks;


/**********************************/
/* GROUP 0                        */
/**********************************/

int host_va_to_host_pa(addr_t host_va, addr_t * host_pa) {
    if ((os_hooks) && (os_hooks)->vaddr_to_paddr) {

	*host_pa = (addr_t)(os_hooks)->vaddr_to_paddr((void *)host_va);
  
	if (*host_pa == 0) {
	    PrintError("In HVA->HPA: Invalid HVA(%p)->HPA lookup\n",  
		       (void *)host_va);
	    return -1;
	}
    } else {
	PrintError("In HVA->HPA: os_hooks not defined\n");
	return -1;
    }
    return 0;
}


int host_pa_to_host_va(addr_t host_pa, addr_t * host_va) {
    if ((os_hooks) && (os_hooks)->paddr_to_vaddr) {

	*host_va = (addr_t)(os_hooks)->paddr_to_vaddr((void *)host_pa);
    
	if (*host_va == 0) {
	    PrintError("In HPA->HVA: Invalid HPA(%p)->HVA lookup\n",  
		       (void *)host_pa);
	    return -1;
	}
    } else {
	PrintError("In HPA->HVA: os_hooks not defined\n");
	return -1;
    }
    return 0;
}



int guest_pa_to_host_pa(struct guest_info * info, addr_t guest_pa, addr_t * host_pa) {
    struct v3_shadow_region * shdw_reg = v3_get_shadow_region(info->vm_info, info->cpu_id, guest_pa);

    if (shdw_reg == NULL) {
	PrintError("In GPA->HPA: Could not find address in shadow map (addr=%p) (NULL REGION)\n", 
		   (void *)guest_pa);
	return -1;
    }
    
    if (shdw_reg->host_type == SHDW_REGION_FULL_HOOK) {
	PrintError("In GPA->HPA: Could not find address in shadow map (addr=%p) (reg_type=%s)\n", 
		   (void *)guest_pa, v3_shdw_region_type_to_str(shdw_reg->host_type));
	return -1;
    }
    
    *host_pa = v3_get_shadow_addr(shdw_reg, info->cpu_id, guest_pa);

    return 0;
}


/* !! Currently not implemented !! */
// This is a scan of the shadow map
// For now we ignore it
// 
int host_pa_to_guest_pa(struct guest_info * guest_info, addr_t host_pa, addr_t * guest_pa) {
    *guest_pa = 0;
    PrintError("ERROR!!! HPA->GPA currently not implemented!!!\n");

    return -1;
}



/**********************************/
/* GROUP 1                        */
/**********************************/


/* !! Currently not implemented !! */
// This will return negative until we implement host_pa_to_guest_pa()
int host_va_to_guest_pa(struct guest_info * guest_info, addr_t host_va, addr_t * guest_pa) {
    addr_t host_pa = 0;
    *guest_pa = 0;

    if (host_va_to_host_pa(host_va, &host_pa) != 0) {
	PrintError("In HVA->GPA: Invalid HVA(%p)->HPA lookup\n", 
		   (void *)host_va);
	return -1;
    }

    if (host_pa_to_guest_pa(guest_info, host_pa, guest_pa) != 0) {
	PrintError("In HVA->GPA: Invalid HPA(%p)->GPA lookup\n", 
		   (void *)host_pa);
	return -1;
    }

    return 0;
}




int guest_pa_to_host_va(struct guest_info * guest_info, addr_t guest_pa, addr_t * host_va) {
    addr_t host_pa = 0;

    *host_va = 0;

    if (guest_pa_to_host_pa(guest_info, guest_pa, &host_pa) != 0) {
	PrintError("In GPA->HVA: Invalid GPA(%p)->HPA lookup\n", 
		   (void *)guest_pa);
	return -1;
    }
  
    if (host_pa_to_host_va(host_pa, host_va) != 0) {
	PrintError("In GPA->HVA: Invalid HPA(%p)->HVA lookup\n", 
		   (void *)host_pa);
	return -1;
    }

    return 0;
}


int guest_va_to_guest_pa(struct guest_info * guest_info, addr_t guest_va, addr_t * guest_pa) {
    v3_reg_t guest_cr3 = 0;

    if (guest_info->mem_mode == PHYSICAL_MEM) {
	// guest virtual address is the same as the physical
	*guest_pa = guest_va;
	return 0;
    }

    if (guest_info->shdw_pg_mode == SHADOW_PAGING) {
	guest_cr3 = guest_info->shdw_pg_state.guest_cr3;
    } else {
	guest_cr3 = guest_info->ctrl_regs.cr3;
    }


    // Guest Is in Paged mode
    switch (guest_info->cpu_mode) {
	case PROTECTED:
	    if (v3_translate_guest_pt_32(guest_info, guest_cr3, guest_va, guest_pa) == -1) {
		PrintDebug("Could not translate addr (%p) through 32 bit guest PT at %p\n", 
			   (void *)guest_va, (void *)(addr_t)guest_cr3);
		return -1;
	    }
	    break;
	case PROTECTED_PAE:
	    if (v3_translate_guest_pt_32pae(guest_info, guest_cr3, guest_va, guest_pa) == -1) {
		PrintDebug("Could not translate addr (%p) through 32 bitpae guest PT at %p\n", 
			   (void *)guest_va, (void *)(addr_t)guest_cr3);
		return -1;
	    }
	    break;
	case LONG:
	case LONG_32_COMPAT:
	case LONG_16_COMPAT:
	    if (v3_translate_guest_pt_64(guest_info, guest_cr3, guest_va, guest_pa) == -1) {
		PrintDebug("Could not translate addr (%p) through 64 bit guest PT at %p\n", 
			   (void *)guest_va, (void *)(addr_t)guest_cr3);
		return -1;
	    }
	    break;
	default:
	    return -1;
    }
  
    return 0;
}



/* !! Currently not implemented !! */
/* This will be a real pain.... its your standard page table walker in guest memory
 * 
 * For now we ignore it...
 */
int guest_pa_to_guest_va(struct guest_info * guest_info, addr_t guest_pa, addr_t * guest_va) {
    *guest_va = 0;
    PrintError("ERROR!!: GPA->GVA Not Implemented!!\n");
    return -1;
}


/**********************************/
/* GROUP 2                        */
/**********************************/


int guest_va_to_host_pa(struct guest_info * guest_info, addr_t guest_va, addr_t * host_pa) {
    addr_t guest_pa = 0;

    *host_pa = 0;

    if (guest_va_to_guest_pa(guest_info, guest_va, &guest_pa) != 0) {
	PrintError("In GVA->HPA: Invalid GVA(%p)->GPA lookup\n", 
		   (void *)guest_va);
	return -1;
    }
  
    if (guest_pa_to_host_pa(guest_info, guest_pa, host_pa) != 0) {
	PrintError("In GVA->HPA: Invalid GPA(%p)->HPA lookup\n", 
		   (void *)guest_pa);
	return -1;
    }

    return 0;
}

/* !! Currently not implemented !! */
int host_pa_to_guest_va(struct guest_info * guest_info, addr_t host_pa, addr_t * guest_va) {
    addr_t guest_pa = 0;

    *guest_va = 0;

    if (host_pa_to_guest_pa(guest_info, host_pa, &guest_pa) != 0) {
	PrintError("In HPA->GVA: Invalid HPA(%p)->GPA lookup\n", 
		   (void *)host_pa);
	return -1;
    }

    if (guest_pa_to_guest_va(guest_info, guest_pa, guest_va) != 0) {
	PrintError("In HPA->GVA: Invalid GPA(%p)->GVA lookup\n", 
		   (void *)guest_pa);
	return -1;
    }

    return 0;
}




int guest_va_to_host_va(struct guest_info * guest_info, addr_t guest_va, addr_t * host_va) {
    addr_t guest_pa = 0;
    addr_t host_pa = 0;

    *host_va = 0;

    if (guest_va_to_guest_pa(guest_info, guest_va, &guest_pa) != 0) {
	PrintError("In GVA->HVA: Invalid GVA(%p)->GPA lookup\n", 
		   (void *)guest_va);
	return -1;
    }

    if (guest_pa_to_host_pa(guest_info, guest_pa, &host_pa) != 0) {
	PrintError("In GVA->HVA: Invalid GPA(%p)->HPA lookup\n", 
		   (void *)guest_pa);
	return -1;
    }

    if (host_pa_to_host_va(host_pa, host_va) != 0) {
	PrintError("In GVA->HVA: Invalid HPA(%p)->HVA lookup\n", 
		   (void *)host_pa);
	return -1;
    }

    return 0;
}


/* !! Currently not implemented !! */
int host_va_to_guest_va(struct guest_info * guest_info, addr_t host_va, addr_t * guest_va) {
    addr_t host_pa = 0;
    addr_t guest_pa = 0;

    *guest_va = 0;

    if (host_va_to_host_pa(host_va, &host_pa) != 0) {
	PrintError("In HVA->GVA: Invalid HVA(%p)->HPA lookup\n", 
		   (void *)host_va);
	return -1;
    }

    if (host_pa_to_guest_pa(guest_info, host_pa, &guest_pa) != 0) {
	PrintError("In HVA->GVA: Invalid HPA(%p)->GPA lookup\n", 
		   (void *)host_va);
	return -1;
    }

    if (guest_pa_to_guest_va(guest_info, guest_pa, guest_va) != 0) {
	PrintError("In HVA->GVA: Invalid GPA(%p)->GVA lookup\n", 
		   (void *)guest_pa);
	return -1;
    }

    return 0;
}






/* This is a straight address conversion + copy, 
 *   except for the tiny little issue of crossing page boundries.....
 */
int read_guest_va_memory(struct guest_info * guest_info, addr_t guest_va, int count, uchar_t * dest) {
    addr_t cursor = guest_va;
    int bytes_read = 0;



    while (count > 0) {
	int dist_to_pg_edge = (PAGE_ADDR(cursor) + PAGE_SIZE) - cursor;
	int bytes_to_copy = (dist_to_pg_edge > count) ? count : dist_to_pg_edge;
	addr_t host_addr = 0;

    
	if (guest_va_to_host_va(guest_info, cursor, &host_addr) != 0) {
	    PrintDebug("Invalid GVA(%p)->HVA lookup\n", (void *)cursor);
	    return bytes_read;
	}
    
    

	memcpy(dest + bytes_read, (void*)host_addr, bytes_to_copy);
    
	bytes_read += bytes_to_copy;
	count -= bytes_to_copy;
	cursor += bytes_to_copy;    
    }

    return bytes_read;
}






/* This is a straight address conversion + copy, 
 *   except for the tiny little issue of crossing page boundries.....
 */
int read_guest_pa_memory(struct guest_info * guest_info, addr_t guest_pa, int count, uchar_t * dest) {
    addr_t cursor = guest_pa;
    int bytes_read = 0;

    while (count > 0) {
	int dist_to_pg_edge = (PAGE_ADDR(cursor) + PAGE_SIZE) - cursor;
	int bytes_to_copy = (dist_to_pg_edge > count) ? count : dist_to_pg_edge;
	addr_t host_addr = 0;

	if (guest_pa_to_host_va(guest_info, cursor, &host_addr) != 0) {
	    return bytes_read;
	}    
    
	/*
	  PrintDebug("Trying to read %d bytes\n", bytes_to_copy);
	  PrintDebug("Dist to page edge=%d\n", dist_to_pg_edge);
	  PrintDebug("PAGE_ADDR=0x%x\n", PAGE_ADDR(cursor));
	  PrintDebug("guest_pa=0x%x\n", guest_pa);
	*/
    
	memcpy(dest + bytes_read, (void*)host_addr, bytes_to_copy);

	bytes_read += bytes_to_copy;
	count -= bytes_to_copy;
	cursor += bytes_to_copy;
    }

    return bytes_read;
}




/* This is a straight address conversion + copy, 
 *   except for the tiny little issue of crossing page boundries.....
 */
int write_guest_pa_memory(struct guest_info * guest_info, addr_t guest_pa, int count, uchar_t * src) {
    addr_t cursor = guest_pa;
    int bytes_written = 0;

    while (count > 0) {
	int dist_to_pg_edge = (PAGE_ADDR(cursor) + PAGE_SIZE) - cursor;
	int bytes_to_copy = (dist_to_pg_edge > count) ? count : dist_to_pg_edge;
	addr_t host_addr;

	if (guest_pa_to_host_va(guest_info, cursor, &host_addr) != 0) {
	    return bytes_written;
	}


	memcpy((void*)host_addr, src + bytes_written, bytes_to_copy);

	bytes_written += bytes_to_copy;
	count -= bytes_to_copy;
	cursor += bytes_to_copy;    
    }

    return bytes_written;
}

