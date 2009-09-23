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
#include <palacios/vmm_cpuid.h>

#include <palacios/vmx.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vmx_ctrl_regs.h>
#include <palacios/vmx_assist.h>
#include <palacios/vmm_halt.h>

#ifdef CONFIG_TELEMETRY
#include <palacios/vmm_telemetry.h>
#endif


static int inline check_vmcs_write(vmcs_field_t field, addr_t val) {
    int ret = 0;

    ret = vmcs_write(field, val);

    if (ret != VMX_SUCCESS) {
        PrintError("VMWRITE error on %s!: %d\n", v3_vmcs_field_to_str(field), ret);
    }

    return ret;
}

static int inline check_vmcs_read(vmcs_field_t field, void * val) {
    int ret = 0;

    ret = vmcs_read(field, val);

    if (ret != VMX_SUCCESS) {
        PrintError("VMREAD error on %s!: %d\n", v3_vmcs_field_to_str(field), ret);
    }

    return ret;
}

static int inline handle_cr_access(struct guest_info * info, ulong_t exit_qual) {
    struct vmx_exit_cr_qual * cr_qual = (struct vmx_exit_cr_qual *)&exit_qual;

    // PrintDebug("Control register: %d\n", cr_qual->access_type);
    switch(cr_qual->cr_id) {
        case 0:
	    //PrintDebug("Handling CR0 Access\n");
            return v3_vmx_handle_cr0_access(info);
        case 3:
	    //PrintDebug("Handling CR3 Access\n");
            return v3_vmx_handle_cr3_access(info);
        default:
            PrintError("Unhandled CR access: %d\n", cr_qual->cr_id);
            return -1;
    }
    
    return -1;
}


/* At this point the GPRs are already copied into the guest_info state */
int v3_handle_vmx_exit(struct v3_gprs * gprs, struct guest_info * info, struct v3_ctrl_regs * ctrl_regs) {
    uint64_t tmp_tsc = 0;
    uint32_t exit_reason = 0;
    addr_t exit_qual = 0;
    struct vmx_data * vmx_info = (struct vmx_data *)(info->vmm_data);
    struct vmx_exit_idt_vec_info idt_vec_info;

    rdtscll(tmp_tsc);
    v3_update_time(info, tmp_tsc - info->time_state.cached_host_tsc);

    v3_enable_ints();

    check_vmcs_read(VMCS_EXIT_REASON, &exit_reason);
    check_vmcs_read(VMCS_EXIT_QUAL, &exit_qual);

    //PrintDebug("VMX Exit taken, id-qual: %u-%lu\n", exit_reason, exit_qual);

    /* Update guest state */
    v3_load_vmcs_guest_state(info);

    // Load execution controls
    check_vmcs_read(VMCS_PIN_CTRLS, &(vmx_info->pin_ctrls.value));
    check_vmcs_read(VMCS_PROC_CTRLS, &(vmx_info->pri_proc_ctrls.value));

    if (vmx_info->pri_proc_ctrls.sec_ctrls) {
        check_vmcs_read(VMCS_SEC_PROC_CTRLS, &(vmx_info->sec_proc_ctrls.value));
    }

    info->mem_mode = v3_get_vm_mem_mode(info);
    info->cpu_mode = v3_get_vm_cpu_mode(info);

    // Check if we got interrupted while delivering interrupt
    // Variable will be used later if this is true

    check_vmcs_read(VMCS_IDT_VECTOR_INFO, &(idt_vec_info.value));

    if ((info->intr_state.irq_started == 1) && (idt_vec_info.valid == 0)) {
#ifdef CONFIG_DEBUG_INTERRUPTS
        PrintDebug("Calling v3_injecting_intr\n");
#endif
        info->intr_state.irq_started = 0;
        v3_injecting_intr(info, info->intr_state.irq_vector, V3_EXTERNAL_IRQ);
    }

    info->num_exits++;



    if ((info->num_exits % 5000) == 0) {
	PrintDebug("VMX Exit %d\n", (uint32_t)info->num_exits);
    }

#ifdef CONFIG_TELEMETRY
    if (info->enable_telemetry) {
	v3_telemetry_start_exit(info);
    }
#endif

    switch (exit_reason) {
        case VMEXIT_INFO_EXCEPTION_OR_NMI: {
            uint32_t int_info;
            pf_error_t error_code;

            check_vmcs_read(VMCS_EXIT_INT_INFO, &int_info);
            check_vmcs_read(VMCS_EXIT_INT_ERR, &error_code);

            // JRL: Change "0x0e" to a macro value
            if ((uint8_t)int_info == 0x0e) {
#ifdef CONFIG_DEBUG_SHADOW_PAGING
                PrintDebug("Page Fault at %p error_code=%x\n", (void *)exit_qual, *(uint32_t *)&error_code);
#endif

                if (info->shdw_pg_mode == SHADOW_PAGING) {
                    if (v3_handle_shadow_pagefault(info, (addr_t)exit_qual, error_code) == -1) {
                        PrintError("Error handling shadow page fault\n");
                        return -1;
                    }
                } else {
                    PrintError("Page fault in unimplemented paging mode\n");
                    return -1;
                }
            } else {
                PrintError("Unknown exception: 0x%x\n", (uint8_t)int_info);
                v3_print_GPRs(info);
                return -1;
            }
            break;
        }

        case VMEXIT_INVLPG:
            if (info->shdw_pg_mode == SHADOW_PAGING) {
                if (v3_handle_shadow_invlpg(info) == -1) {
		    PrintError("Error handling INVLPG\n");
                    return -1;
                }
            }

            break;
        case VMEXIT_CPUID:
	    if (v3_handle_cpuid(info) == -1) {
		PrintError("Error Handling CPUID instruction\n");
		return -1;
	    }

            break;
        case VMEXIT_RDMSR: 
            if (v3_handle_msr_read(info) == -1) {
		PrintError("Error handling MSR Read\n");
                return -1;
	    }

            break;
        case VMEXIT_WRMSR:
            if (v3_handle_msr_write(info) == -1) {
		PrintError("Error handling MSR Write\n");
                return -1;
	    }

            break;
	case VMEXIT_VMCALL:
	    /* 
	     * Hypercall 
	     */

	    // VMCALL is a 3 byte op
	    // We do this early because some hypercalls can change the rip...
	    info->rip += 3;	    

	    if (v3_handle_hypercall(info) == -1) {
		return -1;
	    }
	    break;
        case VMEXIT_IO_INSTR: {
	    struct vmx_exit_io_qual * io_qual = (struct vmx_exit_io_qual *)&exit_qual;

            if (io_qual->dir == 0) {
                if (io_qual->string) {
                    if (v3_handle_vmx_io_outs(info) == -1) {
                        PrintError("Error in outs IO handler\n");
                        return -1;
                    }
                } else {
                    if (v3_handle_vmx_io_out(info) == -1) {
                        PrintError("Error in out IO handler\n");
                        return -1;
                    }
                }
            } else {
                if (io_qual->string) {
                    if(v3_handle_vmx_io_ins(info) == -1) {
                        PrintError("Error in ins IO handler\n");
                        return -1;
                    }
                } else {
                    if (v3_handle_vmx_io_in(info) == -1) {
                        PrintError("Error in in IO handler\n");
                        return -1;
                    }
                }
            }
            break;
	}
        case VMEXIT_CR_REG_ACCESSES:
            if (handle_cr_access(info, exit_qual) != 0) {
                PrintError("Error handling CR access\n");
                return -1;
            }

            break;
        case VMEXIT_HLT:
            PrintDebug("Guest halted\n");

            if (v3_handle_halt(info) == -1) {
		PrintError("Error handling halt instruction\n");
                return -1;
            }

            break;
        case VMEXIT_PAUSE:
            // Handled as NOP
            info->rip += 2;

            break;
        case VMEXIT_EXTERNAL_INTR:
            // Interrupts are handled outside switch
            break;
        case VMEXIT_INTR_WINDOW:

            vmx_info->pri_proc_ctrls.int_wndw_exit = 0;
            check_vmcs_write(VMCS_PROC_CTRLS, vmx_info->pri_proc_ctrls.value);

#ifdef CONFIG_DEBUG_INTERRUPTS
            PrintDebug("Interrupts available again! (RIP=%llx)\n", info->rip);
#endif

            break;
        default:
            PrintError("Unhandled VMEXIT: %s (%u), %lu (0x%lx)\n", 
		       v3_vmx_exit_code_to_str(exit_reason),
		       exit_reason, exit_qual, exit_qual);
            return -1;
    }

#ifdef CONFIG_TELEMETRY
    if (info->enable_telemetry) {
        v3_telemetry_end_exit(info, exit_reason);
    }
#endif


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
        PrintDebug("Injecting exception %d (EIP=%p)\n", int_info.vector, (void *)info->rip);
#endif
        check_vmcs_write(VMCS_ENTRY_INT_INFO, int_info.value);

        v3_injecting_excp(info, int_info.vector);

    } else if (((struct rflags *)&(info->ctrl_regs.rflags))->intr == 1) {
       
        if ((info->intr_state.irq_started == 1) && (idt_vec_info.valid == 1)) {

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
                    info->intr_state.irq_vector = v3_get_intr(info); 
                    ent_int.vector = info->intr_state.irq_vector;
                    ent_int.type = 0;
                    ent_int.error_code = 0;
                    ent_int.valid = 1;

#ifdef CONFIG_DEBUG_INTERRUPTS
                    PrintDebug("Injecting Interrupt %d at exit %u(EIP=%p)\n", 
			       info->intr_state.irq_vector, 
			       (uint32_t)info->num_exits, 
			       (void *)info->rip);
#endif

                    check_vmcs_write(VMCS_ENTRY_INT_INFO, ent_int.value);
                    info->intr_state.irq_started = 1;

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

    check_vmcs_write(VMCS_GUEST_CR0, info->ctrl_regs.cr0);
    check_vmcs_write(VMCS_GUEST_CR3, info->ctrl_regs.cr3);
    check_vmcs_write(VMCS_GUEST_CR4, info->ctrl_regs.cr4);
    check_vmcs_write(VMCS_GUEST_RIP, info->rip);
    check_vmcs_write(VMCS_GUEST_RSP, info->vm_regs.rsp);

    check_vmcs_write(VMCS_CR0_READ_SHDW, info->shdw_pg_state.guest_cr0);

    v3_disable_ints();

    rdtscll(info->time_state.cached_host_tsc);

    return 0;
}

static const char VMEXIT_INFO_EXCEPTION_OR_NMI_STR[] = "VMEXIT_INFO_EXCEPTION_OR_NMI";
static const char VMEXIT_EXTERNAL_INTR_STR[] = "VMEXIT_EXTERNAL_INTR";
static const char VMEXIT_TRIPLE_FAULT_STR[] = "VMEXIT_TRIPLE_FAULT";
static const char VMEXIT_INIT_SIGNAL_STR[] = "VMEXIT_INIT_SIGNAL";
static const char VMEXIT_STARTUP_IPI_STR[] = "VMEXIT_STARTUP_IPI";
static const char VMEXIT_IO_SMI_STR[] = "VMEXIT_IO_SMI";
static const char VMEXIT_OTHER_SMI_STR[] = "VMEXIT_OTHER_SMI";
static const char VMEXIT_INTR_WINDOW_STR[] = "VMEXIT_INTR_WINDOW";
static const char VMEXIT_NMI_WINDOW_STR[] = "VMEXIT_NMI_WINDOW";
static const char VMEXIT_TASK_SWITCH_STR[] = "VMEXIT_TASK_SWITCH";
static const char VMEXIT_CPUID_STR[] = "VMEXIT_CPUID";
static const char VMEXIT_HLT_STR[] = "VMEXIT_HLT";
static const char VMEXIT_INVD_STR[] = "VMEXIT_INVD";
static const char VMEXIT_INVLPG_STR[] = "VMEXIT_INVLPG";
static const char VMEXIT_RDPMC_STR[] = "VMEXIT_RDPMC";
static const char VMEXIT_RDTSC_STR[] = "VMEXIT_RDTSC";
static const char VMEXIT_RSM_STR[] = "VMEXIT_RSM";
static const char VMEXIT_VMCALL_STR[] = "VMEXIT_VMCALL";
static const char VMEXIT_VMCLEAR_STR[] = "VMEXIT_VMCLEAR";
static const char VMEXIT_VMLAUNCH_STR[] = "VMEXIT_VMLAUNCH";
static const char VMEXIT_VMPTRLD_STR[] = "VMEXIT_VMPTRLD";
static const char VMEXIT_VMPTRST_STR[] = "VMEXIT_VMPTRST";
static const char VMEXIT_VMREAD_STR[] = "VMEXIT_VMREAD";
static const char VMEXIT_VMRESUME_STR[] = "VMEXIT_VMRESUME";
static const char VMEXIT_VMWRITE_STR[] = "VMEXIT_VMWRITE";
static const char VMEXIT_VMXOFF_STR[] = "VMEXIT_VMXOFF";
static const char VMEXIT_VMXON_STR[] = "VMEXIT_VMXON";
static const char VMEXIT_CR_REG_ACCESSES_STR[] = "VMEXIT_CR_REG_ACCESSES";
static const char VMEXIT_MOV_DR_STR[] = "VMEXIT_MOV_DR";
static const char VMEXIT_IO_INSTR_STR[] = "VMEXIT_IO_INSTR";
static const char VMEXIT_RDMSR_STR[] = "VMEXIT_RDMSR";
static const char VMEXIT_WRMSR_STR[] = "VMEXIT_WRMSR";
static const char VMEXIT_ENTRY_FAIL_INVALID_GUEST_STATE_STR[] = "VMEXIT_ENTRY_FAIL_INVALID_GUEST_STATE";
static const char VMEXIT_ENTRY_FAIL_MSR_LOAD_STR[] = "VMEXIT_ENTRY_FAIL_MSR_LOAD";
static const char VMEXIT_MWAIT_STR[] = "VMEXIT_MWAIT";
static const char VMEXIT_MONITOR_STR[] = "VMEXIT_MONITOR";
static const char VMEXIT_PAUSE_STR[] = "VMEXIT_PAUSE";
static const char VMEXIT_ENTRY_FAILURE_MACHINE_CHECK_STR[] = "VMEXIT_ENTRY_FAILURE_MACHINE_CHECK";
static const char VMEXIT_TPR_BELOW_THRESHOLD_STR[] = "VMEXIT_TPR_BELOW_THRESHOLD";
static const char VMEXIT_APIC_STR[] = "VMEXIT_APIC";
static const char VMEXIT_GDTR_IDTR_STR[] = "VMEXIT_GDTR_IDTR";
static const char VMEXIT_LDTR_TR_STR[] = "VMEXIT_LDTR_TR";
static const char VMEXIT_EPT_VIOLATION_STR[] = "VMEXIT_EPT_VIOLATION";
static const char VMEXIT_EPT_CONFIG_STR[] = "VMEXIT_EPT_CONFIG";
static const char VMEXIT_INVEPT_STR[] = "VMEXIT_INVEPT";
static const char VMEXIT_RDTSCP_STR[] = "VMEXIT_RDTSCP";
static const char VMEXIT_EXPIRED_PREEMPT_TIMER_STR[] = "VMEXIT_EXPIRED_PREEMPT_TIMER";
static const char VMEXIT_INVVPID_STR[] = "VMEXIT_INVVPID";
static const char VMEXIT_WBINVD_STR[] = "VMEXIT_WBINVD";
static const char VMEXIT_XSETBV_STR[] = "VMEXIT_XSETBV";

const char * v3_vmx_exit_code_to_str(vmx_exit_t exit)
{
    switch(exit) {
        case VMEXIT_INFO_EXCEPTION_OR_NMI:
            return VMEXIT_INFO_EXCEPTION_OR_NMI_STR;
        case VMEXIT_EXTERNAL_INTR:
            return VMEXIT_EXTERNAL_INTR_STR;
        case VMEXIT_TRIPLE_FAULT:
            return VMEXIT_TRIPLE_FAULT_STR;
        case VMEXIT_INIT_SIGNAL:
            return VMEXIT_INIT_SIGNAL_STR;
        case VMEXIT_STARTUP_IPI:
            return VMEXIT_STARTUP_IPI_STR;
        case VMEXIT_IO_SMI:
            return VMEXIT_IO_SMI_STR;
        case VMEXIT_OTHER_SMI:
            return VMEXIT_OTHER_SMI_STR;
        case VMEXIT_INTR_WINDOW:
            return VMEXIT_INTR_WINDOW_STR;
        case VMEXIT_NMI_WINDOW:
            return VMEXIT_NMI_WINDOW_STR;
        case VMEXIT_TASK_SWITCH:
            return VMEXIT_TASK_SWITCH_STR;
        case VMEXIT_CPUID:
            return VMEXIT_CPUID_STR;
        case VMEXIT_HLT:
            return VMEXIT_HLT_STR;
        case VMEXIT_INVD:
            return VMEXIT_INVD_STR;
        case VMEXIT_INVLPG:
            return VMEXIT_INVLPG_STR;
        case VMEXIT_RDPMC:
            return VMEXIT_RDPMC_STR;
        case VMEXIT_RDTSC:
            return VMEXIT_RDTSC_STR;
        case VMEXIT_RSM:
            return VMEXIT_RSM_STR;
        case VMEXIT_VMCALL:
            return VMEXIT_VMCALL_STR;
        case VMEXIT_VMCLEAR:
            return VMEXIT_VMCLEAR_STR;
        case VMEXIT_VMLAUNCH:
            return VMEXIT_VMLAUNCH_STR;
        case VMEXIT_VMPTRLD:
            return VMEXIT_VMPTRLD_STR;
        case VMEXIT_VMPTRST:
            return VMEXIT_VMPTRST_STR;
        case VMEXIT_VMREAD:
            return VMEXIT_VMREAD_STR;
        case VMEXIT_VMRESUME:
            return VMEXIT_VMRESUME_STR;
        case VMEXIT_VMWRITE:
            return VMEXIT_VMWRITE_STR;
        case VMEXIT_VMXOFF:
            return VMEXIT_VMXOFF_STR;
        case VMEXIT_VMXON:
            return VMEXIT_VMXON_STR;
        case VMEXIT_CR_REG_ACCESSES:
            return VMEXIT_CR_REG_ACCESSES_STR;
        case VMEXIT_MOV_DR:
            return VMEXIT_MOV_DR_STR;
        case VMEXIT_IO_INSTR:
            return VMEXIT_IO_INSTR_STR;
        case VMEXIT_RDMSR:
            return VMEXIT_RDMSR_STR;
        case VMEXIT_WRMSR:
            return VMEXIT_WRMSR_STR;
        case VMEXIT_ENTRY_FAIL_INVALID_GUEST_STATE:
            return VMEXIT_ENTRY_FAIL_INVALID_GUEST_STATE_STR;
        case VMEXIT_ENTRY_FAIL_MSR_LOAD:
            return VMEXIT_ENTRY_FAIL_MSR_LOAD_STR;
        case VMEXIT_MWAIT:
            return VMEXIT_MWAIT_STR;
        case VMEXIT_MONITOR:
            return VMEXIT_MONITOR_STR;
        case VMEXIT_PAUSE:
            return VMEXIT_PAUSE_STR;
        case VMEXIT_ENTRY_FAILURE_MACHINE_CHECK:
            return VMEXIT_ENTRY_FAILURE_MACHINE_CHECK_STR;
        case VMEXIT_TPR_BELOW_THRESHOLD:
            return VMEXIT_TPR_BELOW_THRESHOLD_STR;
        case VMEXIT_APIC:
            return VMEXIT_APIC_STR;
        case VMEXIT_GDTR_IDTR:
            return VMEXIT_GDTR_IDTR_STR;
        case VMEXIT_LDTR_TR:
            return VMEXIT_LDTR_TR_STR;
        case VMEXIT_EPT_VIOLATION:
            return VMEXIT_EPT_VIOLATION_STR;
        case VMEXIT_EPT_CONFIG:
            return VMEXIT_EPT_CONFIG_STR;
        case VMEXIT_INVEPT:
            return VMEXIT_INVEPT_STR;
        case VMEXIT_RDTSCP:
            return VMEXIT_RDTSCP_STR;
        case VMEXIT_EXPIRED_PREEMPT_TIMER:
            return VMEXIT_EXPIRED_PREEMPT_TIMER_STR;
        case VMEXIT_INVVPID:
            return VMEXIT_INVVPID_STR;
        case VMEXIT_WBINVD:
            return VMEXIT_WBINVD_STR;
        case VMEXIT_XSETBV:
            return VMEXIT_XSETBV_STR;
    }
    return NULL;
}

