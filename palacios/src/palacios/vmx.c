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
#include <palacios/vmm.h>
#include <palacios/vmx_handler.h>
#include <palacios/vmcs.h>
#include <palacios/vmx_lowlevel.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vmm_config.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_direct_paging.h>
#include <palacios/vmx_io.h>
#include <palacios/vmx_msr.h>


#ifndef CONFIG_DEBUG_VMX
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static addr_t host_vmcs_ptrs[CONFIG_MAX_CPUS] = { [0 ... CONFIG_MAX_CPUS - 1] = 0};



extern int v3_vmx_launch(struct v3_gprs * vm_regs, struct guest_info * info, struct v3_ctrl_regs * ctrl_regs);
extern int v3_vmx_resume(struct v3_gprs * vm_regs, struct guest_info * info, struct v3_ctrl_regs * ctrl_regs);

static int inline check_vmcs_write(vmcs_field_t field, addr_t val) {
    int ret = 0;

    ret = vmcs_write(field,val);

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



#endif


static addr_t allocate_vmcs() {
    reg_ex_t msr;
    struct vmcs_data * vmcs_page = NULL;

    PrintDebug("Allocating page\n");

    vmcs_page = (struct vmcs_data *)V3_VAddr(V3_AllocPages(1));
    memset(vmcs_page, 0, 4096);

    v3_get_msr(VMX_BASIC_MSR, &(msr.e_reg.high), &(msr.e_reg.low));
    
    vmcs_page->revision = ((struct vmx_basic_msr*)&msr)->revision;
    PrintDebug("VMX Revision: 0x%x\n",vmcs_page->revision);

    return (addr_t)V3_PAddr((void *)vmcs_page);
}




static int init_vmcs_bios(struct guest_info * info, struct vmx_data * vmx_state) {
    int vmx_ret = 0;

    PrintDebug("Loading VMCS\n");
    vmx_ret = vmcs_load(vmx_state->vmcs_ptr_phys);

    if (vmx_ret != VMX_SUCCESS) {
        PrintError("VMPTRLD failed\n");
        return -1;
    }



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
    vmx_state->host_state.gdtr.base = gdtr_base;

    __asm__ __volatile__(
			 "sidt (%0);"
			 :
			 : "q"(&tmp_seg)
			 : "memory"
			 );
    vmx_state->host_state.idtr.base = tmp_seg.base;

    __asm__ __volatile__(
			 "str (%0);"
			 :
			 : "q"(&tmp_seg)
			 : "memory"
			 );
    vmx_state->host_state.tr.selector = tmp_seg.selector;

    /* The GDTR *index* is bits 3-15 of the selector. */
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

    vmx_state->host_state.tr.base = tmp_seg.base;

  

    /********** Setup and VMX Control Fields from MSR ***********/
    /* Setup IO map */


    struct v3_msr tmp_msr;

    v3_get_msr(VMX_PINBASED_CTLS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));

    /* Add external interrupts, NMI exiting, and virtual NMI */
    vmx_state->pin_ctrls.value =  tmp_msr.lo;
    vmx_state->pin_ctrls.nmi_exit = 1;
    vmx_state->pin_ctrls.ext_int_exit = 1;

    v3_get_msr(VMX_PROCBASED_CTLS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));

    vmx_state->pri_proc_ctrls.value = tmp_msr.lo;
    vmx_state->pri_proc_ctrls.use_io_bitmap = 1;
    vmx_state->pri_proc_ctrls.hlt_exit = 1;
    vmx_state->pri_proc_ctrls.invlpg_exit = 1;
    vmx_state->pri_proc_ctrls.use_msr_bitmap = 1;
    vmx_state->pri_proc_ctrls.pause_exit = 1;
    vmx_state->pri_proc_ctrls.tsc_offset = 1;
#ifdef CONFIG_TIME_VIRTUALIZE_TSC
    vmx_state->pri_proc_ctrls.rdtsc_exit = 1;
#endif

    vmx_ret |= check_vmcs_write(VMCS_IO_BITMAP_A_ADDR, (addr_t)V3_PAddr(info->vm_info->io_map.arch_data));
    vmx_ret |= check_vmcs_write(VMCS_IO_BITMAP_B_ADDR, 
            (addr_t)V3_PAddr(info->vm_info->io_map.arch_data) + PAGE_SIZE_4KB);


    vmx_ret |= check_vmcs_write(VMCS_MSR_BITMAP, (addr_t)V3_PAddr(info->vm_info->msr_map.arch_data));

    v3_get_msr(VMX_EXIT_CTLS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_state->exit_ctrls.value = tmp_msr.lo;
    vmx_state->exit_ctrls.host_64_on = 1;

    if ((vmx_state->exit_ctrls.save_efer == 1) || (vmx_state->exit_ctrls.ld_efer == 1)) {
        vmx_state->ia32e_avail = 1;
    }

    v3_get_msr(VMX_ENTRY_CTLS_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_state->entry_ctrls.value = tmp_msr.lo;

    {
	struct vmx_exception_bitmap excp_bmap;
	excp_bmap.value = 0;
	
	excp_bmap.pf = 1;
    
	vmx_ret |= check_vmcs_write(VMCS_EXCP_BITMAP, excp_bmap.value);
    }
    /******* Setup VMXAssist guest state ***********/

    info->rip = 0xd0000;
    info->vm_regs.rsp = 0x80000;

    struct rflags * flags = (struct rflags *)&(info->ctrl_regs.rflags);
    flags->rsvd1 = 1;

    /* Print Control MSRs */
    v3_get_msr(VMX_CR0_FIXED0_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    PrintDebug("CR0 MSR: %p\n", (void *)(addr_t)tmp_msr.value);

    v3_get_msr(VMX_CR4_FIXED0_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    PrintDebug("CR4 MSR: %p\n", (void *)(addr_t)tmp_msr.value);


#define GUEST_CR0 0x80000031
#define GUEST_CR4 0x00002000
    info->ctrl_regs.cr0 = GUEST_CR0;
    info->ctrl_regs.cr4 = GUEST_CR4;

    ((struct cr0_32 *)&(info->shdw_pg_state.guest_cr0))->pe = 1;
   
    /* Setup paging */
    if (info->shdw_pg_mode == SHADOW_PAGING) {
        PrintDebug("Creating initial shadow page table\n");

        if (v3_init_passthrough_pts(info) == -1) {
            PrintError("Could not initialize passthrough page tables\n");
            return -1;
        }
        
#define CR0_PE 0x00000001
#define CR0_PG 0x80000000


        vmx_ret |= check_vmcs_write(VMCS_CR0_MASK, (CR0_PE | CR0_PG) );
        vmx_ret |= check_vmcs_write(VMCS_CR4_MASK, CR4_VMXE);

        info->ctrl_regs.cr3 = info->direct_map_pt;

        // vmx_state->pinbased_ctrls |= NMI_EXIT;

        /* Add CR exits */
        vmx_state->pri_proc_ctrls.cr3_ld_exit = 1;
        vmx_state->pri_proc_ctrls.cr3_str_exit = 1;
    }

    // Setup segment registers
    {
	struct v3_segment * seg_reg = (struct v3_segment *)&(info->segments);

	int i;

	for (i = 0; i < 10; i++) {
	    seg_reg[i].selector = 3 << 3;
	    seg_reg[i].limit = 0xffff;
	    seg_reg[i].base = 0x0;
	}

	info->segments.cs.selector = 2<<3;

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

	info->segments.cs.type = 0xb;

	info->segments.ldtr.selector = 0x20;
	info->segments.ldtr.type = 2;
	info->segments.ldtr.system = 0;
	info->segments.ldtr.present = 1;
	info->segments.ldtr.granularity = 0;

    
	/************* Map in GDT and vmxassist *************/

	uint64_t  gdt[] __attribute__ ((aligned(32))) = {
	    0x0000000000000000ULL,		/* 0x00: reserved */
	    0x0000830000000000ULL,		/* 0x08: 32-bit TSS */
	    //0x0000890000000000ULL,		/* 0x08: 32-bit TSS */
	    0x00CF9b000000FFFFULL,		/* 0x10: CS 32-bit */
	    0x00CF93000000FFFFULL,		/* 0x18: DS 32-bit */
	    0x000082000000FFFFULL,		/* 0x20: LDTR 32-bit */
	};

#define VMXASSIST_GDT   0x10000
	addr_t vmxassist_gdt = 0;

	if (v3_gpa_to_hva(info, VMXASSIST_GDT, &vmxassist_gdt) == -1) {
	    PrintError("Could not find VMXASSIST GDT destination\n");
	    return -1;
	}

	memcpy((void *)vmxassist_gdt, gdt, sizeof(uint64_t) * 5);
        
	info->segments.gdtr.base = VMXASSIST_GDT;

#define VMXASSIST_TSS   0x40000
	uint64_t vmxassist_tss = VMXASSIST_TSS;
	gdt[0x08 / sizeof(gdt[0])] |=
	    ((vmxassist_tss & 0xFF000000) << (56 - 24)) |
	    ((vmxassist_tss & 0x00FF0000) << (32 - 16)) |
	    ((vmxassist_tss & 0x0000FFFF) << (16)) |
	    (8392 - 1);

	info->segments.tr.selector = 0x08;
	info->segments.tr.base = vmxassist_tss;

	//info->segments.tr.type = 0x9; 
	info->segments.tr.type = 0x3;
	info->segments.tr.system = 0;
	info->segments.tr.present = 1;
	info->segments.tr.granularity = 0;
    }
 
    // setup VMXASSIST
    { 
#define VMXASSIST_START 0x000d0000
	extern uint8_t v3_vmxassist_start[];
	extern uint8_t v3_vmxassist_end[];
	addr_t vmxassist_dst = 0;

	if (v3_gpa_to_hva(info, VMXASSIST_START, &vmxassist_dst) == -1) {
	    PrintError("Could not find VMXASSIST destination\n");
	    return -1;
	}

	memcpy((void *)vmxassist_dst, v3_vmxassist_start, v3_vmxassist_end - v3_vmxassist_start);
    }    

    /*** Write all the info to the VMCS ***/

#define DEBUGCTL_MSR 0x1d9
    v3_get_msr(DEBUGCTL_MSR, &(tmp_msr.hi), &(tmp_msr.lo));
    vmx_ret |= check_vmcs_write(VMCS_GUEST_DBG_CTL, tmp_msr.value);

    info->dbg_regs.dr7 = 0x400;

#ifdef __V3_64BIT__
    vmx_ret |= check_vmcs_write(VMCS_LINK_PTR, (addr_t)0xffffffffffffffffULL);
#else
    vmx_ret |= check_vmcs_write(VMCS_LINK_PTR, (addr_t)0xffffffffUL);
    vmx_ret |= check_vmcs_write(VMCS_LINK_PTR_HIGH, (addr_t)0xffffffffUL);
#endif

    if (v3_update_vmcs_ctrl_fields(info)) {
        PrintError("Could not write control fields!\n");
        return -1;
    }
    
    if (v3_update_vmcs_host_state(info)) {
        PrintError("Could not write host state\n");
        return -1;
    }


    vmx_state->state = VMXASSIST_DISABLED;

    return 0;
}

int v3_init_vmx_vmcs(struct guest_info * info, v3_vm_class_t vm_class) {
    struct vmx_data * vmx_state = NULL;
    int vmx_ret = 0;
    
    vmx_state = (struct vmx_data *)V3_Malloc(sizeof(struct vmx_data));

    PrintDebug("vmx_data pointer: %p\n", (void *)vmx_state);

    PrintDebug("Allocating VMCS\n");
    vmx_state->vmcs_ptr_phys = allocate_vmcs();

    PrintDebug("VMCS pointer: %p\n", (void *)(vmx_state->vmcs_ptr_phys));

    info->vmm_data = vmx_state;

    PrintDebug("Initializing VMCS (addr=%p)\n", info->vmm_data);
    
    // TODO: Fix vmcs fields so they're 32-bit

    PrintDebug("Clearing VMCS: %p\n", (void *)vmx_state->vmcs_ptr_phys);
    vmx_ret = vmcs_clear(vmx_state->vmcs_ptr_phys);

    if (vmx_ret != VMX_SUCCESS) {
        PrintError("VMCLEAR failed\n");
        return -1; 
    }

    if (vm_class == V3_PC_VM) {
	PrintDebug("Initializing VMCS\n");
	init_vmcs_bios(info, vmx_state);
    } else {
	PrintError("Invalid VM Class\n");
	return -1;
    }

    return 0;
}


int v3_deinit_vmx_vmcs(struct guest_info * core) {
    struct vmx_data * vmx_state = core->vmm_data;

    V3_FreePages((void *)(vmx_state->vmcs_ptr_phys), 1);

    V3_Free(vmx_state);

    return 0;
}


static int update_irq_exit_state(struct guest_info * info) {
    struct vmx_exit_idt_vec_info idt_vec_info;

    check_vmcs_read(VMCS_IDT_VECTOR_INFO, &(idt_vec_info.value));

    if ((info->intr_core_state.irq_started == 1) && (idt_vec_info.valid == 0)) {
#ifdef CONFIG_DEBUG_INTERRUPTS
        PrintDebug("Calling v3_injecting_intr\n");
#endif
        info->intr_core_state.irq_started = 0;
        v3_injecting_intr(info, info->intr_core_state.irq_vector, V3_EXTERNAL_IRQ);
    }

    return 0;
}

static int update_irq_entry_state(struct guest_info * info) {
    struct vmx_exit_idt_vec_info idt_vec_info;
    struct vmcs_interrupt_state intr_core_state;
    struct vmx_data * vmx_info = (struct vmx_data *)(info->vmm_data);

    check_vmcs_read(VMCS_IDT_VECTOR_INFO, &(idt_vec_info.value));
    check_vmcs_read(VMCS_GUEST_INT_STATE, &(intr_core_state));

    /* Check for pending exceptions to inject */
    if (v3_excp_pending(info)) {
        struct vmx_entry_int_info int_info;
        int_info.value = 0;

        // In VMX, almost every exception is hardware
        // Software exceptions are pretty much only for breakpoint or overflow
        int_info.type = 3;
        int_info.vector = v3_get_excp_number(info);

        if (info->excp_state.excp_error_code_valid) {
            check_vmcs_write(VMCS_ENTRY_EXCP_ERR, info->excp_state.excp_error_code);
            int_info.error_code = 1;

#ifdef CONFIG_DEBUG_INTERRUPTS
            PrintDebug("Injecting exception %d with error code %x\n", 
                    int_info.vector, info->excp_state.excp_error_code);
#endif
        }

        int_info.valid = 1;
#ifdef CONFIG_DEBUG_INTERRUPTS
        PrintDebug("Injecting exception %d (EIP=%p)\n", int_info.vector, (void *)(addr_t)info->rip);
#endif
        check_vmcs_write(VMCS_ENTRY_INT_INFO, int_info.value);

        v3_injecting_excp(info, int_info.vector);

    } else if ((((struct rflags *)&(info->ctrl_regs.rflags))->intr == 1) && 
	       (intr_core_state.val == 0)) {
       
        if ((info->intr_core_state.irq_started == 1) && (idt_vec_info.valid == 1)) {

#ifdef CONFIG_DEBUG_INTERRUPTS
            PrintDebug("IRQ pending from previous injection\n");
#endif

            // Copy the IDT vectoring info over to reinject the old interrupt
            if (idt_vec_info.error_code == 1) {
                uint32_t err_code = 0;

                check_vmcs_read(VMCS_IDT_VECTOR_ERR, &err_code);
                check_vmcs_write(VMCS_ENTRY_EXCP_ERR, err_code);
            }

            idt_vec_info.undef = 0;
            check_vmcs_write(VMCS_ENTRY_INT_INFO, idt_vec_info.value);

        } else {
            struct vmx_entry_int_info ent_int;
            ent_int.value = 0;

            switch (v3_intr_pending(info)) {
                case V3_EXTERNAL_IRQ: {
                    info->intr_core_state.irq_vector = v3_get_intr(info); 
                    ent_int.vector = info->intr_core_state.irq_vector;
                    ent_int.type = 0;
                    ent_int.error_code = 0;
                    ent_int.valid = 1;

#ifdef CONFIG_DEBUG_INTERRUPTS
                    PrintDebug("Injecting Interrupt %d at exit %u(EIP=%p)\n", 
			       info->intr_core_state.irq_vector, 
			       (uint32_t)info->num_exits, 
			       (void *)(addr_t)info->rip);
#endif

                    check_vmcs_write(VMCS_ENTRY_INT_INFO, ent_int.value);
                    info->intr_core_state.irq_started = 1;

                    break;
                }
                case V3_NMI:
                    PrintDebug("Injecting NMI\n");

                    ent_int.type = 2;
                    ent_int.vector = 2;
                    ent_int.valid = 1;
                    check_vmcs_write(VMCS_ENTRY_INT_INFO, ent_int.value);

                    break;
                case V3_SOFTWARE_INTR:
                    PrintDebug("Injecting software interrupt\n");
                    ent_int.type = 4;

                    ent_int.valid = 1;
                    check_vmcs_write(VMCS_ENTRY_INT_INFO, ent_int.value);

		    break;
                case V3_VIRTUAL_IRQ:
                    // Not sure what to do here, Intel doesn't have virtual IRQs
                    // May be the same as external interrupts/IRQs

		    break;
                case V3_INVALID_INTR:
                default:
                    break;
            }
        }
    } else if ((v3_intr_pending(info)) && (vmx_info->pri_proc_ctrls.int_wndw_exit == 0)) {
        // Enable INTR window exiting so we know when IF=1
        uint32_t instr_len;

        check_vmcs_read(VMCS_EXIT_INSTR_LEN, &instr_len);

#ifdef CONFIG_DEBUG_INTERRUPTS
        PrintDebug("Enabling Interrupt-Window exiting: %d\n", instr_len);
#endif

        vmx_info->pri_proc_ctrls.int_wndw_exit = 1;
        check_vmcs_write(VMCS_PROC_CTRLS, vmx_info->pri_proc_ctrls.value);
    }


    return 0;
}



static struct vmx_exit_info exit_log[10];

static void print_exit_log(struct guest_info * info) {
    int cnt = info->num_exits % 10;
    int i = 0;
    

    V3_Print("\nExit Log (%d total exits):\n", (uint32_t)info->num_exits);

    for (i = 0; i < 10; i++) {
	struct vmx_exit_info * tmp = &exit_log[cnt];

	V3_Print("%d:\texit_reason = %p\n", i, (void *)(addr_t)tmp->exit_reason);
	V3_Print("\texit_qual = %p\n", (void *)tmp->exit_qual);
	V3_Print("\tint_info = %p\n", (void *)(addr_t)tmp->int_info);
	V3_Print("\tint_err = %p\n", (void *)(addr_t)tmp->int_err);
	V3_Print("\tinstr_info = %p\n", (void *)(addr_t)tmp->instr_info);

	cnt--;

	if (cnt == -1) {
	    cnt = 9;
	}

    }

}

/* 
 * CAUTION and DANGER!!! 
 * 
 * The VMCS CANNOT(!!) be accessed outside of the cli/sti calls inside this function
 * When exectuing a symbiotic call, the VMCS WILL be overwritten, so any dependencies 
 * on its contents will cause things to break. The contents at the time of the exit WILL 
 * change before the exit handler is executed.
 */
int v3_vmx_enter(struct guest_info * info) {
    int ret = 0;
    uint32_t tsc_offset_low, tsc_offset_high;
    struct vmx_exit_info exit_info;

    // Conditionally yield the CPU if the timeslice has expired
    v3_yield_cond(info);

    /* If this guest is frequency-lagged behind host time, wait 
     * for the appropriate host time before resuming the guest. */
    v3_adjust_time(info);

    // v3_print_guest_state(info);

    // disable global interrupts for vm state transition
    v3_disable_ints();

    v3_vmx_restore_vmcs(info);


#ifdef CONFIG_SYMCALL
    if (info->sym_core_state.symcall_state.sym_call_active == 0) {
	update_irq_entry_state(info);
    }
#else 
    update_irq_entry_state(info);
#endif

    {
	addr_t guest_cr3;
	vmcs_read(VMCS_GUEST_CR3, &guest_cr3);
	vmcs_write(VMCS_GUEST_CR3, guest_cr3);
    }

    v3_update_timers(info);

    tsc_offset_high = (uint32_t)((v3_tsc_host_offset(&info->time_state) >> 32) & 0xffffffff);
    tsc_offset_low = (uint32_t)(v3_tsc_host_offset(&info->time_state) & 0xffffffff);
    check_vmcs_write(VMCS_TSC_OFFSET_HIGH, tsc_offset_high);
    check_vmcs_write(VMCS_TSC_OFFSET, tsc_offset_low);

    if (info->vm_info->run_state == VM_STOPPED) {
	info->vm_info->run_state = VM_RUNNING;
	ret = v3_vmx_launch(&(info->vm_regs), info, &(info->ctrl_regs));
    } else {
	ret = v3_vmx_resume(&(info->vm_regs), info, &(info->ctrl_regs));
    }

    //  PrintDebug("VMX Exit: ret=%d\n", ret);

    if (ret != VMX_SUCCESS) {
	uint32_t error = 0;

        vmcs_read(VMCS_INSTR_ERR, &error);
        PrintError("VMENTRY Error: %d\n", error);

	return -1;
    }

    info->num_exits++;

    /* Update guest state */
    v3_vmx_save_vmcs(info);

    // info->cpl = info->segments.cs.selector & 0x3;

    info->mem_mode = v3_get_vm_mem_mode(info);
    info->cpu_mode = v3_get_vm_cpu_mode(info);


    check_vmcs_read(VMCS_EXIT_INSTR_LEN, &(exit_info.instr_len));
    check_vmcs_read(VMCS_EXIT_INSTR_INFO, &(exit_info.instr_info));
    check_vmcs_read(VMCS_EXIT_REASON, &(exit_info.exit_reason));
    check_vmcs_read(VMCS_EXIT_QUAL, &(exit_info.exit_qual));
    check_vmcs_read(VMCS_EXIT_INT_INFO, &(exit_info.int_info));
    check_vmcs_read(VMCS_EXIT_INT_ERR, &(exit_info.int_err));
    check_vmcs_read(VMCS_GUEST_LINEAR_ADDR, &(exit_info.guest_linear_addr));

    //PrintDebug("VMX Exit taken, id-qual: %u-%lu\n", exit_info.exit_reason, exit_info.exit_qual);

    exit_log[info->num_exits % 10] = exit_info;


#ifdef CONFIG_SYMCALL
    if (info->sym_core_state.symcall_state.sym_call_active == 0) {
	update_irq_exit_state(info);
    }
#else
    update_irq_exit_state(info);
#endif

    // reenable global interrupts after vm exit
    v3_enable_ints();

    // Conditionally yield the CPU if the timeslice has expired
    v3_yield_cond(info);

    if (v3_handle_vmx_exit(info, &exit_info) == -1) {
	PrintError("Error in VMX exit handler\n");
	return -1;
    }

    return 0;
}


int v3_start_vmx_guest(struct guest_info * info) {

    PrintDebug("Starting VMX core %u\n", info->cpu_id);

    if (info->cpu_id == 0) {
	info->core_run_state = CORE_RUNNING;
	info->vm_info->run_state = VM_RUNNING;
    } else {

        PrintDebug("VMX core %u: Waiting for core initialization\n", info->cpu_id);

        while (info->core_run_state == CORE_STOPPED) {
            v3_yield(info);
            //PrintDebug("VMX core %u: still waiting for INIT\n",info->cpu_id);
        }
	
	PrintDebug("VMX core %u initialized\n", info->cpu_id);
    }


    PrintDebug("VMX core %u: I am starting at CS=0x%x (base=0x%p, limit=0x%x),  RIP=0x%p\n",
               info->cpu_id, info->segments.cs.selector, (void *)(info->segments.cs.base),
               info->segments.cs.limit, (void *)(info->rip));


    PrintDebug("VMX core %u: Launching VMX VM\n", info->cpu_id);

    v3_start_time(info);

    while (1) {

	if (info->vm_info->run_state == VM_STOPPED) {
	    info->core_run_state = CORE_STOPPED;
	    break;
	}

	if (v3_vmx_enter(info) == -1) {
	    v3_print_vmcs();
	    print_exit_log(info);
	    return -1;
	}



	if (info->vm_info->run_state == VM_STOPPED) {
	    info->core_run_state = CORE_STOPPED;
	    break;
	}
/*
	if ((info->num_exits % 5000) == 0) {
	    V3_Print("VMX Exit number %d\n", (uint32_t)info->num_exits);
	}
*/

    }

    return 0;
}


int v3_is_vmx_capable() {
    v3_msr_t feature_msr;
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    v3_cpuid(0x1, &eax, &ebx, &ecx, &edx);

    PrintDebug("ECX: 0x%x\n", ecx);

    if (ecx & CPUID_1_ECX_VTXFLAG) {
        v3_get_msr(VMX_FEATURE_CONTROL_MSR, &(feature_msr.hi), &(feature_msr.lo));
	
        PrintDebug("MSRREGlow: 0x%.8x\n", feature_msr.lo);

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



void v3_init_vmx_cpu(int cpu_id) {
    extern v3_cpu_arch_t v3_cpu_types[];
    struct v3_msr tmp_msr;
    uint64_t ret = 0;

    v3_get_msr(VMX_CR4_FIXED0_MSR,&(tmp_msr.hi),&(tmp_msr.lo));
#ifdef __V3_64BIT__
    __asm__ __volatile__ (
			  "movq %%cr4, %%rbx;"
			  "orq  $0x00002000, %%rbx;"
			  "movq %%rbx, %0;"
			  : "=m"(ret) 
			  :
			  : "%rbx"
			  );

    if ((~ret & tmp_msr.value) == 0) {
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
#elif __V3_32BIT__
    __asm__ __volatile__ (
			  "movl %%cr4, %%ecx;"
			  "orl  $0x00002000, %%ecx;"
			  "movl %%ecx, %0;"
			  : "=m"(ret) 
			  :
			  : "%ecx"
			  );

    if ((~ret & tmp_msr.value) == 0) {
        __asm__ __volatile__ (
			      "movl %0, %%cr4;"
			      :
			      : "q"(ret)
			      );
    } else {
        PrintError("Invalid CR4 Settings!\n");
        return;
    }

    __asm__ __volatile__ (
			  "movl %%cr0, %%ecx; "
			  "orl  $0x00000020,%%ecx; "
			  "movl %%ecx, %%cr0;"
			  :
			  :
			  : "%ecx"
			  );

#endif

    //
    // Should check and return Error here.... 


    // Setup VMXON Region
    host_vmcs_ptrs[cpu_id] = allocate_vmcs();

    PrintDebug("VMXON pointer: 0x%p\n", (void *)host_vmcs_ptrs[cpu_id]);

    if (v3_enable_vmx(host_vmcs_ptrs[cpu_id]) == VMX_SUCCESS) {
        PrintDebug("VMX Enabled\n");
    } else {
        PrintError("VMX initialization failure\n");
        return;
    }
    

    if (has_vmx_nested_paging() == 1) {
        v3_cpu_types[cpu_id] = V3_VMX_EPT_CPU;
    } else {
        v3_cpu_types[cpu_id] = V3_VMX_CPU;
    }

}

