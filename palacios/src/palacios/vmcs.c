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

static void inline translate_v3_seg_to_access(struct v3_segment * v3_seg,  
					      struct vmcs_segment_access * access)
{
    access->type = v3_seg->type;
    access->desc_type = v3_seg->system;
    access->dpl = v3_seg->dpl;
    access->present = v3_seg->present;
    access->avail = v3_seg->avail;
    access->long_mode = v3_seg->long_mode;
    access->db = v3_seg->db;
    access->granularity = v3_seg->granularity;
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


static int inline check_vmcs_write(vmcs_field_t field, addr_t val)
{
    int ret = 0;
    ret = vmcs_write(field, val);

    if (ret != VMX_SUCCESS) {
        PrintError("VMWRITE error on %s!: %d\n", v3_vmcs_field_to_str(field), ret);
        return 1;
    }

    return 0;
}

static int inline check_vmcs_read(vmcs_field_t field, void * val)
{
    int ret = 0;
    ret = vmcs_read(field, val);

    if (ret != VMX_SUCCESS) {
        PrintError("VMREAD error on %s!: %d\n", v3_vmcs_field_to_str(field), ret);
    }

    return ret;
}

// static const char * v3_vmcs_field_to_str(vmcs_field_t field);

//extern char * exception_names;
//
// Ignores "HIGH" addresses - 32 bit only for now
//

int v3_update_vmcs_guest_state(struct guest_info * info)
{
    int vmx_ret = 0;

    vmx_ret |= check_vmcs_write(VMCS_GUEST_RIP, info->rip);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_RSP, info->vm_regs.rsp);
    

    vmx_ret |= check_vmcs_write(VMCS_GUEST_CR0, info->ctrl_regs.cr0);
    vmx_ret |= check_vmcs_write(VMCS_CR0_READ_SHDW, info->shdw_pg_state.guest_cr0);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_CR3, info->ctrl_regs.cr3);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_CR4, info->ctrl_regs.cr4);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_DR7, info->dbg_regs.dr7);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_RFLAGS, info->ctrl_regs.rflags);
    if (((struct vmx_data *)info->vmm_data)->ia32e_avail) {
        vmx_ret |= check_vmcs_write(VMCS_GUEST_EFER, info->ctrl_regs.efer);
    }


    /*** Write VMCS Segments ***/
    struct vmcs_segment_access access;

    memset(&access, 0, sizeof(access));

    /* CS Segment */
    translate_v3_seg_to_access(&(info->segments.cs), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_CS_BASE, info->segments.cs.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_CS_SELECTOR, info->segments.cs.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_CS_LIMIT, info->segments.cs.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_CS_ACCESS, access.value);

    /* SS Segment */
    memset(&access, 0, sizeof(access));
    translate_v3_seg_to_access(&(info->segments.ss), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_SS_BASE, info->segments.ss.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_SS_SELECTOR, info->segments.ss.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_SS_LIMIT, info->segments.ss.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_SS_ACCESS, access.value);

    /* DS Segment */
    memset(&access, 0, sizeof(access));
    translate_v3_seg_to_access(&(info->segments.ds), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_DS_BASE, info->segments.ds.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_DS_SELECTOR, info->segments.ds.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_DS_LIMIT, info->segments.ds.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_DS_ACCESS, access.value);


    /* ES Segment */
    memset(&access, 0, sizeof(access));
    translate_v3_seg_to_access(&(info->segments.es), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_ES_BASE, info->segments.es.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_ES_SELECTOR, info->segments.es.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_ES_LIMIT, info->segments.es.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_ES_ACCESS, access.value);

    /* FS Segment */
    memset(&access, 0, sizeof(access));
    translate_v3_seg_to_access(&(info->segments.fs), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_FS_BASE, info->segments.fs.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_FS_SELECTOR, info->segments.fs.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_FS_LIMIT, info->segments.fs.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_FS_ACCESS, access.value);

    /* GS Segment */
    memset(&access, 0, sizeof(access));
    translate_v3_seg_to_access(&(info->segments.gs), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_GS_BASE, info->segments.gs.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_GS_SELECTOR, info->segments.gs.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_GS_LIMIT, info->segments.gs.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_GS_ACCESS, access.value);

    /* LDTR segment */
    memset(&access, 0, sizeof(access));
    translate_v3_seg_to_access(&(info->segments.ldtr), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_LDTR_BASE, info->segments.ldtr.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_LDTR_SELECTOR, info->segments.ldtr.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_LDTR_LIMIT, info->segments.ldtr.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_LDTR_ACCESS, access.value);

    /* TR Segment */
    memset(&access, 0, sizeof(access));
    translate_v3_seg_to_access(&(info->segments.tr), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_TR_BASE, info->segments.tr.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_TR_SELECTOR, info->segments.tr.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_TR_LIMIT, info->segments.tr.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_TR_ACCESS, access.value);

    /* GDTR Segment */

    vmx_ret |= check_vmcs_write(VMCS_GUEST_GDTR_BASE, info->segments.gdtr.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_GDTR_LIMIT, info->segments.gdtr.limit);

    /* IDTR Segment*/
    vmx_ret |= check_vmcs_write(VMCS_GUEST_IDTR_BASE, info->segments.idtr.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_IDTR_LIMIT, info->segments.idtr.limit);

    return vmx_ret;

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

    return vmx_ret;
}

int v3_update_vmcs_host_state(struct guest_info * info) {
    int vmx_ret = 0;
    addr_t tmp;
    struct vmx_data * arch_data = (struct vmx_data *)(info->vmm_data);
    struct v3_msr tmp_msr;

    __asm__ __volatile__ ( "movq    %%cr0, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmx_ret |= check_vmcs_write(VMCS_HOST_CR0, tmp);


    __asm__ __volatile__ ( "movq %%cr3, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmx_ret |= check_vmcs_write(VMCS_HOST_CR3, tmp);


    __asm__ __volatile__ ( "movq %%cr4, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmx_ret |= check_vmcs_write(VMCS_HOST_CR4, tmp);



    vmx_ret |= check_vmcs_write(VMCS_HOST_GDTR_BASE, arch_data->host_state.gdtr.base);
    vmx_ret |= check_vmcs_write(VMCS_HOST_IDTR_BASE, arch_data->host_state.idtr.base);
    vmx_ret |= check_vmcs_write(VMCS_HOST_TR_BASE, arch_data->host_state.tr.base);

#define FS_BASE_MSR 0xc0000100
#define GS_BASE_MSR 0xc0000101

    // FS.BASE MSR
    v3_get_msr(FS_BASE_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_HOST_FS_BASE, tmp_msr.value);    

    // GS.BASE MSR
    v3_get_msr(GS_BASE_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_HOST_GS_BASE, tmp_msr.value);    



    __asm__ __volatile__ ( "movq %%cs, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmx_ret |= check_vmcs_write(VMCS_HOST_CS_SELECTOR, tmp);

    __asm__ __volatile__ ( "movq %%ss, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmx_ret |= check_vmcs_write(VMCS_HOST_SS_SELECTOR, tmp);

    __asm__ __volatile__ ( "movq %%ds, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmx_ret |= check_vmcs_write(VMCS_HOST_DS_SELECTOR, tmp);

    __asm__ __volatile__ ( "movq %%es, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmx_ret |= check_vmcs_write(VMCS_HOST_ES_SELECTOR, tmp);

    __asm__ __volatile__ ( "movq %%fs, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmx_ret |= check_vmcs_write(VMCS_HOST_FS_SELECTOR, tmp);

    __asm__ __volatile__ ( "movq %%gs, %0; "		
			   : "=q"(tmp)
			   :
    );
    vmx_ret |= check_vmcs_write(VMCS_HOST_GS_SELECTOR, tmp);

    vmx_ret |= check_vmcs_write(VMCS_HOST_TR_SELECTOR, arch_data->host_state.tr.selector);


#define SYSENTER_CS_MSR 0x00000174
#define SYSENTER_ESP_MSR 0x00000175
#define SYSENTER_EIP_MSR 0x00000176

   // SYSENTER CS MSR
    v3_get_msr(SYSENTER_CS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_HOST_SYSENTER_CS, tmp_msr.lo);

    // SYSENTER_ESP MSR
    v3_get_msr(SYSENTER_ESP_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_HOST_SYSENTER_ESP, tmp_msr.value);

    // SYSENTER_EIP MSR
    v3_get_msr(SYSENTER_EIP_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_HOST_SYSENTER_EIP, tmp_msr.value);

    return vmx_ret;
}


int v3_load_vmcs_guest_state(struct guest_info * info)
{

    int error = 0;

    check_vmcs_read(VMCS_GUEST_RIP, &(info->rip));
    check_vmcs_read(VMCS_GUEST_RSP, &(info->vm_regs.rsp));

    check_vmcs_read(VMCS_GUEST_CR0, &(info->ctrl_regs.cr0));
    check_vmcs_read(VMCS_CR0_READ_SHDW, &(info->shdw_pg_state.guest_cr0));
    check_vmcs_read(VMCS_GUEST_CR3, &(info->ctrl_regs.cr3));
    check_vmcs_read(VMCS_GUEST_CR4, &(info->ctrl_regs.cr4));
    check_vmcs_read(VMCS_GUEST_DR7, &(info->dbg_regs.dr7));

    check_vmcs_read(VMCS_GUEST_RFLAGS, &(info->ctrl_regs.rflags));
    if (((struct vmx_data *)info->vmm_data)->ia32e_avail) {
        check_vmcs_read(VMCS_GUEST_EFER, &(info->ctrl_regs.efer));
    }

    // JRL: Add error checking

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
    
    return error;
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


    PrintDebug("\n");

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

    PrintDebug("\n");

    print_vmcs_field(VMCS_GUEST_DBG_CTL);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_GUEST_DBG_CTL_HIGH);
#endif
    print_vmcs_field(VMCS_GUEST_SYSENTER_CS);
    print_vmcs_field(VMCS_GUEST_SYSENTER_ESP);
    print_vmcs_field(VMCS_GUEST_SYSENTER_EIP);

    print_vmcs_field(VMCS_GUEST_PERF_GLOBAL_CTRL);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH);
#endif

    print_vmcs_field(VMCS_GUEST_SMBASE);


    PrintDebug("GUEST_NON_REGISTER_STATE\n");

    print_vmcs_field(VMCS_GUEST_ACTIVITY_STATE);
    print_vmcs_field(VMCS_GUEST_INT_STATE);
    print_vmcs_field(VMCS_GUEST_PENDING_DBG_EXCP);

}
       
static void print_host_state()
{
    PrintDebug("VMCS_HOST_STATE\n");

    print_vmcs_field(VMCS_HOST_RIP);
    print_vmcs_field(VMCS_HOST_RSP);
    print_vmcs_field(VMCS_HOST_CR0);
    print_vmcs_field(VMCS_HOST_CR3);
    print_vmcs_field(VMCS_HOST_CR4);
    
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

    print_vmcs_field(VMCS_HOST_PERF_GLOBAL_CTRL);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_HOST_PERF_GLOBAL_CTRL_HIGH);
#endif
}


static void print_exec_ctrls() {
    PrintDebug("VMCS_EXEC_CTRL_FIELDS\n");
    print_vmcs_field(VMCS_PIN_CTRLS);
    print_vmcs_field(VMCS_PROC_CTRLS);
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

    PrintDebug("\n");

    print_vmcs_field(VMCS_APIC_ACCESS_ADDR);    
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_APIC_ACCESS_ADDR_HIGH);
#endif

    print_vmcs_field(VMCS_VAPIC_ADDR);    
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_VAPIC_ADDR_HIGH);
#endif

    print_vmcs_field(VMCS_TPR_THRESHOLD);

    print_vmcs_field(VMCS_MSR_BITMAP);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_MSR_BITMAP_HIGH);
#endif

    print_vmcs_field(VMCS_EXEC_PTR);
#ifdef __V3_32BIT__
    print_vmcs_field(VMCS_EXEC_PTR_HIGH);
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

    print_exec_ctrls();
    print_exit_ctrls();
    print_entry_ctrls();
    print_exit_info();




}


/*
 * Returns the field length in bytes
 */
int v3_vmcs_get_field_len(vmcs_field_t field) {
    switch(field)  {
	/* 16 bit Control Fields */
        case VMCS_GUEST_ES_SELECTOR:
        case VMCS_GUEST_CS_SELECTOR:
        case VMCS_GUEST_SS_SELECTOR:
        case VMCS_GUEST_DS_SELECTOR:
        case VMCS_GUEST_FS_SELECTOR:
        case VMCS_GUEST_GS_SELECTOR:
        case VMCS_GUEST_LDTR_SELECTOR:
        case VMCS_GUEST_TR_SELECTOR:
        case VMCS_HOST_ES_SELECTOR:
        case VMCS_HOST_CS_SELECTOR:
        case VMCS_HOST_SS_SELECTOR:
        case VMCS_HOST_DS_SELECTOR:
        case VMCS_HOST_FS_SELECTOR:
        case VMCS_HOST_GS_SELECTOR:
        case VMCS_HOST_TR_SELECTOR:
            return 2;

	/* 32 bit Control Fields */
        case VMCS_PIN_CTRLS:
        case VMCS_PROC_CTRLS:
	case VMCS_SEC_PROC_CTRLS:
        case VMCS_EXCP_BITMAP:
        case VMCS_PG_FAULT_ERR_MASK:
        case VMCS_PG_FAULT_ERR_MATCH:
        case VMCS_CR3_TGT_CNT:
        case VMCS_EXIT_CTRLS:
        case VMCS_EXIT_MSR_STORE_CNT:
        case VMCS_EXIT_MSR_LOAD_CNT:
        case VMCS_ENTRY_CTRLS:
        case VMCS_ENTRY_MSR_LOAD_CNT:
        case VMCS_ENTRY_INT_INFO:
        case VMCS_ENTRY_EXCP_ERR:
        case VMCS_ENTRY_INSTR_LEN:
        case VMCS_TPR_THRESHOLD:
        case VMCS_INSTR_ERR:
        case VMCS_EXIT_REASON:
        case VMCS_EXIT_INT_INFO:
        case VMCS_EXIT_INT_ERR:
        case VMCS_IDT_VECTOR_INFO:
        case VMCS_IDT_VECTOR_ERR:
        case VMCS_EXIT_INSTR_LEN:
        case VMCS_EXIT_INSTR_INFO:
        case VMCS_GUEST_ES_LIMIT:
        case VMCS_GUEST_CS_LIMIT:
        case VMCS_GUEST_SS_LIMIT:
        case VMCS_GUEST_DS_LIMIT:
        case VMCS_GUEST_FS_LIMIT:
        case VMCS_GUEST_GS_LIMIT:
        case VMCS_GUEST_LDTR_LIMIT:
        case VMCS_GUEST_TR_LIMIT:
        case VMCS_GUEST_GDTR_LIMIT:
        case VMCS_GUEST_IDTR_LIMIT:
        case VMCS_GUEST_ES_ACCESS:
        case VMCS_GUEST_CS_ACCESS:
        case VMCS_GUEST_SS_ACCESS:
        case VMCS_GUEST_DS_ACCESS:
        case VMCS_GUEST_FS_ACCESS:
        case VMCS_GUEST_GS_ACCESS:
        case VMCS_GUEST_LDTR_ACCESS:
        case VMCS_GUEST_TR_ACCESS:
        case VMCS_GUEST_INT_STATE:
        case VMCS_GUEST_ACTIVITY_STATE:
        case VMCS_GUEST_SMBASE:
        case VMCS_GUEST_SYSENTER_CS:
        case VMCS_HOST_SYSENTER_CS:
            return 4;


	/* high bits of variable width fields
	 * We can probably just delete most of these....
	 */
        case VMCS_IO_BITMAP_A_ADDR_HIGH:
        case VMCS_IO_BITMAP_B_ADDR_HIGH:
        case VMCS_MSR_BITMAP_HIGH:
        case VMCS_EXIT_MSR_STORE_ADDR_HIGH:
        case VMCS_EXIT_MSR_LOAD_ADDR_HIGH:
        case VMCS_ENTRY_MSR_LOAD_ADDR_HIGH:
        case VMCS_EXEC_PTR_HIGH:
        case VMCS_TSC_OFFSET_HIGH:
        case VMCS_VAPIC_ADDR_HIGH:
	case VMCS_APIC_ACCESS_ADDR_HIGH:
        case VMCS_LINK_PTR_HIGH:
        case VMCS_GUEST_DBG_CTL_HIGH:
        case VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH:
	case VMCS_HOST_PERF_GLOBAL_CTRL_HIGH:
            return 4;

            /* Natural Width Control Fields */
        case VMCS_IO_BITMAP_A_ADDR:
        case VMCS_IO_BITMAP_B_ADDR:
        case VMCS_MSR_BITMAP:
        case VMCS_EXIT_MSR_STORE_ADDR:
        case VMCS_EXIT_MSR_LOAD_ADDR:
        case VMCS_ENTRY_MSR_LOAD_ADDR:
        case VMCS_EXEC_PTR:
        case VMCS_TSC_OFFSET:
        case VMCS_VAPIC_ADDR:
	case VMCS_APIC_ACCESS_ADDR:
        case VMCS_LINK_PTR:
        case VMCS_GUEST_DBG_CTL:
        case VMCS_GUEST_PERF_GLOBAL_CTRL:
	case VMCS_HOST_PERF_GLOBAL_CTRL:
        case VMCS_CR0_MASK:
        case VMCS_CR4_MASK:
        case VMCS_CR0_READ_SHDW:
        case VMCS_CR4_READ_SHDW:
        case VMCS_CR3_TGT_VAL_0:
        case VMCS_CR3_TGT_VAL_1:
        case VMCS_CR3_TGT_VAL_2:
        case VMCS_CR3_TGT_VAL_3:
        case VMCS_EXIT_QUAL:
        case VMCS_IO_RCX:
        case VMCS_IO_RSI:
        case VMCS_IO_RDI:
        case VMCS_IO_RIP:
        case VMCS_GUEST_LINEAR_ADDR:
        case VMCS_GUEST_CR0:
        case VMCS_GUEST_CR3:
        case VMCS_GUEST_CR4:
        case VMCS_GUEST_ES_BASE:
        case VMCS_GUEST_CS_BASE:
        case VMCS_GUEST_SS_BASE:
        case VMCS_GUEST_DS_BASE:
        case VMCS_GUEST_FS_BASE:
        case VMCS_GUEST_GS_BASE:
        case VMCS_GUEST_LDTR_BASE:
        case VMCS_GUEST_TR_BASE:
        case VMCS_GUEST_GDTR_BASE:
        case VMCS_GUEST_IDTR_BASE:
        case VMCS_GUEST_DR7:
        case VMCS_GUEST_RSP:
        case VMCS_GUEST_RIP:
        case VMCS_GUEST_RFLAGS:
        case VMCS_GUEST_PENDING_DBG_EXCP:
        case VMCS_GUEST_SYSENTER_ESP:
        case VMCS_GUEST_SYSENTER_EIP:
        case VMCS_HOST_CR0:
        case VMCS_HOST_CR3:
        case VMCS_HOST_CR4:
        case VMCS_HOST_FS_BASE:
        case VMCS_HOST_GS_BASE:
        case VMCS_HOST_TR_BASE:
        case VMCS_HOST_GDTR_BASE:
        case VMCS_HOST_IDTR_BASE:
        case VMCS_HOST_SYSENTER_ESP:
        case VMCS_HOST_SYSENTER_EIP:
        case VMCS_HOST_RSP:
        case VMCS_HOST_RIP:
            return sizeof(addr_t);

        default:
	    PrintError("Invalid VMCS field\n");
            return -1;
    }
}












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
static const char VMCS_LINK_PTR_STR[] = "VMCS_LINK_PTR";
static const char VMCS_LINK_PTR_HIGH_STR[] = "VMCS_LINK_PTR_HIGH";
static const char VMCS_GUEST_DBG_CTL_STR[] = "GUEST_DEBUG_CTL";
static const char VMCS_GUEST_DBG_CTL_HIGH_STR[] = "GUEST_DEBUG_CTL_HIGH";
static const char VMCS_GUEST_PERF_GLOBAL_CTRL_STR[] = "GUEST_PERF_GLOBAL_CTRL";
static const char VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH_STR[] = "GUEST_PERF_GLOBAL_CTRL_HIGH";
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
        case VMCS_LINK_PTR:
            return VMCS_LINK_PTR_STR;
        case VMCS_LINK_PTR_HIGH:
            return VMCS_LINK_PTR_HIGH_STR;
        case VMCS_GUEST_DBG_CTL:
            return VMCS_GUEST_DBG_CTL_STR;
        case VMCS_GUEST_DBG_CTL_HIGH:
            return VMCS_GUEST_DBG_CTL_HIGH_STR;
        case VMCS_GUEST_PERF_GLOBAL_CTRL:
            return VMCS_GUEST_PERF_GLOBAL_CTRL_STR;
        case VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH:
            return VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH_STR;
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



