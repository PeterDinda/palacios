/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Jack Lange <jacklange@cs.pitt.edu> 
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jacklange@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __VMM_TIMEOUT_H__
#define __VMM_TIMEOUT_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>

struct guest_info;

struct v3_core_timeouts {
    uint8_t timeout_active;
    uint64_t next_timeout; // # of cycles until next timeout
    

    int (*callback)(struct guest_info * core, void * private_data);
    void * private_data;
};



int v3_add_core_timeout(struct guest_info * core, uint64_t cycles,
			int (*callback)(struct guest_info * core, 
					void * private_data),
			void * private_data);


int v3_handle_timeouts(struct guest_info * core, uint64_t guest_cycles);


#endif

#endif
