/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2009, Chang Seok Bae <jhuell@gmail.com>
 * Copyright (c) 2009, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author:  Lei Xia <lxia@northwestern.edu>
 *          Chang Seok Bae <jhuell@gmail.com>
 *          Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */ 
 
#include <devices/piix3.h>
#include <palacios/vmm.h>
#include <devices/pci.h>


struct piix3_state {
    uint8_t pci_dev_num;

    struct vm_device * pci;

};


static int init_piix3(struct vm_device * dev) {

    return 0;
}


static int deinit_piix3(struct vm_device * dev) {
    return 0;
}


static struct vm_device_ops dev_ops = {
    .init = init_piix3,
    .deinit = deinit_piix3,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
};


struct vm_device * v3_create_piix3(struct vm_device * pci) {
    struct piix3_state * piix3 = (struct piix3_state *)V3_Malloc(sizeof(struct piix3_state));
    struct vm_device * dev = NULL;

    piix3->pci = pci;
    
    dev = v3_create_device("PIIX3", &dev_ops, piix3);

    PrintDebug("Created PIIX3\n");

    return dev;
}
