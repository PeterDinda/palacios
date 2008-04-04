#include <geekos/svm_io.h>
#include <geekos/vmm_io.h>
#include <geekos/vmm_ctrl_regs.h>
#include <geekos/vmm_emulate.h>
#include <geekos/vm_guest_mem.h>


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


  if (hook->read(io_info->port, &(info->vm_regs.rax), read_size) != read_size) {
    // not sure how we handle errors.....
    return -1;
  }

  info->rip = ctrl_area->exit_info2;

  return 0;
}





/* We might not handle wrap around of the RDI register correctly...
 * In that if we do wrap around the effect will manifest in the higher bits of the register
 */
int handle_svm_io_ins(struct guest_info * info) {
  vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
  
  struct svm_io_info * io_info = (struct svm_io_info *)&(ctrl_area->exit_info1);
  
  vmm_io_hook_t * hook = get_io_hook(&(info->io_map), io_info->port);
  uint_t read_size = 0;
  addr_t base_addr = guest_state->es.base;
  addr_t dst_addr = 0;
  uint_t rep_num = 1;
  ullong_t mask = 0;

  // This is kind of hacky...
  // direction can equal either 1 or -1
  // We will multiply the final added offset by this value to go the correct direction
  int direction = 1;
  struct rflags * flags = (struct rflags *)&(guest_state->rflags);  
  if (flags->df) {
    direction = -1;
  }


  if (hook == NULL) {
    // error, we should not have exited on this port
    return -1;
  }

  PrintDebug("INS on  port %d (0x%x)\n", io_info->port, io_info->port);

  if (io_info->sz8) { 
    read_size = 1;
  } else if (io_info->sz16) {
    read_size = 2;
  } else if (io_info->sz32) {
    read_size = 4;
  }


  if (io_info->addr16) {
    mask = 0xffff;
  } else if (io_info->addr32) {
    mask = 0xffffffff;
  } else if (io_info->addr64) {
    mask = 0xffffffffffffffffLL;
  } else {
    // should never happen
    return -1;
  }

  if (io_info->rep) {
    rep_num = info->vm_regs.rcx & mask;
  }



  while (rep_num > 0) {
    addr_t host_addr;
    dst_addr = base_addr + (info->vm_regs.rdi & mask);
    
    if (guest_va_to_host_va(info, dst_addr, &host_addr) == -1) {
      // either page fault or gpf...
    }

    if (hook->read(io_info->port, (char*)host_addr, read_size) != read_size) {
      // not sure how we handle errors.....
      return -1;
    }

    info->vm_regs.rdi += read_size * direction;

    if (io_info->rep)
      info->vm_regs.rcx--;
    
    rep_num--;
  }


  info->rip = ctrl_area->exit_info2;

  return 0;
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


  if (hook->write(io_info->port, &(info->vm_regs.rax), write_size) != write_size) {
    // not sure how we handle errors.....
    return -1;
  }

  info->rip = ctrl_area->exit_info2;

  return 0;
}


/* We might not handle wrap around of the RSI register correctly...
 * In that if we do wrap around the effect will manifest in the higher bits of the register
 */

int handle_svm_io_outs(struct guest_info * info) {
  vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
  
  struct svm_io_info * io_info = (struct svm_io_info *)&(ctrl_area->exit_info1);
  
  vmm_io_hook_t * hook = get_io_hook(&(info->io_map), io_info->port);
  uint_t write_size = 0;
  addr_t base_addr = guest_state->ds.base;
  addr_t dst_addr = 0;
  uint_t rep_num = 1;
  ullong_t mask = 0;

  // This is kind of hacky...
  // direction can equal either 1 or -1
  // We will multiply the final added offset by this value to go the correct direction
  int direction = 1;
  struct rflags * flags = (struct rflags *)&(guest_state->rflags);  
  if (flags->df) {
    direction = -1;
  }


  if (hook == NULL) {
    // error, we should not have exited on this port
    return -1;
  }

  PrintDebug("OUTS on  port %d (0x%x)\n", io_info->port, io_info->port);

  if (io_info->sz8) { 
    write_size = 1;
  } else if (io_info->sz16) {
    write_size = 2;
  } else if (io_info->sz32) {
    write_size = 4;
  }


  if (io_info->addr16) {
    mask = 0xffff;
  } else if (io_info->addr32) {
    mask = 0xffffffff;
  } else if (io_info->addr64) {
    mask = 0xffffffffffffffffLL;
  } else {
    // should never happen
    return -1;
  }

  if (io_info->rep) {
    rep_num = info->vm_regs.rcx & mask;
  }


  while (rep_num > 0) {
    addr_t host_addr;
    dst_addr = base_addr + (info->vm_regs.rsi & mask);
    
    if (guest_va_to_host_va(info, dst_addr, &host_addr) == -1) {
      // either page fault or gpf...
    }

    if (hook->write(io_info->port, (char*)host_addr, write_size) != write_size) {
      // not sure how we handle errors.....
      return -1;
    }

    info->vm_regs.rsi += write_size * direction;

    if (io_info->rep)
      info->vm_regs.rcx--;
    
    rep_num--;
  }


  info->rip = ctrl_area->exit_info2;


  return 0;



}
