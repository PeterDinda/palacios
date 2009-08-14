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

#ifndef __VMM_SYM_SWAP_H__
#define __VMM_SYM_SWAP_H__

#ifdef __V3VEE__ 
#ifdef CONFIG_SYMBIOTIC_SWAP

#include <palacios/vmm.h>

static inline int is_swapped_pte32(pte32_t * pte) {
    return (*(uint32_t *)pte != 0);
}


struct v3_swap_dev {
    addr_t (*get_page)(int index);

};


struct v3_sym_swap_state {
    struct v3_swap_dev[256];

};


int v3_init_sym_swap(struct guest_info * info);

addr_t v3_get_swapped_pg_addr(pte32_t * pte);



#endif
#endif

#endif
