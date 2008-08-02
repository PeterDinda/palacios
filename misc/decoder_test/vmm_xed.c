#ifdef __DECODER_TEST__
#include "vmm_decoder.h"
#include "vmm_xed.h"
#include <xed/xed-interface.h>
#include "vm_guest.h"
#include "test.h"
#else
#include <palacios/vmm_decoder.h>
#include <palacios/vmm_xed.h>
#include <xed/xed-interface.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm.h>


#endif

static xed_state_t decoder_state;

#define GPR_REGISTER     0
#define SEGMENT_REGISTER 1
#define CTRL_REGISTER    2
#define DEBUG_REGISTER   3



/* Disgusting mask hack...
   I can't think right now, so we'll do it this way...
*/
static const ullong_t mask_1 = 0x00000000000000ffLL;
static const ullong_t mask_2 = 0x000000000000ffffLL;
static const ullong_t mask_4 = 0x00000000ffffffffLL;
static const ullong_t mask_8 = 0xffffffffffffffffLL;


#define MASK(val, length) ({			\
      ullong_t mask = 0x0LL;			\
      switch (length) {				\
      case 1:					\
	mask = mask_1;				\
      case 2:					\
	mask = mask_2;				\
      case 4:					\
	mask = mask_4;				\
      case 8:					\
	mask = mask_8;				\
      }						\
      val & mask;})				\

struct memory_operand {
  uint_t segment_size;
  addr_t segment;
  uint_t base_size;
  addr_t base;
  uint_t index_size;
  addr_t index;
  addr_t scale;
  uint_t displacement_size;
  ullong_t displacement;
};



// This returns a pointer to a V3_OPCODE_[*] array defined in vmm_decoder.h
static int get_opcode(xed_iform_enum_t iform, addr_t * opcode);

static int xed_reg_to_v3_reg(struct guest_info * info, xed_reg_enum_t xed_reg, addr_t * v3_reg, uint_t * reg_len);
static int get_memory_operand(struct guest_info * info,  xed_decoded_inst_t * xed_instr, uint_t index, struct x86_operand * operand);

static int set_decoder_mode(struct guest_info * info, xed_state_t * state) {
  switch (info->cpu_mode) {
  case REAL:
    if (state->mmode != XED_MACHINE_MODE_LEGACY_16) {
      xed_state_init(state,
		     XED_MACHINE_MODE_LEGACY_16, 
		     XED_ADDRESS_WIDTH_16b, 
		     XED_ADDRESS_WIDTH_16b); 
    }
   break;
  case PROTECTED:
  case PROTECTED_PAE:
    if (state->mmode != XED_MACHINE_MODE_LEGACY_32) {
      xed_state_init(state,
		     XED_MACHINE_MODE_LEGACY_32, 
		     XED_ADDRESS_WIDTH_32b, 
		     XED_ADDRESS_WIDTH_32b);
    }
    break;
  case LONG:
    if (state->mmode != XED_MACHINE_MODE_LONG_64) {    
      state->mmode = XED_MACHINE_MODE_LONG_64;
    }
    break;
  default:
    return -1;
  }
  return 0;
}

int is_flags_reg(xed_reg_enum_t xed_reg) {
  switch (xed_reg) {
  case XED_REG_FLAGS:
  case XED_REG_EFLAGS:
  case XED_REG_RFLAGS:
    return 1;
  default:
    return 0;
  }
}



int init_decoder() {
  xed_tables_init();
  xed_state_zero(&decoder_state);
  return 0;
}



int v3_basic_mem_decode(struct guest_info * info, addr_t instr_ptr, struct basic_instr_info * instr_info) {
  xed_decoded_inst_t xed_instr;
  xed_error_enum_t xed_error;
  

  if (set_decoder_mode(info, &decoder_state) == -1) {
    PrintError("Could not set decoder mode\n");
    return -1;
  }


  xed_decoded_inst_zero_set_mode(&xed_instr, &decoder_state);

  xed_error = xed_decode(&xed_instr, 
			 REINTERPRET_CAST(const xed_uint8_t *, instr_ptr), 
			 XED_MAX_INSTRUCTION_BYTES);

  if (xed_error != XED_ERROR_NONE) {
    PrintError("Xed error: %s\n", xed_error_enum_t2str(xed_error));
    return -1;
  }

  instr_info->instr_length = xed_decoded_inst_get_length(&xed_instr);


 if (xed_decoded_inst_number_of_memory_operands(&xed_instr) == 0) {
   PrintError("Tried to decode memory operation with no memory operands\n");
   return -1;
 }

 instr_info->op_size = xed_decoded_inst_get_memory_operand_length(&xed_instr, 0);


 xed_category_enum_t cat = xed_decoded_inst_get_category(&xed_instr);
 if (cat == XED_CATEGORY_STRINGOP) {
   instr_info->str_op = 1;
 } else {
   instr_info->str_op = 0;
 }

 xed_operand_values_t * operands = xed_decoded_inst_operands(&xed_instr);
 if (xed_operand_values_has_real_rep(operands)) {
   instr_info->has_rep = 1;
 } else {
   instr_info->has_rep = 0;
 }

 return 0;
}



int v3_decode(struct guest_info * info, addr_t instr_ptr, struct x86_instr * instr) {
  xed_decoded_inst_t xed_instr;
  xed_error_enum_t xed_error;



  if (set_decoder_mode(info, &decoder_state) == -1) {
    PrintError("Could not set decoder mode\n");
    return -1;
  }



  xed_decoded_inst_zero_set_mode(&xed_instr, &decoder_state);

  xed_error = xed_decode(&xed_instr, 
			 REINTERPRET_CAST(const xed_uint8_t *, instr_ptr), 
			 XED_MAX_INSTRUCTION_BYTES);


  if (xed_error != XED_ERROR_NONE) {
    PrintError("Xed error: %s\n", xed_error_enum_t2str(xed_error));
    return -1;
  }

  const xed_inst_t * xi = xed_decoded_inst_inst(&xed_instr);
  
  instr->instr_length = xed_decoded_inst_get_length(&xed_instr);
  instr->num_operands = xed_decoded_inst_noperands(&xed_instr);

  xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(&xed_instr);


  PrintDebug("iform=%s\n", xed_iform_enum_t2str(iform));


  if (instr->num_operands > 3) {
    PrintDebug("Special Case Not Handled\n");
    return -1;
    // special case
  } else if (instr->num_operands == 3) {
    const xed_operand_t * op = xed_inst_operand(xi, 2);
    xed_operand_enum_t op_enum = xed_operand_name(op);

    if ((!xed_operand_is_register(op_enum)) ||
	(!is_flags_reg(xed_decoded_inst_get_reg(&xed_instr, op_enum)))) {
      // special case
      PrintDebug("Special Case not handled\n");
      return -1;
    }
  }





  if (get_opcode(iform, &(instr->opcode)) == -1) {
    PrintDebug("Could not get opcode. (iform=%s)\n", xed_iform_enum_t2str(iform));
    return -1;
  }




  //PrintDebug("Number of operands: %d\n", instr->num_operands);
  //PrintDebug("INSTR length: %d\n", instr->instr_length);

  // set first operand
  if (instr->num_operands >= 1) {
    const xed_operand_t * op = xed_inst_operand(xi, 0);
    xed_operand_enum_t op_enum = xed_operand_name(op);

    struct x86_operand * v3_op = NULL;

    if (xed_operand_written(op)) {
      v3_op = &(instr->dst_operand);
    } else {
      v3_op = &(instr->src_operand);
    }


    if (xed_operand_is_register(op_enum)) {
      xed_reg_enum_t xed_reg =  xed_decoded_inst_get_reg(&xed_instr, op_enum);
      int v3_reg_type = xed_reg_to_v3_reg(info, 
					  xed_reg, 
					  &(v3_op->operand), 
					  &(v3_op->size));
					  
      if (v3_reg_type == -1) {
	PrintError("First operand is an Unhandled Operand: %s\n", xed_reg_enum_t2str(xed_reg));
	v3_op->type = INVALID_OPERAND;
	return -1;
      } else if (v3_reg_type == SEGMENT_REGISTER) {
	struct v3_segment * seg_reg = (struct v3_segment *)(v3_op->operand);
	v3_op->operand = (addr_t)&(seg_reg->selector);
      }

      v3_op->type = REG_OPERAND;
    } else {

      switch (op_enum) {

      case XED_OPERAND_MEM0:
	{
	  /*
	    struct x86_operand * operand = &(instr->dst_operand);
	    
	    if (xed_decoded_inst_mem_read(&xed_instr, 0)) {
	    operand = &(instr->src_operand);
	    } else if (xed_decoded_inst_mem_written(&xed_instr, 0)) {
	    operand = &(instr->dst_operand);
	    }
	  */
	  
	  if (get_memory_operand(info, &xed_instr, 0, v3_op) == -1) {
	    PrintError("Could not get first memory operand\n");
	    return -1;
	  }
	}
	break;

      case XED_OPERAND_MEM1:
      case XED_OPERAND_IMM1:
	// illegal
	PrintError("Illegal Operand Order\n");
	return -1;


      case XED_OPERAND_IMM0:
      case XED_OPERAND_AGEN:
      case XED_OPERAND_PTR:
      case XED_OPERAND_RELBR:
      default:
	PrintError("Unhandled Operand Type\n");
	return -1;


      }
    }
  }

  // set second operand
  if (instr->num_operands >= 2) {
    const xed_operand_t * op = xed_inst_operand(xi, 1);
    //   xed_operand_type_enum_t op_type = xed_operand_type(op);
    xed_operand_enum_t op_enum = xed_operand_name(op);
    
    struct x86_operand * v3_op;

    if (xed_operand_written(op)) {
      v3_op = &(instr->dst_operand);
    } else {
      v3_op = &(instr->src_operand);
    }


    if (xed_operand_is_register(op_enum)) {
      xed_reg_enum_t xed_reg =  xed_decoded_inst_get_reg(&xed_instr, op_enum);
      int v3_reg_type = xed_reg_to_v3_reg(info, 
					  xed_reg, 
					  &(v3_op->operand), 
					  &(v3_op->size));
      if (v3_reg_type == -1) {
	PrintError("Second operand is an Unhandled Operand: %s\n", xed_reg_enum_t2str(xed_reg));
	v3_op->type = INVALID_OPERAND;
	return -1;
      } else if (v3_reg_type == SEGMENT_REGISTER) {
	struct v3_segment * seg_reg = (struct v3_segment *)(v3_op->operand);
	v3_op->operand = (addr_t)&(seg_reg->selector);
      }

      v3_op->type = REG_OPERAND;
    

    } else {

      switch (op_enum) {

      case XED_OPERAND_MEM0:
	{

	  /*
	  if (xed_decoded_inst_mem_read(&xed_instr, 0)) {
	    v3_op = &(instr->src_operand);
	  } else if (xed_decoded_inst_mem_written(&xed_instr, 0)) {
	    v3_op = &(instr->dst_operand);
	  }
	  */

	  if (get_memory_operand(info, &xed_instr, 0, v3_op) == -1) {
	    PrintError("Could not get first memory operand\n");
	    return -1;
	  }
	}
	break;

      case XED_OPERAND_IMM0:
	{
	  instr->src_operand.size = xed_decoded_inst_get_immediate_width(&xed_instr);

	  if (instr->src_operand.size > 4) {
	    PrintError("Unhandled 64 bit immediates\n");
	    return -1;
	  }
	  instr->src_operand.operand = xed_decoded_inst_get_unsigned_immediate(&xed_instr);

	  instr->src_operand.type = IMM_OPERAND;

	}
	break;

      case XED_OPERAND_MEM1:
      case XED_OPERAND_IMM1:
	// illegal
	PrintError("Illegal Operand Order\n");
	return -1;
	
      case XED_OPERAND_AGEN:
      case XED_OPERAND_PTR:
      case XED_OPERAND_RELBR:
      default:
	PrintError("Unhandled Operand Type\n");
	return -1;
      }
    }

  }

  // set third operand
  if (instr->num_operands >= 3) {
    const xed_operand_t * op = xed_inst_operand(xi, 2);
    //  xed_operand_type_enum_t op_type = xed_operand_type(op);
    xed_operand_enum_t op_enum = xed_operand_name(op);

    if (xed_operand_is_register(op_enum)) {
      xed_reg_enum_t xed_reg =  xed_decoded_inst_get_reg(&xed_instr, op_enum);
      int v3_reg_type = xed_reg_to_v3_reg(info, 
					  xed_reg, 
					  &(instr->third_operand.operand), 
					  &(instr->third_operand.size));

      if (v3_reg_type == -1) {
	PrintError("Third operand is an Unhandled Operand: %s\n", xed_reg_enum_t2str(xed_reg));
	instr->third_operand.type = INVALID_OPERAND;
	return -1;
      } else if (v3_reg_type == SEGMENT_REGISTER) {
	struct v3_segment * seg_reg = (struct v3_segment *)(instr->third_operand.operand);
	instr->third_operand.operand = (addr_t)&(seg_reg->selector);
      }


      instr->third_operand.type = REG_OPERAND;


    } else {
      //      PrintError("Unhandled third operand type %s\n", xed_operand_type_enum_t2str(op_type));
      return -1;
    }


  }



    return 0;
}


int v3_encode(struct guest_info * info, struct x86_instr * instr, char * instr_buf) {

  return -1;
}





static int get_memory_operand(struct guest_info * info,  xed_decoded_inst_t * xed_instr, uint_t op_index, struct x86_operand * operand) {
  struct memory_operand mem_op;

  addr_t seg;
  addr_t base;
  addr_t scale;
  addr_t index;
  ullong_t displacement;
  // struct v3_segment * seg_reg;




  memset((void*)&mem_op, '\0', sizeof(struct memory_operand));

  xed_reg_enum_t xed_seg = xed_decoded_inst_get_seg_reg(xed_instr, op_index);
  if (xed_seg != XED_REG_INVALID) {
    struct v3_segment *tmp_segment;
    if (xed_reg_to_v3_reg(info, xed_seg, (addr_t *)&tmp_segment, &(mem_op.segment_size)) == -1) {
      PrintError("Unhandled Segment Register\n");
      return -1;
    }
    mem_op.segment = tmp_segment->base;
  }

  xed_reg_enum_t xed_base = xed_decoded_inst_get_base_reg(xed_instr, op_index);
  if (xed_base != XED_REG_INVALID) {
    addr_t base_reg;
    if (xed_reg_to_v3_reg(info, xed_base, &base_reg, &(mem_op.base_size)) == -1) {
      PrintError("Unhandled Base register\n");
      return -1;
    }
    mem_op.base = *(addr_t *)base_reg;
  }

  

  xed_reg_enum_t xed_idx = xed_decoded_inst_get_index_reg(xed_instr, op_index);
  if ((op_index == 0) && (xed_idx != XED_REG_INVALID)) {
    addr_t index_reg;
    
    if (xed_reg_to_v3_reg(info, xed_idx, &index_reg, &(mem_op.index_size)) == -1) {
      PrintError("Unhandled Index Register\n");
      return -1;
    }

    mem_op.index= *(addr_t *)index_reg;

    xed_uint_t xed_scale = xed_decoded_inst_get_scale(xed_instr, op_index);
    if (xed_scale != 0) {
      mem_op.scale = xed_scale;
    }
  }


  xed_uint_t disp_bits = xed_decoded_inst_get_memory_displacement_width(xed_instr, op_index);
  if (disp_bits) {
    xed_int64_t xed_disp = xed_decoded_inst_get_memory_displacement(xed_instr, op_index);

    mem_op.displacement_size = disp_bits / 8;
    mem_op.displacement = xed_disp;
    
  }

  operand->type = MEM_OPERAND;
  operand->size = xed_decoded_inst_get_memory_operand_length(xed_instr, op_index);
  
  

  PrintDebug("Struct: Seg=%x, base=%x, index=%x, scale=%x, displacement=%x\n", 
	     mem_op.segment, mem_op.base, mem_op.index, mem_op.scale, mem_op.displacement);


  seg = mem_op.segment;
  base = MASK(mem_op.base, mem_op.base_size);
  index = MASK(mem_op.index, mem_op.index_size);
  scale = mem_op.scale;
  displacement = MASK(mem_op.displacement, mem_op.displacement_size);

  PrintDebug("Seg=%x, base=%x, index=%x, scale=%x, displacement=%x\n", seg, base, index, scale, displacement);
  
  operand->operand = seg + base + (scale * index) + displacement;
  return 0;
}


static int xed_reg_to_v3_reg(struct guest_info * info, xed_reg_enum_t xed_reg, addr_t * v3_reg, uint_t * reg_len) {

  switch (xed_reg) {
  case XED_REG_INVALID:
    *v3_reg = 0;
    *reg_len = 0;
    return -1;

    /* 
     * GPRs
     */
  case XED_REG_RAX: 
    *v3_reg = (addr_t)&(info->vm_regs.rax);
    *reg_len = 8;
    return GPR_REGISTER;
  case XED_REG_EAX:
    *v3_reg = (addr_t)&(info->vm_regs.rax);
    *reg_len = 4;
    return GPR_REGISTER;
  case XED_REG_AX:
    *v3_reg = (addr_t)&(info->vm_regs.rax);
    *reg_len = 2;
    return GPR_REGISTER;
  case XED_REG_AH:
    *v3_reg = (addr_t)(&(info->vm_regs.rax)) + 1;
    *reg_len = 1;
    return GPR_REGISTER;
  case XED_REG_AL:
    *v3_reg = (addr_t)&(info->vm_regs.rax);
    *reg_len = 1;
    return GPR_REGISTER;

  case XED_REG_RCX: 
    *v3_reg = (addr_t)&(info->vm_regs.rcx);
    *reg_len = 8;
    return GPR_REGISTER;
  case XED_REG_ECX:
    *v3_reg = (addr_t)&(info->vm_regs.rcx);
    *reg_len = 4;
    return GPR_REGISTER;
  case XED_REG_CX:
    *v3_reg = (addr_t)&(info->vm_regs.rcx);
    *reg_len = 2;
    return GPR_REGISTER;
  case XED_REG_CH:
    *v3_reg = (addr_t)(&(info->vm_regs.rcx)) + 1;
    *reg_len = 1;
    return GPR_REGISTER;
  case XED_REG_CL:
    *v3_reg = (addr_t)&(info->vm_regs.rcx);
    *reg_len = 1;
    return GPR_REGISTER;

  case XED_REG_RDX: 
    *v3_reg = (addr_t)&(info->vm_regs.rdx);
    *reg_len = 8;
    return GPR_REGISTER;
  case XED_REG_EDX:
    *v3_reg = (addr_t)&(info->vm_regs.rdx);
    *reg_len = 4;
    return GPR_REGISTER;
  case XED_REG_DX:
    *v3_reg = (addr_t)&(info->vm_regs.rdx);
    *reg_len = 2;
    return GPR_REGISTER;
  case XED_REG_DH:
    *v3_reg = (addr_t)(&(info->vm_regs.rdx)) + 1;
    *reg_len = 1;
    return GPR_REGISTER;
  case XED_REG_DL:
    *v3_reg = (addr_t)&(info->vm_regs.rdx);
    *reg_len = 1;
    return GPR_REGISTER;

  case XED_REG_RBX: 
    *v3_reg = (addr_t)&(info->vm_regs.rbx);
    *reg_len = 8;
    return GPR_REGISTER;
  case XED_REG_EBX:
    *v3_reg = (addr_t)&(info->vm_regs.rbx);
    *reg_len = 4;
    return GPR_REGISTER;
  case XED_REG_BX:
    *v3_reg = (addr_t)&(info->vm_regs.rbx);
    *reg_len = 2;
    return GPR_REGISTER;
  case XED_REG_BH:
    *v3_reg = (addr_t)(&(info->vm_regs.rbx)) + 1;
    *reg_len = 1;
    return GPR_REGISTER;
  case XED_REG_BL:
    *v3_reg = (addr_t)&(info->vm_regs.rbx);
    *reg_len = 1;
    return GPR_REGISTER;


  case XED_REG_RSP:
    *v3_reg = (addr_t)&(info->vm_regs.rsp);
    *reg_len = 8;
    return GPR_REGISTER;
  case XED_REG_ESP:
    *v3_reg = (addr_t)&(info->vm_regs.rsp);
    *reg_len = 4;
    return GPR_REGISTER;
  case XED_REG_SP:
    *v3_reg = (addr_t)&(info->vm_regs.rsp);
    *reg_len = 2;
    return GPR_REGISTER;
  case XED_REG_SPL:
    *v3_reg = (addr_t)&(info->vm_regs.rsp);
    *reg_len = 1;
    return GPR_REGISTER;

  case XED_REG_RBP:
    *v3_reg = (addr_t)&(info->vm_regs.rbp);
    *reg_len = 8;
    return GPR_REGISTER;
  case XED_REG_EBP:
    *v3_reg = (addr_t)&(info->vm_regs.rbp);
    *reg_len = 4;
    return GPR_REGISTER;
  case XED_REG_BP:
    *v3_reg = (addr_t)&(info->vm_regs.rbp);
    *reg_len = 2;
    return GPR_REGISTER;
  case XED_REG_BPL:
    *v3_reg = (addr_t)&(info->vm_regs.rbp);
    *reg_len = 1;
    return GPR_REGISTER;



  case XED_REG_RSI:
    *v3_reg = (addr_t)&(info->vm_regs.rsi);
    *reg_len = 8;
    return GPR_REGISTER;
  case XED_REG_ESI:
    *v3_reg = (addr_t)&(info->vm_regs.rsi);
    *reg_len = 4;
    return GPR_REGISTER;
  case XED_REG_SI:
    *v3_reg = (addr_t)&(info->vm_regs.rsi);
    *reg_len = 2;
    return GPR_REGISTER;
  case XED_REG_SIL:
    *v3_reg = (addr_t)&(info->vm_regs.rsi);
    *reg_len = 1;
    return GPR_REGISTER;


  case XED_REG_RDI:
    *v3_reg = (addr_t)&(info->vm_regs.rdi);
    *reg_len = 8;
    return GPR_REGISTER;
  case XED_REG_EDI:
    *v3_reg = (addr_t)&(info->vm_regs.rdi);
    *reg_len = 4;
    return GPR_REGISTER;
  case XED_REG_DI:
    *v3_reg = (addr_t)&(info->vm_regs.rdi);
    *reg_len = 2;
    return GPR_REGISTER;
  case XED_REG_DIL:
    *v3_reg = (addr_t)&(info->vm_regs.rdi);
    *reg_len = 1;
    return GPR_REGISTER;


    /* 
     *  CTRL REGS
     */
  case XED_REG_RIP:
    *v3_reg = (addr_t)&(info->rip);
    *reg_len = 8;
    return CTRL_REGISTER;
  case XED_REG_EIP:
    *v3_reg = (addr_t)&(info->rip);
    *reg_len = 4;
    return CTRL_REGISTER;  
  case XED_REG_IP:
    *v3_reg = (addr_t)&(info->rip);
    *reg_len = 2;
    return CTRL_REGISTER;

  case XED_REG_FLAGS:
    *v3_reg = (addr_t)&(info->ctrl_regs.rflags);
    *reg_len = 2;
    return CTRL_REGISTER;
  case XED_REG_EFLAGS:
    *v3_reg = (addr_t)&(info->ctrl_regs.rflags);
    *reg_len = 4;
    return CTRL_REGISTER;
  case XED_REG_RFLAGS:
    *v3_reg = (addr_t)&(info->ctrl_regs.rflags);
    *reg_len = 8;
    return CTRL_REGISTER;

  case XED_REG_CR0:
    *v3_reg = (addr_t)&(info->ctrl_regs.cr0);
    *reg_len = 4;
    return CTRL_REGISTER;
  case XED_REG_CR2:
    *v3_reg = (addr_t)&(info->ctrl_regs.cr2);
    *reg_len = 4;
    return CTRL_REGISTER;
  case XED_REG_CR3:
    *v3_reg = (addr_t)&(info->ctrl_regs.cr3);
    *reg_len = 4;
    return CTRL_REGISTER;
  case XED_REG_CR4:
    *v3_reg = (addr_t)&(info->ctrl_regs.cr4);
    *reg_len = 4;
    return CTRL_REGISTER;
  case XED_REG_CR8:
    *v3_reg = (addr_t)&(info->ctrl_regs.cr8);
    *reg_len = 4;
    return CTRL_REGISTER;

  case XED_REG_CR1:
  case XED_REG_CR5:
  case XED_REG_CR6:
  case XED_REG_CR7:
  case XED_REG_CR9:
  case XED_REG_CR10:
  case XED_REG_CR11:
  case XED_REG_CR12:
  case XED_REG_CR13:
  case XED_REG_CR14:
  case XED_REG_CR15:
    return -1;




    /* 
     * SEGMENT REGS
     */
  case XED_REG_CS:
    *v3_reg = (addr_t)&(info->segments.cs);
    return SEGMENT_REGISTER;
  case XED_REG_DS:
    *v3_reg = (addr_t)&(info->segments.ds);
    return SEGMENT_REGISTER;
  case XED_REG_ES:
    *v3_reg = (addr_t)&(info->segments.es);
    return SEGMENT_REGISTER;
  case XED_REG_SS:
    *v3_reg = (addr_t)&(info->segments.ss);
    return SEGMENT_REGISTER;
  case XED_REG_FS:
    *v3_reg = (addr_t)&(info->segments.fs);
    return SEGMENT_REGISTER;
  case XED_REG_GS:
    *v3_reg = (addr_t)&(info->segments.gs);
    return SEGMENT_REGISTER;


  case XED_REG_GDTR:
  case XED_REG_LDTR:
  case XED_REG_IDTR:
  case XED_REG_TR:
    PrintError("Segment selector operand... Don't know how to handle this...\n");
    return -1;

    /* 
     *  DEBUG REGS
     */
  case XED_REG_DR0:
  case XED_REG_DR1:
  case XED_REG_DR2:
  case XED_REG_DR3:
  case XED_REG_DR4:
  case XED_REG_DR5:
  case XED_REG_DR6:
  case XED_REG_DR7:
  case XED_REG_DR8:
  case XED_REG_DR9:
  case XED_REG_DR10:
  case XED_REG_DR11:
  case XED_REG_DR12:
  case XED_REG_DR13:
  case XED_REG_DR14:
  case XED_REG_DR15:
    return -1;




  case XED_REG_R8:
  case XED_REG_R8D:
  case XED_REG_R8W:
  case XED_REG_R8B:

  case XED_REG_R9:
  case XED_REG_R9D:
  case XED_REG_R9W:
  case XED_REG_R9B:

  case XED_REG_R10:
  case XED_REG_R10D:
  case XED_REG_R10W:
  case XED_REG_R10B:

  case XED_REG_R11:
  case XED_REG_R11D:
  case XED_REG_R11W:
  case XED_REG_R11B:

  case XED_REG_R12:
  case XED_REG_R12D:
  case XED_REG_R12W:
  case XED_REG_R12B:

  case XED_REG_R13:
  case XED_REG_R13D:
  case XED_REG_R13W:
  case XED_REG_R13B:

  case XED_REG_R14:
  case XED_REG_R14D:
  case XED_REG_R14W:
  case XED_REG_R14B:

  case XED_REG_R15:
  case XED_REG_R15D:
  case XED_REG_R15W:
  case XED_REG_R15B:

  case XED_REG_XMM0:
  case XED_REG_XMM1:
  case XED_REG_XMM2:
  case XED_REG_XMM3:
  case XED_REG_XMM4:
  case XED_REG_XMM5:
  case XED_REG_XMM6:
  case XED_REG_XMM7:
  case XED_REG_XMM8:
  case XED_REG_XMM9:
  case XED_REG_XMM10:
  case XED_REG_XMM11:
  case XED_REG_XMM12:
  case XED_REG_XMM13:
  case XED_REG_XMM14:
  case XED_REG_XMM15:

  case XED_REG_MMX0:
  case XED_REG_MMX1:
  case XED_REG_MMX2:
  case XED_REG_MMX3:
  case XED_REG_MMX4:
  case XED_REG_MMX5:
  case XED_REG_MMX6:
  case XED_REG_MMX7:

  case XED_REG_ST0:
  case XED_REG_ST1:
  case XED_REG_ST2:
  case XED_REG_ST3:
  case XED_REG_ST4:
  case XED_REG_ST5:
  case XED_REG_ST6:
  case XED_REG_ST7:

  case XED_REG_ONE:
  case XED_REG_STACKPUSH:
  case XED_REG_STACKPOP:
    
  case XED_REG_TSC:
  case XED_REG_TSCAUX:
  case XED_REG_MSRS:

  case XED_REG_X87CONTROL:
  case XED_REG_X87STATUS:
  case XED_REG_X87TOP:
  case XED_REG_X87TAG:
  case XED_REG_X87PUSH:
  case XED_REG_X87POP:
  case XED_REG_X87POP2:

  case XED_REG_MXCSR:

  case XED_REG_TMP0:
  case XED_REG_TMP1:
  case XED_REG_TMP2:
  case XED_REG_TMP3:
  case XED_REG_TMP4:
  case XED_REG_TMP5:
  case XED_REG_TMP6:
  case XED_REG_TMP7:
  case XED_REG_TMP8:
  case XED_REG_TMP9:
  case XED_REG_TMP10:
  case XED_REG_TMP11:
  case XED_REG_TMP12:
  case XED_REG_TMP13:
  case XED_REG_TMP14:
  case XED_REG_TMP15:

  case XED_REG_LAST:

  case XED_REG_ERROR:
    // error??
    return -1;

  }


  return 0;
}



static int get_opcode(xed_iform_enum_t iform, addr_t * opcode) {

  switch (iform) {
  case XED_IFORM_MOV_CR_GPR64_CR:
  case XED_IFORM_MOV_CR_GPR32_CR:
    *opcode = (addr_t)&V3_OPCODE_MOVCR2;
    break;

  case XED_IFORM_MOV_CR_CR_GPR64:
  case XED_IFORM_MOV_CR_CR_GPR32:
    *opcode = (addr_t)&V3_OPCODE_MOV2CR;
    break;


  case XED_IFORM_LMSW_GPR16:
    *opcode = (addr_t)&V3_OPCODE_LMSW;
    break;

  case XED_IFORM_CLTS:
    *opcode = (addr_t)&V3_OPCODE_CLTS;
    break;

  default:
    *opcode = 0;
    return -1;
  }

  return 0;
}
