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

#ifndef ICC_BUS_H_
#define ICC_BUS_H_


struct v3_icc_ops {
    int (*raise_intr)(struct guest_info * core, int intr_num, void * private_data);
};


/**
 *
 */
int v3_icc_register_apic(struct guest_info * vm, struct vm_device * icc_bus, uint8_t apic_phys_id, struct v3_icc_ops * ops, void * priv_data);


/**
 * Send an inter-processor interrupt (IPI) from this local APIC to another local APIC.
 *
 * @param icc_bus - The ICC bus that routes IPIs.
 * @param apic_num - The remote APIC number.
 * @param intr_num - The interrupt number.
 */
int v3_icc_send_irq(struct vm_device * icc_bus, uint8_t apic_num, uint32_t irq_num);




#endif /* ICC_BUS_H_ */
