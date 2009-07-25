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


#include <palacios/vmm_intr.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm.h>

struct pic_internal {
    int pending_irq;

};


static int pic_intr_pending(void * private_data) {
    struct pic_internal * data = (struct pic_internal *)private_data;
  
    return (data->pending_irq > 0);
}

static int pic_raise_intr(void * private_data, int irq) {
    struct pic_internal * data = (struct pic_internal *)private_data;

    data->pending_irq = irq;


    return 0;
}


static int pic_get_intr_number(void * private_data) {
    struct pic_internal * data = (struct pic_internal *)private_data;

    return data->pending_irq;
}


static struct intr_ctrl_ops intr_ops = {
    .intr_pending = pic_intr_pending,
    .get_intr_number = pic_get_intr_number,
    .raise_intr = pic_raise_intr
};






static int pic_free(struct vm_device * dev) {
    return 0;
}




static struct v3_device_ops dev_ops = { 
    .free = pic_free,
    .reset = NULL,
    .start = NULL,
    .stop = NULL
};




static int pic_init(struct guest_info * vm, void * cfg_data) {
    struct pic_internal * state = NULL;
    state = (struct pic_internal *)V3_Malloc(sizeof(struct pic_internal));
    V3_ASSERT(state != NULL);

    struct vm_device * dev = v3_allocate_device("SIMPLE_PIC", &dev_ops, state);

    if (v3_attach_device(vm, dev) == -1) {
        PrintError("Could not attach device %s\n", "SIMPLE_PIC");
        return -1;
    }


    v3_register_intr_controller(vm, &intr_ops, state);
    state->pending_irq = 0;

    return 0;
}

device_register("SIMPLE_PIC", pic_init)
