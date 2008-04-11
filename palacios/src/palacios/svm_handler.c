#include <palacios/svm_handler.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_emulate.h>
#include <palacios/svm_ctrl_regs.h>
#include <palacios/svm_io.h>
#include <palacios/vmm_intr.h>

extern struct vmm_os_hooks * os_hooks;


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
  info->vm_regs.rsp = guest_state->rsp;



  


  exit_code = guest_ctrl->exit_code;
  
  // PrintDebugVMCB((vmcb_t*)(info->vmm_data));

  
  if (exit_code == VMEXIT_IOIO) {
    struct svm_io_info * io_info = (struct svm_io_info *)&(guest_ctrl->exit_info1);
    
    if (io_info->type == 0) {
      if (io_info->str) {
	handle_svm_io_outs(info);
      } else {
	handle_svm_io_out(info);
      }
    } else {
      if (io_info->str) {
	handle_svm_io_ins(info);
      } else {
	handle_svm_io_in(info);
      }
    }
  } else if (exit_code == VMEXIT_CR0_WRITE) {
    PrintDebug("CR0 Write\n");

    if (handle_cr0_write(info) == -1) {
      return -1;
    }
    /*
  } else if (( (exit_code == VMEXIT_CR3_READ)  ||
	       (exit_code == VMEXIT_CR3_WRITE) ||
	       (exit_code == VMEXIT_INVLPG)    ||
	       (exit_code == VMEXIT_INVLPGA)   || 
	       (exit_code == VMEXIT_EXCP14)) && 
	     (info->page_mode == SHADOW_PAGING)) {
    handle_shadow_paging(info);
    */
  } else {
    addr_t rip_addr = get_addr_linear(info, guest_state->rip, guest_state->cs.selector);
    char buf[15];
    addr_t host_addr;


    PrintDebug("SVM Returned:(VMCB=%x)\n", info->vmm_data); 
    PrintDebug("RIP: %x\n", guest_state->rip);
    PrintDebug("RIP Linear: %x\n", rip_addr);
    
    PrintDebug("SVM Returned: Exit Code: %x\n",exit_code); 
    
    PrintDebug("io_info1 low = 0x%.8x\n", *(uint_t*)&(guest_ctrl->exit_info1));
    PrintDebug("io_info1 high = 0x%.8x\n", *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info1)) + 4));
    
    PrintDebug("io_info2 low = 0x%.8x\n", *(uint_t*)&(guest_ctrl->exit_info2));
    PrintDebug("io_info2 high = 0x%.8x\n", *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info2)) + 4));

    

    if (guest_pa_to_host_pa(info, guest_state->rip, &host_addr) == -1) {
      PrintDebug("Could not translate guest_state->rip to host address\n");
      return -1;
    }

    PrintDebug("Host Address of rip = 0x%x\n", host_addr);

    memset(buf, 0, 15);
    
    PrintDebug("Reading from 0x%x in guest\n", rip_addr);
    
    read_guest_pa_memory(info, rip_addr, 15, buf);

    PrintTraceMemDump(buf, 15);

    while(1);

  }


  // Update the low level state

  if (intr_pending(&(info->intr_state))) {
    guest_ctrl->EVENTINJ.vector = get_intr_number(&(info->intr_state));
    guest_ctrl->EVENTINJ.valid = 1;
 
    switch (get_intr_type(&(info->intr_state))) {
    case EXTERNAL_IRQ:
      guest_ctrl->EVENTINJ.type = SVM_INJECTION_EXTERNAL_INTR;
      break;
    case NMI:
      guest_ctrl->EVENTINJ.type = SVM_INJECTION_NMI;
      break;
    case EXCEPTION:
      guest_ctrl->EVENTINJ.type = SVM_INJECTION_EXCEPTION;
      guest_ctrl->EVENTINJ.error_code = info->intr_state.excp_error_code;
      break;
    case SOFTWARE:
      guest_ctrl->EVENTINJ.type = SVM_INJECTION_SOFT_INTR;
      break;
    case VIRTUAL:
      guest_ctrl->EVENTINJ.type = SVM_INJECTION_VIRTUAL_INTR;
      break;

    case INVALID_INTR: 
    default:
      PrintDebug("Attempted to issue an invalid interrupt\n");
      return -1;
    }

    // IMPORTANT TODO
    // We need to figure out stack parameters....

    //EVENTINJ.error_code

  }

  guest_state->rax = info->vm_regs.rax;
  guest_state->rip = info->rip;
  guest_state->rsp = info->vm_regs.rsp;

  return 0;
}




int handle_shadow_paging(struct guest_info * info) {
  vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
  //  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));

  if (guest_ctrl->exit_code == VMEXIT_CR3_READ) {

  }

  return 0;
}



