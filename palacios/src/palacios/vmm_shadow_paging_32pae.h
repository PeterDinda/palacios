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


static inline int activate_shadow_pt_32pae(struct guest_info * info) {
    PrintError("Activating 32 bit PAE page tables not implemented\n");
    return -1;
}






/* 
 * *
 * * 
 * * 32 bit PAE  Page table fault handlers
 * *
 * *
 */

static inline int handle_shadow_pagefault_32pae(struct guest_info * info, addr_t fault_addr, pf_error_t error_code) {
    PrintError("32 bit PAE shadow paging not implemented\n");
    return -1;
}


static inline int handle_shadow_invlpg_32pae(struct guest_info * info, addr_t vaddr) {
    PrintError("32 bit PAE shadow paging not implemented\n");
    return -1;
}




