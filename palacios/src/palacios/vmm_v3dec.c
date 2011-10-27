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

#include <palacios/vmm_decoder.h>
#include <palacios/vmm_instr_decoder.h>

#ifndef V3_CONFIG_DEBUG_DECODER
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#define MASK(val, length) ({						\
            uint64_t mask = 0x0LL;					\
            switch (length) {						\
		case 1:							\
		    mask = 0x00000000000000ffLL;			\
                    break;						\
                case 2:							\
                    mask = 0x000000000000ffffLL;			\
                    break;						\
                case 4:							\
                    mask = 0x00000000ffffffffLL;			\
                    break;						\
                case 8:							\
                    mask = 0xffffffffffffffffLL;			\
                    break;						\
            }								\
            val & mask;							\
        })

static v3_op_type_t op_form_to_type(op_form_t form);
static int parse_operands(struct guest_info * core, uint8_t * instr_ptr, struct x86_instr * instr, op_form_t form);


int v3_disasm(struct guest_info * info, void *instr_ptr, addr_t * rip, int mark) {
    return -1;
}


int v3_init_decoder(struct guest_info * core) { 
    return 0;
}


int v3_deinit_decoder(struct guest_info * core) {
    return 0;
}


int v3_encode(struct guest_info * info, struct x86_instr * instr, uint8_t * instr_buf) {
    return -1;
}


int v3_decode(struct guest_info * core, addr_t instr_ptr, struct x86_instr * instr) {
    op_form_t form = INVALID_INSTR; 
    int ret = 0;
    int length = 0;


    PrintDebug("Decoding Instruction at %p\n", (void *)instr_ptr);

    memset(instr, 0, sizeof(struct x86_instr));

    // scan for prefixes
    length = v3_get_prefixes((uint8_t *)instr_ptr, &(instr->prefixes));


    // REX prefix
    if (v3_get_vm_cpu_mode(core) == LONG) {
	uint8_t prefix = *(uint8_t *)(instr_ptr + length);

	if ((prefix & 0xf0) == 0x40) {
	    instr->prefixes.rex = 1;

	    instr->prefixes.rex_rm = (prefix & 0x01);
	    instr->prefixes.rex_sib_idx = ((prefix & 0x02) >> 1);
	    instr->prefixes.rex_reg = ((prefix & 0x04) >> 2);
	    instr->prefixes.rex_op_size = ((prefix & 0x08) >> 3);

	    length += 1;
	}
    }


    form = op_code_to_form((uint8_t *)(instr_ptr + length), &length);

    PrintDebug("\t decoded as (%s)\n", op_form_to_str(form));

    if (form == INVALID_INSTR) {
	PrintError("Could not find instruction form (%x)\n", *(uint32_t *)(instr_ptr + length));
	return -1;
    }

    instr->op_type = op_form_to_type(form);

    ret = parse_operands(core, (uint8_t *)(instr_ptr + length), instr, form);

    if (ret == -1) {
	PrintError("Could not parse instruction operands\n");
	return -1;
    }
    length += ret;

    instr->instr_length += length;

#ifdef V3_CONFIG_DEBUG_DECODER
    V3_Print("Decoding Instr at %p\n", (void *)core->rip);
    v3_print_instr(instr);
    V3_Print("CS DB FLag=%x\n", core->segments.cs.db);
#endif

    return 0;
}


static int parse_operands(struct guest_info * core, uint8_t * instr_ptr, 
			  struct x86_instr * instr, op_form_t form) {
    // get operational mode of the guest for operand width
    uint8_t operand_width = get_operand_width(core, instr, form);
    uint8_t addr_width = get_addr_width(core, instr);
    int ret = 0;
    uint8_t * instr_start = instr_ptr;
    

    PrintDebug("\tOperand width=%d, Addr width=%d\n", operand_width, addr_width);

    switch (form) {
	case ADC_IMM2_8:
	case ADD_IMM2_8:
	case AND_IMM2_8:
	case OR_IMM2_8:
  	case SUB_IMM2_8:
	case XOR_IMM2_8:
	case MOV_IMM2_8:
	case ADC_IMM2:
	case ADD_IMM2:
	case AND_IMM2:	
	case OR_IMM2:
  	case SUB_IMM2:
	case XOR_IMM2:
	case MOV_IMM2: {
	    uint8_t reg_code = 0;

	    ret = decode_rm_operand(core, instr_ptr, form, instr, &(instr->dst_operand), &reg_code);

	    if (ret == -1) {
		PrintError("Error decoding operand\n");
		return -1;
	    }

	    instr_ptr += ret;

	    instr->src_operand.type = IMM_OPERAND;
	    instr->src_operand.size = operand_width;


	    if (operand_width == 1) {
		instr->src_operand.operand = *(uint8_t *)instr_ptr;
	    } else if (operand_width == 2) {
		instr->src_operand.operand = *(uint16_t *)instr_ptr;
	    } else if (operand_width == 4) {
		instr->src_operand.operand = *(uint32_t *)instr_ptr;
	    } else if (operand_width == 8) {
		instr->src_operand.operand = *(sint32_t *)instr_ptr; // This is a special case for sign extended 64bit ops
	    } else {
		PrintError("Illegal operand width (%d)\n", operand_width);
		return -1;
	    }

	    instr->src_operand.read = 1;
	    instr->dst_operand.write = 1;

	    instr_ptr += operand_width;

	    instr->num_operands = 2;

	    break;
	}
	case ADC_2MEM_8:
	case ADD_2MEM_8:
	case AND_2MEM_8:
	case OR_2MEM_8:
	case SUB_2MEM_8:
	case XOR_2MEM_8:
	case MOV_2MEM_8:
	case ADC_2MEM:
	case ADD_2MEM:
	case AND_2MEM:
	case OR_2MEM:
	case SUB_2MEM:
	case XOR_2MEM:
	case MOV_2MEM: {
	    uint8_t reg_code = 0;

	    ret = decode_rm_operand(core, instr_ptr, form, instr, &(instr->dst_operand), &reg_code);

	    if (ret == -1) {
		PrintError("Error decoding operand\n");
		return -1;
	    }

	    instr_ptr += ret;

	    instr->src_operand.type = REG_OPERAND;
	    instr->src_operand.size = operand_width;

	    instr->src_operand.read = 1;
	    instr->dst_operand.write = 1;


	    decode_gpr(core, reg_code, &(instr->src_operand));

	    instr->num_operands = 2;
	    break;
	}
	case ADC_MEM2_8:
	case ADD_MEM2_8:
	case AND_MEM2_8:
	case OR_MEM2_8:
	case SUB_MEM2_8:
	case XOR_MEM2_8:
	case MOV_MEM2_8:
	case ADC_MEM2:
	case ADD_MEM2:
	case AND_MEM2:
	case OR_MEM2:
	case SUB_MEM2:
	case XOR_MEM2:
	case MOV_MEM2: {
	    uint8_t reg_code = 0;

	    ret = decode_rm_operand(core, instr_ptr, form, instr, &(instr->src_operand), &reg_code);

	    if (ret == -1) {
		PrintError("Error decoding operand\n");
		return -1;
	    }

	    instr_ptr += ret;

	    instr->dst_operand.size = operand_width;
	    instr->dst_operand.type = REG_OPERAND;
	    decode_gpr(core, reg_code, &(instr->dst_operand));

	    instr->src_operand.read = 1;
	    instr->dst_operand.write = 1;

	    instr->num_operands = 2;

	    break;
	}
	case MOVSX_8:
	case MOVZX_8: {
	    uint8_t reg_code = 0;

	    ret = decode_rm_operand(core, instr_ptr, form, instr, &(instr->src_operand), &reg_code);
	    instr->src_operand.size = 1;

	    if (ret == -1) {
		PrintError("Error decoding operand\n");
		return -1;
	    }

	    instr_ptr += ret;

	    instr->dst_operand.size = operand_width;
	    instr->dst_operand.type = REG_OPERAND;
	    decode_gpr(core, reg_code, &(instr->dst_operand));

	    instr->src_operand.read = 1;
	    instr->dst_operand.write = 1;

	    instr->num_operands = 2;

	    break;
	}
	case MOVSX:
	case MOVZX: {
	    uint8_t reg_code = 0;

	    ret = decode_rm_operand(core, instr_ptr, form, instr, &(instr->src_operand), &reg_code);
	    instr->src_operand.size = 2;

	    if (ret == -1) {
		PrintError("Error decoding operand\n");
		return -1;
	    }

	    instr_ptr += ret;

	    instr->dst_operand.size = operand_width;
	    instr->dst_operand.type = REG_OPERAND;
	    decode_gpr(core, reg_code, &(instr->dst_operand));

	    instr->src_operand.read = 1;
	    instr->dst_operand.write = 1;

	    instr->num_operands = 2;

	    break;
	}
	case ADC_IMM2SX_8:
	case ADD_IMM2SX_8:
	case AND_IMM2SX_8:
	case OR_IMM2SX_8:
	case SUB_IMM2SX_8:
	case XOR_IMM2SX_8: {
	    uint8_t reg_code = 0;

	    ret = decode_rm_operand(core, instr_ptr, form, instr, &(instr->dst_operand), &reg_code);

	    if (ret == -1) {
		PrintError("Error decoding operand\n");
		return -1;
	    }

	    instr_ptr += ret;

	    instr->src_operand.type = IMM_OPERAND;
	    instr->src_operand.size = operand_width;
	    instr->src_operand.operand = (addr_t)MASK((sint64_t)*(sint8_t *)instr_ptr, operand_width);  // sign extend.

	    instr->src_operand.read = 1;
	    instr->dst_operand.write = 1;

	    instr_ptr += 1;

	    instr->num_operands = 2;

	    break;
	}
	case MOVS:
	case MOVS_8: {
	    instr->is_str_op = 1;
	    
	    if (instr->prefixes.rep == 1) {
		instr->str_op_length = MASK(core->vm_regs.rcx, addr_width);
	    } else {
		instr->str_op_length = 1;
	    }

	    // Source: DS:(E)SI
	    // Destination: ES:(E)DI

	    instr->src_operand.type = MEM_OPERAND;
	    instr->src_operand.size = operand_width;
	    instr->src_operand.operand = get_addr_linear(core,  MASK(core->vm_regs.rsi, addr_width), &(core->segments.ds));


	    instr->dst_operand.type = MEM_OPERAND;
	    instr->dst_operand.size = operand_width;
	    instr->dst_operand.operand = get_addr_linear(core, MASK(core->vm_regs.rdi, addr_width), &(core->segments.es));


	    instr->src_operand.read = 1;
	    instr->dst_operand.write = 1;

	    instr->num_operands = 2;

	    break;
	}
	case MOV_2CR: {
	    uint8_t reg_code = 0;
	    
	    ret = decode_rm_operand(core, instr_ptr, form, instr, &(instr->src_operand),
				    &reg_code);

	    if (ret == -1) {
		PrintError("Error decoding operand for (%s)\n", op_form_to_str(form));
		return -1;
	    }
		
	    instr_ptr += ret;

	    instr->dst_operand.type = REG_OPERAND;
	    instr->dst_operand.size = operand_width;
	    decode_cr(core, reg_code, &(instr->dst_operand));
	    
	    instr->src_operand.read = 1;
	    instr->dst_operand.write = 1;

	    instr->num_operands = 2;
	    break;
	}
	case MOV_CR2: {
	    uint8_t reg_code = 0;
	    
	    ret = decode_rm_operand(core, instr_ptr, form, instr, &(instr->dst_operand),
				    &reg_code);
	    
	    if (ret == -1) {
		PrintError("Error decoding operand for (%s)\n", op_form_to_str(form));
		return -1;
	    }

	    instr_ptr += ret;
	    
	    instr->src_operand.type = REG_OPERAND;
	    instr->src_operand.size = operand_width;
	    decode_cr(core, reg_code, &(instr->src_operand));

	    instr->src_operand.read = 1;
	    instr->dst_operand.write = 1;

	    instr->num_operands = 2;
	    break;
	}
	case STOS:
	case STOS_8: {
	    instr->is_str_op = 1;

	    if (instr->prefixes.rep == 1) {
		instr->str_op_length = MASK(core->vm_regs.rcx, addr_width);
	    } else {
		instr->str_op_length = 1;
	    }

	    instr->src_operand.size = operand_width;
	    instr->src_operand.type = REG_OPERAND;
	    instr->src_operand.operand = (addr_t)&(core->vm_regs.rax);

	    instr->dst_operand.type = MEM_OPERAND;
	    instr->dst_operand.size = operand_width;
	    instr->dst_operand.operand = get_addr_linear(core, MASK(core->vm_regs.rdi, addr_width), &(core->segments.es));

	    instr->src_operand.read = 1;
	    instr->dst_operand.write = 1;

	    instr->num_operands = 2;

	    break;
	}
	case INT: {
	    instr->dst_operand.type = IMM_OPERAND;
	    instr->dst_operand.size = operand_width;
		instr->dst_operand.operand = *(uint8_t *)instr_ptr;
	    instr_ptr += operand_width;
	    instr->num_operands = 1;

	    break;
	}
	case INVLPG: {
	    uint8_t reg_code = 0;

	    ret = decode_rm_operand(core, instr_ptr, form, instr, &(instr->dst_operand), &reg_code);

	    if (ret == -1) {
		PrintError("Error decoding operand for (%s)\n", op_form_to_str(form));
		return -1;
	    }

	    instr_ptr += ret;

	    instr->num_operands = 1;
	    break;
	}
	case LMSW: 
	case SMSW: {
	    uint8_t reg_code = 0;

	    ret = decode_rm_operand(core, instr_ptr, form, instr, &(instr->dst_operand), &reg_code);

	    if (ret == -1) {
		PrintError("Error decoding operand for (%s)\n", op_form_to_str(form));
		return -1;
	    }

	    instr_ptr += ret;

	    instr->dst_operand.read = 1;

	    instr->num_operands = 1;
	    break;
	}
	case CLTS: {
	    // no operands. 
	    break;
	}
	default:
	    PrintError("Invalid Instruction form: %s\n", op_form_to_str(form));
	    return -1;
    }

    return (instr_ptr - instr_start);
}


static v3_op_type_t op_form_to_type(op_form_t form) { 
    switch (form) {
	case LMSW:
	    return V3_OP_LMSW;
	case SMSW:
	    return V3_OP_SMSW;
	case CLTS:
	    return V3_OP_CLTS;
	case INVLPG:
	    return V3_OP_INVLPG;

	case INT:
	    return V3_OP_INT;
	    
	case MOV_CR2:
	    return V3_OP_MOVCR2;
	case MOV_2CR:
	    return V3_OP_MOV2CR;

	case MOV_MEM2_8:
	case MOV_MEM2:
	case MOV_2MEM_8:
	case MOV_2MEM:
	case MOV_MEM2AL_8:
	case MOV_MEM2AX:
	case MOV_AL2MEM_8:
	case MOV_AX2MEM:
	case MOV_IMM2_8:
	case MOV_IMM2:
	    return V3_OP_MOV;

	case MOVS_8:
	case MOVS:
	    return V3_OP_MOVS;

	case MOVSX_8:
	case MOVSX:
	    return V3_OP_MOVSX;

	case MOVZX_8:
	case MOVZX:
	    return V3_OP_MOVZX;


	case ADC_2MEM_8:
	case ADC_2MEM:
	case ADC_MEM2_8:
	case ADC_MEM2:
	case ADC_IMM2_8:
	case ADC_IMM2:
	case ADC_IMM2SX_8:
	    return V3_OP_ADC;


	case ADD_2MEM_8:
	case ADD_2MEM:
	case ADD_MEM2_8:
	case ADD_MEM2:
	case ADD_IMM2_8:
	case ADD_IMM2:
	case ADD_IMM2SX_8:
	    return V3_OP_ADD;

	case AND_MEM2_8:
	case AND_MEM2:
	case AND_2MEM_8:
	case AND_2MEM:
	case AND_IMM2_8:
	case AND_IMM2:
	case AND_IMM2SX_8:
	    return V3_OP_AND;

	case OR_2MEM_8:
	case OR_2MEM:
	case OR_MEM2_8:
	case OR_MEM2:
	case OR_IMM2_8:
	case OR_IMM2:
	case OR_IMM2SX_8:
	    return V3_OP_OR;

	case SUB_2MEM_8:
	case SUB_2MEM:
	case SUB_MEM2_8:
	case SUB_MEM2:
	case SUB_IMM2_8:
	case SUB_IMM2:
	case SUB_IMM2SX_8:
	    return V3_OP_SUB;

	case XOR_2MEM_8:
	case XOR_2MEM:
	case XOR_MEM2_8:
	case XOR_MEM2:
	case XOR_IMM2_8:
	case XOR_IMM2:
	case XOR_IMM2SX_8:
	    return V3_OP_XOR;

	case INC_8:
	case INC:
	    return V3_OP_INC;

	case DEC_8:
	case DEC:
	    return V3_OP_DEC;

	case NEG_8:
	case NEG: 
	    return V3_OP_NEG;

	case NOT_8:
	case NOT:
	    return V3_OP_NOT;

	case XCHG_8:
	case XCHG:
	    return V3_OP_XCHG;
	    
	case SETB:
	    return V3_OP_SETB;
	case SETBE:
	    return V3_OP_SETBE;
	case SETL:
	    return V3_OP_SETL;
	case SETLE:
	    return V3_OP_SETLE;
	case SETNB:
	    return V3_OP_SETNB;
	case SETNBE:
	    return V3_OP_SETNBE;
	case SETNL:
	    return V3_OP_SETNL;
	case SETNLE:
	    return V3_OP_SETNLE;
	case SETNO:
	    return V3_OP_SETNO;
	case SETNP:
	    return V3_OP_SETNP;
	case SETNS:
	    return V3_OP_SETNS;
	case SETNZ:
	    return V3_OP_SETNZ;
	case SETP:
	    return V3_OP_SETP;
	case SETS:
	    return V3_OP_SETS;
	case SETZ:
	    return V3_OP_SETZ;
	case SETO:
	    return V3_OP_SETO;
	    
	case STOS_8:
	case STOS:
	    return V3_OP_STOS;

	case HLT:
	case PUSHF:
	case POPF:
	case MOV_DR2:
	case MOV_2DR:
	case MOV_SR2:
	case MOV_2SR:

	default:
	    return V3_INVALID_OP;

    }
}
