/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef _VMX_ASSIST_H_
#define _VMX_ASSIST_H_

#ifdef __V3VEE__

#include <palacios/vm_guest.h>
#include <palacios/vmx.h>


#define VMXASSIST_GDT     0x10000
#define VMXASSIST_TSS     0x40000
#define VMXASSIST_START   0xd0000
#define VMXASSIST_1to1_PT 0xde000 // We'll shove this at the end, and pray to god VMXASSIST doesn't mess with it


int v3_vmxassist_ctx_switch(struct guest_info * info);
int v3_vmxassist_init(struct guest_info * core, struct vmx_data * vmx_state);

#endif

#endif /* _VMX_ASSIST_H_ */
