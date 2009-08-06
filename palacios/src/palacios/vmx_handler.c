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

#include <palacios/vmx_handler.h>
#include <palacios/vmm_types.h>
#include <palacios/vmm.h>
#include <palacios/vmcs.h>
#include <palacios/vmx_lowlevel.h>


static int inline check_vmcs_write(vmcs_field_t field, addr_t val)
{
    int ret = 0;
    ret = vmcs_write(field,val);

    if (ret != VMX_SUCCESS) {
        PrintError("VMWRITE error on %s!: %d\n", v3_vmcs_field_to_str(field), ret);
        return 1;
    }

    return 0;
}

static int inline check_vmcs_read(vmcs_field_t field, void * val)
{
    int ret = 0;
    ret = vmcs_read(field,val);

    if(ret != VMX_SUCCESS) {
        PrintError("VMREAD error on %s!: %d\n", v3_vmcs_field_to_str(field), ret);
        return 1;
    }

    return 0;
}

int v3_handle_vmx_exit(struct v3_gprs * gprs)
{
    uint32_t exit_reason;
    ulong_t exit_qual;

    check_vmcs_read(VMCS_EXIT_REASON, &exit_reason);
    check_vmcs_read(VMCS_EXIT_QUAL, &exit_qual);
    PrintDebug("VMX Exit taken, id-qual: %x-%ld\n", exit_reason, exit_qual);
    return -1;
}
