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
int v3_icc_register_apic(struct guest_info *core, struct vm_device * icc_bus, uint8_t apic_phys_id, struct v3_icc_ops * ops, void * priv_data);
int v3_icc_register_ioapic(struct v3_vm_info *vm, struct vm_device * icc_bus, uint8_t apic_phys_id);

/**
 * Send an inter-processor interrupt (IPI) from one local APIC to another local APIC.
 *
 * @param icc_bus  - The ICC bus that routes IPIs.
 * @param apic_src - The source APIC id.
 * @param apic_num - The remote APIC number.
 * @param icr      - A copy of the APIC's ICR.  (LAPIC-style ICR, clone from redir table for ioapics)
 & @param extirq   - irq for external interrupts (e.g., from 8259)
 */
int v3_icc_send_ipi(struct vm_device * icc_bus, uint32_t apic_src, uint64_t icr, uint32_t ext_irq);


#if 0
/**
 * Send an IRQinter-processor interrupt (IPI) from one local APIC to another local APIC.
 *
 * @param icc_bus  - The ICC bus that routes IPIs.
 * @param apic_src - The source APIC id.
 * @param apic_num - The remote APIC number.
 * @param icrlo    - The low 32 bites of the APIC's ICR.
 */
int v3_icc_send_irq(struct vm_device * icc_bus, uint32_t ioapic_src, uint8_t apic_num, uint8_t irq);

#endif


#endif /* ICC_BUS_H_ */
