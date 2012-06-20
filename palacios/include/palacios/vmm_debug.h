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


#ifndef __VMM_DEBUG_H__
#define __VMM_DEBUG_H__


#ifdef __V3VEE__

#include <palacios/vmm.h>
#include <palacios/vmm_regs.h>

int v3_init_vm_debugging(struct v3_vm_info * vm);


void v3_print_guest_state(struct guest_info * core);
void v3_print_arch_state(struct guest_info * core);

void v3_print_segments(struct v3_segments * segs);
void v3_print_ctrl_regs(struct guest_info * core);
void v3_print_GPRs(struct guest_info * core);

void v3_print_backtrace(struct guest_info * core);
void v3_print_stack(struct guest_info * core);
void v3_print_guest_state_all(struct v3_vm_info * vm);

#endif // !__V3VEE__

#endif
