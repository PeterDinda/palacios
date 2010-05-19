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

#ifndef __VM_GUEST_MEM_H
#define __VM_GUEST_MEM_H


#ifdef __V3VEE__

#include <palacios/vm_guest.h>
#include <palacios/vmm_mem.h>


/* These functions are ordered such that they can only call the functions defined in a lower order group */
/* This is to avoid infinite lookup loops */

/**********************************/
/* GROUP 0                        */
/**********************************/

/* Fundamental converters */
// Call out to OS
int v3_hva_to_hpa(addr_t host_va, addr_t * host_pa);
int v3_hpa_to_hva(addr_t host_pa, addr_t * host_va);

// guest_pa -> (shadow map) -> host_pa
int v3_gpa_to_hpa(struct guest_info * guest_info, addr_t guest_pa, addr_t * host_pa);

/* !! Currently not implemented !! */
// host_pa -> (shadow_map) -> guest_pa
int v3_hpa_to_gpa(struct guest_info * guest_info, addr_t host_pa, addr_t * guest_pa);


/**********************************/
/* GROUP 1                        */
/**********************************/


/* !! Currently not implemented !! */
// host_va -> host_pa -> guest_pa
int v3_hva_to_gpa(struct guest_info * guest_info, addr_t host_va, addr_t * guest_pa);


// guest_pa -> host_pa -> host_va
int v3_gpa_to_hva(struct guest_info * guest_info, addr_t guest_pa, addr_t * host_va);


// Look up the address in the guests page tables.. This can cause multiple calls that translate
//     ------------------------------------------------
//     |                                              |
//     -->   guest_pa -> host_pa -> host_va ->   (read table) --> guest_pa
int v3_gva_to_gpa(struct guest_info * guest_info, addr_t guest_va, addr_t * guest_pa);



/* !! Currently not implemented !! */
//   A page table walker in the guest's address space
//     ------------------------------------------------
//     |                                              |
//     -->   guest_pa -> host_pa -> host_va ->   (read table) --> guest_va
int v3_gpa_to_gva(struct guest_info * guest_info, addr_t guest_pa, addr_t * guest_va);



/**********************************/
/* GROUP 2                        */
/**********************************/
// guest_va -> guest_pa -> host_pa
int v3_gva_to_hpa(struct guest_info * guest_info, addr_t guest_va, addr_t * host_pa);


/* !! Currently not implemented !! */
// host_pa -> guest_pa -> guest_va
int v3_hpa_to_gva(struct guest_info * guest_info, addr_t host_pa, addr_t * guest_va);

// guest_va -> guest_pa -> host_pa -> host_va
int v3_gva_to_hva(struct guest_info * guest_info, addr_t guest_va, addr_t * host_va);


/* !! Currently not implemented !! */
// host_va -> host_pa -> guest_pa -> guest_va
int v3_hva_to_gva(struct guest_info * guest_info, addr_t host_va, addr_t  * guest_va);









int v3_read_gva_memory(struct guest_info * guest_info, addr_t guest_va, int count, uchar_t * dest);
int v3_read_gpa_memory(struct guest_info * guest_info, addr_t guest_pa, int count, uchar_t * dest);
int v3_write_gpa_memory(struct guest_info * guest_info, addr_t guest_pa, int count, uchar_t * src);
// TODO int write_guest_va_memory(struct guest_info * guest_info, addr_t guest_va, int count, char * src);


#endif // ! __V3VEE__


#endif
