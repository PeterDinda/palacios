/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Jack Lange <jacklange@cs.pitt.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jacklangel@cs.pitt.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <util/vmm_barrier.h>



int v3_init_barrier(struct v3_barrier * barrier) {
    memset(barrier, 0, sizeof(struct v3_barrier));
    v3_lock_init(&(barrier->lock));

    return 0;
}


int v3_activate_barrier(struct guest_info * core, struct v3_barrier * barrier) {
    addr_t flag;
    int acquired = 0;
    
    flag = v3_lock_irqsave(barrier->lock);

    if (barrier->active == 0) {
	barrier->active = 1;
	acquired = 1;
    }

    v3_unlock_irqrestore(barrier->lock, flag);

    if (acquired == 0) {
	return -1;
    }


    // wait for barrier catch


    return 0;
}




int v3_deactivate_barrier(struct v3_barrier * barrier) {

}


int v3_check_barrier(struct guest_info * core, struct v3_barrier * barrier) {

    if (barrier->activated == 0) {
	return 0;
    }
    
    // set cpu bit

    // wait for cpu bit to clear

}
