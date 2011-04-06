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


#ifndef __VMM_MEM_HOOK_H
#define __VMM_MEM_HOOK_H


#ifdef __V3VEE__ 

struct hashtable;


struct v3_mem_hooks {

    /* Scratch memory pages for full hooks (1 per core) */
    void * hook_hvas_1; 

    /* A second set of scratch memory pages */
    /* The ONLY reason this exists is because of 'rep cmps'... */
    void * hook_hvas_2; 


    struct list_head hook_list;

    /* We track memory hooks via a hash table */
    /* keyed to the memory region pointer */
    struct hashtable * reg_table; 
};


int v3_init_mem_hooks(struct v3_vm_info * vm);
int v3_deinit_mem_hooks(struct v3_vm_info * vm);

int v3_hook_full_mem(struct v3_vm_info * vm, uint16_t core_id,
		     addr_t guest_addr_start, addr_t guest_addr_end,
		     int (*read)(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data),
		     int (*write)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data),
		     void * priv_data);

int v3_hook_write_mem(struct v3_vm_info * vm, uint16_t core_id, 
		      addr_t guest_addr_start, addr_t guest_addr_end, addr_t host_addr,
		      int (*write)(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data),
		      void * priv_data);


int v3_unhook_mem(struct v3_vm_info * vm, uint16_t core_id, addr_t guest_addr_start);


		     


#endif /* ! __V3VEE__ */


#endif
