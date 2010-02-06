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

#ifndef __VMM_SWAPBYPASS_H__
#define __VMM_SWAPBYPASS_H__

#ifdef __V3VEE__ 

#include <palacios/vmm_types.h>


struct v3_swap_ops {
    void * (*get_swap_entry)(uint32_t pg_index, void * private_data);
};




int v3_register_swap_disk(struct v3_vm_info * vm, int dev_index, 
			  struct v3_swap_ops * ops, void * private_data);

int v3_swap_in_notify(struct v3_vm_info * vm, int pg_index, int dev_index);


int v3_swap_flush(struct v3_vm_info * vm);



#endif
#endif
