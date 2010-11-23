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

#ifndef __DEVICES_APIC_H__
#define __DEVICES_APIC_H__

#ifdef __V3VEE__

#include <palacios/vmm_dev_mgr.h>


typedef enum {IPI_FIXED = 0,
	      IPI_LOWEST_PRIO = 1,
	      IPI_SMI = 2,
	      IPI_NMI = 4,
	      IPI_INIT = 5,
	      IPI_EXINT = 7 } ipi_mode_t; 


struct v3_gen_ipi {
    uint8_t vector;
    ipi_mode_t mode;

    uint8_t logical      : 1;
    uint8_t trigger_mode : 1;
    uint8_t dst_shorthand : 2;

    uint8_t dst;
} __attribute__((packed));

int v3_apic_send_ipi(struct v3_vm_info * vm, struct v3_gen_ipi * ipi, void * dev_data);

int v3_apic_raise_intr(struct v3_vm_info * vm, 
		       uint32_t irq, uint32_t dst,
		       void * dev_data);




#endif // ! __V3VEE__
#endif
