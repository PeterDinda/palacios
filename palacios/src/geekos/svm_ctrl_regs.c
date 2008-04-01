#include <geekos/svm_ctrl_regs.h>
#include <geekos/vmm_mem.h>
#include <geekos/vmm.h>
#include <geekos/vmcb.h>
#include <geekos/vmm_emulate.h>


int handle_cr0_write(struct guest_info * info, ullong_t * new_cr0) {
  vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
  char instr[15];
  
  


  if (info->cpu_mode == REAL) {
    read_guest_pa_memory(info, (addr_t)guest_state->rip, 15, instr);
    int index = 0;

    while (is_prefix_byte(instr[index])) {
      index++; 
    }
    
    if ((instr[index] == cr_access_byte) && 
	(instr[index + 1] == lmsw_byte)) {
      // LMSW
      // decode mod/RM

    } else if ((instr[index] == cr_access_byte) && 
	       (instr[index + 1] == clts_byte)) {
      // CLTS
    } else {
      // unsupported instruction, GPF the guest
      return -1;
    }


  }


  return 0;
}


