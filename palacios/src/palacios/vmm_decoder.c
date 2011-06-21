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





uint8_t v3_get_prefixes(uint8_t * instr, struct x86_prefixes * prefixes) {
    uint8_t * instr_cursor = instr;

    while (1) {
	switch (*instr_cursor) {
	    case 0xF0:      // lock
		prefixes->lock = 1;
		break;

	    case 0xF2:      // REPNE/REPNZ
		prefixes->repnz = 1;
		prefixes->repne = 1;
		break;

	    case 0xF3:      // REP or REPE/REPZ
		prefixes->rep = 1;
		prefixes->repe = 1;
		prefixes->repz = 1; 
		break;

	    case 0x2E:      // CS override or Branch hint not taken (with Jcc instr_cursors)
		prefixes->cs_override = 1;
		prefixes->br_not_taken = 1;
		break;

	    case 0x36:      // SS override
		prefixes->ss_override = 1;
		break;

	    case 0x3E:      // DS override or Branch hint taken (with Jcc instr_cursors)
		prefixes->ds_override = 1;
		prefixes->br_taken = 1;
		break;

	    case 0x26:      // ES override
		prefixes->es_override = 1;
		break;

	    case 0x64:      // FS override
		prefixes->fs_override = 1;
		break;
      
	    case 0x65:      // GS override
		prefixes->gs_override = 1;
		break;

	    case 0x66:      // operand size override
		prefixes->op_size = 1;
		break;

	    case 0x67:    // address size override
		prefixes->addr_size = 1;
		break;

	    default:
		return (instr_cursor - instr);
	}

	instr_cursor++;
    }
}

void v3_strip_rep_prefix(uchar_t * instr, int length) {
    int read_ctr = 0;
    int write_ctr = 0;
    int found = 0;

    while (read_ctr < length) {
	if ((!found) && 
	    ( (instr[read_ctr] == 0xF2) ||
	      (instr[read_ctr] == 0xF3))) {
	    read_ctr++;
	    found = 1;
	} else {
	    instr[write_ctr] = instr[read_ctr];
	    write_ctr++;
	    read_ctr++;
	}
    }
}


static char * op_type_to_str(v3_op_type_t type) {
    switch (type) {
	case V3_OP_MOVCR2: return "V3_OP_MOVCR2"; 
	case V3_OP_MOV2CR: return "V3_OP_MOV2CR"; 
	case V3_OP_SMSW: return "V3_OP_SMSW"; 
	case V3_OP_LMSW: return "V3_OP_LMSW"; 
	case V3_OP_CLTS: return "V3_OP_CLTS";
	case V3_OP_INVLPG: return "V3_OP_INVLPG";
	case V3_OP_ADC: return "V3_OP_ADC"; 
	case V3_OP_ADD: return "V3_OP_ADD";
	case V3_OP_AND: return "V3_OP_AND"; 
	case V3_OP_OR: return "V3_OP_OR"; 
	case V3_OP_XOR: return "V3_OP_XOR"; 
	case V3_OP_SUB: return "V3_OP_SUB";
	case V3_OP_INC: return "V3_OP_INC"; 
	case V3_OP_DEC: return "V3_OP_DEC"; 
	case V3_OP_NEG: return "V3_OP_NEG"; 
	case V3_OP_MOV: return "V3_OP_MOV"; 
	case V3_OP_NOT: return "V3_OP_NOT"; 
	case V3_OP_XCHG: return "V3_OP_XCHG"; 
	case V3_OP_SETB: return "V3_OP_SETB"; 
	case V3_OP_SETBE: return "V3_OP_SETBE"; 
	case V3_OP_SETL: return "V3_OP_SETL"; 
	case V3_OP_SETLE: return "V3_OP_SETLE"; 
	case V3_OP_SETNB: return "V3_OP_SETNB"; 
	case V3_OP_SETNBE: return "V3_OP_SETNBE"; 
	case V3_OP_SETNL: return "V3_OP_SETNL"; 
	case V3_OP_SETNLE: return "V3_OP_SETNLE"; 
	case V3_OP_SETNO: return "V3_OP_SETNO"; 
	case V3_OP_SETNP: return "V3_OP_SETNP";
	case V3_OP_SETNS: return "V3_OP_SETNS"; 
	case V3_OP_SETNZ: return "V3_OP_SETNZ"; 
	case V3_OP_SETO: return "V3_OP_SETO"; 
	case V3_OP_SETP: return "V3_OP_SETP"; 
	case V3_OP_SETS: return "V3_OP_SETS"; 
	case V3_OP_SETZ: return "V3_OP_SETZ"; 
	case V3_OP_MOVS: return "V3_OP_MOVS"; 
	case V3_OP_STOS: return "V3_OP_STOS"; 
	case V3_OP_MOVZX: return "V3_OP_MOVZX"; 
	case V3_OP_MOVSX: return "V3_OP_MOVSX";
 	case V3_OP_INT: return "V3_OP_INT";
	case V3_INVALID_OP: 
	default:
	    return "V3_INVALID_OP";
    }
}


static char * operand_type_to_str(v3_operand_type_t op) {
    switch (op) {
	case REG_OPERAND: return "REG_OPERAND";
	case MEM_OPERAND: return "MEM_OPERAND";
	case IMM_OPERAND: return "IMM_OPERAND";
	default:
	    return "INVALID_OPERAND";
    }
}


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

void v3_print_instr(struct x86_instr * instr) {
    V3_Print("Instr: %s (Len: %d)\n", op_type_to_str(instr->op_type), instr->instr_length);

    V3_Print("Prefixes= %x\n", instr->prefixes.val);

    if (instr->is_str_op) {
	V3_Print("String OP (len=%d)\n", (uint32_t)instr->str_op_length);
    }

    V3_Print("Number of operands: %d\n", instr->num_operands);

    if (instr->num_operands > 0) {
	V3_Print("Src Operand (%s)\n", operand_type_to_str(instr->src_operand.type));
	V3_Print("\tLen=%d (Addr: %p)\n", instr->src_operand.size, 
		 (void *)instr->src_operand.operand);
	if (instr->src_operand.type == REG_OPERAND) {
	    V3_Print("\tVal: 0x%llx\n", MASK(*(uint64_t *)(instr->src_operand.operand), instr->src_operand.size));
	}
    }

    if (instr->num_operands > 1) {
	V3_Print("Dst Operand (%s)\n", operand_type_to_str(instr->dst_operand.type));
	V3_Print("\tLen=%d (Addr: %p)\n", instr->dst_operand.size, 
		 (void *)instr->dst_operand.operand);
	if (instr->dst_operand.type == REG_OPERAND) {
	    V3_Print("\tVal: 0x%llx\n", MASK(*(uint64_t *)(instr->dst_operand.operand), instr->dst_operand.size));
	}
    }

    if (instr->num_operands > 2) {
	V3_Print("Third Operand (%s)\n", operand_type_to_str(instr->third_operand.type));
	V3_Print("\tLen=%d (Addr: %p)\n", instr->third_operand.size, 
		 (void *)instr->third_operand.operand);
	if (instr->third_operand.type == REG_OPERAND) {
	    V3_Print("\tVal: 0x%llx\n", MASK(*(uint64_t *)(instr->third_operand.operand), instr->third_operand.size));
	}
    }
}


