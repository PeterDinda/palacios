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

#include <devices/8237_dma.h>




struct dma_state {
    int tmp
};


static int dma_init(struct vm_device * dev) {
    return 0;
}



static struct v3_device_ops dev_ops = {
    .free = NULL,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};

struct vm_device * v3_create_dma() {
    struct dma_state * dma = NULL;

    dma = (struct dma_state *)V3_Malloc(sizeof(struct dma_state));
    V3_ASSERT(dma != NULL);

    struct vm_device * dev = v3_create_device("DMA", &dev_ops, dma);

    return dma;
}
