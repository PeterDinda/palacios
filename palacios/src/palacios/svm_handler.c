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

  case VMEXIT_EXCP1: 
    {
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
      break;
    } 


  case VMEXIT_VMMCALL: 
    {
#ifdef DEBUG_EMULATOR
      PrintDebug("VMMCALL\n");
#endif
      if (info->run_state == VM_EMULATING) {
	if (v3_emulation_exit_handler(info) == -1) {
	  return -1;
	}
      } else {
	/*
	ulong_t tsc_spread = 0;
	ullong_t exit_tsc = 0;

	ulong_t rax = (ulong_t)info->vm_regs.rbx;
	ulong_t rdx = (ulong_t)info->vm_regs.rcx;

	*(ulong_t *)(&exit_tsc) = rax;
	*(((ulong_t *)(&exit_tsc)) + 1) = rdx; 

	tsc_spread = info->exit_tsc - exit_tsc;

	PrintError("VMMCALL tsc diff = %lu\n",tsc_spread); 
	info->rip += 3;
	*/
	PrintError("VMMCALL with not emulator...\n");
	return -1;
      }
      break;
    } 
    


  case VMEXIT_WBINVD: 
    {
#ifdef DEBUG_EMULATOR
      PrintDebug("WBINVD\n");
#endif
      if (!handle_svm_wbinvd(info)) { 
	return -1;
      }
      break;
    }




    /* Exits Following this line are NOT HANDLED */
    /*=======================================================================*/

  default: {

    addr_t rip_addr;
    char buf[15];
    addr_t host_addr;

    PrintDebug("Unhandled SVM Exit: %s\n", vmexit_code_to_str(exit_code));

    rip_addr = get_addr_linear(info, guest_state->rip, &(info->segments.cs));


    PrintError("SVM Returned:(VMCB=%x)\n", info->vmm_data); 
    PrintError("RIP: %x\n", guest_state->rip);
    PrintError("RIP Linear: %x\n", rip_addr);
    
    PrintError("SVM Returned: Exit Code: %x\n", exit_code); 
    
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




const uchar_t * vmexit_code_to_str(uint_t exit_code) {
  switch(exit_code) {
  case VMEXIT_CR0_READ:
    return VMEXIT_CR0_READ_STR;
  case VMEXIT_CR1_READ:
    return VMEXIT_CR1_READ_STR;
  case VMEXIT_CR2_READ:
    return VMEXIT_CR2_READ_STR;
  case VMEXIT_CR3_READ:
    return VMEXIT_CR3_READ_STR;
  case VMEXIT_CR4_READ:
    return VMEXIT_CR4_READ_STR;
  case VMEXIT_CR5_READ:
    return VMEXIT_CR5_READ_STR;
  case VMEXIT_CR6_READ:
    return VMEXIT_CR6_READ_STR;
  case VMEXIT_CR7_READ:
    return VMEXIT_CR7_READ_STR;
  case VMEXIT_CR8_READ:
    return VMEXIT_CR8_READ_STR;
  case VMEXIT_CR9_READ:
    return VMEXIT_CR9_READ_STR;
  case VMEXIT_CR10_READ:
    return VMEXIT_CR10_READ_STR;
  case VMEXIT_CR11_READ:
    return VMEXIT_CR11_READ_STR;
  case VMEXIT_CR12_READ:
    return VMEXIT_CR12_READ_STR;
  case VMEXIT_CR13_READ:
    return VMEXIT_CR13_READ_STR;
  case VMEXIT_CR14_READ:
    return VMEXIT_CR14_READ_STR;
  case VMEXIT_CR15_READ:
    return VMEXIT_CR15_READ_STR;
  case VMEXIT_CR0_WRITE:
    return VMEXIT_CR0_WRITE_STR;
  case VMEXIT_CR1_WRITE:
    return VMEXIT_CR1_WRITE_STR;
  case VMEXIT_CR2_WRITE:
    return VMEXIT_CR2_WRITE_STR;
  case VMEXIT_CR3_WRITE:
    return VMEXIT_CR3_WRITE_STR;
  case VMEXIT_CR4_WRITE:
    return VMEXIT_CR4_WRITE_STR;
  case VMEXIT_CR5_WRITE:
    return VMEXIT_CR5_WRITE_STR;
  case VMEXIT_CR6_WRITE:
    return VMEXIT_CR6_WRITE_STR;
  case VMEXIT_CR7_WRITE:
    return VMEXIT_CR7_WRITE_STR;
  case VMEXIT_CR8_WRITE:
    return VMEXIT_CR8_WRITE_STR;
  case VMEXIT_CR9_WRITE:
    return VMEXIT_CR9_WRITE_STR;
  case VMEXIT_CR10_WRITE:
    return VMEXIT_CR10_WRITE_STR;
  case VMEXIT_CR11_WRITE:
    return VMEXIT_CR11_WRITE_STR;
  case VMEXIT_CR12_WRITE:
    return VMEXIT_CR12_WRITE_STR;
  case VMEXIT_CR13_WRITE:
    return VMEXIT_CR13_WRITE_STR;
  case VMEXIT_CR14_WRITE:
    return VMEXIT_CR14_WRITE_STR;
  case VMEXIT_CR15_WRITE:
    return VMEXIT_CR15_WRITE_STR;
  case VMEXIT_DR0_READ:
    return VMEXIT_DR0_READ_STR;
  case VMEXIT_DR1_READ:
    return VMEXIT_DR1_READ_STR;
  case VMEXIT_DR2_READ:
    return VMEXIT_DR2_READ_STR;
  case VMEXIT_DR3_READ:
    return VMEXIT_DR3_READ_STR;
  case VMEXIT_DR4_READ:
    return VMEXIT_DR4_READ_STR;
  case VMEXIT_DR5_READ:
    return VMEXIT_DR5_READ_STR;
  case VMEXIT_DR6_READ:
    return VMEXIT_DR6_READ_STR;
  case VMEXIT_DR7_READ:
    return VMEXIT_DR7_READ_STR;
  case VMEXIT_DR8_READ:
    return VMEXIT_DR8_READ_STR;
  case VMEXIT_DR9_READ:
    return VMEXIT_DR9_READ_STR;
  case VMEXIT_DR10_READ:
    return VMEXIT_DR10_READ_STR;
  case VMEXIT_DR11_READ:
    return VMEXIT_DR11_READ_STR;
  case VMEXIT_DR12_READ:
    return VMEXIT_DR12_READ_STR;
  case VMEXIT_DR13_READ:
    return VMEXIT_DR13_READ_STR;
  case VMEXIT_DR14_READ:
    return VMEXIT_DR14_READ_STR;
  case VMEXIT_DR15_READ:
    return VMEXIT_DR15_READ_STR;
  case VMEXIT_DR0_WRITE:
    return VMEXIT_DR0_WRITE_STR;
  case VMEXIT_DR1_WRITE:
    return VMEXIT_DR1_WRITE_STR;
  case VMEXIT_DR2_WRITE:
    return VMEXIT_DR2_WRITE_STR;
  case VMEXIT_DR3_WRITE:
    return VMEXIT_DR3_WRITE_STR;
  case VMEXIT_DR4_WRITE:
    return VMEXIT_DR4_WRITE_STR;
  case VMEXIT_DR5_WRITE:
    return VMEXIT_DR5_WRITE_STR;
  case VMEXIT_DR6_WRITE:
    return VMEXIT_DR6_WRITE_STR;
  case VMEXIT_DR7_WRITE:
    return VMEXIT_DR7_WRITE_STR;
  case VMEXIT_DR8_WRITE:
    return VMEXIT_DR8_WRITE_STR;
  case VMEXIT_DR9_WRITE:
    return VMEXIT_DR9_WRITE_STR;
  case VMEXIT_DR10_WRITE:
    return VMEXIT_DR10_WRITE_STR;
  case VMEXIT_DR11_WRITE:
    return VMEXIT_DR11_WRITE_STR;
  case VMEXIT_DR12_WRITE:
    return VMEXIT_DR12_WRITE_STR;
  case VMEXIT_DR13_WRITE:
    return VMEXIT_DR13_WRITE_STR;
  case VMEXIT_DR14_WRITE:
    return VMEXIT_DR14_WRITE_STR;
  case VMEXIT_DR15_WRITE:
    return VMEXIT_DR15_WRITE_STR;
  case VMEXIT_EXCP0:
    return VMEXIT_EXCP0_STR;
  case VMEXIT_EXCP1:
    return VMEXIT_EXCP1_STR;
  case VMEXIT_EXCP2:
    return VMEXIT_EXCP2_STR;
  case VMEXIT_EXCP3:
    return VMEXIT_EXCP3_STR;
  case VMEXIT_EXCP4:
    return VMEXIT_EXCP4_STR;
  case VMEXIT_EXCP5:
    return VMEXIT_EXCP5_STR;
  case VMEXIT_EXCP6:
    return VMEXIT_EXCP6_STR;
  case VMEXIT_EXCP7:
    return VMEXIT_EXCP7_STR;
  case VMEXIT_EXCP8:
    return VMEXIT_EXCP8_STR;
  case VMEXIT_EXCP9:
    return VMEXIT_EXCP9_STR;
  case VMEXIT_EXCP10:
    return VMEXIT_EXCP10_STR;
  case VMEXIT_EXCP11:
    return VMEXIT_EXCP11_STR;
  case VMEXIT_EXCP12:
    return VMEXIT_EXCP12_STR;
  case VMEXIT_EXCP13:
    return VMEXIT_EXCP13_STR;
  case VMEXIT_EXCP14:
    return VMEXIT_EXCP14_STR;
  case VMEXIT_EXCP15:
    return VMEXIT_EXCP15_STR;
  case VMEXIT_EXCP16:
    return VMEXIT_EXCP16_STR;
  case VMEXIT_EXCP17:
    return VMEXIT_EXCP17_STR;
  case VMEXIT_EXCP18:
    return VMEXIT_EXCP18_STR;
  case VMEXIT_EXCP19:
    return VMEXIT_EXCP19_STR;
  case VMEXIT_EXCP20:
    return VMEXIT_EXCP20_STR;
  case VMEXIT_EXCP21:
    return VMEXIT_EXCP21_STR;
  case VMEXIT_EXCP22:
    return VMEXIT_EXCP22_STR;
  case VMEXIT_EXCP23:
    return VMEXIT_EXCP23_STR;
  case VMEXIT_EXCP24:
    return VMEXIT_EXCP24_STR;
  case VMEXIT_EXCP25:
    return VMEXIT_EXCP25_STR;
  case VMEXIT_EXCP26:
    return VMEXIT_EXCP26_STR;
  case VMEXIT_EXCP27:
    return VMEXIT_EXCP27_STR;
  case VMEXIT_EXCP28:
    return VMEXIT_EXCP28_STR;
  case VMEXIT_EXCP29:
    return VMEXIT_EXCP29_STR;
  case VMEXIT_EXCP30:
    return VMEXIT_EXCP30_STR;
  case VMEXIT_EXCP31:
    return VMEXIT_EXCP31_STR;
  case VMEXIT_INTR:
    return VMEXIT_INTR_STR;
  case VMEXIT_NMI:
    return VMEXIT_NMI_STR;
  case VMEXIT_SMI:
    return VMEXIT_SMI_STR;
  case VMEXIT_INIT:
    return VMEXIT_INIT_STR;
  case VMEXIT_VINITR:
    return VMEXIT_VINITR_STR;
  case VMEXIT_CR0_SEL_WRITE:
    return VMEXIT_CR0_SEL_WRITE_STR;
  case VMEXIT_IDTR_READ:
    return VMEXIT_IDTR_READ_STR;
  case VMEXIT_GDTR_READ:
    return VMEXIT_GDTR_READ_STR;
  case VMEXIT_LDTR_READ:
    return VMEXIT_LDTR_READ_STR;
  case VMEXIT_TR_READ:
    return VMEXIT_TR_READ_STR;
  case VMEXIT_IDTR_WRITE:
    return VMEXIT_IDTR_WRITE_STR;
  case VMEXIT_GDTR_WRITE:
    return VMEXIT_GDTR_WRITE_STR;
  case VMEXIT_LDTR_WRITE:
    return VMEXIT_LDTR_WRITE_STR;
  case VMEXIT_TR_WRITE:
    return VMEXIT_TR_WRITE_STR;
  case VMEXIT_RDTSC:
    return VMEXIT_RDTSC_STR;
  case VMEXIT_RDPMC:
    return VMEXIT_RDPMC_STR;
  case VMEXIT_PUSHF:
    return VMEXIT_PUSHF_STR;
  case VMEXIT_POPF:
    return VMEXIT_POPF_STR;
  case VMEXIT_CPUID:
    return VMEXIT_CPUID_STR;
  case VMEXIT_RSM:
    return VMEXIT_RSM_STR;
  case VMEXIT_IRET:
    return VMEXIT_IRET_STR;
  case VMEXIT_SWINT:
    return VMEXIT_SWINT_STR;
  case VMEXIT_INVD:
    return VMEXIT_INVD_STR;
  case VMEXIT_PAUSE:
    return VMEXIT_PAUSE_STR;
  case VMEXIT_HLT:
    return VMEXIT_HLT_STR;
  case VMEXIT_INVLPG:
    return VMEXIT_INVLPG_STR;
  case VMEXIT_INVLPGA:
    return VMEXIT_INVLPGA_STR;
  case VMEXIT_IOIO:
    return VMEXIT_IOIO_STR;
  case VMEXIT_MSR:
    return VMEXIT_MSR_STR;
  case VMEXIT_TASK_SWITCH:
    return VMEXIT_TASK_SWITCH_STR;
  case VMEXIT_FERR_FREEZE:
    return VMEXIT_FERR_FREEZE_STR;
  case VMEXIT_SHUTDOWN:
    return VMEXIT_SHUTDOWN_STR;
  case VMEXIT_VMRUN:
    return VMEXIT_VMRUN_STR;
  case VMEXIT_VMMCALL:
    return VMEXIT_VMMCALL_STR;
  case VMEXIT_VMLOAD:
    return VMEXIT_VMLOAD_STR;
  case VMEXIT_VMSAVE:
    return VMEXIT_VMSAVE_STR;
  case VMEXIT_STGI:
    return VMEXIT_STGI_STR;
  case VMEXIT_CLGI:
    return VMEXIT_CLGI_STR;
  case VMEXIT_SKINIT:
    return VMEXIT_SKINIT_STR;
  case VMEXIT_RDTSCP:
    return VMEXIT_RDTSCP_STR;
  case VMEXIT_ICEBP:
    return VMEXIT_ICEBP_STR;
  case VMEXIT_WBINVD:
    return VMEXIT_WBINVD_STR;
  case VMEXIT_MONITOR:
    return VMEXIT_MONITOR_STR;
  case VMEXIT_MWAIT:
    return VMEXIT_MWAIT_STR;
  case VMEXIT_MWAIT_CONDITIONAL:
    return VMEXIT_MWAIT_CONDITIONAL_STR;
  case VMEXIT_NPF:
    return VMEXIT_NPF_STR;
  case VMEXIT_INVALID_VMCB:
    return VMEXIT_INVALID_VMCB_STR;
  }
  return NULL;
}
