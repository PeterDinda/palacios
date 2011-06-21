/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Kyle C. Hale <kh@u.norhtwestern.edu>
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_extensions.h>



static int init_swintr_intercept (struct v3_vm_info * vm, v3_cfg_tree_t * cfg, void ** priv_data);

    return 0;
}


static int deinit_swintr_intercept (struct v3_vm_info * vm, void * priv_data) {
    
    return 0;
}


static int init_swintr_intercept_core (struct guest_info * core, void * priv_data) {

    return 0;
}


static int deinit_swintr_intercept_core (struct guest_info * core, void * priv_data) {

    return 0;
}


static int swintr_entry (struct guest_info * core, void * priv_data) {

    return 0;
}

static int swintr_exit (struct guest_info * core, void * priv_data) {

    return 0;
}



static struct v3_extension_impl swintr_impl = {
    .name = "swintr_intercept",
    .init = init_swintr_intercept,
    .deinit = deinit_swintr_intercept,
    .core_init = init_swintr_intercept_core,
    .core_deinit = deinit_swintr_intercept_core,
    .on_entry = swintr_entry,
    .on_exit = swintr_exit
};

register_extension(&swintr_impl);


int v3_handle_swintr (struct guest_info * core) {

    return 0;
}



