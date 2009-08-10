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
#include <palacios/vmx_io.h>


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
        return ret;
    }

    return 0;
}

int v3_handle_vmx_exit(struct v3_gprs * gprs, struct guest_info * info)
{
    uint32_t exit_reason;
    ulong_t exit_qual;

    check_vmcs_read(VMCS_EXIT_REASON, &exit_reason);
    check_vmcs_read(VMCS_EXIT_QUAL, &exit_qual);

    PrintDebug("VMX Exit taken, id-qual: %d-%ld\n", exit_reason, exit_qual);

    /* Update guest state */
    check_vmcs_read(VMCS_GUEST_RIP, &(info->rip));
    check_vmcs_read(VMCS_GUEST_RSP, &(info->vm_regs.rsp));
    check_vmcs_read(VMCS_GUEST_CR0, &(info->ctrl_regs.cr0));
    check_vmcs_read(VMCS_GUEST_CR3, &(info->ctrl_regs.cr3));
    check_vmcs_read(VMCS_GUEST_CR4, &(info->ctrl_regs.cr4));

    // read out segments

    switch(exit_reason)
    {
        case VMEXIT_INFO_EXCEPTION_OR_NMI:
        {
            uint32_t int_info;
            pf_error_t error_code;
            check_vmcs_read(VMCS_EXIT_INT_INFO, &int_info);
            check_vmcs_read(VMCS_EXIT_INT_ERR, &error_code);

            if((uint8_t)int_info == 0x0e) {
                PrintDebug("Page Fault at %p\n", (void*)exit_qual);
                if(info->shdw_pg_mode == SHADOW_PAGING) {
                    if(v3_handle_shadow_pagefault(info, (addr_t)exit_qual, error_code) == -1) {
                        return -1;
                    }
                } else {
                    PrintError("Page fault in unimplemented paging mode\n");
                    return -1;
                }
            }
            break;
        }

        case VMEXIT_IO_INSTR: 
        {
            struct vmcs_io_qual * io_qual = (struct vmcs_io_qual *)&exit_qual;

            if(io_qual->dir == 0) {
                if(io_qual->string) {
                    if(v3_handle_vmx_io_outs(info) == -1) {
                        return -1;
                    }
                } else {
                    if(v3_handle_vmx_io_out(info) == -1) {
                        return -1;
                    }
                }
            } else {
                if(io_qual->string) {
                    if(v3_handle_vmx_io_ins(info) == -1) {
                        return -1;
                    }
                } else {
                    if(v3_handle_vmx_io_in(info) == -1) {
                        return -1;
                    }
                }
            }
            break;
        }

        default:
            PrintError("Unhandled VMEXIT\n");
            return -1;
    }

    check_vmcs_write(VMCS_GUEST_CR0, info->ctrl_regs.cr0);
    check_vmcs_write(VMCS_GUEST_CR3, info->ctrl_regs.cr3);
    check_vmcs_write(VMCS_GUEST_CR4, info->ctrl_regs.cr4);
    check_vmcs_write(VMCS_GUEST_RIP, info->rip);
    check_vmcs_write(VMCS_GUEST_RSP, info->vm_regs.rsp);

    PrintDebug("Executing VMRESUME\n");
    return 0;
}
