#include <geekos/svm_handler.h>
#include <geekos/vmm.h>
#include <geekos/svm_ctrl_regs.h>

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


  PrintDebug("SVM Returned:(VMCB=%x)\n", info->vmm_data); 
  PrintDebug("RIP: %x\n", guest_state->rip);
  


  exit_code = guest_ctrl->exit_code;
  
  // PrintDebugVMCB((vmcb_t*)(info->vmm_data));
  PrintDebug("SVM Returned: Exit Code: %x\n",exit_code); 

  PrintDebug("io_info1 low = 0x%.8x\n", *(uint_t*)&(guest_ctrl->exit_info1));
  PrintDebug("io_info1 high = 0x%.8x\n", *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info1)) + 4));

  PrintDebug("io_info2 low = 0x%.8x\n", *(uint_t*)&(guest_ctrl->exit_info2));
  PrintDebug("io_info2 high = 0x%.8x\n", *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info2)) + 4));
  
  if (exit_code == VMEXIT_IOIO) {
    handle_svm_io(info);

  } else if (exit_code == VMEXIT_CR0_WRITE) {
    PrintDebug("CR0 Write\n");

    if (handle_cr0_write(info) == -1) {
      return -1;
    }

  } else if (( (exit_code == VMEXIT_CR3_READ)  ||
	       (exit_code == VMEXIT_CR3_WRITE) ||
	       (exit_code == VMEXIT_INVLPG)    ||
	       (exit_code == VMEXIT_INVLPGA)   || 
	       (exit_code == VMEXIT_EXCP14)) && 
	     (info->page_mode == SHADOW_PAGING)) {
    handle_shadow_paging(info);
  }


  // Update the low level state
  guest_state->rax = info->vm_regs.rax;
  guest_state->rip = info->rip;
  guest_state->rsp = info->vm_regs.rsp;

  return 0;
}



// This should package up an IO request and call vmm_handle_io
int handle_svm_io(struct guest_info * info) {
  vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));

  PrintDebug("Ctrl Area=%x\n", ctrl_area);

  //  struct svm_io_info * io_info = (struct svm_io_info *)&(ctrl_area->exit_info1);



  //  PrintDebugVMCB((vmcb_t*)(info->vmm_data));

  guest_state->rip = ctrl_area->exit_info2;


  

  //  PrintDebug("Exit On Port %d\n", io_info->port);

  return 0;
}


int handle_shadow_paging(struct guest_info * info) {
  vmcb_ctrl_t * guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
  //  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));

  if (guest_ctrl->exit_code == VMEXIT_CR3_READ) {

  }

  return 0;
}



