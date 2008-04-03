#include <geekos/svm_ctrl_regs.h>
#include <geekos/vmm_mem.h>
#include <geekos/vmm.h>
#include <geekos/vmcb.h>
#include <geekos/vmm_emulate.h>
#include <geekos/vm_guest_mem.h>
#include <geekos/vmm_ctrl_regs.h>


int handle_cr0_write(struct guest_info * info) {
  //vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
  char instr[15];
  
  
  if (info->cpu_mode == REAL) {
    int index = 0;
    int ret;

    // The real rip address is actually a combination of the rip + CS base 
    ret = read_guest_pa_memory(info, (addr_t)guest_state->rip, 15, instr);
    if (ret != 0) {
      // I think we should inject a GPF into the guest
      PrintDebug("Could not read instruction (ret=%d)\n", ret);
      return -1;
    }

    while (is_prefix_byte(instr[index])) {
      index++; 
    }

    if ((instr[index] == cr_access_byte) && 
	(instr[index + 1] == lmsw_byte) && 
	(MODRM_REG(instr[index + 2]) == lmsw_reg_byte)) {
 
      addr_t first_operand;
      addr_t second_operand;
      struct cr0_real *old_cr0;
      struct cr0_real *new_cr0;
     
      // LMSW
      // decode mod/RM
      index += 2;
 
      old_cr0 = (struct cr0_real*)&(guest_state->cr0);

      if (decode_operands16(&(info->vm_regs), instr + index, &first_operand, &second_operand, REG16) != 0) {
	// error... don't know what to do
	return -1;
      }
      
      index += 3;

      new_cr0 = (struct cr0_real *)first_operand;

      if ((new_cr0->pe == 1) && (old_cr0->pe == 0)) {
	info->cpu_mode = PROTECTED;
      }
      

      if (info->page_mode == SHADOW_PAGING) {
	struct cr0_real * virt_cr0 = (struct cr0_real*)&(info->shdw_pg_state.guest_cr0);

	/* struct cr0_real is only 4 bits wide, 
	 * so we can overwrite the old_cr0 without worrying about the shadow fields
	 */
	*old_cr0 = *new_cr0;
	*virt_cr0 = *new_cr0;
      } else {
	// for now we just pass through....
	*old_cr0 = *new_cr0;
      }

      PrintDebug("index = %d, rip = %x\n", index, (ulong_t)(info->rip));
      info->rip += index;
      PrintDebug("new_rip = %x\n", (ulong_t)(info->rip));
    } else if ((instr[index] == cr_access_byte) && 
	       (instr[index + 1] == clts_byte)) {
      // CLTS
    } else {
      // unsupported instruction, UD the guest
      return -1;
    }


  } else if (info->cpu_mode == PROTECTED) {
    PrintDebug("Protected Mode write to CR0\n");
    while(1);
  } else {
    PrintDebug("Unknown Mode write to CR0\n");
    while(1);
  }
  return 0;
}


