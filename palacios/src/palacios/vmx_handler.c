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
#include <palacios/vmx_ept.h>

#ifndef V3_CONFIG_DEBUG_VMX
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#ifdef V3_CONFIG_TELEMETRY
#include <palacios/vmm_telemetry.h>
#endif

/* At this point the GPRs are already copied into the guest_info state */
int v3_handle_vmx_exit(struct guest_info * info, struct vmx_exit_info * exit_info) {
    struct vmx_basic_exit_info * basic_info = (struct vmx_basic_exit_info *)&(exit_info->exit_reason);

    /*
      PrintError("Handling VMX_EXIT: %s (%u), %lu (0x%lx)\n", 
      v3_vmx_exit_code_to_str(exit_info->exit_reason),
      exit_info->exit_reason, 
      exit_info->exit_qual, exit_info->exit_qual);
      
      v3_print_vmcs();
    */


    if (basic_info->entry_error == 1) {
	switch (basic_info->reason) {
	    case VMX_EXIT_INVALID_GUEST_STATE:
		PrintError("VM Entry failed due to invalid guest state\n");
		PrintError("Printing VMCS: (NOTE: This VMCS may not belong to the correct guest)\n");
		v3_print_vmcs();
		break;
	    case VMX_EXIT_INVALID_MSR_LOAD:
		PrintError("VM Entry failed due to error loading MSRs\n");
		break;
	    default:
		PrintError("Entry failed for unknown reason (%d)\n", basic_info->reason);
		break;
	}
	
	return -1;
    }



#ifdef V3_CONFIG_TELEMETRY
    if (info->vm_info->enable_telemetry) {
	v3_telemetry_start_exit(info);
    }
#endif

    switch (basic_info->reason) {
        case VMX_EXIT_INFO_EXCEPTION_OR_NMI: {
            pf_error_t error_code = *(pf_error_t *)&(exit_info->int_err);


            // JRL: Change "0x0e" to a macro value
            if ((uint8_t)exit_info->int_info == 14) {
#ifdef V3_CONFIG_DEBUG_SHADOW_PAGING
                PrintDebug("Page Fault at %p error_code=%x\n", (void *)exit_info->exit_qual, *(uint32_t *)&error_code);
#endif

                if (info->shdw_pg_mode == SHADOW_PAGING) {
                    if (v3_handle_shadow_pagefault(info, (addr_t)exit_info->exit_qual, error_code) == -1) {
                        PrintError("Error handling shadow page fault\n");
                        return -1;
                    }
	    
                } else {
                    PrintError("Page fault in unimplemented paging mode\n");
                    return -1;
                }
	    } else if ((uint8_t)exit_info->int_info == 2) {
		// NMI. Don't do anything
		V3_Print("NMI Exception Received\n");
            } else {
                PrintError("Unknown exception: 0x%x\n", (uint8_t)exit_info->int_info);
                v3_print_GPRs(info);
                return -1;
            }
            break;
        }

	case VMX_EXIT_EPT_VIOLATION: {
	    struct ept_exit_qual * ept_qual = (struct ept_exit_qual *)&(exit_info->exit_qual);

	    if (v3_handle_ept_fault(info, exit_info->ept_fault_addr, ept_qual) == -1) {
		PrintError("Error handling EPT fault\n");
		return -1;
	    }

	    break;
	}
        case VMX_EXIT_INVLPG:
            if (info->shdw_pg_mode == SHADOW_PAGING) {
                if (v3_handle_shadow_invlpg(info) == -1) {
		    PrintError("Error handling INVLPG\n");
                    return -1;
                }
            }

            break;

        case VMX_EXIT_RDTSC:
#ifdef V3_CONFIG_DEBUG_TIME
	    PrintDebug("RDTSC\n");
#endif 
	    if (v3_handle_rdtsc(info) == -1) {
		PrintError("Error Handling RDTSC instruction\n");
		return -1;
	    }
	    
	    break;

        case VMX_EXIT_CPUID:
	    if (v3_handle_cpuid(info) == -1) {
		PrintError("Error Handling CPUID instruction\n");
		return -1;
	    }

            break;
        case VMX_EXIT_RDMSR: 
            if (v3_handle_msr_read(info) == -1) {
		PrintError("Error handling MSR Read\n");
                return -1;
	    }

            break;
        case VMX_EXIT_WRMSR:
            if (v3_handle_msr_write(info) == -1) {
		PrintError("Error handling MSR Write\n");
                return -1;
	    }

            break;
	case VMX_EXIT_VMCALL:
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
        case VMX_EXIT_IO_INSTR: {
	    struct vmx_exit_io_qual * io_qual = (struct vmx_exit_io_qual *)&(exit_info->exit_qual);

            if (io_qual->dir == 0) {
                if (io_qual->string) {
                    if (v3_handle_vmx_io_outs(info, exit_info) == -1) {
                        PrintError("Error in outs IO handler\n");
                        return -1;
                    }
                } else {
                    if (v3_handle_vmx_io_out(info, exit_info) == -1) {
                        PrintError("Error in out IO handler\n");
                        return -1;
                    }
                }
            } else {
                if (io_qual->string) {
                    if(v3_handle_vmx_io_ins(info, exit_info) == -1) {
                        PrintError("Error in ins IO handler\n");
                        return -1;
                    }
                } else {
                    if (v3_handle_vmx_io_in(info, exit_info) == -1) {
                        PrintError("Error in in IO handler\n");
                        return -1;
                    }
                }
            }
            break;
	}
        case VMX_EXIT_CR_REG_ACCESSES: {
	    struct vmx_exit_cr_qual * cr_qual = (struct vmx_exit_cr_qual *)&(exit_info->exit_qual);
	    
	    // PrintDebug("Control register: %d\n", cr_qual->access_type);
	    switch(cr_qual->cr_id) {
		case 0:
		    //PrintDebug("Handling CR0 Access\n");
		    if (v3_vmx_handle_cr0_access(info, cr_qual, exit_info) == -1) {
			PrintError("Error in CR0 access handler\n");
			return -1;
		    }
		    break;
		case 3:
		    //PrintDebug("Handling CR3 Access\n");
		    if (v3_vmx_handle_cr3_access(info, cr_qual) == -1) {
			PrintError("Error in CR3 access handler\n");
			return -1;
		    }
		    break;
		case 4:
		    //PrintDebug("Handling CR4 Access\n");
		    if (v3_vmx_handle_cr4_access(info, cr_qual) == -1) {
			PrintError("Error in CR4 access handler\n");
			return -1;
		    }
		    break;
		default:
		    PrintError("Unhandled CR access: %d\n", cr_qual->cr_id);
		    return -1;
	    }
	    
	    // TODO: move RIP increment into all of the above individual CR
	    //       handlers, not just v3_vmx_handle_cr4_access()
	    if (cr_qual->cr_id != 4)
		info->rip += exit_info->instr_len;

	    break;
	}
        case VMX_EXIT_HLT:
            PrintDebug("Guest halted\n");

            if (v3_handle_halt(info) == -1) {
		PrintError("Error handling halt instruction\n");
                return -1;
            }

            break;



        case VMX_EXIT_PAUSE:
            // Handled as NOP
            info->rip += 2;

            break;
        case VMX_EXIT_EXTERNAL_INTR:
            // Interrupts are handled outside switch
            break;
        case VMX_EXIT_INTR_WINDOW:
	    // This is handled in the atomic part of the vmx code,
	    // not in the generic (interruptable) vmx handler
            break;
        case VMX_EXIT_EXPIRED_PREEMPT_TIMER:
	    V3_Print("VMX Preempt Timer Expired.\n");
	    // This just forces an exit and is handled outside the switch
	    break;
	    
        default:
            PrintError("Unhandled VMX_EXIT: %s (%u), %lu (0x%lx)\n", 
		       v3_vmx_exit_code_to_str(basic_info->reason),
		       basic_info->reason, 
		       exit_info->exit_qual, exit_info->exit_qual);
            return -1;
    }


#ifdef V3_CONFIG_TELEMETRY
    if (info->vm_info->enable_telemetry) {
        v3_telemetry_end_exit(info, exit_info->exit_reason);
    }
#endif


    return 0;
}

static const char VMX_EXIT_INFO_EXCEPTION_OR_NMI_STR[] = "VMX_EXIT_INFO_EXCEPTION_OR_NMI";
static const char VMX_EXIT_EXTERNAL_INTR_STR[] = "VMX_EXIT_EXTERNAL_INTR";
static const char VMX_EXIT_TRIPLE_FAULT_STR[] = "VMX_EXIT_TRIPLE_FAULT";
static const char VMX_EXIT_INIT_SIGNAL_STR[] = "VMX_EXIT_INIT_SIGNAL";
static const char VMX_EXIT_STARTUP_IPI_STR[] = "VMX_EXIT_STARTUP_IPI";
static const char VMX_EXIT_IO_SMI_STR[] = "VMX_EXIT_IO_SMI";
static const char VMX_EXIT_OTHER_SMI_STR[] = "VMX_EXIT_OTHER_SMI";
static const char VMX_EXIT_INTR_WINDOW_STR[] = "VMX_EXIT_INTR_WINDOW";
static const char VMX_EXIT_NMI_WINDOW_STR[] = "VMX_EXIT_NMI_WINDOW";
static const char VMX_EXIT_TASK_SWITCH_STR[] = "VMX_EXIT_TASK_SWITCH";
static const char VMX_EXIT_CPUID_STR[] = "VMX_EXIT_CPUID";
static const char VMX_EXIT_HLT_STR[] = "VMX_EXIT_HLT";
static const char VMX_EXIT_INVD_STR[] = "VMX_EXIT_INVD";
static const char VMX_EXIT_INVLPG_STR[] = "VMX_EXIT_INVLPG";
static const char VMX_EXIT_RDPMC_STR[] = "VMX_EXIT_RDPMC";
static const char VMX_EXIT_RDTSC_STR[] = "VMX_EXIT_RDTSC";
static const char VMX_EXIT_RSM_STR[] = "VMX_EXIT_RSM";
static const char VMX_EXIT_VMCALL_STR[] = "VMX_EXIT_VMCALL";
static const char VMX_EXIT_VMCLEAR_STR[] = "VMX_EXIT_VMCLEAR";
static const char VMX_EXIT_VMLAUNCH_STR[] = "VMX_EXIT_VMLAUNCH";
static const char VMX_EXIT_VMPTRLD_STR[] = "VMX_EXIT_VMPTRLD";
static const char VMX_EXIT_VMPTRST_STR[] = "VMX_EXIT_VMPTRST";
static const char VMX_EXIT_VMREAD_STR[] = "VMX_EXIT_VMREAD";
static const char VMX_EXIT_VMRESUME_STR[] = "VMX_EXIT_VMRESUME";
static const char VMX_EXIT_VMWRITE_STR[] = "VMX_EXIT_VMWRITE";
static const char VMX_EXIT_VMXOFF_STR[] = "VMX_EXIT_VMXOFF";
static const char VMX_EXIT_VMXON_STR[] = "VMX_EXIT_VMXON";
static const char VMX_EXIT_CR_REG_ACCESSES_STR[] = "VMX_EXIT_CR_REG_ACCESSES";
static const char VMX_EXIT_MOV_DR_STR[] = "VMX_EXIT_MOV_DR";
static const char VMX_EXIT_IO_INSTR_STR[] = "VMX_EXIT_IO_INSTR";
static const char VMX_EXIT_RDMSR_STR[] = "VMX_EXIT_RDMSR";
static const char VMX_EXIT_WRMSR_STR[] = "VMX_EXIT_WRMSR";
static const char VMX_EXIT_INVALID_GUEST_STATE_STR[] = "VMX_EXIT_INVALID_GUEST_STATE";
static const char VMX_EXIT_INVALID_MSR_LOAD_STR[] = "VMX_EXIT_INVALID_MSR_LOAD";
static const char VMX_EXIT_MWAIT_STR[] = "VMX_EXIT_MWAIT";
static const char VMX_EXIT_MONITOR_STR[] = "VMX_EXIT_MONITOR";
static const char VMX_EXIT_PAUSE_STR[] = "VMX_EXIT_PAUSE";
static const char VMX_EXIT_INVALID_MACHINE_CHECK_STR[] = "VMX_EXIT_INVALIDE_MACHINE_CHECK";
static const char VMX_EXIT_TPR_BELOW_THRESHOLD_STR[] = "VMX_EXIT_TPR_BELOW_THRESHOLD";
static const char VMX_EXIT_APIC_STR[] = "VMX_EXIT_APIC";
static const char VMX_EXIT_GDTR_IDTR_STR[] = "VMX_EXIT_GDTR_IDTR";
static const char VMX_EXIT_LDTR_TR_STR[] = "VMX_EXIT_LDTR_TR";
static const char VMX_EXIT_EPT_VIOLATION_STR[] = "VMX_EXIT_EPT_VIOLATION";
static const char VMX_EXIT_EPT_CONFIG_STR[] = "VMX_EXIT_EPT_CONFIG";
static const char VMX_EXIT_INVEPT_STR[] = "VMX_EXIT_INVEPT";
static const char VMX_EXIT_RDTSCP_STR[] = "VMX_EXIT_RDTSCP";
static const char VMX_EXIT_EXPIRED_PREEMPT_TIMER_STR[] = "VMX_EXIT_EXPIRED_PREEMPT_TIMER";
static const char VMX_EXIT_INVVPID_STR[] = "VMX_EXIT_INVVPID";
static const char VMX_EXIT_WBINVD_STR[] = "VMX_EXIT_WBINVD";
static const char VMX_EXIT_XSETBV_STR[] = "VMX_EXIT_XSETBV";

const char * v3_vmx_exit_code_to_str(vmx_exit_t exit)
{
    switch (exit) {
        case VMX_EXIT_INFO_EXCEPTION_OR_NMI:
            return VMX_EXIT_INFO_EXCEPTION_OR_NMI_STR;
        case VMX_EXIT_EXTERNAL_INTR:
            return VMX_EXIT_EXTERNAL_INTR_STR;
        case VMX_EXIT_TRIPLE_FAULT:
            return VMX_EXIT_TRIPLE_FAULT_STR;
        case VMX_EXIT_INIT_SIGNAL:
            return VMX_EXIT_INIT_SIGNAL_STR;
        case VMX_EXIT_STARTUP_IPI:
            return VMX_EXIT_STARTUP_IPI_STR;
        case VMX_EXIT_IO_SMI:
            return VMX_EXIT_IO_SMI_STR;
        case VMX_EXIT_OTHER_SMI:
            return VMX_EXIT_OTHER_SMI_STR;
        case VMX_EXIT_INTR_WINDOW:
            return VMX_EXIT_INTR_WINDOW_STR;
        case VMX_EXIT_NMI_WINDOW:
            return VMX_EXIT_NMI_WINDOW_STR;
        case VMX_EXIT_TASK_SWITCH:
            return VMX_EXIT_TASK_SWITCH_STR;
        case VMX_EXIT_CPUID:
            return VMX_EXIT_CPUID_STR;
        case VMX_EXIT_HLT:
            return VMX_EXIT_HLT_STR;
        case VMX_EXIT_INVD:
            return VMX_EXIT_INVD_STR;
        case VMX_EXIT_INVLPG:
            return VMX_EXIT_INVLPG_STR;
        case VMX_EXIT_RDPMC:
            return VMX_EXIT_RDPMC_STR;
        case VMX_EXIT_RDTSC:
            return VMX_EXIT_RDTSC_STR;
        case VMX_EXIT_RSM:
            return VMX_EXIT_RSM_STR;
        case VMX_EXIT_VMCALL:
            return VMX_EXIT_VMCALL_STR;
        case VMX_EXIT_VMCLEAR:
            return VMX_EXIT_VMCLEAR_STR;
        case VMX_EXIT_VMLAUNCH:
            return VMX_EXIT_VMLAUNCH_STR;
        case VMX_EXIT_VMPTRLD:
            return VMX_EXIT_VMPTRLD_STR;
        case VMX_EXIT_VMPTRST:
            return VMX_EXIT_VMPTRST_STR;
        case VMX_EXIT_VMREAD:
            return VMX_EXIT_VMREAD_STR;
        case VMX_EXIT_VMRESUME:
            return VMX_EXIT_VMRESUME_STR;
        case VMX_EXIT_VMWRITE:
            return VMX_EXIT_VMWRITE_STR;
        case VMX_EXIT_VMXOFF:
            return VMX_EXIT_VMXOFF_STR;
        case VMX_EXIT_VMXON:
            return VMX_EXIT_VMXON_STR;
        case VMX_EXIT_CR_REG_ACCESSES:
            return VMX_EXIT_CR_REG_ACCESSES_STR;
        case VMX_EXIT_MOV_DR:
            return VMX_EXIT_MOV_DR_STR;
        case VMX_EXIT_IO_INSTR:
            return VMX_EXIT_IO_INSTR_STR;
        case VMX_EXIT_RDMSR:
            return VMX_EXIT_RDMSR_STR;
        case VMX_EXIT_WRMSR:
            return VMX_EXIT_WRMSR_STR;
        case VMX_EXIT_INVALID_GUEST_STATE:
            return VMX_EXIT_INVALID_GUEST_STATE_STR;
        case VMX_EXIT_INVALID_MSR_LOAD:
            return VMX_EXIT_INVALID_MSR_LOAD_STR;
        case VMX_EXIT_MWAIT:
            return VMX_EXIT_MWAIT_STR;
        case VMX_EXIT_MONITOR:
            return VMX_EXIT_MONITOR_STR;
        case VMX_EXIT_PAUSE:
            return VMX_EXIT_PAUSE_STR;
        case VMX_EXIT_INVALID_MACHINE_CHECK:
            return VMX_EXIT_INVALID_MACHINE_CHECK_STR;
        case VMX_EXIT_TPR_BELOW_THRESHOLD:
            return VMX_EXIT_TPR_BELOW_THRESHOLD_STR;
        case VMX_EXIT_APIC:
            return VMX_EXIT_APIC_STR;
        case VMX_EXIT_GDTR_IDTR:
            return VMX_EXIT_GDTR_IDTR_STR;
        case VMX_EXIT_LDTR_TR:
            return VMX_EXIT_LDTR_TR_STR;
        case VMX_EXIT_EPT_VIOLATION:
            return VMX_EXIT_EPT_VIOLATION_STR;
        case VMX_EXIT_EPT_CONFIG:
            return VMX_EXIT_EPT_CONFIG_STR;
        case VMX_EXIT_INVEPT:
            return VMX_EXIT_INVEPT_STR;
        case VMX_EXIT_RDTSCP:
            return VMX_EXIT_RDTSCP_STR;
        case VMX_EXIT_EXPIRED_PREEMPT_TIMER:
            return VMX_EXIT_EXPIRED_PREEMPT_TIMER_STR;
        case VMX_EXIT_INVVPID:
            return VMX_EXIT_INVVPID_STR;
        case VMX_EXIT_WBINVD:
            return VMX_EXIT_WBINVD_STR;
        case VMX_EXIT_XSETBV:
            return VMX_EXIT_XSETBV_STR;
    }
    return NULL;
}

