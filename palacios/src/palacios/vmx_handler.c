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
#include <palacios/vmx.h>
#include <palacios/vmm_ctrl_regs.h>


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

static void inline translate_access_to_v3_seg(struct vmcs_segment_access * access, 
        struct v3_segment * v3_seg)
{
    v3_seg->type = access->type;
    v3_seg->system = access->desc_type;
    v3_seg->dpl = access->dpl;
    v3_seg->present = access->present;
    v3_seg->avail = access->avail;
    v3_seg->long_mode = access->long_mode;
    v3_seg->db = access->db;
    v3_seg->granularity = access->granularity;
}

static void load_vmcs_guest_state(struct guest_info * info)
{
    check_vmcs_read(VMCS_GUEST_RIP, &(info->rip));
    check_vmcs_read(VMCS_GUEST_RSP, &(info->vm_regs.rsp));
    check_vmcs_read(VMCS_GUEST_CR0, &(info->ctrl_regs.cr0));
    check_vmcs_read(VMCS_GUEST_CR3, &(info->ctrl_regs.cr3));
    check_vmcs_read(VMCS_GUEST_CR4, &(info->ctrl_regs.cr4));

    struct vmcs_segment_access access;

    memset(&access, 0, sizeof(access));

    /* CS Segment */
    check_vmcs_read(VMCS_GUEST_CS_BASE, &(info->segments.cs.base));
    check_vmcs_read(VMCS_GUEST_CS_SELECTOR, &(info->segments.cs.selector));
    check_vmcs_read(VMCS_GUEST_CS_LIMIT, &(info->segments.cs.limit));
    check_vmcs_read(VMCS_GUEST_CS_ACCESS, &(access.value));

    translate_access_to_v3_seg(&access, &(info->segments.cs));

    /* SS Segment */
    check_vmcs_read(VMCS_GUEST_SS_BASE, &(info->segments.ss.base));
    check_vmcs_read(VMCS_GUEST_SS_SELECTOR, &(info->segments.ss.selector));
    check_vmcs_read(VMCS_GUEST_SS_LIMIT, &(info->segments.ss.limit));
    check_vmcs_read(VMCS_GUEST_SS_ACCESS, &(access.value));

    translate_access_to_v3_seg(&access, &(info->segments.ss));

    /* DS Segment */
    check_vmcs_read(VMCS_GUEST_DS_BASE, &(info->segments.ds.base));
    check_vmcs_read(VMCS_GUEST_DS_SELECTOR, &(info->segments.ds.selector));
    check_vmcs_read(VMCS_GUEST_DS_LIMIT, &(info->segments.ds.limit));
    check_vmcs_read(VMCS_GUEST_DS_ACCESS, &(access.value));

    translate_access_to_v3_seg(&access, &(info->segments.ds));

    /* ES Segment */
    check_vmcs_read(VMCS_GUEST_ES_BASE, &(info->segments.es.base));
    check_vmcs_read(VMCS_GUEST_ES_SELECTOR, &(info->segments.es.selector));
    check_vmcs_read(VMCS_GUEST_ES_LIMIT, &(info->segments.es.limit));
    check_vmcs_read(VMCS_GUEST_ES_ACCESS, &(access.value));

    translate_access_to_v3_seg(&access, &(info->segments.es));

    /* FS Segment */
    check_vmcs_read(VMCS_GUEST_FS_BASE, &(info->segments.fs.base));
    check_vmcs_read(VMCS_GUEST_FS_SELECTOR, &(info->segments.fs.selector));
    check_vmcs_read(VMCS_GUEST_FS_LIMIT, &(info->segments.fs.limit));
    check_vmcs_read(VMCS_GUEST_FS_ACCESS, &(access.value));

    translate_access_to_v3_seg(&access, &(info->segments.fs));


    /* GS Segment */
    check_vmcs_read(VMCS_GUEST_GS_BASE, &(info->segments.gs.base));
    check_vmcs_read(VMCS_GUEST_GS_SELECTOR, &(info->segments.gs.selector));
    check_vmcs_read(VMCS_GUEST_GS_LIMIT, &(info->segments.gs.limit));
    check_vmcs_read(VMCS_GUEST_GS_ACCESS, &(access.value));

    translate_access_to_v3_seg(&access, &(info->segments.gs));

    /* LDTR Segment */
    check_vmcs_read(VMCS_GUEST_LDTR_BASE, &(info->segments.ldtr.base));
    check_vmcs_read(VMCS_GUEST_LDTR_SELECTOR, &(info->segments.ldtr.selector));
    check_vmcs_read(VMCS_GUEST_LDTR_LIMIT, &(info->segments.ldtr.limit));
    check_vmcs_read(VMCS_GUEST_LDTR_ACCESS, &(access.value));

    translate_access_to_v3_seg(&access, &(info->segments.ldtr));

    /* TR Segment */
    check_vmcs_read(VMCS_GUEST_TR_BASE, &(info->segments.tr.base));
    check_vmcs_read(VMCS_GUEST_TR_SELECTOR, &(info->segments.tr.selector));
    check_vmcs_read(VMCS_GUEST_TR_LIMIT, &(info->segments.tr.limit));
    check_vmcs_read(VMCS_GUEST_TR_ACCESS, &(access.value));

    translate_access_to_v3_seg(&access, &(info->segments.tr));

    /* GDTR Segment */
    check_vmcs_read(VMCS_GUEST_GDTR_BASE, &(info->segments.gdtr.base));
    check_vmcs_read(VMCS_GUEST_GDTR_LIMIT, &(info->segments.gdtr.limit));
    
    /* IDTR Segment */
    check_vmcs_read(VMCS_GUEST_IDTR_BASE, &(info->segments.idtr.base));
    check_vmcs_read(VMCS_GUEST_IDTR_LIMIT, &(info->segments.idtr.limit));
}


static void setup_v8086_mode_for_boot(struct guest_info * info)
{

    ((struct vmx_data *)info->vmm_data)->state = VMXASSIST_V8086_BIOS;
    struct rflags * flags = (struct rflags *)&(info->ctrl_regs.rflags);
    flags->rsvd1 = 1;
    flags->vm = 1;
    flags->iopl = 3;

    info->rip = 0xfff0;
    //info->vm_regs.rsp = 0x0;
   
    /* Zero the segment registers */
    memset(&(info->segments), 0, sizeof(struct v3_segment)*6);


    info->segments.cs.selector = 0xf000;
    info->segments.cs.base = 0xf000 << 4;
    info->segments.cs.limit = 0xffff;
    info->segments.cs.type = 3;
    info->segments.cs.system = 1;
    info->segments.cs.dpl = 3;
    info->segments.cs.present = 1;
    info->segments.cs.granularity = 0;

    int i;
    
    /* Set values for selectors ds through ss */
    struct v3_segment * seg_ptr = (struct v3_segment *)&(info->segments);
    for(i = 1; i < 6 ; i++) {
        seg_ptr[i].selector = 0x0000;
        seg_ptr[i].base = 0x00000;
        seg_ptr[i].limit = 0xffff;
        seg_ptr[i].type = 3;
        seg_ptr[i].system = 1;
        seg_ptr[i].dpl = 3;
        seg_ptr[i].present = 1;
        seg_ptr[i].granularity = 0;
    }

    PrintDebug("END INFO!\n");
#if 0
    for(i = 6; i < 10; i++) {
        seg_ptr[i].base = 0x0;
        seg_ptr[i].limit = 0xffff;
    }

    info->segments.ldtr.type = 2;
    info->segments.ldtr.system = 0;
    info->segments.ldtr.present = 1;
    info->segments.ldtr.granularity = 0;

    info->segments.tr.type = 3;
    info->segments.tr.system = 0;
    info->segments.tr.present = 1;
    info->segments.tr.granularity = 0;
#endif
}

static int inline handle_cr_access(struct guest_info * info, ulong_t exit_qual)
{
    struct vmexit_cr_qual * cr_qual = (struct vmexit_cr_qual *)&exit_qual;

    if(cr_qual->access_type < 2) {
        ulong_t reg = 0;
        switch(cr_qual->gpr) {
            case 0:
                reg = info->vm_regs.rax;
                break;
            case 1:
                reg = info->vm_regs.rcx;
                break;
            case 2:
                reg = info->vm_regs.rdx;
                break;
            case 3:
                reg = info->vm_regs.rbx;
                break;
            case 4:
                reg = info->vm_regs.rsp;
                break;
            case 5:
                reg = info->vm_regs.rbp;
                break;
            case 6:
                reg = info->vm_regs.rsi;
                break;
            case 7:
                reg = info->vm_regs.rdi;
                break;
            case 8:
                reg = info->vm_regs.r8;
                break;
            case 9:
                reg = info->vm_regs.r9;
                break;
            case 10:
                reg = info->vm_regs.r10;
                break;
            case 11:
                reg = info->vm_regs.r11;
                break;
            case 12:
                reg = info->vm_regs.r11;
                break;
            case 13:
                reg = info->vm_regs.r13;
                break;
            case 14:
                reg = info->vm_regs.r14;
                break;
            case 15:
                reg = info->vm_regs.r15;
                break;
        }
        PrintDebug("RAX: %p\n", (void *)info->vm_regs.rax);

        if(cr_qual->cr_id == 0
                && (~reg & CR0_PE)
                && ((struct vmx_data*)info->vmm_data)->state == VMXASSIST_STARTUP) {
            setup_v8086_mode_for_boot(info);
            info->shdw_pg_state.guest_cr0 = 0x0;
            v3_update_vmcs_guest_state(info);
            return 0;
        }
    }
    PrintError("Unhandled CR access\n");
    return -1;
}


int v3_handle_vmx_exit(struct v3_gprs * gprs, struct guest_info * info)
{
    uint32_t exit_reason;
    ulong_t exit_qual;

    check_vmcs_read(VMCS_EXIT_REASON, &exit_reason);
    check_vmcs_read(VMCS_EXIT_QUAL, &exit_qual);

    PrintDebug("VMX Exit taken, id-qual: %u-%lu\n", exit_reason, exit_qual);

    /* Update guest state */
    load_vmcs_guest_state(info);
  
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
            } else {
                PrintDebug("Unknown exception: 0x%x\n", (uint8_t)int_info);
                v3_print_GPRs(info);
                return -1;
            }
            break;
        }

        case VMEXIT_IO_INSTR: 
        {
            struct vmexit_io_qual * io_qual = (struct vmexit_io_qual *)&exit_qual;

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

        case VMEXIT_CR_REG_ACCESSES:
            if(handle_cr_access(info,exit_qual) != 0)
                return -1;
            break;

        default:
            PrintError("Unhandled VMEXIT\n");
            return -1;
    }

    check_vmcs_write(VMCS_GUEST_CR0, info->ctrl_regs.cr0);
    check_vmcs_write(VMCS_GUEST_CR3, info->ctrl_regs.cr3);
    check_vmcs_write(VMCS_GUEST_CR4, info->ctrl_regs.cr4);
    check_vmcs_write(VMCS_GUEST_RIP, info->rip);
    check_vmcs_write(VMCS_GUEST_RSP, info->vm_regs.rsp);

    check_vmcs_write(VMCS_CR0_READ_SHDW, info->shdw_pg_state.guest_cr0);

    return 0;
}
