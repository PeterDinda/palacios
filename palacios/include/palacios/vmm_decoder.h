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

#ifndef __VMM_DECODER_H
#define __VMM_DECODER_H

#ifdef __V3VEE__

#include <palacios/vm_guest.h>
#include <palacios/vmm.h>


typedef enum { V3_INVALID_OP,
	       V3_OP_MOVCR2, V3_OP_MOV2CR, V3_OP_SMSW, V3_OP_LMSW, V3_OP_CLTS,
	       V3_OP_INVLPG,
 	       V3_OP_ADC, V3_OP_ADD, V3_OP_AND, V3_OP_OR, V3_OP_XOR, V3_OP_SUB,
	       V3_OP_INC, V3_OP_DEC, V3_OP_NEG, V3_OP_MOV, V3_OP_NOT, V3_OP_XCHG, 
	       V3_OP_SETB, V3_OP_SETBE, V3_OP_SETL, V3_OP_SETLE, V3_OP_SETNB, 
	       V3_OP_SETNBE, V3_OP_SETNL, V3_OP_SETNLE, V3_OP_SETNO, V3_OP_SETNP,
	       V3_OP_SETNS, V3_OP_SETNZ, V3_OP_SETO, V3_OP_SETP, V3_OP_SETS, 
	       V3_OP_SETZ, V3_OP_MOVS, V3_OP_STOS, V3_OP_MOVZX, V3_OP_MOVSX, V3_OP_INT } v3_op_type_t;


typedef enum {INVALID_OPERAND, REG_OPERAND, MEM_OPERAND, IMM_OPERAND} v3_operand_type_t;

struct x86_operand {
    addr_t operand;
    uint_t size;
    v3_operand_type_t type;
    uint8_t read : 1;   // This operand value will be read by the instruction
    uint8_t write : 1;  // This operand value will be written to by the instruction
} __attribute__((packed));

struct x86_prefixes {
    union {
	uint32_t val;
	
	struct {
	    uint32_t lock   : 1;  // 0xF0
	    uint32_t repne  : 1;  // 0xF2
	    uint32_t repnz  : 1;  // 0xF2
	    uint32_t rep    : 1;  // 0xF3
	    uint32_t repe   : 1;  // 0xF3
	    uint32_t repz   : 1;  // 0xF3
	    uint32_t cs_override : 1;  // 0x2E
	    uint32_t ss_override : 1;  // 0x36
	    uint32_t ds_override : 1;  // 0x3E
	    uint32_t es_override : 1;  // 0x26
	    uint32_t fs_override : 1;  // 0x64
	    uint32_t gs_override : 1;  // 0x65
	    uint32_t br_not_taken : 1;  // 0x2E
	    uint32_t br_taken   : 1;  // 0x3E
	    uint32_t op_size     : 1;  // 0x66
	    uint32_t addr_size   : 1;  // 0x67

	    uint32_t rex   : 1;
    
	    uint32_t rex_rm        : 1;  // REX.B
	    uint32_t rex_sib_idx   : 1;  // REX.X
	    uint32_t rex_reg       : 1;  // REX.R
	    uint32_t rex_op_size   : 1;  // REX.W

	    uint32_t rsvd          : 11;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct x86_instr {
    struct x86_prefixes prefixes;
    uint8_t instr_length;
    v3_op_type_t op_type;
    uint_t num_operands;
    struct x86_operand dst_operand;
    struct x86_operand src_operand;
    struct x86_operand third_operand;
    addr_t str_op_length;
    addr_t is_str_op;
};


/************************/
/* EXTERNAL DECODER API */
/************************/
/* 
   This is an External API definition that must be implemented by a decoder
*/


/* 
 * Initializes a decoder
 */
int v3_init_decoder(struct guest_info * core);
int v3_deinit_decoder(struct guest_info * core);

/* 
 * Decodes an instruction 
 * All addresses in arguments are in the host address space
 * instr_ptr is the host address of the instruction 
 * IMPORTANT: make sure the instr_ptr is in contiguous host memory
 *   ie. Copy it to a buffer before the call
 */
int v3_decode(struct guest_info * info, addr_t instr_ptr, struct x86_instr * instr);

/* 
 * Encodes an instruction
 * All addresses in arguments are in the host address space
 * The instruction is encoded from the struct, and copied into a 15 byte host buffer
 * referenced by instr_buf
 * any unused bytes at the end of instr_buf will be filled with nops
 * IMPORTANT: instr_buf must be allocated and 15 bytes long
 */
int v3_encode(struct guest_info * info, struct x86_instr * instr, uint8_t * instr_buf);



/* Removes a rep prefix in place */
void v3_strip_rep_prefix(uint8_t * instr, int length);
uint8_t v3_get_prefixes(uint8_t * instr, struct x86_prefixes * prefixes);


void v3_print_instr(struct x86_instr * instr);


#define PREFIX_LOCK         0xF0
#define PREFIX_REPNE        0xF2
#define PREFIX_REPNZ        0xF2
#define PREFIX_REP          0xF3
#define PREFIX_REPE         0xF3
#define PREFIX_REPZ         0xF3
#define PREFIX_CS_OVERRIDE  0x2E
#define PREFIX_SS_OVERRIDE  0x36
#define PREFIX_DS_OVERRIDE  0x3E
#define PREFIX_ES_OVERRIDE  0x26
#define PREFIX_FS_OVERRIDE  0x64
#define PREFIX_GS_OVERRIDE  0x65
#define PREFIX_BR_NOT_TAKEN 0x2E
#define PREFIX_BR_TAKEN     0x3E
#define PREFIX_OP_SIZE      0x66
#define PREFIX_ADDR_SIZE    0x67




static inline int is_prefix_byte(uchar_t byte) {
    switch (byte) {
	case 0xF0:      // lock
	case 0xF2:      // REPNE/REPNZ
	case 0xF3:      // REP or REPE/REPZ
	case 0x2E:      // CS override or Branch hint not taken (with Jcc instrs)
	case 0x36:      // SS override
	case 0x3E:      // DS override or Branch hint taken (with Jcc instrs)
	case 0x26:      // ES override
	case 0x64:      // FS override
	case 0x65:      // GS override
	    //case 0x2E:      // branch not taken hint
	    //  case 0x3E:      // branch taken hint
	case 0x66:      // operand size override
	case 0x67:      // address size override
	    return 1;
	    break;
	default:
	    return 0;
	    break;
    }
}


static inline v3_reg_t get_gpr_mask(struct guest_info * info) {
    switch (info->cpu_mode) {
	case REAL: 
	case LONG_16_COMPAT:
	    return 0xffff;
	    break;
	case PROTECTED:
	case LONG_32_COMPAT:
	case PROTECTED_PAE:
	    return 0xffffffff;
	case LONG:
	    return 0xffffffffffffffffLL;
	default:
	    PrintError("Unsupported Address Mode\n");
	    return -1;
    }
}



static inline addr_t get_addr_linear(struct guest_info * info, addr_t addr, struct v3_segment * seg) {
    switch (info->cpu_mode) {
	case REAL:
	    // It appears that the segment values are computed and cached in the vmcb structure 
	    // We Need to check this for Intel
	    /*   return addr + (seg->selector << 4);
		 break;*/

	case PROTECTED:
	case PROTECTED_PAE:
	case LONG_32_COMPAT:
	    return addr + seg->base;
	    break;

	case LONG: {
	    uint64_t seg_base = 0;

	    // In long mode the segment bases are disregarded (forced to 0), unless using 
	    // FS or GS, then the base addresses are added

	    if (seg) {
		seg_base = seg->base;
	    }

	    return addr + seg_base;
	}
	case LONG_16_COMPAT:
	default:
	    PrintError("Unsupported CPU Mode: %d\n", info->cpu_mode);
	    return -1;
    }
}



#endif // !__V3VEE__


#endif
