/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */




#include <palacios/svm_wbinvd.h>
#include <palacios/vmm_intr.h>


// Writeback and invalidate caches
// should raise #GP if CPL is not zero
// Otherwise execute

int v3_handle_svm_wbinvd(struct guest_info * info) {

    if (info->cpl != 0) { 
	PrintDebug("WBINVD: cpl != 0, injecting GPF\n");
	v3_raise_exception(info, GPF_EXCEPTION);
    } else {
	info->rip += 2;
	asm volatile ("wbinvd" ::: "memory");
    }

    return 0;
}
