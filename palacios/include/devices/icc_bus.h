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

/**
 *
 */
int v3_icc_register_apic(struct v3_vm_info *info, struct vm_device *icc_bus, struct vm_device *apic, uint32_t apic_num);

/**
 * Send an inter-processor interrupt (IPI) from this local APIC to another local APIC.
 *
 * @param icc_bus The ICC bus that facilitates the communication.
 * @param apic_num The remote APIC number.
 * @param intr_num The interrupt number.
 */
int v3_icc_send_ipi(struct vm_device * icc_bus, uint32_t apic_num, uint32_t intr_num);

#endif /* ICC_BUS_H_ */
