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
      PrintError("Handling VMEXIT: %s (%u), %lu (0x%lx)\n", 
      v3_vmx_exit_code_to_str(exit_info->exit_reason),
      exit_info->exit_reason, 
      exit_info->exit_qual, exit_info->exit_qual);
      
      v3_print_vmcs();
    */


    if (basic_info->entry_error == 1) {
	switch (basic_info->reason) {
	    case VMEXIT_INVALID_GUEST_STATE:
		PrintError("VM Entry failed due to invalid guest state\n");
		PrintError("Printing VMCS: (NOTE: This VMCS may not belong to the correct guest)\n");
		v3_print_vmcs();
		break;
	    case VMEXIT_INVALID_MSR_LOAD:
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
        case VMEXIT_INFO_EXCEPTION_OR_NMI: {
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

	case VMEXIT_EPT_VIOLATION: {
	    struct ept_exit_qual * ept_qual = (struct ept_exit_qual *)&(exit_info->exit_qual);

	    if (v3_handle_ept_fault(info, exit_info->ept_fault_addr, ept_qual) == -1) {
		PrintError("Error handling EPT fault\n");
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

        case VMEXIT_RDTSC:
#ifdef V3_CONFIG_DEBUG_TIME
	    PrintDebug("RDTSC\n");
#endif 
	    if (v3_handle_rdtsc(info) == -1) {
		PrintError("Error Handling RDTSC instruction\n");
		return -1;
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
        case VMEXIT_CR_REG_ACCESSES: {
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
	    // This is handled in the atomic part of the vmx code,
	    // not in the generic (interruptable) vmx handler
            break;

	    
        default:
            PrintError("Unhandled VMEXIT: %s (%u), %lu (0x%lx)\n", 
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
static const char VMEXIT_INVALID_GUEST_STATE_STR[] = "VMEXIT_INVALID_GUEST_STATE";
static const char VMEXIT_INVALID_MSR_LOAD_STR[] = "VMEXIT_INVALID_MSR_LOAD";
static const char VMEXIT_MWAIT_STR[] = "VMEXIT_MWAIT";
static const char VMEXIT_MONITOR_STR[] = "VMEXIT_MONITOR";
static const char VMEXIT_PAUSE_STR[] = "VMEXIT_PAUSE";
static const char VMEXIT_INVALID_MACHINE_CHECK_STR[] = "VMEXIT_INVALIDE_MACHINE_CHECK";
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
        case VMEXIT_INVALID_GUEST_STATE:
            return VMEXIT_INVALID_GUEST_STATE_STR;
        case VMEXIT_INVALID_MSR_LOAD:
            return VMEXIT_INVALID_MSR_LOAD_STR;
        case VMEXIT_MWAIT:
            return VMEXIT_MWAIT_STR;
        case VMEXIT_MONITOR:
            return VMEXIT_MONITOR_STR;
        case VMEXIT_PAUSE:
            return VMEXIT_PAUSE_STR;
        case VMEXIT_INVALID_MACHINE_CHECK:
            return VMEXIT_INVALID_MACHINE_CHECK_STR;
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

