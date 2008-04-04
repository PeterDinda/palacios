#include <geekos/svm_io.h>
#include <geekos/vmm_io.h>



// This should package up an IO request and call vmm_handle_io
int handle_svm_io_in(struct guest_info * info) {
  vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
  //  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
  struct svm_io_info * io_info = (struct svm_io_info *)&(ctrl_area->exit_info1);

  vmm_io_hook_t * hook = get_io_hook(&(info->io_map), io_info->port);
  uint_t read_size = 0;

  if (hook == NULL) {
    // error, we should not have exited on this port
    return -1;
  }

  PrintDebug("IN on  port %d (0x%x)\n", io_info->port, io_info->port);

  if (io_info->sz8) { 
    read_size = 1;
  } else if (io_info->sz16) {
    read_size = 2;
  } else if (io_info->sz32) {
    read_size = 4;
  }


  if (hook->read(io_info->port, &(info->vm_regs.rax), read_size, read_size) != read_size) {
    // not sure how we handle errors.....
    return -1;
  }

  info->rip = ctrl_area->exit_info2;

  return 0;
}


int handle_svm_io_ins(struct guest_info * info) {

  //  PrintDebug("INS on  port %d (0x%x)\n", io_info->port, io_info->port);

  return -1;

}

int handle_svm_io_out(struct guest_info * info) {
  vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
  //  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
  struct svm_io_info * io_info = (struct svm_io_info *)&(ctrl_area->exit_info1);

  vmm_io_hook_t * hook = get_io_hook(&(info->io_map), io_info->port);
  uint_t write_size = 0;

  if (hook == NULL) {
    // error, we should not have exited on this port
    return -1;
  }

  PrintDebug("OUT on  port %d (0x%x)\n", io_info->port, io_info->port);

  if (io_info->sz8) { 
    write_size = 1;
  } else if (io_info->sz16) {
    write_size = 2;
  } else if (io_info->sz32) {
    write_size = 4;
  }


  if (hook->write(io_info->port, &(info->vm_regs.rax), write_size, write_size) != write_size) {
    // not sure how we handle errors.....
    return -1;
  }

  info->rip = ctrl_area->exit_info2;

  return 0;
}


int handle_svm_io_outs(struct guest_info * info) {
  return -1;
}
