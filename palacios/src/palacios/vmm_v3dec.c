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


/* .... Giant fucking switch tables */



#define MODRM_MOD(x) (((x) >> 6) & 0x3)
#define MODRM_REG(x) (((x) >> 3) & 0x7)
#define MODRM_RM(x)  ((x) & 0x7)

struct modrm_byte {
    uint_t rm   :   3;
    uint_t reg  :   3;
    uint_t mod  :   2;
} __attribute__((packed));


#define SIB_BASE(x) (((x) >> 6) & 0x3)
#define SIB_INDEX(x) (((x) >> 3) & 0x7)
#define SIB_SCALE(x) ((x) & 0x7)

struct sib_byte {
    uint_t base     :   3;
    uint_t index    :   3;
    uint_t scale    :   2;
} __attribute__((packed));




typedef enum {
    INVALID_INSTR,
    LMSW,
    SMSW,
    CLTS,
    INVLPG,

    MOV_CR2,
    MOV_2CR,
    MOV_DR2,
    MOV_2DR,
    MOV_SR2,
    MOV_2SR,

    MOV_2GPR_8,
    MOV_2GPR,
    MOV_GPR2_8,
    MOV_GPR2,
    MOV_MEM2AL_8,
    MOV_MEM2AX,
    MOV_AL2MEM_8,
    MOV_AX2MEM,
    MOV_IMM2_8,
    MOV_IMM2,

    MOVS_8,
    MOVS,
    MOVSX_8,
    MOVSX,
    MOVZX_8,
    MOVZX,

    HLT,
    PUSHF,
    POPF,

    ADC_2MEM_8,
    ADC_2MEM,
    ADC_MEM2_8,
    ADC_MEM2,
    ADC_IMM2_8,
    ADC_IMM2,
    ADC_IMM2SX_8,
    ADD_IMM2_8,
    ADD_IMM2,
    ADD_IMM2SX_8,
    ADD_2MEM_8,
    ADD_2MEM,
    ADD_MEM2_8,
    ADD_MEM2,
    AND_MEM2_8,
    AND_MEM2,
    AND_2MEM_8,
    AND_2MEM,
    AND_IMM2_8,
    AND_IMM2,
    AND_IMM2SX_8,
    OR_2MEM_8,
    OR_2MEM,
    OR_MEM2_8,
    OR_MEM2,
    OR_IMM2_8,
    OR_IMM2,
    OR_IMM2SX_8,
    SUB_2MEM_8,
    SUB_2MEM,
    SUB_MEM2_8,
    SUB_MEM2,
    SUB_IMM2_8,
    SUB_IMM2,
    SUB_IMM2SX_8,
    XOR_2MEM_8,
    XOR_2MEM,
    XOR_MEM2_8,
    XOR_MEM2,
    XOR_IMM2_8,
    XOR_IMM2,
    XOR_IMM2SX_8,

    INC_8,
    INC,
    DEC_8,
    DEC,
    NEG_8,
    NEG, 
    NOT_8,
    NOT,
    XCHG_8,
    XCHG,

    SETB,
    SETBE,
    SETL,
    SETLE,
    SETNB,
    SETNBE,
    SETNL,
    SETNLE,
    SETNO,
    SETNP,
    SETNS,
    SETNZ,
    SETP,
    SETS,
    SETZ,
    SETO,

    STOS_8,
    STOS
} op_form_t;

static op_form_t op_code_to_form(uint8_t * instr);



typedef enum {INVALID_ADDR_TYPE, REG, DISP0, DISP8, DISP16, DISP32} modrm_mode_t;
typedef enum {INVALID_REG_SIZE, REG64, REG32, REG16, REG8} reg_size_t;


struct v3_gprs;

static inline addr_t decode_register(struct v3_gprs * gprs, char reg_code, reg_size_t reg_size) {
    addr_t reg_addr;

    switch (reg_code) {
	case 0:
	    reg_addr = (addr_t)&(gprs->rax);
	    break;
	case 1:
	    reg_addr = (addr_t)&(gprs->rcx);
	    break;
	case 2:
	    reg_addr = (addr_t)&(gprs->rdx);
	    break;
	case 3:
	    reg_addr = (addr_t)&(gprs->rbx);
	    break;
	case 4:
	    if (reg_size == REG8) {
		reg_addr = (addr_t)&(gprs->rax) + 1;
	    } else {
		reg_addr = (addr_t)&(gprs->rsp);
	    }
	    break;
	case 5:
	    if (reg_size == REG8) {
		reg_addr = (addr_t)&(gprs->rcx) + 1;
	    } else {
		reg_addr = (addr_t)&(gprs->rbp);
	    }
	    break;
	case 6:
	    if (reg_size == REG8) {
		reg_addr = (addr_t)&(gprs->rdx) + 1;
	    } else {
		reg_addr = (addr_t)&(gprs->rsi);
	    }
	    break;
	case 7:
	    if (reg_size == REG8) {
		reg_addr = (addr_t)&(gprs->rbx) + 1;
	    } else {
		reg_addr = (addr_t)&(gprs->rdi);
	    }
	    break;
	default:
	    reg_addr = 0;
	    break;
    }

    return reg_addr;
}



static inline v3_operand_type_t decode_operands16(struct v3_gprs * gprs, // input/output
						  char * modrm_instr,       // input
						  int * offset,             // output
						  addr_t * first_operand,   // output
						  addr_t * second_operand,  // output
						  reg_size_t reg_size) {    // input
  
    struct modrm_byte * modrm = (struct modrm_byte *)modrm_instr;
    addr_t base_addr = 0;
    modrm_mode_t mod_mode = 0;
    v3_operand_type_t addr_type = INVALID_OPERAND;
    char * instr_cursor = modrm_instr;

    //  PrintDebug("ModRM mod=%d\n", modrm->mod);

    instr_cursor += 1;

    if (modrm->mod == 3) {
	mod_mode = REG;
	addr_type = REG_OPERAND;
	//PrintDebug("first operand = Register (RM=%d)\n",modrm->rm);

	*first_operand = decode_register(gprs, modrm->rm, reg_size);

    } else {

	addr_type = MEM_OPERAND;

	if (modrm->mod == 0) {
	    mod_mode = DISP0;
	} else if (modrm->mod == 1) {
	    mod_mode = DISP8;
	} else if (modrm->mod == 2) {
	    mod_mode = DISP16;
	}

	switch (modrm->rm) {
	    case 0:
		base_addr = gprs->rbx + gprs->rsi;
		break;
	    case 1:
		base_addr = gprs->rbx + gprs->rdi;
		break;
	    case 2:
		base_addr = gprs->rbp + gprs->rsi;
		break;
	    case 3:
		base_addr = gprs->rbp + gprs->rdi;
		break;
	    case 4:
		base_addr = gprs->rsi;
		break;
	    case 5:
		base_addr = gprs->rdi;
		break;
	    case 6:
		if (modrm->mod == 0) {
		    base_addr = 0;
		    mod_mode = DISP16;
		} else {
		    base_addr = gprs->rbp;
		}
		break;
	    case 7:
		base_addr = gprs->rbx;
		break;
	}



	if (mod_mode == DISP8) {
	    base_addr += (uchar_t)*(instr_cursor);
	    instr_cursor += 1;
	} else if (mod_mode == DISP16) {
	    base_addr += (ushort_t)*(instr_cursor);
	    instr_cursor += 2;
	}
    
	*first_operand = base_addr;
    }

    *offset +=  (instr_cursor - modrm_instr);
    *second_operand = decode_register(gprs, modrm->reg, reg_size);

    return addr_type;
}



static inline v3_operand_type_t decode_operands32(struct v3_gprs * gprs, // input/output
						  uchar_t * modrm_instr,       // input
						  int * offset,             // output
						  addr_t * first_operand,   // output
						  addr_t * second_operand,  // output
						  reg_size_t reg_size) {    // input
  
    uchar_t * instr_cursor = modrm_instr;
    struct modrm_byte * modrm = (struct modrm_byte *)modrm_instr;
    addr_t base_addr = 0;
    modrm_mode_t mod_mode = 0;
    uint_t has_sib_byte = 0;
    v3_operand_type_t addr_type = INVALID_OPERAND;



    instr_cursor += 1;

    if (modrm->mod == 3) {
	mod_mode = REG;
	addr_type = REG_OPERAND;
    
	//    PrintDebug("first operand = Register (RM=%d)\n",modrm->rm);

	*first_operand = decode_register(gprs, modrm->rm, reg_size);

    } else {

	addr_type = MEM_OPERAND;

	if (modrm->mod == 0) {
	    mod_mode = DISP0;
	} else if (modrm->mod == 1) {
	    mod_mode = DISP8;
	} else if (modrm->mod == 2) {
	    mod_mode = DISP32;
	}
    
	switch (modrm->rm) {
	    case 0:
		base_addr = gprs->rax;
		break;
	    case 1:
		base_addr = gprs->rcx;
		break;
	    case 2:
		base_addr = gprs->rdx;
		break;
	    case 3:
		base_addr = gprs->rbx;
		break;
	    case 4:
		has_sib_byte = 1;
		break;
	    case 5:
		if (modrm->mod == 0) {
		    base_addr = 0;
		    mod_mode = DISP32;
		} else {
		    base_addr = gprs->rbp;
		}
		break;
	    case 6:
		base_addr = gprs->rsi;
		break;
	    case 7:
		base_addr = gprs->rdi;
		break;
	}

	if (has_sib_byte) {
	    instr_cursor += 1;
	    struct sib_byte * sib = (struct sib_byte *)(instr_cursor);
	    int scale = 1;

	    instr_cursor += 1;


	    if (sib->scale == 1) {
		scale = 2;
	    } else if (sib->scale == 2) {
		scale = 4;
	    } else if (sib->scale == 3) {
		scale = 8;
	    }


	    switch (sib->index) {
		case 0:
		    base_addr = gprs->rax;
		    break;
		case 1:
		    base_addr = gprs->rcx;
		    break;
		case 2:
		    base_addr = gprs->rdx;
		    break;
		case 3:
		    base_addr = gprs->rbx;
		    break;
		case 4:
		    base_addr = 0;
		    break;
		case 5:
		    base_addr = gprs->rbp;
		    break;
		case 6:
		    base_addr = gprs->rsi;
		    break;
		case 7:
		    base_addr = gprs->rdi;
		    break;
	    }

	    base_addr *= scale;


	    switch (sib->base) {
		case 0:
		    base_addr += gprs->rax;
		    break;
		case 1:
		    base_addr += gprs->rcx;
		    break;
		case 2:
		    base_addr += gprs->rdx;
		    break;
		case 3:
		    base_addr += gprs->rbx;
		    break;
		case 4:
		    base_addr += gprs->rsp;
		    break;
		case 5:
		    if (modrm->mod != 0) {
			base_addr += gprs->rbp;
		    }
		    break;
		case 6:
		    base_addr += gprs->rsi;
		    break;
		case 7:
		    base_addr += gprs->rdi;
		    break;
	    }

	} 


	if (mod_mode == DISP8) {
	    base_addr += (uchar_t)*(instr_cursor);
	    instr_cursor += 1;
	} else if (mod_mode == DISP32) {
	    base_addr += (uint_t)*(instr_cursor);
	    instr_cursor += 4;
	}
    

	*first_operand = base_addr;
    }

    *offset += (instr_cursor - modrm_instr);

    *second_operand = decode_register(gprs, modrm->reg, reg_size);

    return addr_type;
}



static inline op_form_t op_code_to_form_0f(uint8_t * instr) {
    switch (instr[1]) {
	case 0x01: {
	    struct modrm_byte * modrm = (struct modrm_byte *)&(instr[2]);

	    switch (modrm->reg) {
		case 4:
		    return SMSW;
		case 6:
		    return LMSW;
		case 7:
		    return INVLPG;
		default:
		    return INVALID_INSTR;
	    }
	}

	case 0x06:
	    return CLTS;
	case 0x20:
	    return MOV_CR2;
	case 0x21:
	    return MOV_DR2;

	case 0x22:
	    return MOV_2CR;
	case 0x23:
	    return MOV_2DR;

	case 0x90:
	    return SETO;
	case 0x91:
	    return SETNO;
	case 0x92:
	    return SETB;
	case 0x93:
	    return SETNB;
	case 0x94:
	    return SETZ;
	case 0x95:
	    return SETNZ;
	case 0x96:
	    return SETBE;
	case 0x97:
	    return SETNBE;
	case 0x98:
	    return SETS;
	case 0x99:
	    return SETNS;
	case 0x9a:
	    return SETP;
	case 0x9b:
	    return SETNP;
	case 0x9c:
	    return SETL;
	case 0x9d:
	    return SETNL;
	case 0x9e:
	    return SETLE;
	case 0x9f:
	    return SETNLE;

	case 0xb6:
	    return MOVZX_8;
	case 0xb7:
	    return MOVZX;

	case 0xbe:
	    return MOVSX_8;
	case 0xbf:
	    return MOVSX;
	    

	default:
	    return INVALID_INSTR;
    }
}


static op_form_t op_code_to_form(uint8_t * instr) {
    switch (instr[0]) {
	case 0x00:
	    return ADD_2MEM_8;
	case 0x01:
	    return ADD_2MEM;
	case 0x02:
	    return ADD_MEM2_8;
	case 0x03:
	    return ADD_MEM2;

	case 0x08:
	    return OR_2MEM_8;
	case 0x09:
	    return OR_2MEM;
	case 0x0a:
	    return OR_MEM2_8;
	case 0x0b:
	    return OR_MEM2;


	case 0x0f:
	    return op_code_to_form_0f(instr);

	case 0x10:
	    return ADC_2MEM_8;
	case 0x11:
	    return ADC_2MEM;
	case 0x12:
	    return ADC_MEM2_8;
	case 0x13:
	    return ADC_MEM2;

	case 0x20:
	    return AND_2MEM_8; 
	case 0x21:
	    return AND_2MEM;
	case 0x22:
	    return AND_MEM2_8;
	case 0x23:
	    return AND_MEM2;

	case 0x28:
	    return SUB_2MEM_8;
	case 0x29:
	    return SUB_2MEM;
	case 0x2a:
	    return SUB_MEM2_8;
	case 0x2b:
	    return SUB_MEM2;


	case 0x30:
	    return XOR_2MEM_8;
	case 0x31:
	    return XOR_2MEM;
	case 0x32:
	    return XOR_MEM2_8;
	case 0x33:
	    return XOR_MEM2;

	case 0x80:{
	    struct modrm_byte * modrm = (struct modrm_byte *)&(instr[1]);

	    switch (modrm->reg) {
		case 0:
		    return ADD_IMM2_8;
		case 1:
		    return OR_IMM2_8;
		case 2:
		    return ADC_IMM2_8;
		case 4:
		    return AND_IMM2_8;
		case 5:
		    return SUB_IMM2_8;
		case 6:
		    return XOR_IMM2_8;
		default:
		    return INVALID_INSTR;
	    }
	}
	case 0x81: {
	    struct modrm_byte * modrm = (struct modrm_byte *)&(instr[1]);
	    
	    switch (modrm->reg) {
		case 0:
		    return ADD_IMM2;
		case 1:
		    return OR_IMM2;
		case 2:
		    return ADC_IMM2;
		case 4:
		    return AND_IMM2;
		case 5:
		    return SUB_IMM2;
		case 6:
		    return XOR_IMM2;
		default:
		    return INVALID_INSTR;
	    }
	}
	case 0x83: {
	    struct modrm_byte * modrm = (struct modrm_byte *)&(instr[1]);

	    switch (modrm->reg) {
		case 0:
		    return ADD_IMM2SX_8;
		case 1:
		    return OR_IMM2SX_8;
		case 2:
		    return ADC_IMM2SX_8;
		case 4:
		    return AND_IMM2SX_8;
		case 5:
		    return SUB_IMM2SX_8;
		case 6:
		    return XOR_IMM2SX_8;
		default:
		    return INVALID_INSTR;
	    }
	}

	case 0x86:
	    return XCHG_8;
	case 0x87:
	    return XCHG;
	case 0x88:
	    return MOV_2GPR_8;
	case 0x89:
	    return MOV_2GPR;
	case 0x8a:
	    return MOV_GPR2_8;
	case 0x8b:
	    return MOV_GPR2;
	    
	case 0x8c:
	    return MOV_SR2;
	case 0x8e:
	    return MOV_2SR;


	case 0x9c:
	    return PUSHF;
	case 0x9d:
	    return POPF;

	case 0xa0:
	    return MOV_MEM2AL_8;
	case 0xa1:
	    return MOV_MEM2AX;
	case 0xa2:
	    return MOV_AL2MEM_8;
	case 0xa3:
	    return MOV_AX2MEM;

	case 0xa4:
	    return MOVS_8;
	case 0xa5:
	    return MOVS;

	case 0xaa:
	    return STOS_8;
	case 0xab:
	    return STOS;

	case 0xc6:
	    return MOV_IMM2_8;
	case 0xc7:
	    return MOV_IMM2;

	case 0xf4:
	    return HLT;


	case 0xf6: {
	    struct modrm_byte * modrm = (struct modrm_byte *)&(instr[1]);

	    switch (modrm->reg) {
		case 2:
		    return NOT_8;
		case 3:
		    return NEG_8;
		default:
		    return INVALID_INSTR;
	    }
	}
	case 0xf7: {
	    struct modrm_byte * modrm = (struct modrm_byte *)&(instr[1]);

	    switch (modrm->reg) {
		case 2:
		    return NOT;
		case 3:
		    return NEG;
		default:
		    return INVALID_INSTR;
	    }
	}
	    

	case 0xfe: {
	    struct modrm_byte * modrm = (struct modrm_byte *)&(instr[1]);

	    switch (modrm->reg) {
		case 0:
		    return INC_8;
		case 1:
		    return DEC_8;
		default:
		    return INVALID_INSTR;
	    }
	}

	case 0xff: {
	    struct modrm_byte * modrm = (struct modrm_byte *)&(instr[1]);

	    switch (modrm->reg) {
		case 0:
		    return INC;
		case 1:
		    return DEC;
		default:
		    return INVALID_INSTR;
	    }
	}

	default:
	    return INVALID_INSTR;
    }
}

int v3_disasm(struct guest_info * info, void *instr_ptr, addr_t * rip, int mark) {
    return 0;
}



int v3_init_decoder(struct guest_info * core) { 
    return 0;
}


int v3_deinit_decoder(struct guest_info * core) {
    return 0;
}


int v3_encode(struct guest_info * info, struct x86_instr * instr, char * instr_buf) {
    return 0;
}

int v3_decode(struct guest_info * info, addr_t instr_ptr, struct x86_instr * instr) {
    op_code_to_form((void *)instr_ptr);

    return 0;
}
