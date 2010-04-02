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

#include <palacios/vmm.h>
#include <palacios/vmm_muxer.h>
#include <palacios/vmm_list.h>



static struct v3_vm_info * foreground_vm = NULL;

// list of notification callbacks
static LIST_HEAD(cb_list);


struct mux_callback {
    struct list_head cb_node;

    int (*focus_change)(struct v3_vm_info * old_vm, struct v3_vm_info * new_vm);
};


struct v3_vm_info * v3_get_foreground_vm() {
    return foreground_vm;
}


void v3_set_foreground_vm(struct v3_vm_info * vm) {
    struct mux_callback * tmp_cb;

    list_for_each_entry(tmp_cb, &(cb_list), cb_node) {
	tmp_cb->focus_change(foreground_vm, vm);
    }

    foreground_vm = vm;
}


int v3_add_mux_notification(int (*focus_change)(struct v3_vm_info * old_vm, 
						struct v3_vm_info * new_vm)) {

    struct mux_callback * cb = (struct mux_callback *)V3_Malloc(sizeof(struct mux_callback));

    cb->focus_change = focus_change;
    
    list_add(&(cb->cb_node), &cb_list);

    return 0;
}
