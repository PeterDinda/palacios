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
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *         Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <palacios/vmx.h>
#include <palacios/vmcs.h>
#include <palacios/vmm.h>
#include <palacios/vmx_lowlevel.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vmm_config.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vm_guest_mem.h>

static addr_t vmxon_ptr_phys;
extern int v3_vmx_exit_handler();
extern int v3_vmx_vmlaunch(struct v3_gprs * vm_regs);

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

static void inline translate_segment_access(struct v3_segment * v3_seg,  
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


static int update_vmcs_host_state(struct guest_info * info) {
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


static int inline update_vmcs_guest_state(struct guest_info * info)
{
    struct v3_msr tmp_msr;
    int vmx_ret = 0;

    vmx_ret |= check_vmcs_write(VMCS_GUEST_RIP, info->rip);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_RSP, info->vm_regs.rsp);
    

    vmx_ret |= check_vmcs_write(VMCS_GUEST_CR0, info->ctrl_regs.cr0);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_CR4, info->ctrl_regs.cr4);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_RFLAGS, info->ctrl_regs.rflags);
#define DEBUGCTL_MSR 0x1d9

    v3_get_msr(DEBUGCTL_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_GUEST_DBG_CTL, tmp_msr.value);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_DR7, 0x400);

    vmx_ret |= check_vmcs_write(VMCS_LINK_PTR, 0xffffffffffffffff);

    /*** Write VMCS Segments ***/
    struct vmcs_segment_access access;

    memset(&access, 0, sizeof(access));

    /* CS Segment */
    translate_segment_access(&(info->segments.cs), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_CS_BASE, info->segments.cs.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_CS_SELECTOR, info->segments.cs.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_CS_LIMIT, info->segments.cs.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_CS_ACCESS, access.value);

    /* SS Segment */
    translate_segment_access(&(info->segments.ss), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_SS_BASE, info->segments.ss.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_SS_SELECTOR, info->segments.ss.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_SS_LIMIT, info->segments.ss.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_SS_ACCESS, access.value);

    /* DS Segment */
    translate_segment_access(&(info->segments.ds), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_DS_BASE, info->segments.ds.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_DS_SELECTOR, info->segments.ds.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_DS_LIMIT, info->segments.ds.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_DS_ACCESS, access.value);


    /* ES Segment */
    translate_segment_access(&(info->segments.es), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_ES_BASE, info->segments.es.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_ES_SELECTOR, info->segments.es.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_ES_LIMIT, info->segments.es.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_ES_ACCESS, access.value);

    /* FS Segment */
    translate_segment_access(&(info->segments.fs), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_FS_BASE, info->segments.fs.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_FS_SELECTOR, info->segments.fs.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_FS_LIMIT, info->segments.fs.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_FS_ACCESS, access.value);

    /* GS Segment */
    translate_segment_access(&(info->segments.gs), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_GS_BASE, info->segments.gs.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_GS_SELECTOR, info->segments.gs.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_GS_LIMIT, info->segments.gs.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_GS_ACCESS, access.value);

    /* LDTR segment */
    translate_segment_access(&(info->segments.ldtr), &access);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_LDTR_BASE, info->segments.ldtr.base);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_LDTR_SELECTOR, info->segments.ldtr.selector);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_LDTR_LIMIT, info->segments.ldtr.limit);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_LDTR_ACCESS, access.value);

    /* TR Segment */
    translate_segment_access(&(info->segments.tr), &access);

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




#if 0
// For the 32 bit reserved bit fields 
// MB1s are in the low 32 bits, MBZs are in the high 32 bits of the MSR
static uint32_t sanitize_bits1(uint32_t msr_num, uint32_t val) {
    v3_msr_t mask_msr;

    PrintDebug("sanitize_bits1 (MSR:%x)\n", msr_num);

    v3_get_msr(msr_num, &mask_msr.hi, &mask_msr.lo);

    PrintDebug("MSR %x = %x : %x \n", msr_num, mask_msr.hi, mask_msr.lo);

    val |= mask_msr.lo;
    val |= mask_msr.hi;
  
    return val;
}



static addr_t sanitize_bits2(uint32_t msr_num0, uint32_t msr_num1, addr_t val) {
    v3_msr_t msr0, msr1;
    addr_t msr0_val, msr1_val;

    PrintDebug("sanitize_bits2 (MSR0=%x, MSR1=%x)\n", msr_num0, msr_num1);

    v3_get_msr(msr_num0, &msr0.hi, &msr0.lo);
    v3_get_msr(msr_num1, &msr1.hi, &msr1.lo);
  
    // This generates a mask that is the natural bit width of the CPU
    msr0_val = msr0.value;
    msr1_val = msr1.value;

    PrintDebug("MSR %x = %p, %x = %p \n", msr_num0, (void*)msr0_val, msr_num1, (void*)msr1_val);

    val |= msr0_val;
    val |= msr1_val;

    return val;
}

static int setup_base_host_state() {
    


    //   vmwrite(HOST_IDTR_BASE, 


}


#endif


static addr_t allocate_vmcs() 
{
    reg_ex_t msr;
    PrintDebug("Allocating page\n");
    struct vmcs_data * vmcs_page = (struct vmcs_data *)V3_VAddr(V3_AllocPages(1));


    memset(vmcs_page, 0, 4096);

    v3_get_msr(VMX_BASIC_MSR, &(msr.e_reg.high), &(msr.e_reg.low));
    
    vmcs_page->revision = ((struct vmx_basic_msr*)&msr)->revision;
    PrintDebug("VMX Revision: 0x%x\n",vmcs_page->revision);

    return (addr_t)V3_PAddr((void *)vmcs_page);
}

#if 0
static void setup_v8086_mode_for_boot(struct guest_info * vm_info)
{

    ((struct vmx_data *)vm_info->vmm_data)->state = VMXASSIST_V8086_BIOS;
    struct rflags * flags = (struct rflags *)&(vm_info->ctrl_regs.rflags);
    flags->rsvd1 = 1;
    flags->vm = 1;
    flags->iopl = 3;

#define GUEST_CR0_MASK 0x80000021
#define GUEST_CR4_MASK 0x00002000
    vm_info->ctrl_regs.cr0 = GUEST_CR0_MASK;
    vm_info->ctrl_regs.cr4 = GUEST_CR4_MASK;
   
    vm_info->rip = 0xd0000;
    vm_info->vm_regs.rsp = 0x80000;

    vm_info->segments.cs.selector = 0xf000;
    vm_info->segments.cs.base = 0xf000 << 4;
    vm_info->segments.cs.limit = 0xffff;
    vm_info->segments.cs.type = 3;
    vm_info->segments.cs.system = 1;
    vm_info->segments.cs.dpl = 3;
    vm_info->segments.cs.present = 1;
    vm_info->segments.cs.granularity = 0;

    int i = 0;
    struct v3_segment * seg_ptr = (struct v3_segment *)&(vm_info->segments);

    /* Set values for selectors ds through ss */
    for(i = 1; i < 6 ; i++) {
        seg_ptr[i].selector = 0x0000;
        seg_ptr[i].base = 0x00000;
        seg_ptr[i].limit = 0xffff;
           }

    for(i = 6; i < 10; i++) {
        seg_ptr[i].base = 0x0;
        seg_ptr[i].limit = 0xffff;
    }

    vm_info->segments.ldtr.selector = 0x0;
    vm_info->segments.ldtr.type = 2;
    vm_info->segments.ldtr.system = 0;
    vm_info->segments.ldtr.present = 1;
    vm_info->segments.ldtr.granularity = 0;

    vm_info->segments.tr.selector = 0x0;
    vm_info->segments.tr.type = 3;
    vm_info->segments.tr.system = 0;
    vm_info->segments.tr.present = 1;
    vm_info->segments.tr.granularity = 0;
}
#endif

#if 0
static int init_vmcs_bios(struct guest_info * vm_info) 
{
#if 0

    setup_v8086_mode_for_boot(vm_info);


    // Setup guest state 
    // TODO: This is not 32-bit safe!
    vmx_ret |= check_vmcs_write(VMCS_GUEST_RIP, vm_info->rip);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_RSP, vm_info->vm_regs.rsp);
    

    vmx_ret |= check_vmcs_write(VMCS_GUEST_CR0, vm_info->ctrl_regs.cr0);
    vmx_ret |= check_vmcs_write(VMCS_GUEST_CR4, vm_info->ctrl_regs.cr4);

    vmx_ret |= vmcs_write_guest_segments(vm_info);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_RFLAGS, vm_info->ctrl_regs.rflags);
#define DEBUGCTL_MSR 0x1d9

    v3_get_msr(DEBUGCTL_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_GUEST_DBG_CTL, tmp_msr.value);

    vmx_ret |= check_vmcs_write(VMCS_GUEST_DR7, 0x400);

    vmx_ret |= check_vmcs_write(VMCS_LINK_PTR, 0xffffffffffffffff);

    if (vmx_ret != 0) {
	PrintError("Could not initialize VMCS segments\n");
        return -1;
    }

#endif
    return 0;
}
#endif

static int init_vmx_guest(struct guest_info * info, struct v3_vm_config * config_ptr) {
    v3_pre_config_guest(info, config_ptr);

    struct vmx_data * vmx_data = NULL;

    vmx_data = (struct vmx_data *)V3_Malloc(sizeof(struct vmx_data));

    PrintDebug("vmx_data pointer: %p\n", (void *)vmx_data);

    PrintDebug("Allocating VMCS\n");
    vmx_data->vmcs_ptr_phys = allocate_vmcs();

    PrintDebug("VMCS pointer: %p\n", (void *)(vmx_data->vmcs_ptr_phys));

    info->vmm_data = vmx_data;

    PrintDebug("Initializing VMCS (addr=%p)\n", info->vmm_data);
    
    // TODO: Fix vmcs fields so they're 32-bit
    int vmx_ret = 0;

    PrintDebug("Clearing VMCS: %p\n",(void*)vmx_data->vmcs_ptr_phys);
    vmx_ret = vmcs_clear(vmx_data->vmcs_ptr_phys);

    if (vmx_ret != VMX_SUCCESS) {
        PrintError("VMCLEAR failed\n");
        return -1;
    }

    PrintDebug("Loading VMCS\n");
    vmx_ret = vmcs_load(vmx_data->vmcs_ptr_phys);

    if (vmx_ret != VMX_SUCCESS) {
        PrintError("VMPTRLD failed\n");
        return -1;
    }



    /********** Setup and write VMX Control Fields ***********/
    struct v3_msr tmp_msr;

    v3_get_msr(VMX_PINBASED_CTLS_MSR,&(tmp_msr.hi),&(tmp_msr.lo));
    /* Add NMI exiting */
    tmp_msr.lo |= NMI_EXIT;
    check_vmcs_write(VMCS_PIN_CTRLS, tmp_msr.lo);

    v3_get_msr(VMX_PROCBASED_CTLS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    /* Add unconditional I/O */
    tmp_msr.lo |= UNCOND_IO_EXIT;
    check_vmcs_write(VMCS_PROC_CTRLS, tmp_msr.lo);

    v3_get_msr(VMX_EXIT_CTLS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    tmp_msr.lo |= HOST_ADDR_SPACE_SIZE;
    check_vmcs_write(VMCS_EXIT_CTRLS, tmp_msr.lo);

    v3_get_msr(VMX_ENTRY_CTLS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    check_vmcs_write(VMCS_ENTRY_CTRLS, tmp_msr.lo);

    check_vmcs_write(VMCS_EXCP_BITMAP, 0xffffffff);




    /******* Setup Host State **********/

    /* Cache GDTR, IDTR, and TR in host struct */
    addr_t gdtr_base;
    struct {
        uint16_t selector;
        addr_t   base;
    } __attribute__((packed)) tmp_seg;
    

    __asm__ __volatile__(
			 "sgdt (%0);"
			 :
			 : "q"(&tmp_seg)
			 : "memory"
			 );
    gdtr_base = tmp_seg.base;
    vmx_data->host_state.gdtr.base = gdtr_base;

    __asm__ __volatile__(
			 "sidt (%0);"
			 :
			 : "q"(&tmp_seg)
			 : "memory"
			 );
    vmx_data->host_state.idtr.base = tmp_seg.base;

    __asm__ __volatile__(
			 "str (%0);"
			 :
			 : "q"(&tmp_seg)
			 : "memory"
			 );
    vmx_data->host_state.tr.selector = tmp_seg.selector;

    /* The GDTR *index* is bits 3-15 of the selector. */
    struct tss_descriptor * desc = (struct tss_descriptor *)
                        (gdtr_base + 8*(tmp_seg.selector>>3));

    tmp_seg.base = (
		    (desc->base1) |
		    (desc->base2 << 16) |
		    (desc->base3 << 24) |
#ifdef __V3_64BIT__
		    ((uint64_t)desc->base4 << 32)
#else 
		    (0)
#endif
                );

    vmx_data->host_state.tr.base = tmp_seg.base;

    if(update_vmcs_host_state(info)) {
        PrintError("Could not write host state\n");
        return -1;
    }



    /******* Setup VMXAssist guest state ***********/
    info->rip = 0xd0000;
    info->vm_regs.rsp = 0x80000;

    struct rflags * flags = (struct rflags *)&(info->ctrl_regs.rflags);
    flags->rsvd1 = 1;

#define GUEST_CR0 0x80000031
#define GUEST_CR4 0x00002000
    info->ctrl_regs.cr0 = GUEST_CR0;
    info->ctrl_regs.cr4 = GUEST_CR4;
   
    addr_t guest_cr3 = (addr_t)V3_AllocPages(1);

    memset(V3_VAddr((void*)guest_cr3), 0, 4096);
    vmcs_write(VMCS_GUEST_CR3, guest_cr3);

    v3_get_msr(VMX_CR0_FIXED0_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    PrintDebug("CR0 MSR: %p\n", (void*)tmp_msr.value);

    v3_get_msr(VMX_CR4_FIXED0_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    PrintDebug("CR4 MSR: %p\n", (void*)tmp_msr.value);

    struct v3_segment * seg_reg = (struct v3_segment *)&(info->segments);

    int i;
    for(i=0; i < 10; i++)
    {
        seg_reg[i].selector = 3<<3;
        seg_reg[i].limit = 0xffff;
        seg_reg[i].base = 0x0;
    }
    info->segments.cs.selector = 2<<3;

    /* Set only the segment registers */
    for(i=0; i < 6; i++) {
        seg_reg[i].limit = 0xfffff;
        seg_reg[i].granularity = 1;
        seg_reg[i].type = 3;
        seg_reg[i].system = 1;
        seg_reg[i].dpl = 0;
        seg_reg[i].present = 1;
        seg_reg[i].db = 1;
    }
    info->segments.cs.type = 0xb;

    info->segments.ldtr.selector = 0x20;
    info->segments.ldtr.type = 2;
    info->segments.ldtr.system = 0;
    info->segments.ldtr.present = 1;
    info->segments.ldtr.granularity = 0;
    



uint64_t  gdt[] __attribute__ ((aligned(32))) = {
	0x0000000000000000ULL,		/* 0x00: reserved */
	0x0000830000000000ULL,		/* 0x08: 32-bit TSS */
	//	0x0000890000000000ULL,		/* 0x08: 32-bit TSS */
	0x00CF9b000000FFFFULL,		/* 0x10: CS 32-bit */
	0x00CF93000000FFFFULL,		/* 0x18: DS 32-bit */
	0x000082000000FFFFULL,		/* 0x20: LDTR 32-bit */
};
 
#define VMXASSIST_GDT   0x10000
    addr_t vmxassist_gdt = 0;
    if(guest_pa_to_host_va(info, VMXASSIST_GDT, &vmxassist_gdt) == -1) {
        PrintError("Could not find VMXASSIST GDT destination\n");
        return -1;
    }
    memcpy((void*)vmxassist_gdt, gdt, sizeof(uint64_t) * 5);
        
    info->segments.gdtr.base = VMXASSIST_GDT;

#define VMXASSIST_TSS   0x40000
    addr_t vmxassist_tss = VMXASSIST_TSS;
    gdt[0x08 / sizeof(gdt[0])] |=
	((vmxassist_tss & 0xFF000000) << (56-24)) |
	((vmxassist_tss & 0x00FF0000) << (32-16)) |
	((vmxassist_tss & 0x0000FFFF) << (16)) |
	(8392 - 1);

    info->segments.tr.selector = 0x08;
    info->segments.tr.base = vmxassist_tss;

    // info->segments.tr.type = 0x9; 
    info->segments.tr.type = 0x3;
    info->segments.tr.system = 0;
    info->segments.tr.present = 1;
    info->segments.tr.granularity = 0;

 
#define VMXASSIST_START 0x000d0000
    extern uint8_t vmxassist_start[];
    extern uint8_t vmxassist_end[];

    addr_t vmxassist_dst = 0;
    if(guest_pa_to_host_va(info, VMXASSIST_START, &vmxassist_dst) == -1) {
        PrintError("Could not find VMXASSIST destination\n");
        return -1;
    }
    memcpy((void*)vmxassist_dst, vmxassist_start, vmxassist_end-vmxassist_start);

    if(update_vmcs_guest_state(info) != VMX_SUCCESS) {
        PrintDebug("Writing guest state failed!\n");
        return -1;
    }

    v3_print_vmcs();


     //v3_post_config_guest(info, config_ptr);

    return 0;
}


static int start_vmx_guest(struct guest_info* info) {
    uint32_t error = 0;
    int ret = 0;

    PrintDebug("Attempting VMLAUNCH\n");

    ret = v3_vmx_vmlaunch(&(info->vm_regs));
    if (ret != VMX_SUCCESS) {
        vmcs_read(VMCS_INSTR_ERR, &error);
        PrintError("VMLAUNCH failed: %d\n", error);

        v3_print_vmcs();

    }
    PrintDebug("Returned from VMLAUNCH ret=%d(0x%x)\n", ret, ret);

    return -1;
}


int v3_is_vmx_capable() {
    v3_msr_t feature_msr;
    addr_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    v3_cpuid(0x1, &eax, &ebx, &ecx, &edx);

    PrintDebug("ECX: %p\n", (void*)ecx);

    if (ecx & CPUID_1_ECX_VTXFLAG) {
        v3_get_msr(VMX_FEATURE_CONTROL_MSR, &(feature_msr.hi), &(feature_msr.lo));
	
        PrintTrace("MSRREGlow: 0x%.8x\n", feature_msr.lo);

        if ((feature_msr.lo & FEATURE_CONTROL_VALID) != FEATURE_CONTROL_VALID) {
            PrintDebug("VMX is locked -- enable in the BIOS\n");
            return 0;
        }

    } else {
        PrintDebug("VMX not supported on this cpu\n");
        return 0;
    }

    return 1;
}

static int has_vmx_nested_paging() {
    return 0;
}



void v3_init_vmx(struct v3_ctrl_ops * vm_ops) {
    extern v3_cpu_arch_t v3_cpu_type;

    struct v3_msr tmp_msr;
    uint64_t ret=0;

    v3_get_msr(VMX_CR4_FIXED0_MSR,&(tmp_msr.hi),&(tmp_msr.lo));
    
    __asm__ __volatile__ (
			  "movq %%cr4, %%rbx;"
			  "orq  $0x00002000, %%rbx;"
			  "movq %%rbx, %0;"
			  : "=m"(ret) 
			  :
			  : "%rbx"
			  );

    if((~ret & tmp_msr.value) == 0) {
        __asm__ __volatile__ (
			      "movq %0, %%cr4;"
			      :
			      : "q"(ret)
			      );
    } else {
        PrintError("Invalid CR4 Settings!\n");
        return;
    }
      __asm__ __volatile__ (
			    "movq %%cr0, %%rbx; "
			    "orq  $0x00000020,%%rbx; "
			    "movq %%rbx, %%cr0;"
			    :
			    :
			    : "%rbx"
			    );
      //
    // Should check and return Error here.... 


    // Setup VMXON Region
    vmxon_ptr_phys = allocate_vmcs();
    PrintDebug("VMXON pointer: 0x%p\n", (void*)vmxon_ptr_phys);

    if (v3_enable_vmx(vmxon_ptr_phys) == VMX_SUCCESS) {
        PrintDebug("VMX Enabled\n");
    } else {
        PrintError("VMX initialization failure\n");
        return;
    }
	

    if (has_vmx_nested_paging() == 1) {
        v3_cpu_type = V3_VMX_EPT_CPU;
    } else {
        v3_cpu_type = V3_VMX_CPU;
    }

    // Setup the VMX specific vmm operations
    vm_ops->init_guest = &init_vmx_guest;
    vm_ops->start_guest = &start_vmx_guest;
    vm_ops->has_nested_paging = &has_vmx_nested_paging;

}

