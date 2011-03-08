/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Andy Gocke <agocke@gmail.com>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Andy Gocke <agocke@gmail.com>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmx_assist.h>
#include <palacios/vmx_lowlevel.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmx.h>

#ifndef CONFIG_DEBUG_VMX
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

static void vmx_save_world_ctx(struct guest_info * info, struct vmx_assist_context * ctx);
static void vmx_restore_world_ctx(struct guest_info * info, struct vmx_assist_context * ctx);

int v3_vmxassist_ctx_switch(struct guest_info * info) {
    struct vmx_assist_context * old_ctx = NULL;
    struct vmx_assist_context * new_ctx = NULL;
    struct vmx_assist_header * hdr = NULL;
    struct vmx_data * vmx_info = (struct vmx_data *)info->vmm_data;
 


    if (v3_gpa_to_hva(info, VMXASSIST_BASE, (addr_t *)&hdr) == -1) {
        PrintError("Could not translate address for vmxassist header\n");
        return -1;
    }

    if (hdr->magic != VMXASSIST_MAGIC) {
        PrintError("VMXASSIST_MAGIC field is invalid\n");
        return -1;
    }


    if (v3_gpa_to_hva(info, (addr_t)(hdr->old_ctx_gpa), (addr_t *)&(old_ctx)) == -1) {
        PrintError("Could not translate address for VMXASSIST old context\n");
        return -1;
    }

    if (v3_gpa_to_hva(info, (addr_t)(hdr->new_ctx_gpa), (addr_t *)&(new_ctx)) == -1) {
        PrintError("Could not translate address for VMXASSIST new context\n");
        return -1;
    }

    if (vmx_info->assist_state == VMXASSIST_DISABLED) {
        
        /* Save the old Context */
	vmx_save_world_ctx(info, old_ctx);

        /* restore new context, vmxassist should launch the bios the first time */
        vmx_restore_world_ctx(info, new_ctx);

        vmx_info->assist_state = VMXASSIST_ENABLED;

    } else if (vmx_info->assist_state == VMXASSIST_ENABLED) {
        /* restore old context */
	vmx_restore_world_ctx(info, old_ctx);

        vmx_info->assist_state = VMXASSIST_DISABLED;
    }

    return 0;
}


static void save_segment(struct v3_segment * seg, struct vmx_assist_segment * vmx_assist_seg) {
    struct vmcs_segment tmp_seg;

    memset(&tmp_seg, 0, sizeof(struct vmcs_segment));

    v3_seg_to_vmxseg(seg, &tmp_seg);

    vmx_assist_seg->sel = tmp_seg.selector;
    vmx_assist_seg->limit = tmp_seg.limit;
    vmx_assist_seg->base = tmp_seg.base;
    vmx_assist_seg->arbytes.bytes = tmp_seg.access.val;
}


static void load_segment(struct vmx_assist_segment * vmx_assist_seg, struct v3_segment * seg)  {
    struct vmcs_segment tmp_seg;

    memset(&tmp_seg, 0, sizeof(struct vmcs_segment));

    tmp_seg.selector = vmx_assist_seg->sel;
    tmp_seg.limit = vmx_assist_seg->limit;
    tmp_seg.base = vmx_assist_seg->base;
    tmp_seg.access.val = vmx_assist_seg->arbytes.bytes;

    v3_vmxseg_to_seg(&tmp_seg, seg);
}

static void vmx_save_world_ctx(struct guest_info * info, struct vmx_assist_context * ctx) {
    struct vmx_data * vmx_info = (struct vmx_data *)(info->vmm_data);

    PrintDebug("Writing from RIP: 0x%p\n", (void *)(addr_t)info->rip);
    
    ctx->eip = info->rip;
    ctx->esp = info->vm_regs.rsp;
    ctx->eflags = info->ctrl_regs.rflags;

    ctx->cr0 = info->shdw_pg_state.guest_cr0;
    ctx->cr3 = info->shdw_pg_state.guest_cr3;
    ctx->cr4 = vmx_info->guest_cr4;

    
    save_segment(&(info->segments.cs), &(ctx->cs));
    save_segment(&(info->segments.ds), &(ctx->ds));
    save_segment(&(info->segments.es), &(ctx->es));
    save_segment(&(info->segments.ss), &(ctx->ss));
    save_segment(&(info->segments.fs), &(ctx->fs));
    save_segment(&(info->segments.gs), &(ctx->gs));
    save_segment(&(info->segments.tr), &(ctx->tr));
    save_segment(&(info->segments.ldtr), &(ctx->ldtr));

    // Odd segments 
    ctx->idtr_limit = info->segments.idtr.limit;
    ctx->idtr_base = info->segments.idtr.base;

    ctx->gdtr_limit = info->segments.gdtr.limit;
    ctx->gdtr_base = info->segments.gdtr.base;
}

static void vmx_restore_world_ctx(struct guest_info * info, struct vmx_assist_context * ctx) {
    struct vmx_data * vmx_info = (struct vmx_data *)(info->vmm_data);

    PrintDebug("ctx rip: %p\n", (void *)(addr_t)ctx->eip);
    
    info->rip = ctx->eip;
    info->vm_regs.rsp = ctx->esp;
    info->ctrl_regs.rflags = ctx->eflags;

    info->shdw_pg_state.guest_cr0 = ctx->cr0;
    info->shdw_pg_state.guest_cr3 = ctx->cr3;
    vmx_info->guest_cr4 = ctx->cr4;

    load_segment(&(ctx->cs), &(info->segments.cs));
    load_segment(&(ctx->ds), &(info->segments.ds));
    load_segment(&(ctx->es), &(info->segments.es));
    load_segment(&(ctx->ss), &(info->segments.ss));
    load_segment(&(ctx->fs), &(info->segments.fs));
    load_segment(&(ctx->gs), &(info->segments.gs));
    load_segment(&(ctx->tr), &(info->segments.tr));
    load_segment(&(ctx->ldtr), &(info->segments.ldtr));

    // odd segments
    info->segments.idtr.limit = ctx->idtr_limit;
    info->segments.idtr.base = ctx->idtr_base;

    info->segments.gdtr.limit = ctx->gdtr_limit;
    info->segments.gdtr.base = ctx->gdtr_base;

}


