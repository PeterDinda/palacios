#include <palacios/svm_handler.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/svm_io.h>
#include <palacios/svm_halt.h>
#include <palacios/svm_pause.h>
#include <palacios/svm_wbinvd.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_emulator.h>

int handle_svm_exit(struct guest_info * info) {
  vmcb_ctrl_t * guest_ctrl = 0;
  vmcb_saved_state_t * guest_state = 0;
  ulong_t exit_code = 0;
  
  guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
  guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
  

  // Update the high level state 
  info->rip = guest_state->rip;
  info->vm_regs.rsp = guest_state->rsp;
  info->vm_regs.rax = guest_state->rax;

  info->cpl = guest_state->cpl;


  info->ctrl_regs.cr0 = guest_state->cr0;
  info->ctrl_regs.cr2 = guest_state->cr2;
  info->ctrl_regs.cr3 = guest_state->cr3;
  info->ctrl_regs.cr4 = guest_state->cr4;
  info->dbg_regs.dr6 = guest_state->dr6;
  info->dbg_regs.dr7 = guest_state->dr7;
  info->ctrl_regs.cr8 = guest_ctrl->guest_ctrl.V_TPR;
  info->ctrl_regs.rflags = guest_state->rflags;
  info->ctrl_regs.efer = guest_state->efer;

  get_vmcb_segments((vmcb_t*)(info->vmm_data), &(info->segments));
  info->cpu_mode = get_cpu_mode(info);
  info->mem_mode = get_mem_mode(info);


  exit_code = guest_ctrl->exit_code;
 

  // Disable printing io exits due to bochs debug messages
  //if (!((exit_code == VMEXIT_IOIO) && ((ushort_t)(guest_ctrl->exit_info1 >> 16) == 0x402))) {


  //  PrintDebug("SVM Returned: Exit Code: 0x%x \t\t(tsc=%ul)\n",exit_code, (uint_t)info->time_state.guest_tsc); 
  
  if ((0) && (exit_code < 0x4f)) {
    char instr[32];
    int ret;
    // Dump out the instr stream

    //PrintDebug("RIP: %x\n", guest_state->rip);
    PrintDebug("RIP Linear: %x\n", get_addr_linear(info, info->rip, &(info->segments.cs)));

    // OK, now we will read the instruction
    // The only difference between PROTECTED and PROTECTED_PG is whether we read
    // from guest_pa or guest_va
    if (info->mem_mode == PHYSICAL_MEM) { 
      // The real rip address is actually a combination of the rip + CS base 
      ret = read_guest_pa_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 32, instr);
    } else { 
      ret = read_guest_va_memory(info, get_addr_linear(info, info->rip, &(info->segments.cs)), 32, instr);
    }
    
    if (ret != 32) {
      // I think we should inject a GPF into the guest
      PrintDebug("Could not read instruction (ret=%d)\n", ret);
    } else {

      PrintDebug("Instr Stream:\n");
      PrintTraceMemDump(instr, 32);
    }
  }


    //  }
  // PrintDebugVMCB((vmcb_t*)(info->vmm_data));


  // PrintDebug("SVM Returned:(VMCB=%x)\n", info->vmm_data); 
  //PrintDebug("RIP: %x\n", guest_state->rip);

  
  //PrintDebug("SVM Returned: Exit Code: %x\n",exit_code); 

  switch (exit_code) {

  case VMEXIT_IOIO: {
    struct svm_io_info * io_info = (struct svm_io_info *)&(guest_ctrl->exit_info1);
    
    if (io_info->type == 0) {
      if (io_info->str) {
	if (handle_svm_io_outs(info) == -1 ) {
	  return -1;
	}
      } else {
	if (handle_svm_io_out(info) == -1) {
	  return -1;
	}
      }
    } else {
      if (io_info->str) {
	if (handle_svm_io_ins(info) == -1) {
	  return -1;
	}
      } else {
	if (handle_svm_io_in(info) == -1) {
	  return -1;
	}
      }
    }
  }
    break;


  case  VMEXIT_CR0_WRITE: {
#ifdef DEBUG_CTRL_REGS
    PrintDebug("CR0 Write\n");
#endif
    if (handle_cr0_write(info) == -1) {
      return -1;
    }
  } 
    break;

  case VMEXIT_CR0_READ: {
#ifdef DEBUG_CTRL_REGS
    PrintDebug("CR0 Read\n");
#endif
    if (handle_cr0_read(info) == -1) {
      return -1;
    }
  } 
    break;

  case VMEXIT_CR3_WRITE: {
#ifdef DEBUG_CTRL_REGS
    PrintDebug("CR3 Write\n");
#endif
    if (handle_cr3_write(info) == -1) {
      return -1;
    }    
  } 
    break;

  case  VMEXIT_CR3_READ: {
#ifdef DEBUG_CTRL_REGS
    PrintDebug("CR3 Read\n");
#endif
    if (handle_cr3_read(info) == -1) {
      return -1;
    }
  }
    break;

  case VMEXIT_EXCP14: {
    addr_t fault_addr = guest_ctrl->exit_info2;
    pf_error_t * error_code = (pf_error_t *)&(guest_ctrl->exit_info1);
#ifdef DEBUG_SHADOW_PAGING
    PrintDebug("PageFault at %x (error=%d)\n", fault_addr, *error_code);
#endif
    if (info->shdw_pg_mode == SHADOW_PAGING) {
      if (handle_shadow_pagefault(info, fault_addr, *error_code) == -1) {
	return -1;
      }
    } else {
      PrintError("Page fault in un implemented paging mode\n");
      return -1;
    }
  } 
    break;

  case VMEXIT_NPF: {
    PrintError("Currently unhandled Nested Page Fault\n");
    return -1;
    
  } 
    break;

  case VMEXIT_INVLPG: {
    if (info->shdw_pg_mode == SHADOW_PAGING) {
#ifdef DEBUG_SHADOW_PAGING
      PrintDebug("Invlpg\n");
#endif
      if (handle_shadow_invlpg(info) == -1) {
	return -1;
      }
    }
   
    /*
      (exit_code == VMEXIT_INVLPGA)   || 
    */
    
  } 
    break;

  case VMEXIT_INTR: { 
    
    //    handle_svm_intr(info); // handled by interrupt dispatch earlier

  } 
    break;
    
  case VMEXIT_SMI: {
    
    //   handle_svm_smi(info); // ignored for now

  } 
    break;

  case VMEXIT_HLT: {
    PrintDebug("Guest halted\n");
    if (handle_svm_halt(info) == -1) {
      return -1;
    }
  } 
    break;

  case VMEXIT_PAUSE: {
    PrintDebug("Guest paused\n");
    if (handle_svm_pause(info) == -1) { 
      return -1;
    }
  } 
    break;

  case VMEXIT_EXCP1: {
#ifdef DEBUG_EMULATOR
    PrintDebug("DEBUG EXCEPTION\n");
#endif
    if (info->run_state == VM_EMULATING) {
      if (v3_emulation_exit_handler(info) == -1) {
	return -1;
      }
    } else {
      PrintError("VMMCALL with not emulator...\n");
      return -1;
    }
  } 
    break;

  case VMEXIT_VMMCALL: {
#ifdef DEBUG_EMULATOR
    PrintDebug("VMMCALL\n");
#endif
    if (info->run_state == VM_EMULATING) {
      if (v3_emulation_exit_handler(info) == -1) {
	return -1;
      }
    } else {
      PrintError("VMMCALL with not emulator...\n");
      return -1;
    }
    
  } 
    break;


  case VMEXIT_WBINVD: {
#ifdef DEBUG_EMULATOR
    PrintDebug("WBINVD\n");
#endif
    if (!handle_svm_wbinvd(info)) { 
      return -1;
    }
  }
    break;



    /* Exits Following this line are NOT HANDLED */
    /*=======================================================================*/

  case VMEXIT_CR_READ_MASK:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR_READ_MASK\n"); 
    goto unhandled_exit;
    break;
    
  case VMEXIT_CR_WRITE_MASK:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR_WRITE_MASK\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR1_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR1_READ\n");
    goto unhandled_exit;
    break;
     
  case VMEXIT_CR1_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR1_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR2_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR2_READ\n");
    goto unhandled_exit;
    break;
     
  case VMEXIT_CR2_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR2_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR4_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR4_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR4_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR4_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR5_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR5_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR5_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR5_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR6_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR6_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR6_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR6_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR7_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR7_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR7_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR7_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR8_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR8_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR8_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR8_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR9_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR9_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR9_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR9_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR10_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR10_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR10_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR10_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR11_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR11_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR11_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR11_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR12_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR12_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR12_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR12_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR13_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR13_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR13_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR13_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR14_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR14_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR14_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR14_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR15_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR15_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR15_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR15_WRITE\n");
    goto unhandled_exit;
    break;


  case VMEXIT_DR_READ_MASK:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR_READ_MASK\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR_WRITE_MASK:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR_WRITE_MASK\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR0_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR0_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR0_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR0_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR1_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR1_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR1_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR1_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR2_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR2_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR2_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR2_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR3_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR3_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR3_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR3_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR4_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR4_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR4_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR4_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR5_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR5_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR5_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR5_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR6_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR6_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR6_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR6_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR7_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR7_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR7_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR7_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR8_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR8_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR8_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR8_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR9_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR9_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR9_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR9_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR10_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR10_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR10_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR10_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR11_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR11_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR11_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR11_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR12_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR12_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR12_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR12_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR13_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR13_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR13_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR13_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR14_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR14_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR14_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR14_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR15_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR15_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_DR15_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_DR15_WRITE\n");
    goto unhandled_exit;
    break;

    
  case VMEXIT_EXCP_MASK:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP_MASK\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP0:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP0\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP2:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP2\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP3:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP3\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP4:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP4\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP5:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP5\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP6:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP6\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP7:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP7\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP8:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP8\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP9:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP9\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP10:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP10\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP11:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP11\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP12:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP12\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP13:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP13\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP15:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP15\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP16:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP16\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP17:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP17\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP18:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP18\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP19:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP19\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP20:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP20\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP21:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP21\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP22:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP22\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP23:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP23\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP24:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP24\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP25:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP25\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP26:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP26\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP27:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP27\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP28:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP28\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP29:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP29\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP30:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP30\n");
    goto unhandled_exit;
    break;

  case VMEXIT_EXCP31:
    PrintDebug("Unhandled SVM Exit: VMEXIT_EXCP31\n");
    goto unhandled_exit;
    break;

    
  case VMEXIT_NMI:
    PrintDebug("Unhandled SVM Exit: VMEXIT_NMI\n");
    goto unhandled_exit;
    break;

  case VMEXIT_INIT:
    PrintDebug("Unhandled SVM Exit: VMEXIT_INIT\n");
    goto unhandled_exit;
    break;

  case VMEXIT_VINITR:
    PrintDebug("Unhandled SVM Exit: VMEXIT_VINITR\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CR0_SEL_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CR0_SEL_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_IDTR_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_IDTR_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_IDTR_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_IDTR_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_GDTR_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_GDTR_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_GDTR_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_GDTR_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_LDTR_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_LDTR_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_LDTR_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_LDTR_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_TR_READ:
    PrintDebug("Unhandled SVM Exit: VMEXIT_TR_READ\n");
    goto unhandled_exit;
    break;

  case VMEXIT_TR_WRITE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_TR_WRITE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_RDTSC:
    PrintDebug("Unhandled SVM Exit: VMEXIT_RDTSC\n");
    goto unhandled_exit;
    break;

  case VMEXIT_RDPMC:
    PrintDebug("Unhandled SVM Exit: VMEXIT_RDPMC\n");
    goto unhandled_exit;
    break;

  case VMEXIT_PUSHF:
    PrintDebug("Unhandled SVM Exit: VMEXIT_PUSHF\n");
    goto unhandled_exit;
    break;

  case VMEXIT_POPF:
    PrintDebug("Unhandled SVM Exit: VMEXIT_POPF\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CPUID:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CPUID\n");
    goto unhandled_exit;
    break;

  case VMEXIT_RSM:
    PrintDebug("Unhandled SVM Exit: VMEXIT_RSM\n");
    goto unhandled_exit;
    break;

  case VMEXIT_IRET:
    PrintDebug("Unhandled SVM Exit: VMEXIT_IRET\n");
    goto unhandled_exit;
    break;

  case VMEXIT_SWINT:
    PrintDebug("Unhandled SVM Exit: VMEXIT_SWINT\n");
    goto unhandled_exit;
    break;

  case VMEXIT_INVD:
    PrintDebug("Unhandled SVM Exit: VMEXIT_INVD\n");
    goto unhandled_exit;
    break;

  case VMEXIT_INVLPGA:
    PrintDebug("Unhandled SVM Exit: VMEXIT_INVLPGA\n");
    goto unhandled_exit;
    break;

  case VMEXIT_TASK_SWITCH:
    PrintDebug("Unhandled SVM Exit: VMEXIT_TASK_SWITCH\n");
    goto unhandled_exit;
    break;

  case VMEXIT_FERR_FREEZE:
    PrintDebug("Unhandled SVM Exit: VMEXIT_FERR_FREEZE\n");
    goto unhandled_exit;
    break;

  case VMEXIT_SHUTDOWN:
    PrintDebug("Unhandled SVM Exit: VMEXIT_SHUTDOWN\n");
    goto unhandled_exit;
    break;

  case VMEXIT_VMRUN:
    PrintDebug("Unhandled SVM Exit: VMEXIT_VMRUN\n");
    goto unhandled_exit;
    break;


  case VMEXIT_STGI:
    PrintDebug("Unhandled SVM Exit: VMEXIT_STGI\n");
    goto unhandled_exit;
    break;

  case VMEXIT_CLGI:
    PrintDebug("Unhandled SVM Exit: VMEXIT_CLGI\n");
    goto unhandled_exit;
    break;

  case VMEXIT_SKINIT:
    PrintDebug("Unhandled SVM Exit: VMEXIT_SKINIT\n");
    goto unhandled_exit;
    break;

  case VMEXIT_RDTSCP:
    PrintDebug("Unhandled SVM Exit: VMEXIT_RDTSCP\n");
    goto unhandled_exit;
    break;

  case VMEXIT_ICEBP:
    PrintDebug("Unhandled SVM Exit: VMEXIT_ICEBP\n");
    goto unhandled_exit;
    break;

  case VMEXIT_MONITOR:
    PrintDebug("Unhandled SVM Exit: VMEXIT_MONITOR\n");
    goto unhandled_exit;
    break;

  case VMEXIT_MWAIT:
    PrintDebug("Unhandled SVM Exit: VMEXIT_MWAIT\n");
    goto unhandled_exit;
    break;

  case VMEXIT_MWAIT_CONDITIONAL:
    PrintDebug("Unhandled SVM Exit: VMEXIT_MWAIT_CONDITIONAL\n");
    goto unhandled_exit;
    break;

    
  case VMEXIT_INVALID_VMCB:
    PrintDebug("Unhandled SVM Exit: VMEXIT_INVALID_VMCB\n");
    goto unhandled_exit;
    break;


  unhandled_exit:

  default: {
  
    addr_t rip_addr;
    char buf[15];
    addr_t host_addr;


    rip_addr = get_addr_linear(info, guest_state->rip, &(info->segments.cs));


    PrintError("SVM Returned:(VMCB=%x)\n", info->vmm_data); 
    PrintError("RIP: %x\n", guest_state->rip);
    PrintError("RIP Linear: %x\n", rip_addr);
    
    PrintError("SVM Returned: Exit Code: %x\n",exit_code); 
    
    PrintError("io_info1 low = 0x%.8x\n", *(uint_t*)&(guest_ctrl->exit_info1));
    PrintError("io_info1 high = 0x%.8x\n", *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info1)) + 4));
    
    PrintError("io_info2 low = 0x%.8x\n", *(uint_t*)&(guest_ctrl->exit_info2));
    PrintError("io_info2 high = 0x%.8x\n", *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info2)) + 4));

    

    if (info->mem_mode == PHYSICAL_MEM) {
      if (guest_pa_to_host_pa(info, guest_state->rip, &host_addr) == -1) {
	PrintError("Could not translate guest_state->rip to host address\n");
	return -1;
      }
    } else if (info->mem_mode == VIRTUAL_MEM) {
      if (guest_va_to_host_pa(info, guest_state->rip, &host_addr) == -1) {
	PrintError("Could not translate guest_state->rip to host address\n");
	return -1;
      }
    } else {
      PrintError("Invalid memory mode\n");
      return -1;
    }
    
    PrintError("Host Address of rip = 0x%x\n", host_addr);
    
    memset(buf, 0, 32);
    
    PrintError("Reading instruction stream in guest\n", rip_addr);
    
    if (info->mem_mode == PHYSICAL_MEM) {
      read_guest_pa_memory(info, rip_addr-16, 32, buf);
    } else {
      read_guest_va_memory(info, rip_addr-16, 32, buf);
    }
    
    PrintDebug("16 bytes before Rip\n");
    PrintTraceMemDump(buf, 16);
    PrintDebug("Rip onward\n");
    PrintTraceMemDump(buf+16, 16);
    
    return -1;

  }
    break;

  }
  // END OF SWITCH (EXIT_CODE)


  // Update the low level state

  if (intr_pending(info)) {

    switch (get_intr_type(info)) {
    case EXTERNAL_IRQ: 
      {
	uint_t irq = get_intr_number(info);

        // check to see if ==-1 (non exists)

	/*	
	  guest_ctrl->EVENTINJ.vector = irq;
	  guest_ctrl->EVENTINJ.valid = 1;
	  guest_ctrl->EVENTINJ.type = SVM_INJECTION_EXTERNAL_INTR;
	*/
	
	guest_ctrl->guest_ctrl.V_IRQ = 1;
	guest_ctrl->guest_ctrl.V_INTR_VECTOR = irq;
	guest_ctrl->guest_ctrl.V_IGN_TPR = 1;
	guest_ctrl->guest_ctrl.V_INTR_PRIO = 0xf;
#ifdef DEBUG_INTERRUPTS
	PrintDebug("Injecting Interrupt %d (EIP=%x)\n", guest_ctrl->guest_ctrl.V_INTR_VECTOR, info->rip);
#endif
	injecting_intr(info, irq, EXTERNAL_IRQ);
	
	break;
      }
    case NMI:
      guest_ctrl->EVENTINJ.type = SVM_INJECTION_NMI;
      break;
    case EXCEPTION:
      {
	uint_t excp = get_intr_number(info);

	guest_ctrl->EVENTINJ.type = SVM_INJECTION_EXCEPTION;
	
	if (info->intr_state.excp_error_code_valid) {  //PAD
	  guest_ctrl->EVENTINJ.error_code = info->intr_state.excp_error_code;
	  guest_ctrl->EVENTINJ.ev = 1;
#ifdef DEBUG_INTERRUPTS
	  PrintDebug("Injecting error code %x\n", guest_ctrl->EVENTINJ.error_code);
#endif
	}
	
	guest_ctrl->EVENTINJ.vector = excp;
	
	guest_ctrl->EVENTINJ.valid = 1;
#ifdef DEBUG_INTERRUPTS
	PrintDebug("Injecting Interrupt %d (EIP=%x)\n", guest_ctrl->EVENTINJ.vector, info->rip);
#endif
	injecting_intr(info, excp, EXCEPTION);
	break;
      }
    case SOFTWARE_INTR:
      guest_ctrl->EVENTINJ.type = SVM_INJECTION_SOFT_INTR;
      break;
    case VIRTUAL_INTR:
      guest_ctrl->EVENTINJ.type = SVM_INJECTION_VIRTUAL_INTR;
      break;

    case INVALID_INTR: 
    default:
      PrintError("Attempted to issue an invalid interrupt\n");
      return -1;
    }

  } else {
#ifdef DEBUG_INTERRUPTS
    PrintDebug("No interrupts/exceptions pending\n");
#endif
  }

  guest_state->cr0 = info->ctrl_regs.cr0;
  guest_state->cr2 = info->ctrl_regs.cr2;
  guest_state->cr3 = info->ctrl_regs.cr3;
  guest_state->cr4 = info->ctrl_regs.cr4;
  guest_state->dr6 = info->dbg_regs.dr6;
  guest_state->dr7 = info->dbg_regs.dr7;
  guest_ctrl->guest_ctrl.V_TPR = info->ctrl_regs.cr8 & 0xff;
  guest_state->rflags = info->ctrl_regs.rflags;
  guest_state->efer = info->ctrl_regs.efer;

  guest_state->cpl = info->cpl;

  guest_state->rax = info->vm_regs.rax;
  guest_state->rip = info->rip;
  guest_state->rsp = info->vm_regs.rsp;


  set_vmcb_segments((vmcb_t*)(info->vmm_data), &(info->segments));

  if (exit_code == VMEXIT_INTR) {
    //PrintDebug("INTR ret IP = %x\n", guest_state->rip);
  }

  return 0;
}

