#include <palacios/svm_handler.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/svm_io.h>
#include <palacios/svm_halt.h>
#include <palacios/svm_pause.h>
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
  
  if (exit_code == VMEXIT_IOIO) {
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
  } else if (exit_code == VMEXIT_CR0_WRITE) {
#ifdef DEBUG_CTRL_REGS
    PrintDebug("CR0 Write\n");
#endif
    if (handle_cr0_write(info) == -1) {
      return -1;
    }
  } else if (exit_code == VMEXIT_CR0_READ) {
#ifdef DEBUG_CTRL_REGS
    PrintDebug("CR0 Read\n");
#endif
    if (handle_cr0_read(info) == -1) {
      return -1;
    }
  } else if (exit_code == VMEXIT_CR3_WRITE) {
#ifdef DEBUG_CTRL_REGS
    PrintDebug("CR3 Write\n");
#endif
    if (handle_cr3_write(info) == -1) {
      return -1;
    }    
  } else if (exit_code == VMEXIT_CR3_READ) {
#ifdef DEBUG_CTRL_REGS
    PrintDebug("CR3 Read\n");
#endif
    if (handle_cr3_read(info) == -1) {
      return -1;
    }

  } else if (exit_code == VMEXIT_EXCP14) {
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
  } else if (exit_code == VMEXIT_NPF) {
    PrintError("Currently unhandled Nested Page Fault\n");
    return -1;

  } else if (exit_code == VMEXIT_INVLPG) {
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
    
  } else if (exit_code == VMEXIT_INTR) {

    //    handle_svm_intr(info);

  } else if (exit_code == VMEXIT_SMI) { 

    //   handle_svm_smi(info); // ignored for now

  } else if (exit_code == VMEXIT_HLT) {
    PrintDebug("Guest halted\n");
    if (handle_svm_halt(info) == -1) {
      return -1;
    }
  } else if (exit_code == VMEXIT_PAUSE) { 
    PrintDebug("Guest paused\n");
    if (handle_svm_pause(info) == -1) { 
      return -1;
    }
  } else if (exit_code == VMEXIT_VMMCALL) {
    PrintDebug("VMMCALL\n");
    if (info->run_state == VM_EMULATING) {
      if (v3_emulation_exit_handler(info) == -1) {
	return -1;
      }
    } else {
      PrintError("VMMCALL with not emulator...\n");
      return -1;
    }

  } else {
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

