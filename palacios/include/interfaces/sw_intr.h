/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Kyle C. Hale <kh@u.northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __SW_INTR_H__
#define __SW_INTR_H__

#include <palacios/vmm.h>


int v3_handle_swintr (struct guest_info * core);

int v3_hook_swintr (struct guest_info * core,
        uint8_t vector,
        int (*handler)(struct guest_info * core, uint8_t vector, void * priv_data),
        void * priv_data);
int v3_hook_passthrough_swintr (struct guest_info * core, uint8_t vector);


#endif
