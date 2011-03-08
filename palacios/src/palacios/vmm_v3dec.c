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

static v3_op_type_t op_form_to_type(op_form_t form);
static int parse_operands(struct guest_info * core, uint8_t * instr_ptr, struct x86_instr * instr, op_form_t form);


int v3_disasm(struct guest_info * info, void *instr_ptr, addr_t * rip, int mark) {
    return 0;
}


int v3_init_decoder(struct guest_info * core) { 
    return 0;
}


int v3_deinit_decoder(struct guest_info * core) {
    return 0;
}


int v3_encode(struct guest_info * info, struct x86_instr * instr, uint8_t * instr_buf) {
    return 0;
}


int v3_decode(struct guest_info * core, addr_t instr_ptr, struct x86_instr * instr) {
    op_form_t form = INVALID_INSTR; 
    int ret = 0;
    int length = 0;

    memset(instr, 0, sizeof(struct x86_instr));

    // scan for prefixes
    length = v3_get_prefixes((uint8_t *)instr_ptr, &(instr->prefixes));


    // check for REX prefix


    form = op_code_to_form((uint8_t *)(instr_ptr + length), &length);

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


    return 0;
}


static int parse_operands(struct guest_info * core, uint8_t * instr_ptr, 
			  struct x86_instr * instr, op_form_t form) {
    // get operational mode of the guest for operand width
    uint8_t operand_width = get_operand_width(core, instr, form);
    uint8_t addr_width = get_addr_width(core, instr, form);;
    int ret = 0;
    uint8_t * instr_start = instr_ptr;
    

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
	case MOV_IMM2:{
	    uint8_t reg_code = 0;

	    instr->dst_operand.size = operand_width;

	    ret = decode_rm_operand(core, instr_ptr, instr, &(instr->dst_operand), &reg_code);

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
	    } else {
		PrintError("Illegal operand width (%d)\n", operand_width);
		return -1;
	    }

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

	    instr->dst_operand.size = operand_width;

	    ret = decode_rm_operand(core, instr_ptr, instr, &(instr->dst_operand), &reg_code);

	    if (ret == -1) {
		PrintError("Error decoding operand\n");
		return -1;
	    }

	    instr_ptr += ret;

	    instr->src_operand.type = REG_OPERAND;
	    instr->src_operand.size = operand_width;

	    decode_gpr(&(core->vm_regs), reg_code, &(instr->src_operand));

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
	    instr->src_operand.size = operand_width;

	    ret = decode_rm_operand(core, instr_ptr, instr, &(instr->src_operand), &reg_code);

	    if (ret == -1) {
		PrintError("Error decoding operand\n");
		return -1;
	    }

	    instr_ptr += ret;

	    instr->dst_operand.size = operand_width;
	    instr->dst_operand.type = REG_OPERAND;
	    decode_gpr(&(core->vm_regs), reg_code, &(instr->dst_operand));

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
	    instr->src_operand.size = operand_width;

	    ret = decode_rm_operand(core, instr_ptr, instr, &(instr->src_operand), &reg_code);

	    if (ret == -1) {
		PrintError("Error decoding operand\n");
		return -1;
	    }

	    instr_ptr += ret;

	    instr->src_operand.type = IMM_OPERAND;
	    instr->src_operand.size = operand_width;
	    instr->src_operand.operand = *(sint8_t *)instr_ptr;  // sign extend.

	    instr_ptr += 1;

	    instr->num_operands = 2;

	    break;
	}
	case MOVS:
	case MOVS_8: {
	    instr->is_str_op = 1;
	    
	    if (instr->prefixes.rep == 1) {
		instr->str_op_length = MASK(core->vm_regs.rcx, operand_width);
	    } else {
		instr->str_op_length = 1;
	    }

	    // Source: DS:(E)SI
	    // Source: ES:(E)DI

	    instr->src_operand.type = MEM_OPERAND;
	    instr->src_operand.size = operand_width;
	    instr->src_operand.operand = core->segments.ds.base + MASK(core->vm_regs.rsi, addr_width);

	    instr->src_operand.type = MEM_OPERAND;
	    instr->src_operand.size = operand_width;
	    instr->src_operand.operand = core->segments.es.base + MASK(core->vm_regs.rdi, addr_width);

	    instr->num_operands = 2;

	    break;
	}
	default:
	    PrintError("Invalid Instruction form: %d\n", form);
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
