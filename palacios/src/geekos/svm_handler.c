#include <geekos/svm_handler.h>
#include <geekos/vmm.h>




int handle_svm_exit(guest_info_t * info) {
  vmcb_ctrl_t * guest_ctrl = 0;
  vmcb_saved_state_t * guest_state = 0;
  ulong_t exit_code = 0;
  
  guest_ctrl = GET_VMCB_CTRL_AREA((vmcb_t*)(info->vmm_data));
  guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
  
  PrintDebug("SVM Returned: (Exit Code=%x) (VMCB=%x)\n",&(guest_ctrl->exit_code), info->vmm_data); 
  PrintDebug("RIP: %x\n", guest_state->rip);
  
  
  exit_code = guest_ctrl->exit_code;
  
  //  PrintDebugVMCB((vmcb_t*)(info->vmm_data));
  PrintDebug("SVM Returned: Exit Code: %x\n",exit_code); 
  PrintDebug("io_info1 low = 0x%.8x\n", *(uint_t*)&(guest_ctrl->exit_info1));
  PrintDebug("io_info1 high = 0x%.8x\n", *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info1)) + 4));

  PrintDebug("io_info2 low = 0x%.8x\n", *(uint_t*)&(guest_ctrl->exit_info2));
  PrintDebug("io_info2 high = 0x%.8x\n", *(uint_t *)(((uchar_t *)&(guest_ctrl->exit_info2)) + 4));
  if (exit_code == VMEXIT_IOIO) {
    handle_svm_io(info);
  }


  return 0;
}



// This should package up an IO request and call vmm_handle_io
int handle_svm_io(guest_info_t * info) {
  vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));

  PrintDebug("Ctrl Area=%x\n", ctrl_area);

  //  struct svm_io_info * io_info = (struct svm_io_info *)&(ctrl_area->exit_info1);



  //  PrintDebugVMCB((vmcb_t*)(info->vmm_data));

  guest_state->rip = ctrl_area->exit_info2;


  

  //  PrintDebug("Exit On Port %d\n", io_info->port);

  return 0;
}
