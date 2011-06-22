/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

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



#ifndef V3_CONFIG_DEBUG_DECODER
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif




static uint_t tables_inited = 0;


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
	    ullong_t mask = 0x0LL;		\
	    switch (length) {			\
		case 1:				\
		    mask = mask_1;		\
		    break;			\
		case 2:				\
		    mask = mask_2;		\
		    break;			\
		case 4:				\
		    mask = mask_4;		\
		    break;			\
		case 8:				\
		    mask = mask_8;		\
		    break;			\
	    }					\
	    val & mask;				\
	})
						
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




static v3_op_type_t get_opcode(xed_iform_enum_t iform);

static int xed_reg_to_v3_reg(struct guest_info * info, xed_reg_enum_t xed_reg, addr_t * v3_reg, uint_t * reg_len);
static int get_memory_operand(struct guest_info * info,  xed_decoded_inst_t * xed_instr, uint_t index, struct x86_operand * operand);

static int set_decoder_mode(struct guest_info * info, xed_state_t * state) {
    switch (v3_get_vm_cpu_mode(info)) {
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
	case LONG_32_COMPAT:
	    if (state->mmode != XED_MACHINE_MODE_LONG_COMPAT_32) {
		xed_state_init(state,
			       XED_MACHINE_MODE_LONG_COMPAT_32, 
			       XED_ADDRESS_WIDTH_32b, 
			       XED_ADDRESS_WIDTH_32b);
	    }
	    break;
	case LONG:
	    if (state->mmode != XED_MACHINE_MODE_LONG_64) {
		PrintDebug("Setting decoder to long mode\n");
		//      state->mmode = XED_MACHINE_MODE_LONG_64;
		//xed_state_set_machine_mode(state, XED_MACHINE_MODE_LONG_64);
		xed_state_init(state,
			       XED_MACHINE_MODE_LONG_64, 
			       XED_ADDRESS_WIDTH_64b, 
			       XED_ADDRESS_WIDTH_64b);
	    }
	    break;
	default:
	    PrintError("Unsupported CPU mode: %d\n", info->cpu_mode);
	    return -1;
    }
    return 0;
}

/*
  static int is_flags_reg(xed_reg_enum_t xed_reg) {
  switch (xed_reg) {
  case XED_REG_FLAGS:
  case XED_REG_EFLAGS:
  case XED_REG_RFLAGS:
  return 1;
  default:
  return 0;
  }
  }
*/

int v3_init_decoder(struct guest_info * info) {
    // Global library initialization, only do it once
    if (tables_inited == 0) {
	xed_tables_init();
	tables_inited = 1;
    }

    xed_state_t * decoder_state = (xed_state_t *)V3_Malloc(sizeof(xed_state_t));
    xed_state_zero(decoder_state);
    xed_state_init(decoder_state,
		   XED_MACHINE_MODE_LEGACY_32, 
		   XED_ADDRESS_WIDTH_32b, 
		   XED_ADDRESS_WIDTH_32b);

    info->decoder_state = decoder_state;

    return 0;
}



int v3_deinit_decoder(struct guest_info * core) {
    V3_Free(core->decoder_state);

    return 0;
}




static int decode_string_op(struct guest_info * info, 
			    xed_decoded_inst_t * xed_instr,  const xed_inst_t * xi,
			    struct x86_instr * instr) {

    PrintDebug("String operation\n");

    if (instr->op_type == V3_OP_MOVS) {
	instr->num_operands = 2;

	if (get_memory_operand(info, xed_instr, 0, &(instr->dst_operand)) == -1) {
	    PrintError("Could not get Destination memory operand\n");
	    return -1;
	}


	if (get_memory_operand(info, xed_instr, 1, &(instr->src_operand)) == -1) {
	    PrintError("Could not get Source memory operand\n");
	    return -1;
	}

	instr->dst_operand.write = 1;
	instr->src_operand.read = 1;

	if (instr->prefixes.rep == 1) {
	    addr_t reg_addr = 0;
	    uint_t reg_length = 0;

	    xed_reg_to_v3_reg(info, xed_decoded_inst_get_reg(xed_instr, XED_OPERAND_REG0), &reg_addr, &reg_length);
	    instr->str_op_length = MASK(*(addr_t *)reg_addr, reg_length);
	} else {
	    instr->str_op_length = 1;
	}

    } else if (instr->op_type == V3_OP_STOS) {
	instr->num_operands = 2;

	if (get_memory_operand(info, xed_instr, 0, &(instr->dst_operand)) == -1) {
	    PrintError("Could not get Destination memory operand\n");
	    return -1;
	}

	// STOS reads from rax
	xed_reg_to_v3_reg(info, xed_decoded_inst_get_reg(xed_instr, XED_OPERAND_REG0), 
			  &(instr->src_operand.operand), 
			  &(instr->src_operand.size));
	instr->src_operand.type = REG_OPERAND;
    
	instr->src_operand.read = 1;
	instr->dst_operand.write = 1;

	if (instr->prefixes.rep == 1) {
	    addr_t reg_addr = 0;
	    uint_t reg_length = 0;

	    xed_reg_to_v3_reg(info, xed_decoded_inst_get_reg(xed_instr, XED_OPERAND_REG1), 
			      &reg_addr, &reg_length);
	    instr->str_op_length = MASK(*(addr_t *)reg_addr, reg_length);
	} else {
	    instr->str_op_length = 1;
	}

    } else {
	PrintError("Unhandled String OP\n");
	return -1;
    }

    return 0;
}



int v3_disasm(struct guest_info * info, void *instr_ptr, addr_t * rip, int mark) {
    char buffer[256];
    int i;
    unsigned length;
    xed_decoded_inst_t xed_instr;
    xed_error_enum_t xed_error;

    /* disassemble the specified instruction */
    if (set_decoder_mode(info, info->decoder_state) == -1) {
	PrintError("Could not set decoder mode\n");
	return -1;
    }

    xed_decoded_inst_zero_set_mode(&xed_instr, info->decoder_state);

    xed_error = xed_decode(&xed_instr, 
			   REINTERPRET_CAST(const xed_uint8_t *, instr_ptr), 
			   XED_MAX_INSTRUCTION_BYTES);

    if (xed_error != XED_ERROR_NONE) {
	PrintError("Xed error: %s\n", xed_error_enum_t2str(xed_error));
	return -1;
    }

    /* obtain string representation in AT&T syntax */
    if (!xed_format_att(&xed_instr, buffer, sizeof(buffer), *rip)) {
	PrintError("Xed error: cannot disaaemble\n");
	return -1;
    }

    /* print address, opcode bytes and the disassembled instruction */
    length = xed_decoded_inst_get_length(&xed_instr);
    V3_Print("0x%p %c ", (void *) *rip, mark ? '*' : ' ');
    for (i = 0; i < length; i++) {
    	unsigned char b = ((unsigned char *) instr_ptr)[i];
    	V3_Print("%x%x ", b >> 4, b & 0xf);
    }
    while (i++ < 8) {
    	V3_Print("   ");
    }
    V3_Print("%s\n", buffer);

    /* move on to next instruction */
    *rip += length;
    return 0;
}



int v3_decode(struct guest_info * info, addr_t instr_ptr, struct x86_instr * instr) {
    xed_decoded_inst_t xed_instr;
    xed_error_enum_t xed_error;

    memset(instr, 0, sizeof(struct x86_instr));


    v3_get_prefixes((uchar_t *)instr_ptr, &(instr->prefixes));

    if (set_decoder_mode(info, info->decoder_state) == -1) {
	PrintError("Could not set decoder mode\n");
	return -1;
    }

    xed_decoded_inst_zero_set_mode(&xed_instr, info->decoder_state);

    xed_error = xed_decode(&xed_instr, 
			   REINTERPRET_CAST(const xed_uint8_t *, instr_ptr), 
			   XED_MAX_INSTRUCTION_BYTES);


    if (xed_error != XED_ERROR_NONE) {
	PrintError("Xed error: %s\n", xed_error_enum_t2str(xed_error));
	return -1;
    }

    const xed_inst_t * xi = xed_decoded_inst_inst(&xed_instr);
  
    instr->instr_length = xed_decoded_inst_get_length(&xed_instr);


    xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(&xed_instr);

#ifdef V3_CONFIG_DEBUG_DECODER
    xed_iclass_enum_t iclass = xed_decoded_inst_get_iclass(&xed_instr);

    PrintDebug("iform=%s, iclass=%s\n", xed_iform_enum_t2str(iform), xed_iclass_enum_t2str(iclass));
#endif


    if ((instr->op_type = get_opcode(iform)) == V3_INVALID_OP) {
	PrintError("Could not get opcode. (iform=%s)\n", xed_iform_enum_t2str(iform));
	return -1;
    }


    // We special case the string operations...
    if (xed_decoded_inst_get_category(&xed_instr) == XED_CATEGORY_STRINGOP) {
	instr->is_str_op = 1;
	return decode_string_op(info, &xed_instr, xi, instr); 
    } else {
	instr->is_str_op = 0;
	instr->str_op_length = 0;
    }

    instr->num_operands = xed_decoded_inst_noperands(&xed_instr);

    /*
      if (instr->num_operands > 3) {
      PrintDebug("Special Case Not Handled (more than 3 operands) (iform=%s)\n", xed_iform_enum_t2str(iform)
      return -1;
      // special case
      } else if (instr->num_operands == 3) {
      const xed_operand_t * op = xed_inst_operand(xi, 2);
      xed_operand_enum_t op_enum = xed_operand_name(op);
      
      if ((!xed_operand_is_register(op_enum)) ||
      (!is_flags_reg(xed_decoded_inst_get_reg(&xed_instr, op_enum)))) {
      // special case
      PrintError("Special Case not handled (iform=%s)\n", xed_iform_enum_t2str(iform));
      return -1;
      }
      }
    */

    //PrintDebug("Number of operands: %d\n", instr->num_operands);
    //PrintDebug("INSTR length: %d\n", instr->instr_length);

    // set first operand
    if (instr->num_operands >= 1) {
	const xed_operand_t * op = xed_inst_operand(xi, 0);
	xed_operand_enum_t op_enum = xed_operand_name(op);

	struct x86_operand * v3_op = NULL;

	/*
	  if (xed_operand_written(op)) {
	  v3_op = &(instr->dst_operand);
	  } else {
	  v3_op = &(instr->src_operand);
	  }
	*/

	v3_op = &(instr->dst_operand);

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
			PrintDebug("Memory operand (1)\n");
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
		    {
                v3_op->size = xed_decoded_inst_get_immediate_width(&xed_instr);

                if (v3_op->size > 4) {
                    PrintError("Unhandled 64 bit immediates\n");
                    return -1;
                }
                v3_op->operand = xed_decoded_inst_get_unsigned_immediate(&xed_instr);

                v3_op->type = IMM_OPERAND;

		    }
		    break;
		case XED_OPERAND_AGEN:
		case XED_OPERAND_PTR:
		case XED_OPERAND_RELBR:
		default:
		    PrintError("Unhandled Operand Type\n");
		    return -1;
	    }
	}

//	V3_Print("Operand 0 mode: %s\n", xed_operand_action_enum_t2str(xed_operand_rw(op)));


	if (xed_operand_read(op)) {
	    v3_op->read = 1;
	}

	if (xed_operand_written(op)) {
	    v3_op->write = 1;
	}

    }

    // set second operand
    if (instr->num_operands >= 2) {
	const xed_operand_t * op = xed_inst_operand(xi, 1);
	//   xed_operand_type_enum_t op_type = xed_operand_type(op);
	xed_operand_enum_t op_enum = xed_operand_name(op);
    
	struct x86_operand * v3_op;

	/*
	  if (xed_operand_written(op)) {
	  v3_op = &(instr->dst_operand);
	  } else {
	  v3_op = &(instr->src_operand);
	  }
	*/
	v3_op = &(instr->src_operand);


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
			PrintDebug("Memory operand (2)\n");
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

//	V3_Print("Operand 1 mode: %s\n", xed_operand_action_enum_t2str(xed_operand_rw(op)));

	if (xed_operand_read(op)) {
	    v3_op->read = 1;
	}

	if (xed_operand_written(op)) {
	    v3_op->write = 1;
	}

    }

    // set third operand
    if (instr->num_operands >= 3) {
	const xed_operand_t * op = xed_inst_operand(xi, 2);
	xed_operand_type_enum_t op_type = xed_operand_type(op);
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

	    PrintDebug("Operand 2 mode: %s\n", xed_operand_action_enum_t2str(xed_operand_rw(op)));


	    if (xed_operand_read(op)) {
		instr->third_operand.read = 1;
	    }

	    if (xed_operand_written(op)) {
		instr->third_operand.write = 1;
	    }

	} else {
	    PrintError("Unhandled third operand type %s\n", xed_operand_type_enum_t2str(op_type));
	    instr->num_operands = 2;
	}
    }

    return 0;
}


int v3_encode(struct guest_info * info, struct x86_instr * instr, uint8_t * instr_buf) {

    return -1;
}





static int get_memory_operand(struct guest_info * info,  xed_decoded_inst_t * xed_instr, uint_t op_index, struct x86_operand * operand) {
    struct memory_operand mem_op;

    addr_t seg;
    addr_t base;
    addr_t scale;
    addr_t index;
    ullong_t displacement;
    int addr_width = v3_get_addr_width(info);
    v3_cpu_mode_t cpu_mode = v3_get_vm_cpu_mode(info);
    // struct v3_segment * seg_reg;

    PrintDebug("Xed mode = %s\n", xed_machine_mode_enum_t2str(xed_state_get_machine_mode(info->decoder_state)));
    PrintDebug("Address width: %s\n",
	       xed_address_width_enum_t2str(xed_state_get_address_width(info->decoder_state)));
    PrintDebug("Stack Address width: %s\n",
	       xed_address_width_enum_t2str(xed_state_get_stack_address_width(info->decoder_state)));

  

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

	mem_op.displacement_size = disp_bits;
	mem_op.displacement = xed_disp;
    }

    operand->type = MEM_OPERAND;
    operand->size = xed_decoded_inst_get_memory_operand_length(xed_instr, op_index);
  
  

    PrintDebug("Struct: Seg=%p (size=%d), base=%p, index=%p, scale=%p, displacement=%p (size=%d)\n", 
	       (void *)mem_op.segment, mem_op.segment_size, (void*)mem_op.base, (void *)mem_op.index, 
	       (void *)mem_op.scale, (void *)(addr_t)mem_op.displacement, mem_op.displacement_size);


    PrintDebug("operand size: %d\n", operand->size);

    seg = MASK(mem_op.segment, mem_op.segment_size);
    base = MASK(mem_op.base, mem_op.base_size);
    index = MASK(mem_op.index, mem_op.index_size);
    scale = mem_op.scale;

    // XED returns the displacement as a 2s complement signed number, but it can
    // have different sizes, depending on the instruction encoding.
    // we put that into a 64 bit unsigned (the unsigned doesn't matter since
    // we only ever do 2s complement arithmetic on it.   However, this means we
    // need to sign-extend what XED provides through 64 bits.
    displacement = mem_op.displacement;
    displacement <<= 64 - mem_op.displacement_size * 8;
    displacement = ((sllong_t)displacement) >> (64 - mem_op.displacement_size * 8);
    

    PrintDebug("Seg=%p, base=%p, index=%p, scale=%p, displacement=%p\n", 
	       (void *)seg, (void *)base, (void *)index, (void *)scale, (void *)(addr_t)displacement);
  
    if (cpu_mode == REAL) {
	operand->operand = seg +  MASK((base + (scale * index) + displacement), addr_width);
    } else {
	operand->operand = MASK((seg + base + (scale * index) + displacement), addr_width);
    }

    return 0;
}


static int xed_reg_to_v3_reg(struct guest_info * info, xed_reg_enum_t xed_reg, 
			     addr_t * v3_reg, uint_t * reg_len) {

    PrintDebug("Xed Register: %s\n", xed_reg_enum_t2str(xed_reg));

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





	case XED_REG_R8:
	    *v3_reg = (addr_t)&(info->vm_regs.r8);
	    *reg_len = 8;
	    return GPR_REGISTER;
	case XED_REG_R8D:
	    *v3_reg = (addr_t)&(info->vm_regs.r8);
	    *reg_len = 4;
	    return GPR_REGISTER;
	case XED_REG_R8W:
	    *v3_reg = (addr_t)&(info->vm_regs.r8);
	    *reg_len = 2;
	    return GPR_REGISTER;
	case XED_REG_R8B:
	    *v3_reg = (addr_t)&(info->vm_regs.r8);
	    *reg_len = 1;
	    return GPR_REGISTER;

	case XED_REG_R9:
	    *v3_reg = (addr_t)&(info->vm_regs.r9);
	    *reg_len = 8;
	    return GPR_REGISTER;
	case XED_REG_R9D:
	    *v3_reg = (addr_t)&(info->vm_regs.r9);
	    *reg_len = 4;
	    return GPR_REGISTER;
	case XED_REG_R9W:
	    *v3_reg = (addr_t)&(info->vm_regs.r9);
	    *reg_len = 2;
	    return GPR_REGISTER;
	case XED_REG_R9B:
	    *v3_reg = (addr_t)&(info->vm_regs.r9);
	    *reg_len = 1;
	    return GPR_REGISTER;

	case XED_REG_R10:
	    *v3_reg = (addr_t)&(info->vm_regs.r10);
	    *reg_len = 8;
	    return GPR_REGISTER;
	case XED_REG_R10D:
	    *v3_reg = (addr_t)&(info->vm_regs.r10);
	    *reg_len = 4;
	    return GPR_REGISTER;
	case XED_REG_R10W:
	    *v3_reg = (addr_t)&(info->vm_regs.r10);
	    *reg_len = 2;
	    return GPR_REGISTER;
	case XED_REG_R10B:
	    *v3_reg = (addr_t)&(info->vm_regs.r10);
	    *reg_len = 1;
	    return GPR_REGISTER;

	case XED_REG_R11:
	    *v3_reg = (addr_t)&(info->vm_regs.r11);
	    *reg_len = 8;
	    return GPR_REGISTER;
	case XED_REG_R11D:
	    *v3_reg = (addr_t)&(info->vm_regs.r11);
	    *reg_len = 4;
	    return GPR_REGISTER;
	case XED_REG_R11W:
	    *v3_reg = (addr_t)&(info->vm_regs.r11);
	    *reg_len = 2;
	    return GPR_REGISTER;
	case XED_REG_R11B:
	    *v3_reg = (addr_t)&(info->vm_regs.r11);
	    *reg_len = 1;
	    return GPR_REGISTER;

	case XED_REG_R12:
	    *v3_reg = (addr_t)&(info->vm_regs.r12);
	    *reg_len = 8;
	    return GPR_REGISTER;
	case XED_REG_R12D:
	    *v3_reg = (addr_t)&(info->vm_regs.r12);
	    *reg_len = 4;
	    return GPR_REGISTER;
	case XED_REG_R12W:
	    *v3_reg = (addr_t)&(info->vm_regs.r12);
	    *reg_len = 2;
	    return GPR_REGISTER;
	case XED_REG_R12B:
	    *v3_reg = (addr_t)&(info->vm_regs.r12);
	    *reg_len = 1;
	    return GPR_REGISTER;

	case XED_REG_R13:
	    *v3_reg = (addr_t)&(info->vm_regs.r13);
	    *reg_len = 8;
	    return GPR_REGISTER;
	case XED_REG_R13D:
	    *v3_reg = (addr_t)&(info->vm_regs.r13);
	    *reg_len = 4;
	    return GPR_REGISTER;
	case XED_REG_R13W:
	    *v3_reg = (addr_t)&(info->vm_regs.r13);
	    *reg_len = 2;
	    return GPR_REGISTER;
	case XED_REG_R13B:
	    *v3_reg = (addr_t)&(info->vm_regs.r13);
	    *reg_len = 1;
	    return GPR_REGISTER;

	case XED_REG_R14:
	    *v3_reg = (addr_t)&(info->vm_regs.r14);
	    *reg_len = 8;
	    return GPR_REGISTER;
	case XED_REG_R14D:
	    *v3_reg = (addr_t)&(info->vm_regs.r14);
	    *reg_len = 4;
	    return GPR_REGISTER;
	case XED_REG_R14W:
	    *v3_reg = (addr_t)&(info->vm_regs.r14);
	    *reg_len = 2;
	    return GPR_REGISTER;
	case XED_REG_R14B:
	    *v3_reg = (addr_t)&(info->vm_regs.r14);
	    *reg_len = 1;
	    return GPR_REGISTER;

	case XED_REG_R15:
	    *v3_reg = (addr_t)&(info->vm_regs.r15);
	    *reg_len = 8;
	    return GPR_REGISTER;
	case XED_REG_R15D:
	    *v3_reg = (addr_t)&(info->vm_regs.r15);
	    *reg_len = 4;
	    return GPR_REGISTER;
	case XED_REG_R15W:
	    *v3_reg = (addr_t)&(info->vm_regs.r15);
	    *reg_len = 2;
	    return GPR_REGISTER;
	case XED_REG_R15B:
	    *v3_reg = (addr_t)&(info->vm_regs.r15);
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
	    *reg_len = 8;
	    return SEGMENT_REGISTER;
	case XED_REG_DS:
	    *v3_reg = (addr_t)&(info->segments.ds);
	    *reg_len = 8;
	    return SEGMENT_REGISTER;
	case XED_REG_ES:
	    *v3_reg = (addr_t)&(info->segments.es);
	    *reg_len = 8;
	    return SEGMENT_REGISTER;
	case XED_REG_SS:
	    *v3_reg = (addr_t)&(info->segments.ss);
	    *reg_len = 8;
	    return SEGMENT_REGISTER;
	case XED_REG_FS:
	    *v3_reg = (addr_t)&(info->segments.fs);
	    *reg_len = 8;
	    return SEGMENT_REGISTER;
	case XED_REG_GS:
	    *v3_reg = (addr_t)&(info->segments.gs);
	    *reg_len = 8;
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



static v3_op_type_t get_opcode(xed_iform_enum_t iform) {

    switch (iform) {

	/* Control Instructions */

	case XED_IFORM_MOV_CR_GPR64_CR:
	case XED_IFORM_MOV_CR_GPR32_CR:
	    return V3_OP_MOVCR2;

	case XED_IFORM_MOV_CR_CR_GPR64:
	case XED_IFORM_MOV_CR_CR_GPR32:
	    return V3_OP_MOV2CR;

	case XED_IFORM_SMSW_GPRv:
	    return V3_OP_SMSW;

	case XED_IFORM_LMSW_GPR16:
	    return V3_OP_LMSW;

	case XED_IFORM_CLTS:
	    return V3_OP_CLTS;

	case XED_IFORM_INVLPG_MEMb:
	    return V3_OP_INVLPG;

    case XED_IFORM_INT_IMM:
        return V3_OP_INT;


	    /* Data Instructions */

	    // Write
	case XED_IFORM_ADC_MEMv_GPRv:
	case XED_IFORM_ADC_MEMv_IMM:
	case XED_IFORM_ADC_MEMb_GPR8:
	case XED_IFORM_ADC_MEMb_IMM:
	    // Read
	case XED_IFORM_ADC_GPRv_MEMv:
	case XED_IFORM_ADC_GPR8_MEMb:
	    return V3_OP_ADC;

	    // Write
	case XED_IFORM_ADD_MEMv_GPRv:
	case XED_IFORM_ADD_MEMb_IMM:
	case XED_IFORM_ADD_MEMb_GPR8:
	case XED_IFORM_ADD_MEMv_IMM:
	    // Read
	case XED_IFORM_ADD_GPRv_MEMv:
	case XED_IFORM_ADD_GPR8_MEMb:
	    return V3_OP_ADD;

	    // Write
	case XED_IFORM_AND_MEMv_IMM:
	case XED_IFORM_AND_MEMb_GPR8:
	case XED_IFORM_AND_MEMv_GPRv:
	case XED_IFORM_AND_MEMb_IMM:
	    // Read
	case XED_IFORM_AND_GPR8_MEMb:
	case XED_IFORM_AND_GPRv_MEMv:
	    return V3_OP_AND;

	    // Write
	case XED_IFORM_SUB_MEMv_IMM:
	case XED_IFORM_SUB_MEMb_GPR8:
	case XED_IFORM_SUB_MEMb_IMM:
	case XED_IFORM_SUB_MEMv_GPRv:
	    // Read
	case XED_IFORM_SUB_GPR8_MEMb:
	case XED_IFORM_SUB_GPRv_MEMv:
	    return V3_OP_SUB;

	    // Write
	case XED_IFORM_MOV_MEMv_GPRv:
	case XED_IFORM_MOV_MEMb_GPR8:
	case XED_IFORM_MOV_MEMv_OrAX:
	case XED_IFORM_MOV_MEMb_AL:
	case XED_IFORM_MOV_MEMv_IMM:
	case XED_IFORM_MOV_MEMb_IMM:
	    // Read 
	case XED_IFORM_MOV_GPRv_MEMv:
	case XED_IFORM_MOV_GPR8_MEMb:
	case XED_IFORM_MOV_OrAX_MEMv:
	case XED_IFORM_MOV_AL_MEMb:
	    return V3_OP_MOV;


	    // Read 
	case XED_IFORM_MOVZX_GPRv_MEMb:
	case XED_IFORM_MOVZX_GPRv_MEMw:
	    return V3_OP_MOVZX;

	    // Read 
	case XED_IFORM_MOVSX_GPRv_MEMb:
	case XED_IFORM_MOVSX_GPRv_MEMw:
	    return V3_OP_MOVSX;



	case XED_IFORM_DEC_MEMv:
	case XED_IFORM_DEC_MEMb:
	    return V3_OP_DEC;

	case XED_IFORM_INC_MEMb:
	case XED_IFORM_INC_MEMv:
	    return V3_OP_INC;

	    // Write
	case XED_IFORM_OR_MEMv_IMM:
	case XED_IFORM_OR_MEMb_IMM:
	case XED_IFORM_OR_MEMv_GPRv:
	case XED_IFORM_OR_MEMb_GPR8:
	    // Read
	case XED_IFORM_OR_GPRv_MEMv:
	case XED_IFORM_OR_GPR8_MEMb:
	    return V3_OP_OR;

	    // Write
	case XED_IFORM_XOR_MEMv_GPRv:
	case XED_IFORM_XOR_MEMb_IMM:
	case XED_IFORM_XOR_MEMb_GPR8:
	case XED_IFORM_XOR_MEMv_IMM:
	    // Read
	case XED_IFORM_XOR_GPRv_MEMv:
	case XED_IFORM_XOR_GPR8_MEMb:
	    return V3_OP_XOR;

	case XED_IFORM_NEG_MEMb:
	case XED_IFORM_NEG_MEMv:
	    return V3_OP_NEG;

	case XED_IFORM_NOT_MEMv:
	case XED_IFORM_NOT_MEMb:
	    return V3_OP_NOT;

	case XED_IFORM_XCHG_MEMv_GPRv:
	case XED_IFORM_XCHG_MEMb_GPR8:
	    return V3_OP_XCHG;

	case XED_IFORM_SETB_MEMb:
	    return V3_OP_SETB;

	case XED_IFORM_SETBE_MEMb:
	    return V3_OP_SETBE;

	case XED_IFORM_SETL_MEMb:
	    return V3_OP_SETL;

	case XED_IFORM_SETLE_MEMb:
	    return V3_OP_SETLE;

	case XED_IFORM_SETNB_MEMb:
	    return V3_OP_SETNB;

	case XED_IFORM_SETNBE_MEMb:
	    return V3_OP_SETNBE;

	case XED_IFORM_SETNL_MEMb:
	    return V3_OP_SETNL;

	case XED_IFORM_SETNLE_MEMb:
	    return V3_OP_SETNLE;

	case XED_IFORM_SETNO_MEMb:
	    return V3_OP_SETNO;
    
	case XED_IFORM_SETNP_MEMb:
	    return V3_OP_SETNP;

	case XED_IFORM_SETNS_MEMb:
	    return V3_OP_SETNS;

	case XED_IFORM_SETNZ_MEMb:
	    return V3_OP_SETNZ;

	case XED_IFORM_SETO_MEMb:
	    return V3_OP_SETO;
    
	case XED_IFORM_SETP_MEMb:
	    return V3_OP_SETP;

	case XED_IFORM_SETS_MEMb:
	    return V3_OP_SETS;

	case XED_IFORM_SETZ_MEMb:
	    return V3_OP_SETZ;

	case XED_IFORM_MOVSB:
	case XED_IFORM_MOVSW:
	case XED_IFORM_MOVSD:
	case XED_IFORM_MOVSQ:
	    return V3_OP_MOVS;

	case XED_IFORM_STOSB:
	case XED_IFORM_STOSW:
	case XED_IFORM_STOSD:
	case XED_IFORM_STOSQ:
	    return V3_OP_STOS;


	default:
	    return V3_INVALID_OP;
    }
}
