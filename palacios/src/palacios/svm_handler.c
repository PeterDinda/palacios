#include <palacios/svm_handler.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_ctrl_regs.h>
#include <palacios/svm_io.h>
#include <palacios/vmm_intr.h>


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

  get_vmcb_segments((vmcb_t*)(info->vmm_data), &(info->segments));


  exit_code = guest_ctrl->exit_code;
 

  // Disable printing io exits due to bochs debug messages
  //if (!((exit_code == VMEXIT_IOIO) && ((ushort_t)(guest_ctrl->exit_info1 >> 16) == 0x402))) {

  PrintDebug("SVM Returned: Exit Code: %x \t\t(tsc=%ul)\n",exit_code, (uint_t)info->time_state.guest_tsc); 
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
    PrintDebug("CR0 Write\n");

    if (handle_cr0_write(info) == -1) {
      return -1;
    }
  } else if (exit_code == VMEXIT_CR0_READ) {
    PrintDebug("CR0 Read\n");

    if (handle_cr0_read(info) == -1) {
      return -1;
    }
  } else if (exit_code == VMEXIT_CR3_WRITE) {
    PrintDebug("CR3 Write\n");

    if (handle_cr3_write(info) == -1) {
      return -1;
    }    
  } else if (exit_code == VMEXIT_CR3_READ) {
    PrintDebug("CR3 Read\n");

    if (handle_cr3_read(info) == -1) {
      return -1;
    }

  } else if (exit_code == VMEXIT_EXCP14) {
    addr_t fault_addr = guest_ctrl->exit_info2;
    pf_error_t * error_code = (pf_error_t *)&(guest_ctrl->exit_info1);
    
    PrintDebug("PageFault at %x (error=%d)\n", fault_addr, *error_code);

    if (info->shdw_pg_mode == SHADOW_PAGING) {
      if (handle_shadow_pagefault(info, fault_addr, *error_code) == -1) {
	return -1;
      }
    } else {
      PrintDebug("Page fault in un implemented paging mode\n");
      return -1;
    }

  } else if (exit_code == VMEXIT_INVLPG) {
    if (info->shdw_pg_mode == SHADOW_PAGING) {
      PrintDebug("Invlpg\n");
      if (handle_shadow_invlpg(info) == -1) {
	return -1;
      }
    }
   
    /*
      (exit_code == VMEXIT_INVLPGA)   || 
    */
    
  } else if (exit_code == VMEXIT_INTR) {

    //    handle_svm_intr(info);

  } else if (exit_code == VMEXIT_HLT) {
    PrintDebug("Guest halted\n");
    return -1;
  } else {
    addr_t rip_addr;
    char buf[15];
    addr_t host_addr;


    rip_addr = get_addr_linear(info, guest_state->rip, &(info->segments.cs));



    PrintDebug("SVM Returned:(VMCB=%x)\n", info->vmm_data); 
    PrintDebug("RIP: %x\n", guest_state->rip);
    PrintDebug("RIP Linear: %x\n", rip_addr);
    
    PrintDebug("SVM Returned: Exit Code: %x\n",exit_code); 
    
    PrintDebug("io_info1 low = 0x%.8x\n", *(uint_t*)&(guest_ctrl->exit_info1));
    PrintDebug("io_info1 high = 0x%.8x\n", *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info1)) + 4));
    
    PrintDebug("io_info2 low = 0x%.8x\n", *(uint_t*)&(guest_ctrl->exit_info2));
    PrintDebug("io_info2 high = 0x%.8x\n", *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info2)) + 4));

    

    if (info->mem_mode == PHYSICAL_MEM) {
      if (guest_pa_to_host_pa(info, guest_state->rip, &host_addr) == -1) {
	PrintDebug("Could not translate guest_state->rip to host address\n");
	return -1;
      }
    } else if (info->mem_mode == VIRTUAL_MEM) {
      if (guest_va_to_host_pa(info, guest_state->rip, &host_addr) == -1) {
	PrintDebug("Could not translate guest_state->rip to host address\n");
	return -1;
      }
    } else {
      PrintDebug("Invalid memory mode\n");
      return -1;
    }

    PrintDebug("Host Address of rip = 0x%x\n", host_addr);

    memset(buf, 0, 15);
    
    PrintDebug("Reading from 0x%x in guest\n", rip_addr);
    
    if (info->mem_mode == PHYSICAL_MEM) {
      read_guest_pa_memory(info, rip_addr, 15, buf);
    } else {
      read_guest_va_memory(info, rip_addr, 15, buf);
    }

    PrintTraceMemDump(buf, 15);

    while(1);

  }


  // Update the low level state

  if (intr_pending(info)) {

    switch (get_intr_type(info)) {
    case EXTERNAL_IRQ: 
      {
	uint_t irq = get_intr_number(info);
	/*	
	  guest_ctrl->EVENTINJ.vector = irq;
	  guest_ctrl->EVENTINJ.valid = 1;
	  guest_ctrl->EVENTINJ.type = SVM_INJECTION_EXTERNAL_INTR;
	*/
	
	guest_ctrl->guest_ctrl.V_IRQ = 1;
	guest_ctrl->guest_ctrl.V_INTR_VECTOR = irq;
	guest_ctrl->guest_ctrl.V_IGN_TPR = 1;
	guest_ctrl->guest_ctrl.V_INTR_PRIO = 0xf;

	PrintDebug("Injecting Interrupt %d (EIP=%x)\n", guest_ctrl->guest_ctrl.V_INTR_VECTOR, info->rip);

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
	
	if (info->intr_state.excp_error_code) {
	  guest_ctrl->EVENTINJ.error_code = info->intr_state.excp_error_code;
	  guest_ctrl->EVENTINJ.ev = 1;
	}
	
	guest_ctrl->EVENTINJ.vector = excp;
	
	PrintDebug("Injecting Interrupt %d (EIP=%x)\n", guest_ctrl->EVENTINJ.vector, info->rip);
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
      PrintDebug("Attempted to issue an invalid interrupt\n");
      return -1;
    }

  }


  guest_state->cr0 = info->ctrl_regs.cr0;
  guest_state->cr2 = info->ctrl_regs.cr2;
  guest_state->cr3 = info->ctrl_regs.cr3;
  guest_state->cr4 = info->ctrl_regs.cr4;
  guest_ctrl->guest_ctrl.V_TPR = info->ctrl_regs.cr8 & 0xff;
  guest_state->rflags = info->ctrl_regs.rflags;


  guest_state->cpl = info->cpl;

  guest_state->rax = info->vm_regs.rax;
  guest_state->rip = info->rip;
  guest_state->rsp = info->vm_regs.rsp;


  set_vmcb_segments((vmcb_t*)(info->vmm_data), &(info->segments));

  if (exit_code == VMEXIT_INTR) {
    PrintDebug("INTR ret IP = %x\n", guest_state->rip);
  }

  return 0;
}

