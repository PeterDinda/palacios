/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2012, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm_mwait.h>

#ifndef V3_CONFIG_DEBUG_MWAIT
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


//
// Currently we disallow mwait in the CPUID field, so we need to raise an exception
//
int v3_handle_mwait(struct guest_info * info) 
{
    PrintDebug("Raising undefined opcode due to mwait instruction\n");

    v3_raise_exception(info, UD_EXCEPTION );

    return 0;
}

//
// Currently we disallow mwait in the CPUID field, so we need to raise an exception
//
int v3_handle_monitor(struct guest_info * info) 
{
    PrintDebug("Raising undefined opcode due to monitor instruction\n");

    v3_raise_exception(info, UD_EXCEPTION );

    return 0;
}
