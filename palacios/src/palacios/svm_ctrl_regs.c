#include <palacios/svm_ctrl_regs.h>
#include <palacios/vmm_mem.h>
#include <palacios/vmm.h>
#include <palacios/vmcb.h>
#include <palacios/vmm_emulate.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_ctrl_regs.h>


/* Segmentation is a problem here...
 *
 * When we get a memory operand, presumably we use the default segment (which is?) 
 * unless an alternate segment was specfied in the prefix...
 */


int handle_cr0_write(struct guest_info * info) {
  //vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
  char instr[15];
  
  
  if (info->cpu_mode == REAL) {
    int index = 0;
    int ret;

    // The real rip address is actually a combination of the rip + CS base 
    ret = read_guest_pa_memory(info, get_addr_linear(info, guest_state->rip, guest_state->cs.selector), 15, instr);
    if (ret != 15) {
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
      operand_type_t addr_type;
      char new_cr0_val = 0;
      // LMSW
      // decode mod/RM
      index += 2;
 
      old_cr0 = (struct cr0_real*)&(guest_state->cr0);


      addr_type = decode_operands16(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG16);


      if (addr_type == REG_OPERAND) {
	new_cr0 = (struct cr0_real *)first_operand;
      } else if (addr_type == MEM_OPERAND) {
	addr_t host_addr;

	if (guest_pa_to_host_va(info, first_operand + (guest_state->ds.base << 4), &host_addr) == -1) {
	  // gpf the guest
	  return -1;
	}

	new_cr0 = (struct cr0_real *)host_addr;
      } else {
	// error... don't know what to do
	return -1;
      }
		 
      if ((new_cr0->pe == 1) && (old_cr0->pe == 0)) {
	info->cpu_mode = PROTECTED;
      } else if ((new_cr0->pe == 0) && (old_cr0->pe == 1)) {
	info->cpu_mode = REAL;
      }
      
      new_cr0_val = *(char*)(new_cr0) & 0x0f;


      if (info->page_mode == SHADOW_PAGING) {
	struct cr0_real * virt_cr0 = (struct cr0_real*)&(info->shdw_pg_state.guest_cr0);

	/* struct cr0_real is only 4 bits wide, 
	 * so we can overwrite the old_cr0 without worrying about the shadow fields
	 */
	*(char*)old_cr0 &= 0xf0;
	*(char*)old_cr0 |= new_cr0_val;
	
	*(char*)virt_cr0 &= 0xf0;
	*(char*)virt_cr0 |= new_cr0_val;
      } else {
	// for now we just pass through....
	*(char*)old_cr0 &= 0xf0;
	*(char*)old_cr0 |= new_cr0_val;
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
    int index = 0;
    int ret;

    PrintDebug("Protected Mode write to CR0\n");

    // The real rip address is actually a combination of the rip + CS base 
    ret = read_guest_pa_memory(info, get_addr_linear(info, guest_state->rip, guest_state->cs.base), 15, instr);
    if (ret != 0) {
      // I think we should inject a GPF into the guest
      PrintDebug("Could not read instruction (ret=%d)\n", ret);
      return -1;
    }

    while (is_prefix_byte(instr[index])) {
      index++; 
    }


    /* CHECK IF MOV_TO_CR CAN TAKE MEMORY OPERANDS... */
    if ((instr[index] == cr_access_byte) && 
	(instr[index + 1] == mov_to_cr_byte)) {
    
      addr_t first_operand;
      addr_t second_operand;
      struct cr0_32 *old_cr0;
      struct cr0_32 *new_cr0;
      operand_type_t addr_type;

      index += 2;
 
      old_cr0 = (struct cr0_32*)&(guest_state->cr0);

      addr_type = decode_operands32(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG32);


      if (addr_type == REG_OPERAND) {
	new_cr0 = (struct cr0_32 *)first_operand;
      } else if (addr_type == MEM_OPERAND) {
	addr_t host_addr;

	if (guest_pa_to_host_va(info, first_operand + guest_state->ds.base, &host_addr) == -1) {
	  // gpf the guest
	  return -1;
	}

	new_cr0 = (struct cr0_32 *)host_addr;
      } else {
	// error... don't know what to do
	return -1;
      }


      if (info->page_mode == SHADOW_PAGING) {
	struct cr0_32 * virt_cr0 = (struct cr0_32 *)&(info->shdw_pg_state.guest_cr0);

	if ((new_cr0->pg == 1) && (virt_cr0->pg == 0)){
	  info->cpu_mode = PROTECTED_PG;

	  // Activate Shadow Paging
	}

	*virt_cr0 = *new_cr0;
	*old_cr0 = *new_cr0;
      } else {
	// fill in
      }

      info->rip += index;
    }
    
  } else {
    PrintDebug("Unknown Mode write to CR0\n");
    while(1);
  }
  return 0;
}


int handle_cr0_read(struct guest_info * info) {
  //vmcb_ctrl_t * ctrl_area = GET_VMCB_CTRL_AREA((vmcb_t *)(info->vmm_data));
  vmcb_saved_state_t * guest_state = GET_VMCB_SAVE_STATE_AREA((vmcb_t*)(info->vmm_data));
  char instr[15];

  if (info->cpu_mode == REAL) {
    int index = 0;
    int ret;

    // The real rip address is actually a combination of the rip + CS base 
    ret = read_guest_pa_memory(info, get_addr_linear(info, guest_state->rip, guest_state->cs.selector), 15, instr);
    if (ret != 15) {
      // I think we should inject a GPF into the guest
      PrintDebug("Could not read Real Mode instruction (ret=%d)\n", ret);
      return -1;
    }


    while (is_prefix_byte(instr[index])) {
      index++; 
    }

    if ((instr[index] == cr_access_byte) && 
	(instr[index + 1] == smsw_byte) && 
	(MODRM_REG(instr[index + 2]) == smsw_reg_byte)) {

      addr_t first_operand;
      addr_t second_operand;
      struct cr0_real *cr0;
      operand_type_t addr_type;
      char cr0_val = 0;

      index += 2;
      
      cr0 = (struct cr0_real*)&(guest_state->cr0);
      
      
      addr_type = decode_operands16(&(info->vm_regs), instr + index, &index, &first_operand, &second_operand, REG16);
      
      if (addr_type == MEM_OPERAND) {
	addr_t host_addr;
	
	if (guest_pa_to_host_va(info, first_operand + (guest_state->ds.base << 4), &host_addr) == -1) {
	  // gpf the guest
	  return -1;
	}
	
	first_operand = host_addr;
      } else {
	// error... don't know what to do
	return -1;
      }

      cr0_val = *(char*)cr0 & 0x0f;

      *(char *)first_operand &= 0xf0;
      *(char *)first_operand |= cr0_val;

      PrintDebug("index = %d, rip = %x\n", index, (ulong_t)(info->rip));
      info->rip += index;
      PrintDebug("new_rip = %x\n", (ulong_t)(info->rip));
    } else {
      addr_t host_addr;

      PrintDebug("Unknown read instr to CR0\n");
      guest_pa_to_host_pa(info, get_addr_linear(info, guest_state->rip, guest_state->cs.selector), &host_addr);
      
      PrintDebug("Instr (15 bytes) at %x:\n", host_addr);
      PrintTraceMemDump((char*)host_addr, 15);

      return -1;
    }

  } else if (info->cpu_mode == PROTECTED) {
    int index = 0;
    int ret;

    // The real rip address is actually a combination of the rip + CS base 
    ret = read_guest_pa_memory(info, get_addr_linear(info, guest_state->rip, guest_state->cs.base), 15, instr);
    if (ret != 15) {
      // I think we should inject a GPF into the guest
      PrintDebug("Could not read Proteced mode instruction (ret=%d)\n", ret);
      return -1;
    }

    while (is_prefix_byte(instr[index])) {
      index++; 
    }


  }


  return 0;
}
