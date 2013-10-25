/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2013, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_debug.h>
#include <palacios/vmm_types.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_lowlevel.h>

#include <interfaces/vmm_lazy_fpu.h>

struct v3_lazy_fpu_iface * palacios_lazy_fpu_hooks = 0;



void V3_Init_Lazy_FPU (struct v3_lazy_fpu_iface * lazy_fpu_iface) 
{
    palacios_lazy_fpu_hooks = lazy_fpu_iface;
}


