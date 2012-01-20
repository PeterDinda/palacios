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

#include <palacios/vmcs.h>
#include <palacios/vmx_lowlevel.h>
#include <palacios/vmm.h>
#include <palacios/vmx.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vmm_lowlevel.h>





static int inline check_vmcs_write(vmcs_field_t field, addr_t val) {
    int ret = 0;
    ret = vmcs_write(field, val);

    if (ret != VMX_SUCCESS) {
        PrintError("VMWRITE error on %s!: %d\n", v3_vmcs_field_to_str(field), ret);
        return 1;
    }

    return 0;
}

static int inline check_vmcs_read(vmcs_field_t field, void * val) {
    int ret = 0;
    ret = vmcs_read(field, val);

    if (ret != VMX_SUCCESS) {
        PrintError("VMREAD error on %s!: %d\n", v3_vmcs_field_to_str(field), ret);
    }

    return ret;
}







typedef enum { ES = 0, 
	       CS = 2,
 	       SS = 4,
	       DS = 6, 
	       FS = 8, 
	       GS = 10, 
	       LDTR = 12, 
	       TR = 14, 
	       GDTR = 16, 
	       IDTR = 18} vmcs_seg_offsets_t;

typedef enum {BASE = VMCS_GUEST_ES_BASE,
	      LIMIT = VMCS_GUEST_ES_LIMIT, 
	      ACCESS = VMCS_GUEST_ES_ACCESS, 
	      SELECTOR = VMCS_GUEST_ES_SELECTOR } vmcs_seg_bases_t;
 


static int v3_read_vmcs_segment(struct v3_segment * seg, vmcs_seg_offsets_t seg_type) {
    vmcs_field_t selector = VMCS_GUEST_ES_SELECTOR + seg_type;
    vmcs_field_t base = VMCS_GUEST_ES_BASE + seg_type;
    vmcs_field_t limit = VMCS_GUEST_ES_LIMIT + seg_type;
    vmcs_field_t access = VMCS_GUEST_ES_ACCESS + seg_type;
    struct vmcs_segment vmcs_seg;

    memset(&vmcs_seg, 0, sizeof(struct vmcs_segment));

    check_vmcs_read(limit, &(vmcs_seg.limit));
    check_vmcs_read(base, &(vmcs_seg.base));

    if ((seg_type != GDTR) && (seg_type != IDTR)) {
	check_vmcs_read(selector, &(vmcs_seg.selector));
	check_vmcs_read(access, &(vmcs_seg.access.val)); 
    }

    v3_vmxseg_to_seg(&vmcs_seg, seg);

    return 0;
}

static int v3_write_vmcs_segment(struct v3_segment * seg, vmcs_seg_offsets_t seg_type) {
    vmcs_field_t selector = VMCS_GUEST_ES_SELECTOR + seg_type;
    vmcs_field_t base = VMCS_GUEST_ES_BASE + seg_type;
    vmcs_field_t limit = VMCS_GUEST_ES_LIMIT + seg_type;
    vmcs_field_t access = VMCS_GUEST_ES_ACCESS + seg_type;
    struct vmcs_segment vmcs_seg;

    v3_seg_to_vmxseg(seg, &vmcs_seg);

    check_vmcs_write(limit, vmcs_seg.limit);
    check_vmcs_write(base, vmcs_seg.base);

    if ((seg_type != GDTR) && (seg_type != IDTR)) {
	check_vmcs_write(access, vmcs_seg.access.val); 
	check_vmcs_write(selector, vmcs_seg.selector);
    }

    return 0;
}

int v3_read_vmcs_segments(struct v3_segments * segs) {
    v3_read_vmcs_segment(&(segs->cs), CS);
    v3_read_vmcs_segment(&(segs->ds), DS);
    v3_read_vmcs_segment(&(segs->es), ES);
    v3_read_vmcs_segment(&(segs->fs), FS);
    v3_read_vmcs_segment(&(segs->gs), GS);
    v3_read_vmcs_segment(&(segs->ss), SS);
    v3_read_vmcs_segment(&(segs->ldtr), LDTR);
    v3_read_vmcs_segment(&(segs->gdtr), GDTR);
    v3_read_vmcs_segment(&(segs->idtr), IDTR);
    v3_read_vmcs_segment(&(segs->tr), TR);

    return 0;
}

int v3_write_vmcs_segments(struct v3_segments * segs) {
    v3_write_vmcs_segment(&(segs->cs), CS);
    v3_write_vmcs_segment(&(segs->ds), DS);
    v3_write_vmcs_segment(&(segs->es), ES);
    v3_write_vmcs_segment(&(segs->fs), FS);
    v3_write_vmcs_segment(&(segs->gs), GS);
    v3_write_vmcs_segment(&(segs->ss), SS);
    v3_write_vmcs_segment(&(segs->ldtr), LDTR);
    v3_write_vmcs_segment(&(segs->gdtr), GDTR);
    v3_write_vmcs_segment(&(segs->idtr), IDTR);
    v3_write_vmcs_segment(&(segs->tr), TR);

    return 0;
}


void v3_vmxseg_to_seg(struct vmcs_segment * vmcs_seg, struct v3_segment * seg) {
    memset(seg, 0, sizeof(struct v3_segment));

    seg->selector = vmcs_seg->selector;
    seg->limit = vmcs_seg->limit;
    seg->base = vmcs_seg->base;

    seg->type = vmcs_seg->access.type;
    seg->system = vmcs_seg->access.desc_type;
    seg->dpl = vmcs_seg->access.dpl;
    seg->present = vmcs_seg->access.present;
    seg->avail = vmcs_seg->access.avail;
    seg->long_mode = vmcs_seg->access.long_mode;
    seg->db = vmcs_seg->access.db;
    seg->granularity = vmcs_seg->access.granularity;
    seg->unusable = vmcs_seg->access.unusable;

}

void v3_seg_to_vmxseg(struct v3_segment * seg, struct vmcs_segment * vmcs_seg) {
    memset(vmcs_seg, 0, sizeof(struct vmcs_segment));

    vmcs_seg->selector = seg->selector;
    vmcs_seg->limit = seg->limit;
    vmcs_seg->base = seg->base;

    vmcs_seg->access.type = seg->type;
    vmcs_seg->access.desc_type = seg->system;
    vmcs_seg->access.dpl = seg->dpl;
    vmcs_seg->access.present = seg->present;
    vmcs_seg->access.avail = seg->avail;
    vmcs_seg->access.long_mode = seg->long_mode;
    vmcs_seg->access.db = seg->db;
    vmcs_seg->access.granularity = seg->granularity;
    vmcs_seg->access.unusable = seg->unusable;
}




int v3_update_vmcs_ctrl_fields(struct guest_info * info) {
    int vmx_ret = 0;
    struct vmx_data * arch_data = (struct vmx_data *)(info->vmm_data);

    vmx_ret |= check_vmcs_write(VMCS_PIN_CTRLS, arch_data->pin_ctrls.value);
    vmx_ret |= check_vmcs_write(VMCS_PROC_CTRLS, arch_data->pri_proc_ctrls.value);

    if (arch_data->pri_proc_ctrls.sec_ctrls) {
        vmx_ret |= check_vmcs_write(VMCS_SEC_PROC_CTRLS, arch_data->sec_proc_ctrls.value);
    }

    vmx_ret |= check_vmcs_write(VMCS_EXIT_CTRLS, arch_data->exit_ctrls.value);
    vmx_ret |= check_vmcs_write(VMCS_ENTRY_CTRLS, arch_data->entry_ctrls.value);
    vmx_ret |= check_vmcs_write(VMCS_EXCP_BITMAP, arch_data->excp_bmap.value);

    if (info->shdw_pg_mode == NESTED_PAGING) {
	vmx_ret |= check_vmcs_write(VMCS_EPT_PTR, info->direct_map_pt);
    }

    return vmx_ret;
}






int v3_vmx_save_vmcs(struct guest_info * info) {
    struct vmx_data * vmx_info = (struct vmx_data *)(info->vmm_data);
    int error = 0;

    check_vmcs_read(VMCS_GUEST_RIP, &(info->rip));
    check_vmcs_read(VMCS_GUEST_RSP, &(info->vm_regs.rsp));

    check_vmcs_read(VMCS_GUEST_CR0, &(info->ctrl_regs.cr0));
    check_vmcs_read(VMCS_CR0_READ_SHDW, &(info->shdw_pg_state.guest_cr0));
    check_vmcs_read(VMCS_GUEST_CR3, &(info->ctrl_regs.cr3));
    check_vmcs_read(VMCS_GUEST_CR4, &(info->ctrl_regs.cr4));
    check_vmcs_read(VMCS_CR4_READ_SHDW, &(vmx_info->guest_cr4));
    check_vmcs_read(VMCS_GUEST_DR7, &(info->dbg_regs.dr7));

    check_vmcs_read(VMCS_GUEST_RFLAGS, &(info->ctrl_regs.rflags));

#ifdef __V3_64BIT__
    check_vmcs_read(VMCS_GUEST_EFER, &(info->ctrl_regs.efer));
#endif
    
    error =  v3_read_vmcs_segments(&(info->segments));

    return error;
}


int v3_vmx_restore_vmcs(struct guest_info * info) {
    struct vmx_data * vmx_info = (struct vmx_data *)(info->vmm_data);
    int error = 0;

    check_vmcs_write(VMCS_GUEST_RIP, info->rip);
    check_vmcs_write(VMCS_GUEST_RSP, info->vm_regs.rsp);

    check_vmcs_write(VMCS_GUEST_CR0, info->ctrl_regs.cr0);
    check_vmcs_write(VMCS_CR0_READ_SHDW, info->shdw_pg_state.guest_cr0);
    check_vmcs_write(VMCS_GUEST_CR3, info->ctrl_regs.cr3);
    check_vmcs_write(VMCS_GUEST_CR4, info->ctrl_regs.cr4);
    check_vmcs_write(VMCS_CR4_READ_SHDW, vmx_info->guest_cr4);
    check_vmcs_write(VMCS_GUEST_DR7, info->dbg_regs.dr7);

    check_vmcs_write(VMCS_GUEST_RFLAGS, info->ctrl_regs.rflags);

#ifdef __V3_64BIT__
    check_vmcs_write(VMCS_GUEST_EFER, info->ctrl_regs.efer);
    check_vmcs_write(VMCS_ENTRY_CTRLS, vmx_info->entry_ctrls.value);
#endif




    error = v3_write_vmcs_segments(&(info->segments));

    return error;

}



int v3_update_vmcs_host_state(struct guest_info * info) {
    int vmx_ret = 0;
    addr_t tmp;
    struct v3_msr tmp_msr;
    addr_t gdtr_base;
    struct {
        uint16_t selector;
        addr_t   base;
    } __attribute__((packed)) tmp_seg;

#ifdef __V3_64BIT__
    __asm__ __volatile__ ( "movq    %%cr0, %0; "		
			   : "=q"(tmp)
			   :
    );
#else
    __asm__ __volatile__ ( "movl    %%cr0, %0; "		
			   : "=q"(tmp)
			   :
    );
#endif    
    vmx_ret |= check_vmcs_write(VMCS_HOST_CR0, tmp);


#ifdef __V3_64BIT__
    __asm__ __volatile__ ( "movq %%cr3, %0; "		
			   : "=q"(tmp)
			   :
    );
#else
    __asm__ __volatile__ ( "movl %%cr3, %0; "		
			   : "=q"(tmp)
			   :
    );
#endif
    vmx_ret |= check_vmcs_write(VMCS_HOST_CR3, tmp);


#ifdef __V3_64BIT__
    __asm__ __volatile__ ( "movq %%cr4, %0; "		
			   : "=q"(tmp)
			   :
    );
#else
    __asm__ __volatile__ ( "movl %%cr4, %0; "		
			   : "=q"(tmp)
			   :
    );
#endif
    vmx_ret |= check_vmcs_write(VMCS_HOST_CR4, tmp);


    __asm__ __volatile__(
			 "sgdt (%0);"
			 :
			 : "q"(&tmp_seg)
			 : "memory"
			 );
    gdtr_base = tmp_seg.base;
    vmx_ret |= check_vmcs_write(VMCS_HOST_GDTR_BASE, tmp_seg.base);

    __asm__ __volatile__(
			 "sidt (%0);"
			 :
			 : "q"(&tmp_seg)
			 : "memory"
			 );
    vmx_ret |= check_vmcs_write(VMCS_HOST_IDTR_BASE, tmp_seg.base);

    __asm__ __volatile__(
			 "str (%0);"
			 :
			 : "q"(&tmp_seg)
			 : "memory"
			 );
    vmx_ret |= check_vmcs_write(VMCS_HOST_TR_SELECTOR, tmp_seg.selector);

    /* The GDTR *index* is bits 3-15 of the selector. */
    {
	struct tss_descriptor * desc = NULL;
	desc = (struct tss_descriptor *)(gdtr_base + (8 * (tmp_seg.selector >> 3)));

	tmp_seg.base = ((desc->base1) |
			(desc->base2 << 16) |
			(desc->base3 << 24) |
#ifdef __V3_64BIT__
			((uint64_t)desc->base4 << 32)
#else
			(0)
#endif
			);

	vmx_ret |= check_vmcs_write(VMCS_HOST_TR_BASE, tmp_seg.base);
    }


#ifdef __V3_64BIT__
    __asm__ __volatile__ ( "movq %%cs, %0; "		
			   : "=q"(tmp)
			   :
    );
#else
    __asm__ __volatile__ ( "movl %%cs, %0; "		
			   : "=q"(tmp)
			   :
    );
#endif
    vmx_ret |= check_vmcs_write(VMCS_HOST_CS_SELECTOR, tmp);

#ifdef __V3_64BIT__
    __asm__ __volatile__ ( "movq %%ss, %0; "		
			   : "=q"(tmp)
			   :
    );
#else
    __asm__ __volatile__ ( "movl %%ss, %0; "		
			   : "=q"(tmp)
			   :
    );
#endif
    vmx_ret |= check_vmcs_write(VMCS_HOST_SS_SELECTOR, tmp);

#ifdef __V3_64BIT__
    __asm__ __volatile__ ( "movq %%ds, %0; "		
			   : "=q"(tmp)
			   :
    );
#else
    __asm__ __volatile__ ( "movl %%ds, %0; "		
			   : "=q"(tmp)
			   :
    );
#endif
    vmx_ret |= check_vmcs_write(VMCS_HOST_DS_SELECTOR, tmp);

#ifdef __V3_64BIT__
    __asm__ __volatile__ ( "movq %%es, %0; "		
			   : "=q"(tmp)
			   :
    );
#else
    __asm__ __volatile__ ( "movl %%es, %0; "		
			   : "=q"(tmp)
			   :
    );
#endif
    vmx_ret |= check_vmcs_write(VMCS_HOST_ES_SELECTOR, tmp);

#ifdef __V3_64BIT__
    __asm__ __volatile__ ( "movq %%fs, %0; "		
			   : "=q"(tmp)
			   :
    );
#else
    __asm__ __volatile__ ( "movl %%fs, %0; "		
			   : "=q"(tmp)
			   :
    );
#endif
    vmx_ret |= check_vmcs_write(VMCS_HOST_FS_SELECTOR, tmp);

#ifdef __V3_64BIT__
    __asm__ __volatile__ ( "movq %%gs, %0; "		
			   : "=q"(tmp)
			   :
    );
#else
    __asm__ __volatile__ ( "movl %%gs, %0; "		
			   : "=q"(tmp)
			   :
    );
#endif
    vmx_ret |= check_vmcs_write(VMCS_HOST_GS_SELECTOR, tmp);


#define SYSENTER_CS_MSR 0x00000174
#define SYSENTER_ESP_MSR 0x00000175
#define SYSENTER_EIP_MSR 0x00000176
#define FS_BASE_MSR 0xc0000100
#define GS_BASE_MSR 0xc0000101
#define EFER_MSR 0xc0000080


    // SYSENTER CS MSR
    v3_get_msr(SYSENTER_CS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_HOST_SYSENTER_CS, tmp_msr.lo);

    // SYSENTER_ESP MSR
    v3_get_msr(SYSENTER_ESP_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_HOST_SYSENTER_ESP, tmp_msr.value);

    // SYSENTER_EIP MSR
    v3_get_msr(SYSENTER_EIP_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_HOST_SYSENTER_EIP, tmp_msr.value);


    // FS.BASE MSR
    v3_get_msr(FS_BASE_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_HOST_FS_BASE, tmp_msr.value);    

    // GS.BASE MSR
    v3_get_msr(GS_BASE_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_HOST_GS_BASE, tmp_msr.value);    


    // EFER
    v3_get_msr(EFER_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_HOST_EFER, tmp_msr.value);

    // PERF GLOBAL CONTROL

    // PAT


    // save STAR, LSTAR, FMASK, KERNEL_GS_BASE MSRs in MSR load/store area
    {
	struct vmx_data * vmx_state = (struct vmx_data *)info->vmm_data;
	struct vmcs_msr_save_area * msr_entries = vmx_state->msr_area;

    
	v3_get_msr(IA32_STAR_MSR, &(msr_entries->host_star.hi), &(msr_entries->host_star.lo));
	v3_get_msr(IA32_LSTAR_MSR, &(msr_entries->host_lstar.hi), &(msr_entries->host_lstar.lo));
	v3_get_msr(IA32_FMASK_MSR, &(msr_entries->host_fmask.hi), &(msr_entries->host_fmask.lo));
	v3_get_msr(IA32_KERN_GS_BASE_MSR, &(msr_entries->host_kern_gs.hi), &(msr_entries->host_kern_gs.lo));
    }

    



    return vmx_ret;
}






static inline void print_vmcs_field(vmcs_field_t vmcs_index) {
    int len = v3_vmcs_get_field_len(vmcs_index);
    addr_t val;
    
    if (vmcs_read(vmcs_index, &val) != VMX_SUCCESS) {
	PrintError("VMCS_READ error for %s\n", v3_vmcs_field_to_str(vmcs_index));
	return;
    };
    
    if (len == 2) {
	PrintDebug("\t%s: 0x%.4x\n", v3_vmcs_field_to_str(vmcs_index), (uint16_t)val);
    } else if (len == 4) {
	PrintDebug("\t%s: 0x%.8x\n", v3_vmcs_field_to_str(vmcs_index), (uint32_t)val);
    } else if (len == 8) {
	PrintDebug("\t%s: 0x%p\n", v3_vmcs_field_to_str(vmcs_index), (void *)(addr_t)val);
    }
}


static void print_vmcs_segments() {
    struct v3_segments segs; 

    v3_read_vmcs_segments(&segs);
    v3_print_segments(&segs);


    PrintDebug("   ==> CS\n");
    print_vmcs_field(VMCS_GUEST_CS_SELECTOR);
    print_vmcs_field(VMCS_GUEST_CS_BASE);
    print_vmcs_field(VMCS_GUEST_CS_LIMIT);
    print_vmcs_field(VMCS_GUEST_CS_ACCESS);

    PrintDebug("   ==> SS\n");
    print_vmcs_field(VMCS_GUEST_SS_SELECTOR);
    print_vmcs_field(VMCS_GUEST_SS_BASE);
    print_vmcs_field(VMCS_GUEST_SS_LIMIT);
    print_vmcs_field(VMCS_GUEST_SS_ACCESS);

    PrintDebug("   ==> DS\n");
    print_vmcs_field(VMCS_GUEST_DS_SELECTOR);
    print_vmcs_field(VMCS_GUEST_DS_BASE);
    print_vmcs_field(VMCS_GUEST_DS_LIMIT);
    print_vmcs_field(VMCS_GUEST_DS_ACCESS);

    PrintDebug("   ==> ES\n");
    print_vmcs_field(VMCS_GUEST_ES_SELECTOR);
    print_vmcs_field(VMCS_GUEST_ES_BASE);
    print_vmcs_field(VMCS_GUEST_ES_LIMIT);
    print_vmcs_field(VMCS_GUEST_ES_ACCESS);

    PrintDebug("   ==> FS\n");
    print_vmcs_field(VMCS_GUEST_FS_SELECTOR);
    print_vmcs_field(VMCS_GUEST_FS_BASE);
    print_vmcs_field(VMCS_GUEST_FS_LIMIT);
    print_vmcs_field(VMCS_GUEST_FS_ACCESS);

    PrintDebug("   ==> GS\n");
    print_vmcs_field(VMCS_GUEST_GS_SELECTOR);
    print_vmcs_field(VMCS_GUEST_GS_BASE);
    print_vmcs_field(VMCS_GUEST_GS_LIMIT);
    print_vmcs_field(VMCS_GUEST_GS_ACCESS);

    PrintDebug("   ==> LDTR\n");
    print_vmcs_field(VMCS_GUEST_LDTR_SELECTOR);
    print_vmcs_field(VMCS_GUEST_LDTR_BASE);
    print_vmcs_field(VMCS_GUEST_LDTR_LIMIT);
    print_vmcs_field(VMCS_GUEST_LDTR_ACCESS);

    PrintDebug("   ==> TR\n");
    print_vmcs_field(VMCS_GUEST_TR_SELECTOR);
    print_vmcs_field(VMCS_GUEST_TR_BASE);
    print_vmcs_field(VMCS_GUEST_TR_LIMIT);
    print_vmcs_field(VMCS_GUEST_TR_ACCESS);

    PrintDebug("   ==> GDTR\n");
    print_vmcs_field(VMCS_GUEST_GDTR_BASE);
    print_vmcs_field(VMCS_GUEST_GDTR_LIMIT);

    PrintDebug("   ==> IDTR\n");
    print_vmcs_field(VMCS_GUEST_IDTR_BASE);
    print_vmcs_field(VMCS_GUEST_IDTR_LIMIT);


}




static void print_guest_state()
{
    PrintDebug("VMCS_GUEST_STATE\n");
    print_vmcs_field(VMCS_GUEST_RIP);
    print_vmcs_field(VMCS_GUEST_RSP);
    print_vmcs_field(VMCS_GUEST_RFLAGS);
    print_vmcs_field(VMCS_GUEST_CR0);
    print_vmcs_field(VMCS_GUEST_CR3);
    print_vmcs_field(VMCS_GUEST_CR4);
    print_vmcs_field(VMCS_GUEST_DR7);

    // if save IA32_EFER
    print_vmcs_field(VMCS_GUEST_EFER);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_GUEST_EFER_HIGH);
#endif


    PrintDebug("\n");

    print_vmcs_segments();

    PrintDebug("\n");

    print_vmcs_field(VMCS_GUEST_DBG_CTL);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_GUEST_DBG_CTL_HIGH);
#endif
    print_vmcs_field(VMCS_GUEST_SYSENTER_CS);
    print_vmcs_field(VMCS_GUEST_SYSENTER_ESP);
    print_vmcs_field(VMCS_GUEST_SYSENTER_EIP);


    // if save IA32_PAT
    print_vmcs_field(VMCS_GUEST_PAT);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_GUEST_PAT_HIGH);
#endif

    //if load  IA32_PERF_GLOBAL_CTRL
    print_vmcs_field(VMCS_GUEST_PERF_GLOBAL_CTRL);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH);
#endif

    print_vmcs_field(VMCS_GUEST_SMBASE);




    PrintDebug("GUEST_NON_REGISTER_STATE\n");

    print_vmcs_field(VMCS_GUEST_ACTIVITY_STATE);
    print_vmcs_field(VMCS_GUEST_INT_STATE);
    print_vmcs_field(VMCS_GUEST_PENDING_DBG_EXCP);

    // if VMX preempt timer
    print_vmcs_field(VMCS_PREEMPT_TIMER);

}
       
static void print_host_state()
{
    PrintDebug("VMCS_HOST_STATE\n");

    print_vmcs_field(VMCS_HOST_RIP);
    print_vmcs_field(VMCS_HOST_RSP);
    print_vmcs_field(VMCS_HOST_CR0);
    print_vmcs_field(VMCS_HOST_CR3);
    print_vmcs_field(VMCS_HOST_CR4);
    


    // if load IA32_EFER
    print_vmcs_field(VMCS_HOST_EFER);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_HOST_EFER_HIGH);
#endif


    PrintDebug("\n");
    print_vmcs_field(VMCS_HOST_CS_SELECTOR);
    print_vmcs_field(VMCS_HOST_SS_SELECTOR);
    print_vmcs_field(VMCS_HOST_DS_SELECTOR);
    print_vmcs_field(VMCS_HOST_ES_SELECTOR);
    print_vmcs_field(VMCS_HOST_FS_SELECTOR);
    print_vmcs_field(VMCS_HOST_GS_SELECTOR);
    print_vmcs_field(VMCS_HOST_TR_SELECTOR);

    PrintDebug("\n");
    print_vmcs_field(VMCS_HOST_FS_BASE);
    print_vmcs_field(VMCS_HOST_GS_BASE);
    print_vmcs_field(VMCS_HOST_TR_BASE);
    print_vmcs_field(VMCS_HOST_GDTR_BASE);
    print_vmcs_field(VMCS_HOST_IDTR_BASE);

    PrintDebug("\n");
    print_vmcs_field(VMCS_HOST_SYSENTER_CS);
    print_vmcs_field(VMCS_HOST_SYSENTER_ESP);
    print_vmcs_field(VMCS_HOST_SYSENTER_EIP);


    // if load IA32_PAT
    print_vmcs_field(VMCS_HOST_PAT);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_HOST_PAT_HIGH);
#endif

    // if load IA32_PERF_GLOBAL_CTRL
    print_vmcs_field(VMCS_HOST_PERF_GLOBAL_CTRL);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_HOST_PERF_GLOBAL_CTRL_HIGH);
#endif
}


static void print_exec_ctrls() {
    PrintDebug("VMCS_EXEC_CTRL_FIELDS\n");
    print_vmcs_field(VMCS_PIN_CTRLS);
    print_vmcs_field(VMCS_PROC_CTRLS);
    
    // if activate secondary controls
    print_vmcs_field(VMCS_SEC_PROC_CTRLS);
    
    print_vmcs_field(VMCS_EXCP_BITMAP);
    print_vmcs_field(VMCS_PG_FAULT_ERR_MASK);
    print_vmcs_field(VMCS_PG_FAULT_ERR_MATCH);

    print_vmcs_field(VMCS_IO_BITMAP_A_ADDR);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_IO_BITMAP_A_ADDR_HIGH);
#endif

    print_vmcs_field(VMCS_IO_BITMAP_B_ADDR);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_IO_BITMAP_B_ADDR_HIGH);
#endif

    print_vmcs_field(VMCS_TSC_OFFSET);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_TSC_OFFSET_HIGH);
#endif

    PrintDebug("\n");

    print_vmcs_field(VMCS_CR0_MASK);
    print_vmcs_field(VMCS_CR0_READ_SHDW);
    print_vmcs_field(VMCS_CR4_MASK);
    print_vmcs_field(VMCS_CR4_READ_SHDW);

    print_vmcs_field(VMCS_CR3_TGT_CNT);
    print_vmcs_field(VMCS_CR3_TGT_VAL_0);
    print_vmcs_field(VMCS_CR3_TGT_VAL_1);
    print_vmcs_field(VMCS_CR3_TGT_VAL_2);
    print_vmcs_field(VMCS_CR3_TGT_VAL_3);

    // Check max number of CR3 targets... may continue...


    PrintDebug("\n");

    // if virtualize apic accesses
    print_vmcs_field(VMCS_APIC_ACCESS_ADDR);    
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_APIC_ACCESS_ADDR_HIGH);
#endif

    // if use tpr shadow
    print_vmcs_field(VMCS_VAPIC_ADDR);    
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_VAPIC_ADDR_HIGH);
#endif

    // if use tpr shadow
    print_vmcs_field(VMCS_TPR_THRESHOLD);


    // if use MSR bitmaps
    print_vmcs_field(VMCS_MSR_BITMAP);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_MSR_BITMAP_HIGH);
#endif

    print_vmcs_field(VMCS_EXEC_PTR);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_EXEC_PTR_HIGH);
#endif


}

static void print_ept_state() {
    V3_Print("VMCS EPT INFO\n");

    // if enable vpid
    print_vmcs_field(VMCS_VPID);

    print_vmcs_field(VMCS_EPT_PTR);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_EPT_PTR_HIGH);
#endif

    print_vmcs_field(VMCS_GUEST_PHYS_ADDR);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_GUEST_PHYS_ADDR_HIGH);
#endif



    print_vmcs_field(VMCS_GUEST_PDPTE0);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_GUEST_PDPTE0_HIGH);
#endif

    print_vmcs_field(VMCS_GUEST_PDPTE1);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_GUEST_PDPTE1_HIGH);
#endif

    print_vmcs_field(VMCS_GUEST_PDPTE2);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_GUEST_PDPTE2_HIGH);
#endif

    print_vmcs_field(VMCS_GUEST_PDPTE3);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_GUEST_PDPTE3_HIGH);
#endif



}


static void print_exit_ctrls() {
    PrintDebug("VMCS_EXIT_CTRLS\n");

    print_vmcs_field(VMCS_EXIT_CTRLS);


    print_vmcs_field(VMCS_EXIT_MSR_STORE_CNT);
    print_vmcs_field(VMCS_EXIT_MSR_STORE_ADDR);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_EXIT_MSR_STORE_ADDR_HIGH);
#endif

    print_vmcs_field(VMCS_EXIT_MSR_LOAD_CNT);
    print_vmcs_field(VMCS_EXIT_MSR_LOAD_ADDR);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_EXIT_MSR_LOAD_ADDR_HIGH);
#endif


    // if pause loop exiting
    print_vmcs_field(VMCS_PLE_GAP);
    print_vmcs_field(VMCS_PLE_WINDOW);

}


static void print_entry_ctrls() {
    PrintDebug("VMCS_ENTRY_CTRLS\n");
    
    print_vmcs_field(VMCS_ENTRY_CTRLS);

    print_vmcs_field(VMCS_ENTRY_MSR_LOAD_CNT);
    print_vmcs_field(VMCS_ENTRY_MSR_LOAD_ADDR);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_ENTRY_MSR_LOAD_ADDR_HIGH);
#endif

    print_vmcs_field(VMCS_ENTRY_INT_INFO);
    print_vmcs_field(VMCS_ENTRY_EXCP_ERR);
    print_vmcs_field(VMCS_ENTRY_INSTR_LEN);


}


static void print_exit_info() {
    PrintDebug("VMCS_EXIT_INFO\n");

    print_vmcs_field(VMCS_EXIT_REASON);
    print_vmcs_field(VMCS_EXIT_QUAL);

    print_vmcs_field(VMCS_EXIT_INT_INFO);
    print_vmcs_field(VMCS_EXIT_INT_ERR);

    print_vmcs_field(VMCS_IDT_VECTOR_INFO);
    print_vmcs_field(VMCS_IDT_VECTOR_ERR);

    print_vmcs_field(VMCS_EXIT_INSTR_LEN);

    print_vmcs_field(VMCS_GUEST_LINEAR_ADDR);
    print_vmcs_field(VMCS_EXIT_INSTR_INFO);

    print_vmcs_field(VMCS_IO_RCX);
    print_vmcs_field(VMCS_IO_RSI);
    print_vmcs_field(VMCS_IO_RDI);
    print_vmcs_field(VMCS_IO_RIP);


    print_vmcs_field(VMCS_INSTR_ERR);
}

void v3_print_vmcs() {

    print_vmcs_field(VMCS_LINK_PTR);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_LINK_PTR_HIGH);
#endif

    print_guest_state();
    print_host_state();

    print_ept_state();

    print_exec_ctrls();
    print_exit_ctrls();
    print_entry_ctrls();
    print_exit_info();

}


/*
 * Returns the field length in bytes
 *   It doesn't get much uglier than this... Thanks Intel
 */
int v3_vmcs_get_field_len(vmcs_field_t field) {
    struct vmcs_field_encoding * enc = (struct vmcs_field_encoding *)&field;

    switch (enc->width)  {
	case 0:
            return 2;
	case 1: {
	    if (enc->access_type == 1) {
		return 4;
	    } else {
		return sizeof(addr_t);
	    }
	}
	case 2:
            return 4;
	case 3:
            return sizeof(addr_t);
        default:
	    PrintError("Invalid VMCS field: 0x%x\n", field);
            return -1;
    }
}











static const char VMCS_VPID_STR[] = "VPID";
static const char VMCS_GUEST_ES_SELECTOR_STR[] = "GUEST_ES_SELECTOR";
static const char VMCS_GUEST_CS_SELECTOR_STR[] = "GUEST_CS_SELECTOR";
static const char VMCS_GUEST_SS_SELECTOR_STR[] = "GUEST_SS_SELECTOR";
static const char VMCS_GUEST_DS_SELECTOR_STR[] = "GUEST_DS_SELECTOR";
static const char VMCS_GUEST_FS_SELECTOR_STR[] = "GUEST_FS_SELECTOR";
static const char VMCS_GUEST_GS_SELECTOR_STR[] = "GUEST_GS_SELECTOR";
static const char VMCS_GUEST_LDTR_SELECTOR_STR[] = "GUEST_LDTR_SELECTOR";
static const char VMCS_GUEST_TR_SELECTOR_STR[] = "GUEST_TR_SELECTOR";
static const char VMCS_HOST_ES_SELECTOR_STR[] = "HOST_ES_SELECTOR";
static const char VMCS_HOST_CS_SELECTOR_STR[] = "HOST_CS_SELECTOR";
static const char VMCS_HOST_SS_SELECTOR_STR[] = "HOST_SS_SELECTOR";
static const char VMCS_HOST_DS_SELECTOR_STR[] = "HOST_DS_SELECTOR";
static const char VMCS_HOST_FS_SELECTOR_STR[] = "HOST_FS_SELECTOR";
static const char VMCS_HOST_GS_SELECTOR_STR[] = "HOST_GS_SELECTOR";
static const char VMCS_HOST_TR_SELECTOR_STR[] = "HOST_TR_SELECTOR";
static const char VMCS_IO_BITMAP_A_ADDR_STR[] = "IO_BITMAP_A_ADDR";
static const char VMCS_IO_BITMAP_A_ADDR_HIGH_STR[] = "IO_BITMAP_A_ADDR_HIGH";
static const char VMCS_IO_BITMAP_B_ADDR_STR[] = "IO_BITMAP_B_ADDR";
static const char VMCS_IO_BITMAP_B_ADDR_HIGH_STR[] = "IO_BITMAP_B_ADDR_HIGH";
static const char VMCS_MSR_BITMAP_STR[] = "MSR_BITMAPS";
static const char VMCS_MSR_BITMAP_HIGH_STR[] = "MSR_BITMAPS_HIGH";
static const char VMCS_EXIT_MSR_STORE_ADDR_STR[] = "EXIT_MSR_STORE_ADDR";
static const char VMCS_EXIT_MSR_STORE_ADDR_HIGH_STR[] = "EXIT_MSR_STORE_ADDR_HIGH";
static const char VMCS_EXIT_MSR_LOAD_ADDR_STR[] = "EXIT_MSR_LOAD_ADDR";
static const char VMCS_EXIT_MSR_LOAD_ADDR_HIGH_STR[] = "EXIT_MSR_LOAD_ADDR_HIGH";
static const char VMCS_ENTRY_MSR_LOAD_ADDR_STR[] = "ENTRY_MSR_LOAD_ADDR";
static const char VMCS_ENTRY_MSR_LOAD_ADDR_HIGH_STR[] = "ENTRY_MSR_LOAD_ADDR_HIGH";
static const char VMCS_EXEC_PTR_STR[] = "VMCS_EXEC_PTR";
static const char VMCS_EXEC_PTR_HIGH_STR[] = "VMCS_EXEC_PTR_HIGH";
static const char VMCS_TSC_OFFSET_STR[] = "TSC_OFFSET";
static const char VMCS_TSC_OFFSET_HIGH_STR[] = "TSC_OFFSET_HIGH";
static const char VMCS_VAPIC_ADDR_STR[] = "VAPIC_PAGE_ADDR";
static const char VMCS_VAPIC_ADDR_HIGH_STR[] = "VAPIC_PAGE_ADDR_HIGH";
static const char VMCS_APIC_ACCESS_ADDR_STR[] = "APIC_ACCESS_ADDR";
static const char VMCS_APIC_ACCESS_ADDR_HIGH_STR[] = "APIC_ACCESS_ADDR_HIGH";
static const char VMCS_EPT_PTR_STR[] = "VMCS_EPT_PTR";
static const char VMCS_EPT_PTR_HIGH_STR[] = "VMCS_EPT_PTR_HIGH";
static const char VMCS_GUEST_PHYS_ADDR_STR[] = "VMCS_GUEST_PHYS_ADDR";
static const char VMCS_GUEST_PHYS_ADDR_HIGH_STR[] = "VMCS_GUEST_PHYS_ADDR_HIGH";
static const char VMCS_LINK_PTR_STR[] = "VMCS_LINK_PTR";
static const char VMCS_LINK_PTR_HIGH_STR[] = "VMCS_LINK_PTR_HIGH";
static const char VMCS_GUEST_DBG_CTL_STR[] = "GUEST_DEBUG_CTL";
static const char VMCS_GUEST_DBG_CTL_HIGH_STR[] = "GUEST_DEBUG_CTL_HIGH";
static const char VMCS_GUEST_PAT_STR[] = "GUEST_PAT";
static const char VMCS_GUEST_PAT_HIGH_STR[] = "GUEST_PAT_HIGH";
static const char VMCS_GUEST_EFER_STR[] = "GUEST_EFER";
static const char VMCS_GUEST_EFER_HIGH_STR[] = "GUEST_EFER_HIGH";
static const char VMCS_GUEST_PERF_GLOBAL_CTRL_STR[] = "GUEST_PERF_GLOBAL_CTRL";
static const char VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH_STR[] = "GUEST_PERF_GLOBAL_CTRL_HIGH";
static const char VMCS_GUEST_PDPTE0_STR[] = "GUEST_PDPTE0";
static const char VMCS_GUEST_PDPTE0_HIGH_STR[] = "GUEST_PDPTE0_HIGH";
static const char VMCS_GUEST_PDPTE1_STR[] = "GUEST_PDPTE1";
static const char VMCS_GUEST_PDPTE1_HIGH_STR[] = "GUEST_PDPTE1_HIGH";
static const char VMCS_GUEST_PDPTE2_STR[] = "GUEST_PDPTE2";
static const char VMCS_GUEST_PDPTE2_HIGH_STR[] = "GUEST_PDPTE2_HIGH";
static const char VMCS_GUEST_PDPTE3_STR[] = "GUEST_PDPTE3";
static const char VMCS_GUEST_PDPTE3_HIGH_STR[] = "GUEST_PDPTE3_HIGH";
static const char VMCS_HOST_PAT_STR[] = "HOST_PAT";
static const char VMCS_HOST_PAT_HIGH_STR[] = "HOST_PAT_HIGH";
static const char VMCS_HOST_EFER_STR[] = "VMCS_HOST_EFER";
static const char VMCS_HOST_EFER_HIGH_STR[] = "VMCS_HOST_EFER_HIGH";
static const char VMCS_HOST_PERF_GLOBAL_CTRL_STR[] = "HOST_PERF_GLOBAL_CTRL";
static const char VMCS_HOST_PERF_GLOBAL_CTRL_HIGH_STR[] = "HOST_PERF_GLOBAL_CTRL_HIGH";
static const char VMCS_PIN_CTRLS_STR[] = "PIN_VM_EXEC_CTRLS";
static const char VMCS_PROC_CTRLS_STR[] = "PROC_VM_EXEC_CTRLS";
static const char VMCS_EXCP_BITMAP_STR[] = "EXCEPTION_BITMAP";
static const char VMCS_PG_FAULT_ERR_MASK_STR[] = "PAGE_FAULT_ERROR_MASK";
static const char VMCS_PG_FAULT_ERR_MATCH_STR[] = "PAGE_FAULT_ERROR_MATCH";
static const char VMCS_CR3_TGT_CNT_STR[] = "CR3_TARGET_COUNT";
static const char VMCS_EXIT_CTRLS_STR[] = "VM_EXIT_CTRLS";
static const char VMCS_EXIT_MSR_STORE_CNT_STR[] = "VM_EXIT_MSR_STORE_COUNT";
static const char VMCS_EXIT_MSR_LOAD_CNT_STR[] = "VM_EXIT_MSR_LOAD_COUNT";
static const char VMCS_ENTRY_CTRLS_STR[] = "VM_ENTRY_CTRLS";
static const char VMCS_ENTRY_MSR_LOAD_CNT_STR[] = "VM_ENTRY_MSR_LOAD_COUNT";
static const char VMCS_ENTRY_INT_INFO_STR[] = "VM_ENTRY_INT_INFO_FIELD";
static const char VMCS_ENTRY_EXCP_ERR_STR[] = "VM_ENTRY_EXCEPTION_ERROR";
static const char VMCS_ENTRY_INSTR_LEN_STR[] = "VM_ENTRY_INSTR_LENGTH";
static const char VMCS_TPR_THRESHOLD_STR[] = "TPR_THRESHOLD";
static const char VMCS_SEC_PROC_CTRLS_STR[] = "VMCS_SEC_PROC_CTRLS";
static const char VMCS_PLE_GAP_STR[] = "PLE_GAP";
static const char VMCS_PLE_WINDOW_STR[] = "PLE_WINDOW";
static const char VMCS_INSTR_ERR_STR[] = "VM_INSTR_ERROR";
static const char VMCS_EXIT_REASON_STR[] = "EXIT_REASON";
static const char VMCS_EXIT_INT_INFO_STR[] = "VM_EXIT_INT_INFO";
static const char VMCS_EXIT_INT_ERR_STR[] = "VM_EXIT_INT_ERROR";
static const char VMCS_IDT_VECTOR_INFO_STR[] = "IDT_VECTOR_INFO";
static const char VMCS_IDT_VECTOR_ERR_STR[] = "IDT_VECTOR_ERROR";
static const char VMCS_EXIT_INSTR_LEN_STR[] = "VM_EXIT_INSTR_LENGTH";
static const char VMCS_EXIT_INSTR_INFO_STR[] = "VMX_INSTR_INFO";
static const char VMCS_GUEST_ES_LIMIT_STR[] = "GUEST_ES_LIMIT";
static const char VMCS_GUEST_CS_LIMIT_STR[] = "GUEST_CS_LIMIT";
static const char VMCS_GUEST_SS_LIMIT_STR[] = "GUEST_SS_LIMIT";
static const char VMCS_GUEST_DS_LIMIT_STR[] = "GUEST_DS_LIMIT";
static const char VMCS_GUEST_FS_LIMIT_STR[] = "GUEST_FS_LIMIT";
static const char VMCS_GUEST_GS_LIMIT_STR[] = "GUEST_GS_LIMIT";
static const char VMCS_GUEST_LDTR_LIMIT_STR[] = "GUEST_LDTR_LIMIT";
static const char VMCS_GUEST_TR_LIMIT_STR[] = "GUEST_TR_LIMIT";
static const char VMCS_GUEST_GDTR_LIMIT_STR[] = "GUEST_GDTR_LIMIT";
static const char VMCS_GUEST_IDTR_LIMIT_STR[] = "GUEST_IDTR_LIMIT";
static const char VMCS_GUEST_ES_ACCESS_STR[] = "GUEST_ES_ACCESS";
static const char VMCS_GUEST_CS_ACCESS_STR[] = "GUEST_CS_ACCESS";
static const char VMCS_GUEST_SS_ACCESS_STR[] = "GUEST_SS_ACCESS";
static const char VMCS_GUEST_DS_ACCESS_STR[] = "GUEST_DS_ACCESS";
static const char VMCS_GUEST_FS_ACCESS_STR[] = "GUEST_FS_ACCESS";
static const char VMCS_GUEST_GS_ACCESS_STR[] = "GUEST_GS_ACCESS";
static const char VMCS_GUEST_LDTR_ACCESS_STR[] = "GUEST_LDTR_ACCESS";
static const char VMCS_GUEST_TR_ACCESS_STR[] = "GUEST_TR_ACCESS";
static const char VMCS_GUEST_INT_STATE_STR[] = "GUEST_INT_STATE";
static const char VMCS_GUEST_ACTIVITY_STATE_STR[] = "GUEST_ACTIVITY_STATE";
static const char VMCS_GUEST_SMBASE_STR[] = "GUEST_SMBASE";
static const char VMCS_GUEST_SYSENTER_CS_STR[] = "GUEST_SYSENTER_CS";
static const char VMCS_PREEMPT_TIMER_STR[] = "PREEMPT_TIMER";
static const char VMCS_HOST_SYSENTER_CS_STR[] = "HOST_SYSENTER_CS";
static const char VMCS_CR0_MASK_STR[] = "CR0_GUEST_HOST_MASK";
static const char VMCS_CR4_MASK_STR[] = "CR4_GUEST_HOST_MASK";
static const char VMCS_CR0_READ_SHDW_STR[] = "CR0_READ_SHADOW";
static const char VMCS_CR4_READ_SHDW_STR[] = "CR4_READ_SHADOW";
static const char VMCS_CR3_TGT_VAL_0_STR[] = "CR3_TARGET_VALUE_0";
static const char VMCS_CR3_TGT_VAL_1_STR[] = "CR3_TARGET_VALUE_1";
static const char VMCS_CR3_TGT_VAL_2_STR[] = "CR3_TARGET_VALUE_2";
static const char VMCS_CR3_TGT_VAL_3_STR[] = "CR3_TARGET_VALUE_3";
static const char VMCS_EXIT_QUAL_STR[] = "EXIT_QUALIFICATION";
static const char VMCS_IO_RCX_STR[] = "IO_RCX";
static const char VMCS_IO_RSI_STR[] = "IO_RSI";
static const char VMCS_IO_RDI_STR[] = "IO_RDI";
static const char VMCS_IO_RIP_STR[] = "IO_RIP";
static const char VMCS_GUEST_LINEAR_ADDR_STR[] = "GUEST_LINEAR_ADDR";
static const char VMCS_GUEST_CR0_STR[] = "GUEST_CR0";
static const char VMCS_GUEST_CR3_STR[] = "GUEST_CR3";
static const char VMCS_GUEST_CR4_STR[] = "GUEST_CR4";
static const char VMCS_GUEST_ES_BASE_STR[] = "GUEST_ES_BASE";
static const char VMCS_GUEST_CS_BASE_STR[] = "GUEST_CS_BASE";
static const char VMCS_GUEST_SS_BASE_STR[] = "GUEST_SS_BASE";
static const char VMCS_GUEST_DS_BASE_STR[] = "GUEST_DS_BASE";
static const char VMCS_GUEST_FS_BASE_STR[] = "GUEST_FS_BASE";
static const char VMCS_GUEST_GS_BASE_STR[] = "GUEST_GS_BASE";
static const char VMCS_GUEST_LDTR_BASE_STR[] = "GUEST_LDTR_BASE";
static const char VMCS_GUEST_TR_BASE_STR[] = "GUEST_TR_BASE";
static const char VMCS_GUEST_GDTR_BASE_STR[] = "GUEST_GDTR_BASE";
static const char VMCS_GUEST_IDTR_BASE_STR[] = "GUEST_IDTR_BASE";
static const char VMCS_GUEST_DR7_STR[] = "GUEST_DR7";
static const char VMCS_GUEST_RSP_STR[] = "GUEST_RSP";
static const char VMCS_GUEST_RIP_STR[] = "GUEST_RIP";
static const char VMCS_GUEST_RFLAGS_STR[] = "GUEST_RFLAGS";
static const char VMCS_GUEST_PENDING_DBG_EXCP_STR[] = "GUEST_PENDING_DEBUG_EXCS";
static const char VMCS_GUEST_SYSENTER_ESP_STR[] = "GUEST_SYSENTER_ESP";
static const char VMCS_GUEST_SYSENTER_EIP_STR[] = "GUEST_SYSENTER_EIP";
static const char VMCS_HOST_CR0_STR[] = "HOST_CR0";
static const char VMCS_HOST_CR3_STR[] = "HOST_CR3";
static const char VMCS_HOST_CR4_STR[] = "HOST_CR4";
static const char VMCS_HOST_FS_BASE_STR[] = "HOST_FS_BASE";
static const char VMCS_HOST_GS_BASE_STR[] = "HOST_GS_BASE";
static const char VMCS_HOST_TR_BASE_STR[] = "HOST_TR_BASE";
static const char VMCS_HOST_GDTR_BASE_STR[] = "HOST_GDTR_BASE";
static const char VMCS_HOST_IDTR_BASE_STR[] = "HOST_IDTR_BASE";
static const char VMCS_HOST_SYSENTER_ESP_STR[] = "HOST_SYSENTER_ESP";
static const char VMCS_HOST_SYSENTER_EIP_STR[] = "HOST_SYSENTER_EIP";
static const char VMCS_HOST_RSP_STR[] = "HOST_RSP";
static const char VMCS_HOST_RIP_STR[] = "HOST_RIP";



const char * v3_vmcs_field_to_str(vmcs_field_t field) {   
    switch (field) {
	case VMCS_VPID:
	    return VMCS_VPID_STR;
        case VMCS_GUEST_ES_SELECTOR:
            return VMCS_GUEST_ES_SELECTOR_STR;
        case VMCS_GUEST_CS_SELECTOR:
            return VMCS_GUEST_CS_SELECTOR_STR;
        case VMCS_GUEST_SS_SELECTOR:
            return VMCS_GUEST_SS_SELECTOR_STR;
        case VMCS_GUEST_DS_SELECTOR:
            return VMCS_GUEST_DS_SELECTOR_STR;
        case VMCS_GUEST_FS_SELECTOR:
            return VMCS_GUEST_FS_SELECTOR_STR;
        case VMCS_GUEST_GS_SELECTOR:
            return VMCS_GUEST_GS_SELECTOR_STR;
        case VMCS_GUEST_LDTR_SELECTOR:
            return VMCS_GUEST_LDTR_SELECTOR_STR;
        case VMCS_GUEST_TR_SELECTOR:
            return VMCS_GUEST_TR_SELECTOR_STR;
        case VMCS_HOST_ES_SELECTOR:
            return VMCS_HOST_ES_SELECTOR_STR;
        case VMCS_HOST_CS_SELECTOR:
            return VMCS_HOST_CS_SELECTOR_STR;
        case VMCS_HOST_SS_SELECTOR:
            return VMCS_HOST_SS_SELECTOR_STR;
        case VMCS_HOST_DS_SELECTOR:
            return VMCS_HOST_DS_SELECTOR_STR;
        case VMCS_HOST_FS_SELECTOR:
            return VMCS_HOST_FS_SELECTOR_STR;
        case VMCS_HOST_GS_SELECTOR:
            return VMCS_HOST_GS_SELECTOR_STR;
        case VMCS_HOST_TR_SELECTOR:
            return VMCS_HOST_TR_SELECTOR_STR;
        case VMCS_IO_BITMAP_A_ADDR:
            return VMCS_IO_BITMAP_A_ADDR_STR;
        case VMCS_IO_BITMAP_A_ADDR_HIGH:
            return VMCS_IO_BITMAP_A_ADDR_HIGH_STR;
        case VMCS_IO_BITMAP_B_ADDR:
            return VMCS_IO_BITMAP_B_ADDR_STR;
        case VMCS_IO_BITMAP_B_ADDR_HIGH:
            return VMCS_IO_BITMAP_B_ADDR_HIGH_STR;
        case VMCS_MSR_BITMAP:
            return VMCS_MSR_BITMAP_STR;
        case VMCS_MSR_BITMAP_HIGH:
            return VMCS_MSR_BITMAP_HIGH_STR;
        case VMCS_EXIT_MSR_STORE_ADDR:
            return VMCS_EXIT_MSR_STORE_ADDR_STR;
        case VMCS_EXIT_MSR_STORE_ADDR_HIGH:
            return VMCS_EXIT_MSR_STORE_ADDR_HIGH_STR;
        case VMCS_EXIT_MSR_LOAD_ADDR:
            return VMCS_EXIT_MSR_LOAD_ADDR_STR;
        case VMCS_EXIT_MSR_LOAD_ADDR_HIGH:
            return VMCS_EXIT_MSR_LOAD_ADDR_HIGH_STR;
        case VMCS_ENTRY_MSR_LOAD_ADDR:
            return VMCS_ENTRY_MSR_LOAD_ADDR_STR;
        case VMCS_ENTRY_MSR_LOAD_ADDR_HIGH:
            return VMCS_ENTRY_MSR_LOAD_ADDR_HIGH_STR;
        case VMCS_EXEC_PTR:
            return VMCS_EXEC_PTR_STR;
        case VMCS_EXEC_PTR_HIGH:
            return VMCS_EXEC_PTR_HIGH_STR;
        case VMCS_TSC_OFFSET:
            return VMCS_TSC_OFFSET_STR;
        case VMCS_TSC_OFFSET_HIGH:
            return VMCS_TSC_OFFSET_HIGH_STR;
        case VMCS_VAPIC_ADDR:
            return VMCS_VAPIC_ADDR_STR;
        case VMCS_VAPIC_ADDR_HIGH:
            return VMCS_VAPIC_ADDR_HIGH_STR;
        case VMCS_APIC_ACCESS_ADDR:
            return VMCS_APIC_ACCESS_ADDR_STR;
        case VMCS_APIC_ACCESS_ADDR_HIGH:
            return VMCS_APIC_ACCESS_ADDR_HIGH_STR;
	case VMCS_EPT_PTR:
	    return VMCS_EPT_PTR_STR;
	case VMCS_EPT_PTR_HIGH:
	    return VMCS_EPT_PTR_HIGH_STR;
	case VMCS_GUEST_PHYS_ADDR:
	    return VMCS_GUEST_PHYS_ADDR_STR;
	case VMCS_GUEST_PHYS_ADDR_HIGH:
	    return VMCS_GUEST_PHYS_ADDR_HIGH_STR;
        case VMCS_LINK_PTR:
            return VMCS_LINK_PTR_STR;
        case VMCS_LINK_PTR_HIGH:
            return VMCS_LINK_PTR_HIGH_STR;
        case VMCS_GUEST_DBG_CTL:
            return VMCS_GUEST_DBG_CTL_STR;
        case VMCS_GUEST_DBG_CTL_HIGH:
            return VMCS_GUEST_DBG_CTL_HIGH_STR;
	case VMCS_GUEST_PAT:
	    return VMCS_GUEST_PAT_STR;
	case VMCS_GUEST_PAT_HIGH:
	    return VMCS_GUEST_PAT_HIGH_STR;
	case VMCS_GUEST_EFER:
	    return VMCS_GUEST_EFER_STR;
	case VMCS_GUEST_EFER_HIGH:
	    return VMCS_GUEST_EFER_HIGH_STR;
	case VMCS_GUEST_PERF_GLOBAL_CTRL:
            return VMCS_GUEST_PERF_GLOBAL_CTRL_STR;
        case VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH:
            return VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH_STR;
	case VMCS_GUEST_PDPTE0:
	    return VMCS_GUEST_PDPTE0_STR;
	case VMCS_GUEST_PDPTE0_HIGH:
	    return VMCS_GUEST_PDPTE0_HIGH_STR;
	case VMCS_GUEST_PDPTE1:
	    return VMCS_GUEST_PDPTE1_STR;
	case VMCS_GUEST_PDPTE1_HIGH:
	    return VMCS_GUEST_PDPTE1_HIGH_STR;
	case VMCS_GUEST_PDPTE2:
	    return VMCS_GUEST_PDPTE2_STR;
	case VMCS_GUEST_PDPTE2_HIGH:
	    return VMCS_GUEST_PDPTE2_HIGH_STR;
	case VMCS_GUEST_PDPTE3:
	    return VMCS_GUEST_PDPTE3_STR;
	case VMCS_GUEST_PDPTE3_HIGH:
	    return VMCS_GUEST_PDPTE3_HIGH_STR;
	case VMCS_HOST_PAT:
	    return VMCS_HOST_PAT_STR;
	case VMCS_HOST_PAT_HIGH:
	    return VMCS_HOST_PAT_HIGH_STR;
	case VMCS_HOST_EFER:
	    return VMCS_HOST_EFER_STR;
	case VMCS_HOST_EFER_HIGH:
	    return VMCS_HOST_EFER_HIGH_STR;
        case VMCS_HOST_PERF_GLOBAL_CTRL:
            return VMCS_HOST_PERF_GLOBAL_CTRL_STR;
        case VMCS_HOST_PERF_GLOBAL_CTRL_HIGH:
            return VMCS_HOST_PERF_GLOBAL_CTRL_HIGH_STR;
        case VMCS_PIN_CTRLS:
            return VMCS_PIN_CTRLS_STR;
        case VMCS_PROC_CTRLS:
            return VMCS_PROC_CTRLS_STR;
        case VMCS_EXCP_BITMAP:
            return VMCS_EXCP_BITMAP_STR;
        case VMCS_PG_FAULT_ERR_MASK:
            return VMCS_PG_FAULT_ERR_MASK_STR;
        case VMCS_PG_FAULT_ERR_MATCH:
            return VMCS_PG_FAULT_ERR_MATCH_STR;
        case VMCS_CR3_TGT_CNT:
            return VMCS_CR3_TGT_CNT_STR;
        case VMCS_EXIT_CTRLS:
            return VMCS_EXIT_CTRLS_STR;
        case VMCS_EXIT_MSR_STORE_CNT:
            return VMCS_EXIT_MSR_STORE_CNT_STR;
        case VMCS_EXIT_MSR_LOAD_CNT:
            return VMCS_EXIT_MSR_LOAD_CNT_STR;
        case VMCS_ENTRY_CTRLS:
            return VMCS_ENTRY_CTRLS_STR;
        case VMCS_ENTRY_MSR_LOAD_CNT:
            return VMCS_ENTRY_MSR_LOAD_CNT_STR;
        case VMCS_ENTRY_INT_INFO:
            return VMCS_ENTRY_INT_INFO_STR;
        case VMCS_ENTRY_EXCP_ERR:
            return VMCS_ENTRY_EXCP_ERR_STR;
        case VMCS_ENTRY_INSTR_LEN:
            return VMCS_ENTRY_INSTR_LEN_STR;
        case VMCS_TPR_THRESHOLD:
            return VMCS_TPR_THRESHOLD_STR;
	case VMCS_SEC_PROC_CTRLS:
	    return VMCS_SEC_PROC_CTRLS_STR;
	case VMCS_PLE_GAP:
	    return VMCS_PLE_GAP_STR;
	case VMCS_PLE_WINDOW:
	    return VMCS_PLE_WINDOW_STR;
        case VMCS_INSTR_ERR:
            return VMCS_INSTR_ERR_STR;
        case VMCS_EXIT_REASON:
            return VMCS_EXIT_REASON_STR;
        case VMCS_EXIT_INT_INFO:
            return VMCS_EXIT_INT_INFO_STR;
        case VMCS_EXIT_INT_ERR:
            return VMCS_EXIT_INT_ERR_STR;
        case VMCS_IDT_VECTOR_INFO:
            return VMCS_IDT_VECTOR_INFO_STR;
        case VMCS_IDT_VECTOR_ERR:
            return VMCS_IDT_VECTOR_ERR_STR;
        case VMCS_EXIT_INSTR_LEN:
            return VMCS_EXIT_INSTR_LEN_STR;
        case VMCS_EXIT_INSTR_INFO:
            return VMCS_EXIT_INSTR_INFO_STR;
        case VMCS_GUEST_ES_LIMIT:
            return VMCS_GUEST_ES_LIMIT_STR;
        case VMCS_GUEST_CS_LIMIT:
            return VMCS_GUEST_CS_LIMIT_STR;
        case VMCS_GUEST_SS_LIMIT:
            return VMCS_GUEST_SS_LIMIT_STR;
        case VMCS_GUEST_DS_LIMIT:
            return VMCS_GUEST_DS_LIMIT_STR;
        case VMCS_GUEST_FS_LIMIT:
            return VMCS_GUEST_FS_LIMIT_STR;
        case VMCS_GUEST_GS_LIMIT:
            return VMCS_GUEST_GS_LIMIT_STR;
        case VMCS_GUEST_LDTR_LIMIT:
            return VMCS_GUEST_LDTR_LIMIT_STR;
        case VMCS_GUEST_TR_LIMIT:
            return VMCS_GUEST_TR_LIMIT_STR;
        case VMCS_GUEST_GDTR_LIMIT:
            return VMCS_GUEST_GDTR_LIMIT_STR;
        case VMCS_GUEST_IDTR_LIMIT:
            return VMCS_GUEST_IDTR_LIMIT_STR;
        case VMCS_GUEST_ES_ACCESS:
            return VMCS_GUEST_ES_ACCESS_STR;
        case VMCS_GUEST_CS_ACCESS:
            return VMCS_GUEST_CS_ACCESS_STR;
        case VMCS_GUEST_SS_ACCESS:
            return VMCS_GUEST_SS_ACCESS_STR;
        case VMCS_GUEST_DS_ACCESS:
            return VMCS_GUEST_DS_ACCESS_STR;
        case VMCS_GUEST_FS_ACCESS:
            return VMCS_GUEST_FS_ACCESS_STR;
        case VMCS_GUEST_GS_ACCESS:
            return VMCS_GUEST_GS_ACCESS_STR;
        case VMCS_GUEST_LDTR_ACCESS:
            return VMCS_GUEST_LDTR_ACCESS_STR;
        case VMCS_GUEST_TR_ACCESS:
            return VMCS_GUEST_TR_ACCESS_STR;
        case VMCS_GUEST_INT_STATE:
            return VMCS_GUEST_INT_STATE_STR;
        case VMCS_GUEST_ACTIVITY_STATE:
            return VMCS_GUEST_ACTIVITY_STATE_STR;
        case VMCS_GUEST_SMBASE:
            return VMCS_GUEST_SMBASE_STR;
        case VMCS_GUEST_SYSENTER_CS:
            return VMCS_GUEST_SYSENTER_CS_STR;
	case VMCS_PREEMPT_TIMER:
	    return VMCS_PREEMPT_TIMER_STR;
        case VMCS_HOST_SYSENTER_CS:	    
            return VMCS_HOST_SYSENTER_CS_STR;
        case VMCS_CR0_MASK:
            return VMCS_CR0_MASK_STR;
        case VMCS_CR4_MASK:
            return VMCS_CR4_MASK_STR;
        case VMCS_CR0_READ_SHDW:
            return VMCS_CR0_READ_SHDW_STR;
        case VMCS_CR4_READ_SHDW:
            return VMCS_CR4_READ_SHDW_STR;
        case VMCS_CR3_TGT_VAL_0:
            return VMCS_CR3_TGT_VAL_0_STR;
        case VMCS_CR3_TGT_VAL_1:
            return VMCS_CR3_TGT_VAL_1_STR;
        case VMCS_CR3_TGT_VAL_2:
            return VMCS_CR3_TGT_VAL_2_STR;
        case VMCS_CR3_TGT_VAL_3:
            return VMCS_CR3_TGT_VAL_3_STR;
        case VMCS_EXIT_QUAL:
            return VMCS_EXIT_QUAL_STR;
        case VMCS_IO_RCX:
            return VMCS_IO_RCX_STR;
        case VMCS_IO_RSI:
            return VMCS_IO_RSI_STR;
        case VMCS_IO_RDI:
            return VMCS_IO_RDI_STR;
        case VMCS_IO_RIP:
            return VMCS_IO_RIP_STR;
        case VMCS_GUEST_LINEAR_ADDR:
            return VMCS_GUEST_LINEAR_ADDR_STR;
        case VMCS_GUEST_CR0:
            return VMCS_GUEST_CR0_STR;
        case VMCS_GUEST_CR3:
            return VMCS_GUEST_CR3_STR;
        case VMCS_GUEST_CR4:
            return VMCS_GUEST_CR4_STR;
        case VMCS_GUEST_ES_BASE:
            return VMCS_GUEST_ES_BASE_STR;
        case VMCS_GUEST_CS_BASE:
            return VMCS_GUEST_CS_BASE_STR;
        case VMCS_GUEST_SS_BASE:
            return VMCS_GUEST_SS_BASE_STR;
        case VMCS_GUEST_DS_BASE:
            return VMCS_GUEST_DS_BASE_STR;
        case VMCS_GUEST_FS_BASE:
            return VMCS_GUEST_FS_BASE_STR;
        case VMCS_GUEST_GS_BASE:
            return VMCS_GUEST_GS_BASE_STR;
        case VMCS_GUEST_LDTR_BASE:
            return VMCS_GUEST_LDTR_BASE_STR;
        case VMCS_GUEST_TR_BASE:
            return VMCS_GUEST_TR_BASE_STR;
        case VMCS_GUEST_GDTR_BASE:
            return VMCS_GUEST_GDTR_BASE_STR;
        case VMCS_GUEST_IDTR_BASE:
            return VMCS_GUEST_IDTR_BASE_STR;
        case VMCS_GUEST_DR7:
            return VMCS_GUEST_DR7_STR;
        case VMCS_GUEST_RSP:
            return VMCS_GUEST_RSP_STR;
        case VMCS_GUEST_RIP:
            return VMCS_GUEST_RIP_STR;
        case VMCS_GUEST_RFLAGS:
            return VMCS_GUEST_RFLAGS_STR;
        case VMCS_GUEST_PENDING_DBG_EXCP:
            return VMCS_GUEST_PENDING_DBG_EXCP_STR;
        case VMCS_GUEST_SYSENTER_ESP:
            return VMCS_GUEST_SYSENTER_ESP_STR;
        case VMCS_GUEST_SYSENTER_EIP:
            return VMCS_GUEST_SYSENTER_EIP_STR;
        case VMCS_HOST_CR0:
            return VMCS_HOST_CR0_STR;
        case VMCS_HOST_CR3:
            return VMCS_HOST_CR3_STR;
        case VMCS_HOST_CR4:
            return VMCS_HOST_CR4_STR;
        case VMCS_HOST_FS_BASE:
            return VMCS_HOST_FS_BASE_STR;
        case VMCS_HOST_GS_BASE:
            return VMCS_HOST_GS_BASE_STR;
        case VMCS_HOST_TR_BASE:
            return VMCS_HOST_TR_BASE_STR;
        case VMCS_HOST_GDTR_BASE:
            return VMCS_HOST_GDTR_BASE_STR;
        case VMCS_HOST_IDTR_BASE:
            return VMCS_HOST_IDTR_BASE_STR;
        case VMCS_HOST_SYSENTER_ESP:
            return VMCS_HOST_SYSENTER_ESP_STR;
        case VMCS_HOST_SYSENTER_EIP:
            return VMCS_HOST_SYSENTER_EIP_STR;
        case VMCS_HOST_RSP:
            return VMCS_HOST_RSP_STR;
        case VMCS_HOST_RIP:
            return VMCS_HOST_RIP_STR;
        default:
            return NULL;
    }
}



