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

static int vmx_save_world_ctx(struct guest_info * info, struct vmx_assist_context * ctx);
static int vmx_restore_world_ctx(struct guest_info * info, struct vmx_assist_context * ctx);

int v3_vmxassist_ctx_switch(struct guest_info * info) {
    struct vmx_assist_context * old_ctx = NULL;
    struct vmx_assist_context * new_ctx = NULL;
    struct vmx_assist_header * hdr = NULL;
    struct vmx_data * vmx_info = (struct vmx_data *)info->vmm_data;
 


    if (guest_pa_to_host_va(info, VMXASSIST_BASE, (addr_t *)&hdr) == -1) {
        PrintError("Could not translate address for vmxassist header\n");
        return -1;
    }

    if (hdr->magic != VMXASSIST_MAGIC) {
        PrintError("VMXASSIST_MAGIC field is invalid\n");
        return -1;
    }


    if (guest_pa_to_host_va(info, (addr_t)(hdr->old_ctx_gpa), (addr_t *)&(old_ctx)) == -1) {
        PrintError("Could not translate address for VMXASSIST old context\n");
        return -1;
    }

    if (guest_pa_to_host_va(info, (addr_t)(hdr->new_ctx_gpa), (addr_t *)&(new_ctx)) == -1) {
        PrintError("Could not translate address for VMXASSIST new context\n");
        return -1;
    }

    if (vmx_info->state == VMXASSIST_DISABLED) {
        
        /* Save the old Context */
        if (vmx_save_world_ctx(info, old_ctx) != 0) {
            PrintError("Could not save VMXASSIST world context\n");
            return -1;
        }

        /* restore new context, vmxassist should launch the bios the first time */
        if (vmx_restore_world_ctx(info, new_ctx) != 0) {
            PrintError("VMXASSIST could not restore new context\n");
            return -1;
        }

        vmx_info->state = VMXASSIST_ENABLED;

    } else if (vmx_info->state == VMXASSIST_ENABLED) {
        /* restore old context */
        if (vmx_restore_world_ctx(info, old_ctx) != 0) {
            PrintError("VMXASSIST could not restore old context\n");
            return -1;
        }

        vmx_info->state = VMXASSIST_DISABLED;
    }

    return 0;
}

        
int vmx_save_world_ctx(struct guest_info * info, struct vmx_assist_context * ctx) {
    int error = 0;

    PrintDebug("Writing from RIP: 0x%p\n", (void *)info->rip);

    error |= vmcs_read(VMCS_GUEST_RIP, &(ctx->eip));
    error |= vmcs_read(VMCS_GUEST_RSP, &(ctx->esp));
    error |= vmcs_read(VMCS_GUEST_RFLAGS, &(ctx->eflags));

    error |= vmcs_read(VMCS_CR0_READ_SHDW, &(ctx->cr0));
    ctx->cr3 = info->shdw_pg_state.guest_cr3;
    error |= vmcs_read(VMCS_CR4_READ_SHDW, &(ctx->cr4));

    error |= vmcs_read(VMCS_GUEST_IDTR_LIMIT, &(ctx->idtr_limit));
    error |= vmcs_read(VMCS_GUEST_IDTR_BASE, &(ctx->idtr_base));

    error |= vmcs_read(VMCS_GUEST_GDTR_LIMIT, &(ctx->gdtr_limit));
    error |= vmcs_read(VMCS_GUEST_GDTR_BASE, &(ctx->gdtr_base));

    error |= vmcs_read(VMCS_GUEST_CS_SELECTOR, &(ctx->cs_sel));
    error |= vmcs_read(VMCS_GUEST_CS_LIMIT, &(ctx->cs_limit));
    error |= vmcs_read(VMCS_GUEST_CS_BASE, &(ctx->cs_base));
    error |= vmcs_read(VMCS_GUEST_CS_ACCESS, &(ctx->cs_arbytes.bytes));

    error |= vmcs_read(VMCS_GUEST_DS_SELECTOR, &(ctx->ds_sel));
    error |= vmcs_read(VMCS_GUEST_DS_LIMIT, &(ctx->ds_limit));
    error |= vmcs_read(VMCS_GUEST_DS_BASE, &(ctx->ds_base));
    error |= vmcs_read(VMCS_GUEST_DS_ACCESS, &(ctx->ds_arbytes.bytes));

    error |= vmcs_read(VMCS_GUEST_ES_SELECTOR, &(ctx->es_sel));
    error |= vmcs_read(VMCS_GUEST_ES_LIMIT, &(ctx->es_limit));
    error |= vmcs_read(VMCS_GUEST_ES_BASE, &(ctx->es_base));
    error |= vmcs_read(VMCS_GUEST_ES_ACCESS, &(ctx->es_arbytes.bytes));

    error |= vmcs_read(VMCS_GUEST_SS_SELECTOR, &(ctx->ss_sel));
    error |= vmcs_read(VMCS_GUEST_SS_LIMIT, &(ctx->ss_limit));
    error |= vmcs_read(VMCS_GUEST_SS_BASE, &(ctx->ss_base));
    error |= vmcs_read(VMCS_GUEST_SS_ACCESS, &(ctx->ss_arbytes.bytes));

    error |= vmcs_read(VMCS_GUEST_FS_SELECTOR, &(ctx->fs_sel));
    error |= vmcs_read(VMCS_GUEST_FS_LIMIT, &(ctx->fs_limit));
    error |= vmcs_read(VMCS_GUEST_FS_BASE, &(ctx->fs_base));
    error |= vmcs_read(VMCS_GUEST_FS_ACCESS, &(ctx->fs_arbytes.bytes));

    error |= vmcs_read(VMCS_GUEST_GS_SELECTOR, &(ctx->gs_sel));
    error |= vmcs_read(VMCS_GUEST_GS_LIMIT, &(ctx->gs_limit));
    error |= vmcs_read(VMCS_GUEST_GS_BASE, &(ctx->gs_base));
    error |= vmcs_read(VMCS_GUEST_GS_ACCESS, &(ctx->gs_arbytes.bytes));

    error |= vmcs_read(VMCS_GUEST_TR_SELECTOR, &(ctx->tr_sel));
    error |= vmcs_read(VMCS_GUEST_TR_LIMIT, &(ctx->tr_limit));
    error |= vmcs_read(VMCS_GUEST_TR_BASE, &(ctx->tr_base));
    error |= vmcs_read(VMCS_GUEST_TR_ACCESS, &(ctx->tr_arbytes.bytes));

    error |= vmcs_read(VMCS_GUEST_LDTR_SELECTOR, &(ctx->ldtr_sel));
    error |= vmcs_read(VMCS_GUEST_LDTR_LIMIT, &(ctx->ldtr_limit));
    error |= vmcs_read(VMCS_GUEST_LDTR_BASE, &(ctx->ldtr_base));
    error |= vmcs_read(VMCS_GUEST_LDTR_ACCESS, &(ctx->ldtr_arbytes.bytes));

    return error;
}

int vmx_restore_world_ctx(struct guest_info * info, struct vmx_assist_context * ctx) {
    int error = 0;

    PrintDebug("ctx rip: %p\n", (void *)(addr_t)ctx->eip);

    error |= vmcs_write(VMCS_GUEST_RIP, ctx->eip);
    error |= vmcs_write(VMCS_GUEST_RSP, ctx->esp);
    error |= vmcs_write(VMCS_GUEST_RFLAGS, ctx->eflags);

    error |= vmcs_write(VMCS_CR0_READ_SHDW, ctx->cr0);
    info->shdw_pg_state.guest_cr3 = ctx->cr3;
    error |= vmcs_write(VMCS_CR4_READ_SHDW, ctx->cr4);

    error |= vmcs_write(VMCS_GUEST_IDTR_LIMIT, ctx->idtr_limit);
    error |= vmcs_write(VMCS_GUEST_IDTR_BASE, ctx->idtr_base);

    error |= vmcs_write(VMCS_GUEST_GDTR_LIMIT, ctx->gdtr_limit);
    error |= vmcs_write(VMCS_GUEST_GDTR_BASE, ctx->gdtr_base);

    error |= vmcs_write(VMCS_GUEST_CS_SELECTOR, ctx->cs_sel);
    error |= vmcs_write(VMCS_GUEST_CS_LIMIT, ctx->cs_limit);
    error |= vmcs_write(VMCS_GUEST_CS_BASE, ctx->cs_base);
    error |= vmcs_write(VMCS_GUEST_CS_ACCESS, ctx->cs_arbytes.bytes);

    error |= vmcs_write(VMCS_GUEST_DS_SELECTOR, ctx->ds_sel);
    error |= vmcs_write(VMCS_GUEST_DS_LIMIT, ctx->ds_limit);
    error |= vmcs_write(VMCS_GUEST_DS_BASE, ctx->ds_base);
    error |= vmcs_write(VMCS_GUEST_DS_ACCESS, ctx->ds_arbytes.bytes);

    error |= vmcs_write(VMCS_GUEST_ES_SELECTOR, ctx->es_sel);
    error |= vmcs_write(VMCS_GUEST_ES_LIMIT, ctx->es_limit);
    error |= vmcs_write(VMCS_GUEST_ES_BASE, ctx->es_base);
    error |= vmcs_write(VMCS_GUEST_ES_ACCESS, ctx->es_arbytes.bytes);

    error |= vmcs_write(VMCS_GUEST_SS_SELECTOR, ctx->ss_sel);
    error |= vmcs_write(VMCS_GUEST_SS_LIMIT, ctx->ss_limit);
    error |= vmcs_write(VMCS_GUEST_SS_BASE, ctx->ss_base);
    error |= vmcs_write(VMCS_GUEST_SS_ACCESS, ctx->ss_arbytes.bytes);

    error |= vmcs_write(VMCS_GUEST_FS_SELECTOR, ctx->fs_sel);
    error |= vmcs_write(VMCS_GUEST_FS_LIMIT, ctx->fs_limit);
    error |= vmcs_write(VMCS_GUEST_FS_BASE, ctx->fs_base);
    error |= vmcs_write(VMCS_GUEST_FS_ACCESS, ctx->fs_arbytes.bytes);

    error |= vmcs_write(VMCS_GUEST_GS_SELECTOR, ctx->gs_sel);
    error |= vmcs_write(VMCS_GUEST_GS_LIMIT, ctx->gs_limit);
    error |= vmcs_write(VMCS_GUEST_GS_BASE, ctx->gs_base);
    error |= vmcs_write(VMCS_GUEST_GS_ACCESS, ctx->gs_arbytes.bytes);

    error |= vmcs_write(VMCS_GUEST_TR_SELECTOR, ctx->tr_sel);
    error |= vmcs_write(VMCS_GUEST_TR_LIMIT, ctx->tr_limit);
    error |= vmcs_write(VMCS_GUEST_TR_BASE, ctx->tr_base);
    error |= vmcs_write(VMCS_GUEST_TR_ACCESS, ctx->tr_arbytes.bytes);

    error |= vmcs_write(VMCS_GUEST_LDTR_SELECTOR, ctx->ldtr_sel);
    error |= vmcs_write(VMCS_GUEST_LDTR_LIMIT, ctx->ldtr_limit);
    error |= vmcs_write(VMCS_GUEST_LDTR_BASE, ctx->ldtr_base);
    error |= vmcs_write(VMCS_GUEST_LDTR_ACCESS, ctx->ldtr_arbytes.bytes);

    return error;
}


