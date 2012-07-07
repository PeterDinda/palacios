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
#include <palacios/vmm_dev_mgr.h>

#define TIMER_IRQ 32

struct timer_state {
    int foo;
};


/*
  static int irq_handler(uint_t irq, struct vm_device * dev) {
  PrintDebug("Timer interrupt\n");
  return 0;

  }
*/



static int timer_free(struct vm_device * dev) {

    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = timer_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,

};


static int timer_init(struct guest_info * vm, void * cfg_data) {
    struct timer_state * timer = NULL;
    timer = (struct timer_state *)V3_Malloc( sizeof(struct timer_state));

    if (!timer) {
	PrintError("Cannot allocate in init\n");
	return -1;
    }

    struct vm_device * dev = v3_allocate_device("TIMER", &dev_ops, timer);
    
    if (v3_attach_device(vm, dev) == -1) {
	PrintError("Could not attach device %s\n", "TIMER");
        return -1;
    }



    return -1;

}


device_register("TIMER", timer_init)
