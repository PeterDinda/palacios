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


#include <palacios/svm_handler.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/svm_io.h>
#include <palacios/vmm_halt.h>
#include <palacios/svm_pause.h>
#include <palacios/svm_wbinvd.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_emulator.h>
#include <palacios/svm_msr.h>
#include <palacios/vmm_hypercall.h>
#include <palacios/vmm_cpuid.h>
#include <palacios/vmm_direct_paging.h>

#ifndef V3_CONFIG_DEBUG_SVM
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#ifdef V3_CONFIG_TELEMETRY
#include <palacios/vmm_telemetry.h>
#endif

#ifdef V3_CONFIG_EXT_SW_INTERRUPTS
#include <gears/sw_intr.h>
#endif

int v3_handle_svm_exit(struct guest_info * info, addr_t exit_code, addr_t exit_info1, addr_t exit_info2) {

#ifdef V3_CONFIG_TELEMETRY
    if (info->vm_info->enable_telemetry) {
	v3_telemetry_start_exit(info);
    }
#endif



    //    PrintDebug("SVM Returned: Exit Code: %p\n", (void *)exit_code); 

    switch (exit_code) {
	case SVM_EXIT_IOIO: {
	    struct svm_io_info * io_info = (struct svm_io_info *)&(exit_info1);

	    if (io_info->type == 0) {
		if (io_info->str) {

		    if (v3_handle_svm_io_outs(info, io_info) == -1 ) {
			return -1;
		    }
		} else {
		    if (v3_handle_svm_io_out(info, io_info) == -1) {
			return -1;
		    }
		}

	    } else {

		if (io_info->str) {
		    if (v3_handle_svm_io_ins(info, io_info) == -1) {
			return -1;
		    }
		} else {
		    if (v3_handle_svm_io_in(info, io_info) == -1) {
			return -1;
		    }
		}
	    }

	    info->rip = exit_info2;

	    break;
	}
	case SVM_EXIT_MSR:

	    if (exit_info1 == 0) {
		if (v3_handle_msr_read(info) == -1) {
		    return -1;
		}
	    } else if (exit_info1 == 1) {
		if (v3_handle_msr_write(info) == -1) {
		    return -1;
		}
	    } else {
		PrintError("Invalid MSR Operation\n");
		return -1;
	    }
		
	    break;

	case SVM_EXIT_CPUID:
	    if (v3_handle_cpuid(info) == -1) {
		PrintError("Error handling CPUID\n");
		return -1;
	    }

	    break;
	case SVM_EXIT_CR0_WRITE: 
#ifdef V3_CONFIG_DEBUG_CTRL_REGS
	    PrintDebug("CR0 Write\n");
#endif
	    if (v3_handle_cr0_write(info) == -1) {
		return -1;
	    }
	    break;
	case SVM_EXIT_CR0_READ: 
#ifdef V3_CONFIG_DEBUG_CTRL_REGS
	    PrintDebug("CR0 Read\n");
#endif
	    if (v3_handle_cr0_read(info) == -1) {
		return -1;
	    }
	    break;
	case SVM_EXIT_CR3_WRITE: 
#ifdef V3_CONFIG_DEBUG_CTRL_REGS
	    PrintDebug("CR3 Write\n");
#endif
	    if (v3_handle_cr3_write(info) == -1) {
		return -1;
	    }    

	    break;
	case  SVM_EXIT_CR3_READ: 
#ifdef V3_CONFIG_DEBUG_CTRL_REGS
	    PrintDebug("CR3 Read\n");
#endif
	    if (v3_handle_cr3_read(info) == -1) {
		return -1;
	    }
	    break;
	case SVM_EXIT_CR4_WRITE: 
#ifdef V3_CONFIG_DEBUG_CTRL_REGS
	    PrintDebug("CR4 Write\n");
#endif
	    if (v3_handle_cr4_write(info) == -1) {
		return -1;
	    }    
	    break;
	case  SVM_EXIT_CR4_READ: 
#ifdef V3_CONFIG_DEBUG_CTRL_REGS
	    PrintDebug("CR4 Read\n");
#endif
	    if (v3_handle_cr4_read(info) == -1) {
		return -1;
	    }
	    break;
	case SVM_EXIT_EXCP14: {
	    addr_t fault_addr = exit_info2;
	    pf_error_t * error_code = (pf_error_t *)&(exit_info1);
#ifdef V3_CONFIG_DEBUG_SHADOW_PAGING
	    PrintDebug("PageFault at %p (error=%d)\n", 
		       (void *)fault_addr, *(uint_t *)error_code);
#endif
	    if (info->shdw_pg_mode == SHADOW_PAGING) {
		if (v3_handle_shadow_pagefault(info, fault_addr, *error_code) == -1) {
		    return -1;
		}
	    } else {
		PrintError("Page fault in un implemented paging mode\n");
		return -1;
	    }
	    break;
	} 
 	case SVM_EXIT_NPF: {
	    addr_t fault_addr = exit_info2;
	    pf_error_t * error_code = (pf_error_t *)&(exit_info1);

	    if (info->shdw_pg_mode == NESTED_PAGING) {
		if (v3_handle_nested_pagefault(info, fault_addr, *error_code) == -1) {
		    return -1;
		}
	    } else {
		PrintError("Currently unhandled Nested Page Fault\n");
		return -1;
		    }
	    break;
	    }
	case SVM_EXIT_INVLPG: 
	    if (info->shdw_pg_mode == SHADOW_PAGING) {
#ifdef V3_CONFIG_DEBUG_SHADOW_PAGING
		PrintDebug("Invlpg\n");
#endif
		if (v3_handle_shadow_invlpg(info) == -1) {
		    return -1;
		}
	    }
	    break;    
	case SVM_EXIT_VMMCALL: 
	    /* 
	     * Hypercall 
	     */

	    // VMMCALL is a 3 byte op
	    // We do this early because some hypercalls can change the rip...
	    info->rip += 3;	    

	    if (v3_handle_hypercall(info) == -1) {
		PrintError("Error handling Hypercall\n");
		return -1;
	    }

	    break;	
	case SVM_EXIT_NMI:
	    // handled by interrupt dispatcher
	    break;
	case SVM_EXIT_INTR:
	    // handled by interrupt dispatch earlier
	    break;
	case SVM_EXIT_SMI:
	    //   handle_svm_smi(info); // ignored for now
	    break;
	case SVM_EXIT_HLT:
#ifdef V3_CONFIG_DEBUG_HALT
	    PrintDebug("Guest halted\n");
#endif
	    if (v3_handle_halt(info) == -1) {
		return -1;
	    }
	    break;
	case SVM_EXIT_PAUSE:
	    //	    PrintDebug("Guest paused\n");
	    if (v3_handle_svm_pause(info) == -1) { 
		return -1;
	    }
	    break;
	case SVM_EXIT_WBINVD:   
#ifdef V3_CONFIG_DEBUG_EMULATOR
	    PrintDebug("WBINVD\n");
#endif
	    if (v3_handle_svm_wbinvd(info) == -1) { 
		return -1;
	    }
	    break;
        case SVM_EXIT_RDTSC:
#ifdef V3_CONFIG_DEBUG_TIME
	    PrintDebug("RDTSC/RDTSCP\n");
#endif 
	    if (v3_dispatch_exit_hook(info, V3_EXIT_RDTSC, NULL) == -1) {
		PrintError("Error Handling RDTSC instruction\n");
		return -1;
	    }
	    break;
        case SVM_EXIT_RDTSCP:
#ifdef V3_CONFIG_DEBUG_TIME
	    PrintDebug("RDTSCP\n");
#endif 
	    if (v3_dispatch_exit_hook(info, V3_EXIT_RDTSCP, NULL) == -1) {
		PrintError("Error handling RDTSCP instruction\n");
		return -1;
	    }
	    
	    break;
	case SVM_EXIT_SHUTDOWN:
	    PrintDebug("Guest-initiated shutdown\n");

	    info->vm_info->run_state = VM_STOPPED;

	    // Force exit on other cores

	    break;
#ifdef V3_CONFIG_EXT_SW_INTERRUPTS
    case SVM_EXIT_SWINT:
#ifdef V3_CONFIG_DEBUG_EXT_SW_INTERRUPTS
        PrintDebug("Intercepted a software interrupt\n");
#endif
        if (v3_handle_swintr(info) == -1) {
            PrintError("Error handling software interrupt\n");
            return -1;
        }
        break;
#endif


	    /* Exits Following this line are NOT HANDLED */
	    /*=======================================================================*/
	    
	default: {

	    addr_t rip_addr;
	    
	    PrintError("Unhandled SVM Exit: %s\n", v3_svm_exit_code_to_str(exit_code));
	    
	    rip_addr = get_addr_linear(info, info->rip, &(info->segments.cs));
	    
	    
	    PrintError("SVM Returned:(VMCB=%p)\n", (void *)(info->vmm_data)); 
	    PrintError("RIP: %p\n", (void *)(addr_t)(info->rip));
	    PrintError("RIP Linear: %p\n", (void *)(addr_t)(rip_addr));
	    
	    PrintError("SVM Returned: Exit Code: %p\n", (void *)(addr_t)exit_code); 
	    
	    PrintError("io_info1 low = 0x%.8x\n", *(uint_t*)&(exit_info1));
	    PrintError("io_info1 high = 0x%.8x\n", *(uint_t *)(((uint8_t *)&(exit_info1)) + 4));
	    
	    PrintError("io_info2 low = 0x%.8x\n", *(uint_t*)&(exit_info2));
	    PrintError("io_info2 high = 0x%.8x\n", *(uint_t *)(((uint8_t *)&(exit_info2)) + 4));
	    
	    
	    if (info->shdw_pg_mode == SHADOW_PAGING) {
		//	PrintHostPageTables(info, info->ctrl_regs.cr3);
		//PrintGuestPageTables(info, info->shdw_pg_state.guest_cr3);
	    }
	    
	    return -1;	   	    
	}
    }
    // END OF SWITCH (EXIT_CODE)

#ifdef V3_CONFIG_TELEMETRY
    if (info->vm_info->enable_telemetry) {
	v3_telemetry_end_exit(info, exit_code);
    }
#endif


    if (exit_code == SVM_EXIT_INTR) {
	//PrintDebug("INTR ret IP = %x\n", guest_state->rip);
    }
    
    return 0;
}


static const char SVM_EXIT_CR0_READ_STR[] = "SVM_EXIT_CR0_READ";
static const char SVM_EXIT_CR1_READ_STR[] = "SVM_EXIT_CR1_READ";
static const char SVM_EXIT_CR2_READ_STR[] = "SVM_EXIT_CR2_READ";
static const char SVM_EXIT_CR3_READ_STR[] = "SVM_EXIT_CR3_READ";
static const char SVM_EXIT_CR4_READ_STR[] = "SVM_EXIT_CR4_READ";
static const char SVM_EXIT_CR5_READ_STR[] = "SVM_EXIT_CR5_READ";
static const char SVM_EXIT_CR6_READ_STR[] = "SVM_EXIT_CR6_READ";
static const char SVM_EXIT_CR7_READ_STR[] = "SVM_EXIT_CR7_READ";
static const char SVM_EXIT_CR8_READ_STR[] = "SVM_EXIT_CR8_READ";
static const char SVM_EXIT_CR9_READ_STR[] = "SVM_EXIT_CR9_READ";
static const char SVM_EXIT_CR10_READ_STR[] = "SVM_EXIT_CR10_READ";
static const char SVM_EXIT_CR11_READ_STR[] = "SVM_EXIT_CR11_READ";
static const char SVM_EXIT_CR12_READ_STR[] = "SVM_EXIT_CR12_READ";
static const char SVM_EXIT_CR13_READ_STR[] = "SVM_EXIT_CR13_READ";
static const char SVM_EXIT_CR14_READ_STR[] = "SVM_EXIT_CR14_READ";
static const char SVM_EXIT_CR15_READ_STR[] = "SVM_EXIT_CR15_READ";
static const char SVM_EXIT_CR0_WRITE_STR[] = "SVM_EXIT_CR0_WRITE";
static const char SVM_EXIT_CR1_WRITE_STR[] = "SVM_EXIT_CR1_WRITE";
static const char SVM_EXIT_CR2_WRITE_STR[] = "SVM_EXIT_CR2_WRITE";
static const char SVM_EXIT_CR3_WRITE_STR[] = "SVM_EXIT_CR3_WRITE";
static const char SVM_EXIT_CR4_WRITE_STR[] = "SVM_EXIT_CR4_WRITE";
static const char SVM_EXIT_CR5_WRITE_STR[] = "SVM_EXIT_CR5_WRITE";
static const char SVM_EXIT_CR6_WRITE_STR[] = "SVM_EXIT_CR6_WRITE";
static const char SVM_EXIT_CR7_WRITE_STR[] = "SVM_EXIT_CR7_WRITE";
static const char SVM_EXIT_CR8_WRITE_STR[] = "SVM_EXIT_CR8_WRITE";
static const char SVM_EXIT_CR9_WRITE_STR[] = "SVM_EXIT_CR9_WRITE";
static const char SVM_EXIT_CR10_WRITE_STR[] = "SVM_EXIT_CR10_WRITE";
static const char SVM_EXIT_CR11_WRITE_STR[] = "SVM_EXIT_CR11_WRITE";
static const char SVM_EXIT_CR12_WRITE_STR[] = "SVM_EXIT_CR12_WRITE";
static const char SVM_EXIT_CR13_WRITE_STR[] = "SVM_EXIT_CR13_WRITE";
static const char SVM_EXIT_CR14_WRITE_STR[] = "SVM_EXIT_CR14_WRITE";
static const char SVM_EXIT_CR15_WRITE_STR[] = "SVM_EXIT_CR15_WRITE";
static const char SVM_EXIT_DR0_READ_STR[] = "SVM_EXIT_DR0_READ";
static const char SVM_EXIT_DR1_READ_STR[] = "SVM_EXIT_DR1_READ";
static const char SVM_EXIT_DR2_READ_STR[] = "SVM_EXIT_DR2_READ";
static const char SVM_EXIT_DR3_READ_STR[] = "SVM_EXIT_DR3_READ";
static const char SVM_EXIT_DR4_READ_STR[] = "SVM_EXIT_DR4_READ";
static const char SVM_EXIT_DR5_READ_STR[] = "SVM_EXIT_DR5_READ";
static const char SVM_EXIT_DR6_READ_STR[] = "SVM_EXIT_DR6_READ";
static const char SVM_EXIT_DR7_READ_STR[] = "SVM_EXIT_DR7_READ";
static const char SVM_EXIT_DR8_READ_STR[] = "SVM_EXIT_DR8_READ";
static const char SVM_EXIT_DR9_READ_STR[] = "SVM_EXIT_DR9_READ";
static const char SVM_EXIT_DR10_READ_STR[] = "SVM_EXIT_DR10_READ";
static const char SVM_EXIT_DR11_READ_STR[] = "SVM_EXIT_DR11_READ";
static const char SVM_EXIT_DR12_READ_STR[] = "SVM_EXIT_DR12_READ";
static const char SVM_EXIT_DR13_READ_STR[] = "SVM_EXIT_DR13_READ";
static const char SVM_EXIT_DR14_READ_STR[] = "SVM_EXIT_DR14_READ";
static const char SVM_EXIT_DR15_READ_STR[] = "SVM_EXIT_DR15_READ";
static const char SVM_EXIT_DR0_WRITE_STR[] = "SVM_EXIT_DR0_WRITE";
static const char SVM_EXIT_DR1_WRITE_STR[] = "SVM_EXIT_DR1_WRITE";
static const char SVM_EXIT_DR2_WRITE_STR[] = "SVM_EXIT_DR2_WRITE";
static const char SVM_EXIT_DR3_WRITE_STR[] = "SVM_EXIT_DR3_WRITE";
static const char SVM_EXIT_DR4_WRITE_STR[] = "SVM_EXIT_DR4_WRITE";
static const char SVM_EXIT_DR5_WRITE_STR[] = "SVM_EXIT_DR5_WRITE";
static const char SVM_EXIT_DR6_WRITE_STR[] = "SVM_EXIT_DR6_WRITE";
static const char SVM_EXIT_DR7_WRITE_STR[] = "SVM_EXIT_DR7_WRITE";
static const char SVM_EXIT_DR8_WRITE_STR[] = "SVM_EXIT_DR8_WRITE";
static const char SVM_EXIT_DR9_WRITE_STR[] = "SVM_EXIT_DR9_WRITE";
static const char SVM_EXIT_DR10_WRITE_STR[] = "SVM_EXIT_DR10_WRITE";
static const char SVM_EXIT_DR11_WRITE_STR[] = "SVM_EXIT_DR11_WRITE";
static const char SVM_EXIT_DR12_WRITE_STR[] = "SVM_EXIT_DR12_WRITE";
static const char SVM_EXIT_DR13_WRITE_STR[] = "SVM_EXIT_DR13_WRITE";
static const char SVM_EXIT_DR14_WRITE_STR[] = "SVM_EXIT_DR14_WRITE";
static const char SVM_EXIT_DR15_WRITE_STR[] = "SVM_EXIT_DR15_WRITE";
static const char SVM_EXIT_EXCP0_STR[] = "SVM_EXIT_EXCP0";
static const char SVM_EXIT_EXCP1_STR[] = "SVM_EXIT_EXCP1";
static const char SVM_EXIT_EXCP2_STR[] = "SVM_EXIT_EXCP2";
static const char SVM_EXIT_EXCP3_STR[] = "SVM_EXIT_EXCP3";
static const char SVM_EXIT_EXCP4_STR[] = "SVM_EXIT_EXCP4";
static const char SVM_EXIT_EXCP5_STR[] = "SVM_EXIT_EXCP5";
static const char SVM_EXIT_EXCP6_STR[] = "SVM_EXIT_EXCP6";
static const char SVM_EXIT_EXCP7_STR[] = "SVM_EXIT_EXCP7";
static const char SVM_EXIT_EXCP8_STR[] = "SVM_EXIT_EXCP8";
static const char SVM_EXIT_EXCP9_STR[] = "SVM_EXIT_EXCP9";
static const char SVM_EXIT_EXCP10_STR[] = "SVM_EXIT_EXCP10";
static const char SVM_EXIT_EXCP11_STR[] = "SVM_EXIT_EXCP11";
static const char SVM_EXIT_EXCP12_STR[] = "SVM_EXIT_EXCP12";
static const char SVM_EXIT_EXCP13_STR[] = "SVM_EXIT_EXCP13";
static const char SVM_EXIT_EXCP14_STR[] = "SVM_EXIT_EXCP14";
static const char SVM_EXIT_EXCP15_STR[] = "SVM_EXIT_EXCP15";
static const char SVM_EXIT_EXCP16_STR[] = "SVM_EXIT_EXCP16";
static const char SVM_EXIT_EXCP17_STR[] = "SVM_EXIT_EXCP17";
static const char SVM_EXIT_EXCP18_STR[] = "SVM_EXIT_EXCP18";
static const char SVM_EXIT_EXCP19_STR[] = "SVM_EXIT_EXCP19";
static const char SVM_EXIT_EXCP20_STR[] = "SVM_EXIT_EXCP20";
static const char SVM_EXIT_EXCP21_STR[] = "SVM_EXIT_EXCP21";
static const char SVM_EXIT_EXCP22_STR[] = "SVM_EXIT_EXCP22";
static const char SVM_EXIT_EXCP23_STR[] = "SVM_EXIT_EXCP23";
static const char SVM_EXIT_EXCP24_STR[] = "SVM_EXIT_EXCP24";
static const char SVM_EXIT_EXCP25_STR[] = "SVM_EXIT_EXCP25";
static const char SVM_EXIT_EXCP26_STR[] = "SVM_EXIT_EXCP26";
static const char SVM_EXIT_EXCP27_STR[] = "SVM_EXIT_EXCP27";
static const char SVM_EXIT_EXCP28_STR[] = "SVM_EXIT_EXCP28";
static const char SVM_EXIT_EXCP29_STR[] = "SVM_EXIT_EXCP29";
static const char SVM_EXIT_EXCP30_STR[] = "SVM_EXIT_EXCP30";
static const char SVM_EXIT_EXCP31_STR[] = "SVM_EXIT_EXCP31";
static const char SVM_EXIT_INTR_STR[] = "SVM_EXIT_INTR";
static const char SVM_EXIT_NMI_STR[] = "SVM_EXIT_NMI";
static const char SVM_EXIT_SMI_STR[] = "SVM_EXIT_SMI";
static const char SVM_EXIT_INIT_STR[] = "SVM_EXIT_INIT";
static const char SVM_EXIT_VINITR_STR[] = "SVM_EXIT_VINITR";
static const char SVM_EXIT_CR0_SEL_WRITE_STR[] = "SVM_EXIT_CR0_SEL_WRITE";
static const char SVM_EXIT_IDTR_READ_STR[] = "SVM_EXIT_IDTR_READ";
static const char SVM_EXIT_GDTR_READ_STR[] = "SVM_EXIT_GDTR_READ";
static const char SVM_EXIT_LDTR_READ_STR[] = "SVM_EXIT_LDTR_READ";
static const char SVM_EXIT_TR_READ_STR[] = "SVM_EXIT_TR_READ";
static const char SVM_EXIT_IDTR_WRITE_STR[] = "SVM_EXIT_IDTR_WRITE";
static const char SVM_EXIT_GDTR_WRITE_STR[] = "SVM_EXIT_GDTR_WRITE";
static const char SVM_EXIT_LDTR_WRITE_STR[] = "SVM_EXIT_LDTR_WRITE";
static const char SVM_EXIT_TR_WRITE_STR[] = "SVM_EXIT_TR_WRITE";
static const char SVM_EXIT_RDTSC_STR[] = "SVM_EXIT_RDTSC";
static const char SVM_EXIT_RDPMC_STR[] = "SVM_EXIT_RDPMC";
static const char SVM_EXIT_PUSHF_STR[] = "SVM_EXIT_PUSHF";
static const char SVM_EXIT_POPF_STR[] = "SVM_EXIT_POPF";
static const char SVM_EXIT_CPUID_STR[] = "SVM_EXIT_CPUID";
static const char SVM_EXIT_RSM_STR[] = "SVM_EXIT_RSM";
static const char SVM_EXIT_IRET_STR[] = "SVM_EXIT_IRET";
static const char SVM_EXIT_SWINT_STR[] = "SVM_EXIT_SWINT";
static const char SVM_EXIT_INVD_STR[] = "SVM_EXIT_INVD";
static const char SVM_EXIT_PAUSE_STR[] = "SVM_EXIT_PAUSE";
static const char SVM_EXIT_HLT_STR[] = "SVM_EXIT_HLT";
static const char SVM_EXIT_INVLPG_STR[] = "SVM_EXIT_INVLPG";
static const char SVM_EXIT_INVLPGA_STR[] = "SVM_EXIT_INVLPGA";
static const char SVM_EXIT_IOIO_STR[] = "SVM_EXIT_IOIO";
static const char SVM_EXIT_MSR_STR[] = "SVM_EXIT_MSR";
static const char SVM_EXIT_TASK_SWITCH_STR[] = "SVM_EXIT_TASK_SWITCH";
static const char SVM_EXIT_FERR_FREEZE_STR[] = "SVM_EXIT_FERR_FREEZE";
static const char SVM_EXIT_SHUTDOWN_STR[] = "SVM_EXIT_SHUTDOWN";
static const char SVM_EXIT_VMRUN_STR[] = "SVM_EXIT_VMRUN";
static const char SVM_EXIT_VMMCALL_STR[] = "SVM_EXIT_VMMCALL";
static const char SVM_EXIT_VMLOAD_STR[] = "SVM_EXIT_VMLOAD";
static const char SVM_EXIT_VMSAVE_STR[] = "SVM_EXIT_VMSAVE";
static const char SVM_EXIT_STGI_STR[] = "SVM_EXIT_STGI";
static const char SVM_EXIT_CLGI_STR[] = "SVM_EXIT_CLGI";
static const char SVM_EXIT_SKINIT_STR[] = "SVM_EXIT_SKINIT";
static const char SVM_EXIT_RDTSCP_STR[] = "SVM_EXIT_RDTSCP";
static const char SVM_EXIT_ICEBP_STR[] = "SVM_EXIT_ICEBP";
static const char SVM_EXIT_WBINVD_STR[] = "SVM_EXIT_WBINVD";
static const char SVM_EXIT_MONITOR_STR[] = "SVM_EXIT_MONITOR";
static const char SVM_EXIT_MWAIT_STR[] = "SVM_EXIT_MWAIT";
static const char SVM_EXIT_MWAIT_CONDITIONAL_STR[] = "SVM_EXIT_MWAIT_CONDITIONAL";
static const char SVM_EXIT_NPF_STR[] = "SVM_EXIT_NPF";
static const char SVM_EXIT_INVALID_VMCB_STR[] = "SVM_EXIT_INVALID_VMCB";



const char * v3_svm_exit_code_to_str(uint_t exit_code) {
    switch(exit_code) {
	case SVM_EXIT_CR0_READ:
	    return SVM_EXIT_CR0_READ_STR;
	case SVM_EXIT_CR1_READ:
	    return SVM_EXIT_CR1_READ_STR;
	case SVM_EXIT_CR2_READ:
	    return SVM_EXIT_CR2_READ_STR;
	case SVM_EXIT_CR3_READ:
	    return SVM_EXIT_CR3_READ_STR;
	case SVM_EXIT_CR4_READ:
	    return SVM_EXIT_CR4_READ_STR;
	case SVM_EXIT_CR5_READ:
	    return SVM_EXIT_CR5_READ_STR;
	case SVM_EXIT_CR6_READ:
	    return SVM_EXIT_CR6_READ_STR;
	case SVM_EXIT_CR7_READ:
	    return SVM_EXIT_CR7_READ_STR;
	case SVM_EXIT_CR8_READ:
	    return SVM_EXIT_CR8_READ_STR;
	case SVM_EXIT_CR9_READ:
	    return SVM_EXIT_CR9_READ_STR;
	case SVM_EXIT_CR10_READ:
	    return SVM_EXIT_CR10_READ_STR;
	case SVM_EXIT_CR11_READ:
	    return SVM_EXIT_CR11_READ_STR;
	case SVM_EXIT_CR12_READ:
	    return SVM_EXIT_CR12_READ_STR;
	case SVM_EXIT_CR13_READ:
	    return SVM_EXIT_CR13_READ_STR;
	case SVM_EXIT_CR14_READ:
	    return SVM_EXIT_CR14_READ_STR;
	case SVM_EXIT_CR15_READ:
	    return SVM_EXIT_CR15_READ_STR;
	case SVM_EXIT_CR0_WRITE:
	    return SVM_EXIT_CR0_WRITE_STR;
	case SVM_EXIT_CR1_WRITE:
	    return SVM_EXIT_CR1_WRITE_STR;
	case SVM_EXIT_CR2_WRITE:
	    return SVM_EXIT_CR2_WRITE_STR;
	case SVM_EXIT_CR3_WRITE:
	    return SVM_EXIT_CR3_WRITE_STR;
	case SVM_EXIT_CR4_WRITE:
	    return SVM_EXIT_CR4_WRITE_STR;
	case SVM_EXIT_CR5_WRITE:
	    return SVM_EXIT_CR5_WRITE_STR;
	case SVM_EXIT_CR6_WRITE:
	    return SVM_EXIT_CR6_WRITE_STR;
	case SVM_EXIT_CR7_WRITE:
	    return SVM_EXIT_CR7_WRITE_STR;
	case SVM_EXIT_CR8_WRITE:
	    return SVM_EXIT_CR8_WRITE_STR;
	case SVM_EXIT_CR9_WRITE:
	    return SVM_EXIT_CR9_WRITE_STR;
	case SVM_EXIT_CR10_WRITE:
	    return SVM_EXIT_CR10_WRITE_STR;
	case SVM_EXIT_CR11_WRITE:
	    return SVM_EXIT_CR11_WRITE_STR;
	case SVM_EXIT_CR12_WRITE:
	    return SVM_EXIT_CR12_WRITE_STR;
	case SVM_EXIT_CR13_WRITE:
	    return SVM_EXIT_CR13_WRITE_STR;
	case SVM_EXIT_CR14_WRITE:
	    return SVM_EXIT_CR14_WRITE_STR;
	case SVM_EXIT_CR15_WRITE:
	    return SVM_EXIT_CR15_WRITE_STR;
	case SVM_EXIT_DR0_READ:
	    return SVM_EXIT_DR0_READ_STR;
	case SVM_EXIT_DR1_READ:
	    return SVM_EXIT_DR1_READ_STR;
	case SVM_EXIT_DR2_READ:
	    return SVM_EXIT_DR2_READ_STR;
	case SVM_EXIT_DR3_READ:
	    return SVM_EXIT_DR3_READ_STR;
	case SVM_EXIT_DR4_READ:
	    return SVM_EXIT_DR4_READ_STR;
	case SVM_EXIT_DR5_READ:
	    return SVM_EXIT_DR5_READ_STR;
	case SVM_EXIT_DR6_READ:
	    return SVM_EXIT_DR6_READ_STR;
	case SVM_EXIT_DR7_READ:
	    return SVM_EXIT_DR7_READ_STR;
	case SVM_EXIT_DR8_READ:
	    return SVM_EXIT_DR8_READ_STR;
	case SVM_EXIT_DR9_READ:
	    return SVM_EXIT_DR9_READ_STR;
	case SVM_EXIT_DR10_READ:
	    return SVM_EXIT_DR10_READ_STR;
	case SVM_EXIT_DR11_READ:
	    return SVM_EXIT_DR11_READ_STR;
	case SVM_EXIT_DR12_READ:
	    return SVM_EXIT_DR12_READ_STR;
	case SVM_EXIT_DR13_READ:
	    return SVM_EXIT_DR13_READ_STR;
	case SVM_EXIT_DR14_READ:
	    return SVM_EXIT_DR14_READ_STR;
	case SVM_EXIT_DR15_READ:
	    return SVM_EXIT_DR15_READ_STR;
	case SVM_EXIT_DR0_WRITE:
	    return SVM_EXIT_DR0_WRITE_STR;
	case SVM_EXIT_DR1_WRITE:
	    return SVM_EXIT_DR1_WRITE_STR;
	case SVM_EXIT_DR2_WRITE:
	    return SVM_EXIT_DR2_WRITE_STR;
	case SVM_EXIT_DR3_WRITE:
	    return SVM_EXIT_DR3_WRITE_STR;
	case SVM_EXIT_DR4_WRITE:
	    return SVM_EXIT_DR4_WRITE_STR;
	case SVM_EXIT_DR5_WRITE:
	    return SVM_EXIT_DR5_WRITE_STR;
	case SVM_EXIT_DR6_WRITE:
	    return SVM_EXIT_DR6_WRITE_STR;
	case SVM_EXIT_DR7_WRITE:
	    return SVM_EXIT_DR7_WRITE_STR;
	case SVM_EXIT_DR8_WRITE:
	    return SVM_EXIT_DR8_WRITE_STR;
	case SVM_EXIT_DR9_WRITE:
	    return SVM_EXIT_DR9_WRITE_STR;
	case SVM_EXIT_DR10_WRITE:
	    return SVM_EXIT_DR10_WRITE_STR;
	case SVM_EXIT_DR11_WRITE:
	    return SVM_EXIT_DR11_WRITE_STR;
	case SVM_EXIT_DR12_WRITE:
	    return SVM_EXIT_DR12_WRITE_STR;
	case SVM_EXIT_DR13_WRITE:
	    return SVM_EXIT_DR13_WRITE_STR;
	case SVM_EXIT_DR14_WRITE:
	    return SVM_EXIT_DR14_WRITE_STR;
	case SVM_EXIT_DR15_WRITE:
	    return SVM_EXIT_DR15_WRITE_STR;
	case SVM_EXIT_EXCP0:
	    return SVM_EXIT_EXCP0_STR;
	case SVM_EXIT_EXCP1:
	    return SVM_EXIT_EXCP1_STR;
	case SVM_EXIT_EXCP2:
	    return SVM_EXIT_EXCP2_STR;
	case SVM_EXIT_EXCP3:
	    return SVM_EXIT_EXCP3_STR;
	case SVM_EXIT_EXCP4:
	    return SVM_EXIT_EXCP4_STR;
	case SVM_EXIT_EXCP5:
	    return SVM_EXIT_EXCP5_STR;
	case SVM_EXIT_EXCP6:
	    return SVM_EXIT_EXCP6_STR;
	case SVM_EXIT_EXCP7:
	    return SVM_EXIT_EXCP7_STR;
	case SVM_EXIT_EXCP8:
	    return SVM_EXIT_EXCP8_STR;
	case SVM_EXIT_EXCP9:
	    return SVM_EXIT_EXCP9_STR;
	case SVM_EXIT_EXCP10:
	    return SVM_EXIT_EXCP10_STR;
	case SVM_EXIT_EXCP11:
	    return SVM_EXIT_EXCP11_STR;
	case SVM_EXIT_EXCP12:
	    return SVM_EXIT_EXCP12_STR;
	case SVM_EXIT_EXCP13:
	    return SVM_EXIT_EXCP13_STR;
	case SVM_EXIT_EXCP14:
	    return SVM_EXIT_EXCP14_STR;
	case SVM_EXIT_EXCP15:
	    return SVM_EXIT_EXCP15_STR;
	case SVM_EXIT_EXCP16:
	    return SVM_EXIT_EXCP16_STR;
	case SVM_EXIT_EXCP17:
	    return SVM_EXIT_EXCP17_STR;
	case SVM_EXIT_EXCP18:
	    return SVM_EXIT_EXCP18_STR;
	case SVM_EXIT_EXCP19:
	    return SVM_EXIT_EXCP19_STR;
	case SVM_EXIT_EXCP20:
	    return SVM_EXIT_EXCP20_STR;
	case SVM_EXIT_EXCP21:
	    return SVM_EXIT_EXCP21_STR;
	case SVM_EXIT_EXCP22:
	    return SVM_EXIT_EXCP22_STR;
	case SVM_EXIT_EXCP23:
	    return SVM_EXIT_EXCP23_STR;
	case SVM_EXIT_EXCP24:
	    return SVM_EXIT_EXCP24_STR;
	case SVM_EXIT_EXCP25:
	    return SVM_EXIT_EXCP25_STR;
	case SVM_EXIT_EXCP26:
	    return SVM_EXIT_EXCP26_STR;
	case SVM_EXIT_EXCP27:
	    return SVM_EXIT_EXCP27_STR;
	case SVM_EXIT_EXCP28:
	    return SVM_EXIT_EXCP28_STR;
	case SVM_EXIT_EXCP29:
	    return SVM_EXIT_EXCP29_STR;
	case SVM_EXIT_EXCP30:
	    return SVM_EXIT_EXCP30_STR;
	case SVM_EXIT_EXCP31:
	    return SVM_EXIT_EXCP31_STR;
	case SVM_EXIT_INTR:
	    return SVM_EXIT_INTR_STR;
	case SVM_EXIT_NMI:
	    return SVM_EXIT_NMI_STR;
	case SVM_EXIT_SMI:
	    return SVM_EXIT_SMI_STR;
	case SVM_EXIT_INIT:
	    return SVM_EXIT_INIT_STR;
	case SVM_EXIT_VINITR:
	    return SVM_EXIT_VINITR_STR;
	case SVM_EXIT_CR0_SEL_WRITE:
	    return SVM_EXIT_CR0_SEL_WRITE_STR;
	case SVM_EXIT_IDTR_READ:
	    return SVM_EXIT_IDTR_READ_STR;
	case SVM_EXIT_GDTR_READ:
	    return SVM_EXIT_GDTR_READ_STR;
	case SVM_EXIT_LDTR_READ:
	    return SVM_EXIT_LDTR_READ_STR;
	case SVM_EXIT_TR_READ:
	    return SVM_EXIT_TR_READ_STR;
	case SVM_EXIT_IDTR_WRITE:
	    return SVM_EXIT_IDTR_WRITE_STR;
	case SVM_EXIT_GDTR_WRITE:
	    return SVM_EXIT_GDTR_WRITE_STR;
	case SVM_EXIT_LDTR_WRITE:
	    return SVM_EXIT_LDTR_WRITE_STR;
	case SVM_EXIT_TR_WRITE:
	    return SVM_EXIT_TR_WRITE_STR;
	case SVM_EXIT_RDTSC:
	    return SVM_EXIT_RDTSC_STR;
	case SVM_EXIT_RDPMC:
	    return SVM_EXIT_RDPMC_STR;
	case SVM_EXIT_PUSHF:
	    return SVM_EXIT_PUSHF_STR;
	case SVM_EXIT_POPF:
	    return SVM_EXIT_POPF_STR;
	case SVM_EXIT_CPUID:
	    return SVM_EXIT_CPUID_STR;
	case SVM_EXIT_RSM:
	    return SVM_EXIT_RSM_STR;
	case SVM_EXIT_IRET:
	    return SVM_EXIT_IRET_STR;
	case SVM_EXIT_SWINT:
	    return SVM_EXIT_SWINT_STR;
	case SVM_EXIT_INVD:
	    return SVM_EXIT_INVD_STR;
	case SVM_EXIT_PAUSE:
	    return SVM_EXIT_PAUSE_STR;
	case SVM_EXIT_HLT:
	    return SVM_EXIT_HLT_STR;
	case SVM_EXIT_INVLPG:
	    return SVM_EXIT_INVLPG_STR;
	case SVM_EXIT_INVLPGA:
	    return SVM_EXIT_INVLPGA_STR;
	case SVM_EXIT_IOIO:
	    return SVM_EXIT_IOIO_STR;
	case SVM_EXIT_MSR:
	    return SVM_EXIT_MSR_STR;
	case SVM_EXIT_TASK_SWITCH:
	    return SVM_EXIT_TASK_SWITCH_STR;
	case SVM_EXIT_FERR_FREEZE:
	    return SVM_EXIT_FERR_FREEZE_STR;
	case SVM_EXIT_SHUTDOWN:
	    return SVM_EXIT_SHUTDOWN_STR;
	case SVM_EXIT_VMRUN:
	    return SVM_EXIT_VMRUN_STR;
	case SVM_EXIT_VMMCALL:
	    return SVM_EXIT_VMMCALL_STR;
	case SVM_EXIT_VMLOAD:
	    return SVM_EXIT_VMLOAD_STR;
	case SVM_EXIT_VMSAVE:
	    return SVM_EXIT_VMSAVE_STR;
	case SVM_EXIT_STGI:
	    return SVM_EXIT_STGI_STR;
	case SVM_EXIT_CLGI:
	    return SVM_EXIT_CLGI_STR;
	case SVM_EXIT_SKINIT:
	    return SVM_EXIT_SKINIT_STR;
	case SVM_EXIT_RDTSCP:
	    return SVM_EXIT_RDTSCP_STR;
	case SVM_EXIT_ICEBP:
	    return SVM_EXIT_ICEBP_STR;
	case SVM_EXIT_WBINVD:
	    return SVM_EXIT_WBINVD_STR;
	case SVM_EXIT_MONITOR:
	    return SVM_EXIT_MONITOR_STR;
	case SVM_EXIT_MWAIT:
	    return SVM_EXIT_MWAIT_STR;
	case SVM_EXIT_MWAIT_CONDITIONAL:
	    return SVM_EXIT_MWAIT_CONDITIONAL_STR;
	case SVM_EXIT_NPF:
	    return SVM_EXIT_NPF_STR;
	case SVM_EXIT_INVALID_VMCB:
	    return SVM_EXIT_INVALID_VMCB_STR;
    }
    return NULL;
}
