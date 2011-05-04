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
#include <palacios/vmm_ctrl_regs.h>

#ifndef V3_CONFIG_DEBUG_VMX
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif




#define VMXASSIST_MAGIC        0x17101966


struct vmx_assist_header {
    uint64_t rsvd; // 8 bytes of nothing
    uint32_t magic;
    uint32_t new_ctx_gpa;
    uint32_t old_ctx_gpa;
} __attribute__((packed));


union vmcs_arbytes {
    struct arbyte_fields {
        unsigned int seg_type : 4,
            s         : 1,
            dpl       : 2,
            p         : 1,
            reserved0 : 4,
            avl       : 1,
            reserved1 : 1,
            default_ops_size: 1,
            g         : 1,
            null_bit  : 1,
            reserved2 : 15;
    } __attribute__((packed)) fields;
    unsigned int bytes;
} __attribute__((packed));

struct vmx_assist_segment {
    uint32_t sel;
    uint32_t limit;
    uint32_t base;
    union vmcs_arbytes arbytes;
} __attribute__((packed));


/*
 * World switch state
 */
struct vmx_assist_context {
    uint32_t  eip;        /* execution pointer */
    uint32_t  esp;        /* stack pointer */
    uint32_t  eflags;     /* flags register */
    uint32_t  cr0;
    uint32_t  cr3;        /* page table directory */
    uint32_t  cr4;

    uint32_t  idtr_limit; /* idt */
    uint32_t  idtr_base;

    uint32_t  gdtr_limit; /* gdt */
    uint32_t  gdtr_base;

    struct vmx_assist_segment cs;
    struct vmx_assist_segment ds;
    struct vmx_assist_segment es;
    struct vmx_assist_segment ss;
    struct vmx_assist_segment fs;
    struct vmx_assist_segment gs;
    struct vmx_assist_segment tr;
    struct vmx_assist_segment ldtr;


    unsigned char rm_irqbase[2];
} __attribute__((packed));



static void vmx_save_world_ctx(struct guest_info * info, struct vmx_assist_context * ctx);
static void vmx_restore_world_ctx(struct guest_info * info, struct vmx_assist_context * ctx);

int v3_vmxassist_ctx_switch(struct guest_info * info) {
    struct vmx_assist_context * old_ctx = NULL;
    struct vmx_assist_context * new_ctx = NULL;
    struct vmx_assist_header * hdr = NULL;
    struct vmx_data * vmx_info = (struct vmx_data *)info->vmm_data;
 


    if (v3_gpa_to_hva(info, VMXASSIST_START, (addr_t *)&hdr) == -1) {
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


int v3_vmxassist_init(struct guest_info * core, struct vmx_data * vmx_state) {

    core->rip = 0xd0000;
    core->vm_regs.rsp = 0x80000;
    ((struct rflags *)&(core->ctrl_regs.rflags))->rsvd1 = 1;

#define GUEST_CR0 0x80010031
#define GUEST_CR4 0x00002010
    core->ctrl_regs.cr0 = GUEST_CR0;
    core->ctrl_regs.cr4 = GUEST_CR4;

    ((struct cr0_32 *)&(core->shdw_pg_state.guest_cr0))->pe = 1;
    ((struct cr0_32 *)&(core->shdw_pg_state.guest_cr0))->wp = 1;
   

    // Setup segment registers
    {
	struct v3_segment * seg_reg = (struct v3_segment *)&(core->segments);

	int i;

	for (i = 0; i < 10; i++) {
	    seg_reg[i].selector = 3 << 3;
	    seg_reg[i].limit = 0xffff;
	    seg_reg[i].base = 0x0;
	}

	core->segments.cs.selector = 2 << 3;

	/* Set only the segment registers */
	for (i = 0; i < 6; i++) {
	    seg_reg[i].limit = 0xfffff;
	    seg_reg[i].granularity = 1;
	    seg_reg[i].type = 3;
	    seg_reg[i].system = 1;
	    seg_reg[i].dpl = 0;
	    seg_reg[i].present = 1;
	    seg_reg[i].db = 1;
	}

	core->segments.cs.type = 0xb;

	core->segments.ldtr.selector = 0x20;
	core->segments.ldtr.type = 2;
	core->segments.ldtr.system = 0;
	core->segments.ldtr.present = 1;
	core->segments.ldtr.granularity = 0;

    
	/************* Map in GDT and vmxassist *************/

	uint64_t  gdt[] __attribute__ ((aligned(32))) = {
	    0x0000000000000000ULL,		/* 0x00: reserved */
	    0x0000830000000000ULL,		/* 0x08: 32-bit TSS */
	    //0x0000890000000000ULL,		/* 0x08: 32-bit TSS */
	    0x00CF9b000000FFFFULL,		/* 0x10: CS 32-bit */
	    0x00CF93000000FFFFULL,		/* 0x18: DS 32-bit */
	    0x000082000000FFFFULL,		/* 0x20: LDTR 32-bit */
	};


	addr_t vmxassist_gdt = 0;

	if (v3_gpa_to_hva(core, VMXASSIST_GDT, &vmxassist_gdt) == -1) {
	    PrintError("Could not find VMXASSIST GDT destination\n");
	    return -1;
	}

	memcpy((void *)vmxassist_gdt, gdt, sizeof(uint64_t) * 5);
        
	core->segments.gdtr.base = VMXASSIST_GDT;


	uint64_t vmxassist_tss = VMXASSIST_TSS;
	gdt[0x08 / sizeof(gdt[0])] |=
	    ((vmxassist_tss & 0xFF000000) << (56 - 24)) |
	    ((vmxassist_tss & 0x00FF0000) << (32 - 16)) |
	    ((vmxassist_tss & 0x0000FFFF) << (16)) |
	    (8392 - 1);

	core->segments.tr.selector = 0x08;
	core->segments.tr.base = vmxassist_tss;

	//core->segments.tr.type = 0x9; 
	core->segments.tr.type = 0x3;
	core->segments.tr.system = 0;
	core->segments.tr.present = 1;
	core->segments.tr.granularity = 0;
    }
 
    if (core->shdw_pg_mode == NESTED_PAGING) {
	// setup 1to1 page table internally.
	int i = 0;
	pde32_4MB_t * pde = NULL;

	PrintError("Setting up internal VMXASSIST page tables\n");

	if (v3_gpa_to_hva(core, VMXASSIST_1to1_PT, (addr_t *)(&pde)) == -1) {
	    PrintError("Could not find VMXASSIST 1to1 PT destination\n");
	    return -1;
	}

	memset(pde, 0, PAGE_SIZE);

	for (i = 0; i < 1024; i++) {
	    pde[i].present = 1;
	    pde[i].writable = 1;
	    pde[i].user_page = 1;
	    pde[i].large_page = 1;
	    pde[i].page_base_addr = PAGE_BASE_ADDR_4MB(i * PAGE_SIZE_4MB);

	    //	    PrintError("PDE %d: %x\n", i, *(uint32_t *)&(pde[i]));
	}

	core->ctrl_regs.cr3 = VMXASSIST_1to1_PT;

    }

    // setup VMXASSIST
    { 

	extern uint8_t v3_vmxassist_start[];
	extern uint8_t v3_vmxassist_end[];
	addr_t vmxassist_dst = 0;

	if (v3_gpa_to_hva(core, VMXASSIST_START, &vmxassist_dst) == -1) {
	    PrintError("Could not find VMXASSIST destination\n");
	    return -1;
	}

	memcpy((void *)vmxassist_dst, v3_vmxassist_start, v3_vmxassist_end - v3_vmxassist_start);


	vmx_state->assist_state = VMXASSIST_DISABLED;
    }


    return 0;
}
