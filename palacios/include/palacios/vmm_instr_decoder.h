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

#include <palacios/vmm_types.h>


/* .... Giant fucking switch tables */


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

    MOV_MEM2_8,
    MOV_MEM2,
    MOV_2MEM_8,
    MOV_2MEM,
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


static int get_addr_width(struct guest_info * info, struct x86_instr * instr,
			  op_form_t form) {

    switch (v3_get_vm_cpu_mode(info)) {
	case REAL:
	    return (instr->prefixes.addr_size) ? 4 : 2;
	case PROTECTED:
	case PROTECTED_PAE:
	    return (instr->prefixes.addr_size) ? 2 : 4;
	case LONG_32_COMPAT:
	case LONG:
	default:
	    PrintError("Unsupported CPU mode: %d\n", info->cpu_mode);
	    return -1;
    }
}

static int get_operand_width(struct guest_info * info, struct x86_instr * instr, 
			     op_form_t form) {
    switch (form) {

	case CLTS:
	case HLT:
	    return 0;

	case MOV_MEM2_8:
	case MOV_2MEM_8:
	case MOV_MEM2AL_8:
	case MOV_AL2MEM_8:
	case MOV_IMM2_8:
	case MOVS_8:
	case MOVSX_8:
	case MOVZX_8:
	case ADC_2MEM_8:
	case ADC_MEM2_8:
	case ADC_IMM2_8:
	case ADD_IMM2_8:
	case ADD_2MEM_8:
	case ADD_MEM2_8:
	case AND_MEM2_8:
	case AND_2MEM_8:
	case AND_IMM2_8:
	case OR_2MEM_8:
	case OR_MEM2_8:
	case OR_IMM2_8:
	case SUB_2MEM_8:
	case XOR_2MEM_8:
	case SUB_MEM2_8:
	case SUB_IMM2_8:
	case XOR_MEM2_8:
	case XOR_IMM2_8:
	case INC_8:
	case DEC_8:
	case NEG_8:
	case NOT_8:
	case XCHG_8:
	case STOS_8:
	case SETB:
	case SETBE:
	case SETL:
	case SETLE:
	case SETNB:
	case SETNBE:
	case SETNL:
	case SETNLE:
	case SETNO:
	case SETNP:
	case SETNS:
	case SETNZ:
	case SETP:
	case SETS:
	case SETZ:
	case SETO:
	    return 1;

	case LMSW:
	case SMSW:
	    return 2;

	case MOV_MEM2:
	case MOV_2MEM:
	case MOV_MEM2AX:
	case MOV_AX2MEM:
	case MOV_IMM2:	    
	case MOVS:
	case MOVSX:
	case MOVZX:
	case ADC_2MEM:
	case ADC_MEM2:
	case ADC_IMM2:
	case ADD_IMM2:
	case ADD_2MEM:
	case ADD_MEM2:
	case AND_MEM2:
	case AND_2MEM:
	case AND_IMM2:
	case OR_2MEM:
	case OR_MEM2:
	case OR_IMM2:
	case SUB_2MEM:
	case SUB_MEM2:
	case SUB_IMM2:
	case XOR_2MEM:
	case XOR_MEM2:
	case XOR_IMM2:
	case INC:
	case DEC:
	case NEG: 
	case NOT:
	case STOS:
	case XCHG:
	case ADC_IMM2SX_8:
	case AND_IMM2SX_8:
	case ADD_IMM2SX_8:
	case OR_IMM2SX_8:
	case SUB_IMM2SX_8:
	case XOR_IMM2SX_8:
	    switch (v3_get_vm_cpu_mode(info)) {
		case REAL:
		    return (instr->prefixes.op_size) ? 4 : 2;
		case PROTECTED:
		case PROTECTED_PAE:
		    return (instr->prefixes.op_size) ? 2 : 4;
		case LONG_32_COMPAT:
		case LONG:
		default:
		    PrintError("Unsupported CPU mode: %d\n", info->cpu_mode);
		    return -1;
	    }

	case INVLPG:
	    switch (v3_get_vm_cpu_mode(info)) {
		case REAL:
		    PrintError("Invalid instruction given operating mode (%d)\n", form);
		    return 0;
		case PROTECTED:
		case PROTECTED_PAE:
		    return 4;
		case LONG_32_COMPAT:
		case LONG:
		default:
		    PrintError("Unsupported CPU mode: %d\n", info->cpu_mode);
		    return -1;
	    }

	case PUSHF:
	case POPF:
	    switch (v3_get_vm_cpu_mode(info)) {
		case REAL:
		    return 2;
		case PROTECTED:
		case PROTECTED_PAE:
		    return 4;
		case LONG_32_COMPAT:
		case LONG:
		default:
		    PrintError("Unsupported CPU mode: %d\n", info->cpu_mode);
		    return -1;
	    }

	case MOV_DR2:
	case MOV_2DR:
	case MOV_CR2:
	case MOV_2CR:
	    switch (v3_get_vm_cpu_mode(info)) {
		case REAL:
		case PROTECTED:
		case PROTECTED_PAE:
		    return 4;
		case LONG_32_COMPAT:
		case LONG:
		default:
		    PrintError("Unsupported CPU mode: %d\n", info->cpu_mode);
		    return -1;
	    }

	case MOV_SR2:
	case MOV_2SR:
	default:
	    PrintError("Unsupported instruction form %d\n", form);
	    return -1;
	
    }
    return 0;
}



typedef enum {INVALID_ADDR_TYPE, REG, DISP0, DISP8, DISP16, DISP32} modrm_mode_t;
typedef enum {INVALID_REG_SIZE, REG64, REG32, REG16, REG8} reg_size_t;


struct modrm_byte {
    uint_t rm   :   3;
    uint_t reg  :   3;
    uint_t mod  :   2;
} __attribute__((packed));


struct sib_byte {
    uint_t base     :   3;
    uint_t index    :   3;
    uint_t scale    :   2;
} __attribute__((packed));




struct v3_gprs;

static inline int decode_gpr(struct guest_info * core,
			     uint8_t reg_code,
			     struct x86_operand * reg) {

    struct v3_gprs * gprs = &(core->vm_regs);

    switch (reg_code) {
	case 0:
	    reg->operand = (addr_t)&(gprs->rax);
	    break;
	case 1:
	    reg->operand = (addr_t)&(gprs->rcx);
	    break;
	case 2:
	    reg->operand = (addr_t)&(gprs->rdx);
	    break;
	case 3:
	    reg->operand = (addr_t)&(gprs->rbx);
	    break;
	case 4:
	    if (reg->size == 1) {
		reg->operand = (addr_t)&(gprs->rax) + 1;
	    } else {
		reg->operand = (addr_t)&(gprs->rsp);
	    }
	    break;
	case 5:
	    if (reg->size == 1) {
		reg->operand = (addr_t)&(gprs->rcx) + 1;
	    } else {
		reg->operand = (addr_t)&(gprs->rbp);
	    }
	    break;
	case 6:
	    if (reg->size == 1) {
		reg->operand = (addr_t)&(gprs->rdx) + 1;
	    } else {
		reg->operand = (addr_t)&(gprs->rsi);
	    }
	    break;
	case 7:
	    if (reg->size == 1) {
		reg->operand = (addr_t)&(gprs->rbx) + 1;
	    } else {
		reg->operand = (addr_t)&(gprs->rdi);
	    }
	    break;
	default:
	    PrintError("Invalid Reg Code (%d)\n", reg_code);
	    reg->operand = 0;
	    return -1;
    }

    return 0;
}




static inline int decode_cr(struct guest_info * core, 			
			     uint8_t reg_code,
			     struct x86_operand * reg) {

    struct v3_ctrl_regs * crs = &(core->ctrl_regs);

    PrintDebug("\t Ctrl regs %d\n", reg_code);

    switch (reg_code) {
	case 0:
	    reg->operand = (addr_t)&(crs->cr0);
	    break;
	case 2:
	    reg->operand = (addr_t)&(crs->cr2);
	    break;
	case 3:
	    reg->operand = (addr_t)&(crs->cr3);
	    break;
	case 4:
	    reg->operand = (addr_t)&(crs->cr4);
	    break;
	default:
	    reg->operand = 0;
	    PrintError("Invalid Reg Code (%d)\n", reg_code);
	    return -1;
    }

    return 0;
}

// This converts the displacement into the appropriate masked value
/* 
   QUESTION: Are the register Values signed ?????
 */
#define MASK_DISPLACEMENT(reg, mode) ({					\
	    sint64_t val = 0;						\
	    if (mode == DISP8) {					\
		val = (sint8_t)(reg & 0xff);				\
	    } else if (mode == DISP16) {				\
		val = (sint16_t)(reg & 0xffff);				\
	    } else if (mode == DISP32) {				\
		val = (sint32_t)(reg & 0xffffffff);			\
	    } else {							\
		PrintError("Error invalid displacement size (%d)\n", mode); \
		/*V3_ASSERT(0);*/					\
	    }								\
	    val;							\
	})

static  int decode_rm_operand16(struct guest_info * core,
				uint8_t * modrm_instr, 
				struct x86_instr * instr,
				struct x86_operand * operand, 
				uint8_t * reg_code) { 

    struct v3_gprs * gprs = &(core->vm_regs);
    struct modrm_byte * modrm = (struct modrm_byte *)modrm_instr;
    addr_t base_addr = 0;
    modrm_mode_t mod_mode = 0;
    uint8_t * instr_cursor = modrm_instr;

    //  PrintDebug("ModRM mod=%d\n", modrm->mod);
    
    *reg_code = modrm->reg;

    instr_cursor += 1;

    if (modrm->mod == 3) {
	//PrintDebug("first operand = Register (RM=%d)\n",modrm->rm);
	operand->type = REG_OPERAND;

	decode_gpr(core, modrm->rm, operand);

    } else {
	struct v3_segment * seg = NULL;

	operand->type = MEM_OPERAND;

	if (modrm->mod == 0) {
	    mod_mode = DISP0;
	} else if (modrm->mod == 1) {
	    mod_mode = DISP8;
	} else if (modrm->mod == 2) {
	    mod_mode = DISP16;
	}

	switch (modrm->rm) {
	    case 0:
		base_addr = gprs->rbx + MASK_DISPLACEMENT(gprs->rsi, mod_mode);
		break;
	    case 1:
		base_addr = gprs->rbx + MASK_DISPLACEMENT(gprs->rdi, mod_mode);
		break;
	    case 2:
		base_addr = gprs->rbp + MASK_DISPLACEMENT(gprs->rsi, mod_mode);
		break;
	    case 3:
		base_addr = gprs->rbp + MASK_DISPLACEMENT(gprs->rdi, mod_mode);
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
 	    base_addr += *(sint8_t *)instr_cursor;
	    instr_cursor += 1;
	} else if (mod_mode == DISP16) {
	    base_addr += *(sint16_t *)instr_cursor;
	    instr_cursor += 2;
	}
    
	
	// get appropriate segment
	if (instr->prefixes.cs_override) {
	    seg = &(core->segments.cs);
	} else if (instr->prefixes.es_override) {
	    seg = &(core->segments.es);
	} else if (instr->prefixes.ss_override) {
	    seg = &(core->segments.ss);
	} else if (instr->prefixes.fs_override) {
	    seg = &(core->segments.fs);
	} else if (instr->prefixes.gs_override) {
	    seg = &(core->segments.gs);
	} else {
	    seg = &(core->segments.ds);
	}
	
	operand->operand = get_addr_linear(core, base_addr, seg);
    }


    return (instr_cursor - modrm_instr);
}


// returns num_bytes parsed
static int decode_rm_operand32(struct guest_info * core, 
			       uint8_t * modrm_instr,  
			       struct x86_instr * instr,
			       struct x86_operand * operand, 
			       uint8_t * reg_code) {

    struct v3_gprs * gprs = &(core->vm_regs);
    uint8_t * instr_cursor = modrm_instr;
    struct modrm_byte * modrm = (struct modrm_byte *)modrm_instr;
    addr_t base_addr = 0;
    modrm_mode_t mod_mode = 0;
    uint_t has_sib_byte = 0;


    *reg_code = modrm->reg;

    instr_cursor += 1;

    if (modrm->mod == 3) {
    	operand->type = REG_OPERAND;
	//    PrintDebug("first operand = Register (RM=%d)\n",modrm->rm);

	decode_gpr(core, modrm->rm, operand);

    } else {
	struct v3_segment * seg = NULL;

	operand->type = MEM_OPERAND;

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
	    int scale = 0x1 << sib->scale;

	    instr_cursor += 1;

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
		    base_addr += MASK_DISPLACEMENT(gprs->rax, mod_mode);
		    break;
		case 1:
		    base_addr += MASK_DISPLACEMENT(gprs->rcx, mod_mode);
		    break;
		case 2:
		    base_addr += MASK_DISPLACEMENT(gprs->rdx, mod_mode);
		    break;
		case 3:
		    base_addr += MASK_DISPLACEMENT(gprs->rbx, mod_mode);
		    break;
		case 4:
		    base_addr += MASK_DISPLACEMENT(gprs->rsp, mod_mode);
		    break;
		case 5:
		    if (modrm->mod != 0) {
			base_addr += MASK_DISPLACEMENT(gprs->rbp, mod_mode);
		    }
		    break;
		case 6:
		    base_addr += MASK_DISPLACEMENT(gprs->rsi, mod_mode);
		    break;
		case 7:
		    base_addr += MASK_DISPLACEMENT(gprs->rdi, mod_mode);
		    break;
	    }

	} 


	if (mod_mode == DISP8) {
	    base_addr += *(sint8_t *)instr_cursor;
	    instr_cursor += 1;
	} else if (mod_mode == DISP32) {
	    base_addr += *(sint32_t *)instr_cursor;
	    instr_cursor += 4;
	}
    
	// get appropriate segment
	if (instr->prefixes.cs_override) {
	    seg = &(core->segments.cs);
	} else if (instr->prefixes.es_override) {
	    seg = &(core->segments.es);
	} else if (instr->prefixes.ss_override) {
	    seg = &(core->segments.ss);
	} else if (instr->prefixes.fs_override) {
	    seg = &(core->segments.fs);
	} else if (instr->prefixes.gs_override) {
	    seg = &(core->segments.gs);
	} else {
	    seg = &(core->segments.ds);
	}
	
	operand->operand = get_addr_linear(core, base_addr, seg);
    }


    return (instr_cursor - modrm_instr);
}





static int decode_rm_operand(struct guest_info * core, 
			     uint8_t * instr_ptr,        // input
			     struct x86_instr * instr,
			     struct x86_operand * operand, 
			     uint8_t * reg_code) {
    
    v3_cpu_mode_t mode = v3_get_vm_cpu_mode(core);

    if (mode == REAL) {
	return decode_rm_operand16(core, instr_ptr, instr, operand, reg_code);
    } else if ((mode == PROTECTED) || (mode == PROTECTED_PAE)) {
	return decode_rm_operand32(core, instr_ptr, instr, operand, reg_code);
    } else {
	PrintError("Invalid CPU_MODE (%d)\n", mode);
	return -1;
    }
}
			     


static inline op_form_t op_code_to_form_0f(uint8_t * instr, int * length) {
    *length += 1;

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


static op_form_t op_code_to_form(uint8_t * instr, int * length) {
    *length += 1;

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
	    return op_code_to_form_0f(instr, length);

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
	    return MOV_2MEM_8;
	case 0x89:
	    return MOV_2MEM;
	case 0x8a:
	    return MOV_MEM2_8;
	case 0x8b:
	    return MOV_MEM2;
	    
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



static char * op_form_to_str(op_form_t form) {

    switch (form) {
	case LMSW: return "LMSW";
	case SMSW: return "SMSW";
	case CLTS: return "CLTS";
	case INVLPG: return "INVLPG";
	case MOV_CR2: return "MOV_CR2";
	case MOV_2CR: return "MOV_2CR";
	case MOV_DR2: return "MOV_DR2";
	case MOV_2DR: return "MOV_2DR";
	case MOV_SR2: return "MOV_SR2";
	case MOV_2SR: return "MOV_2SR";
	case MOV_MEM2_8: return "MOV_MEM2_8";
	case MOV_MEM2: return "MOV_MEM2";
	case MOV_2MEM_8: return "MOV_2MEM_8";
	case MOV_2MEM: return "MOV_2MEM";
	case MOV_MEM2AL_8: return "MOV_MEM2AL_8";
	case MOV_MEM2AX: return "MOV_MEM2AX";
	case MOV_AL2MEM_8: return "MOV_AL2MEM_8";
	case MOV_AX2MEM: return "MOV_AX2MEM";
	case MOV_IMM2_8: return "MOV_IMM2_8";
	case MOV_IMM2: return "MOV_IMM2";
	case MOVS_8: return "MOVS_8";
	case MOVS: return "MOVS";
	case MOVSX_8: return "MOVSX_8";
	case MOVSX: return "MOVSX";
	case MOVZX_8: return "MOVZX_8";
	case MOVZX: return "MOVZX";
	case HLT: return "HLT";
	case PUSHF: return "PUSHF";
	case POPF: return "POPF";
	case ADC_2MEM_8: return "ADC_2MEM_8";
	case ADC_2MEM: return "ADC_2MEM";
	case ADC_MEM2_8: return "ADC_MEM2_8";
	case ADC_MEM2: return "ADC_MEM2";
	case ADC_IMM2_8: return "ADC_IMM2_8";
	case ADC_IMM2: return "ADC_IMM2";
	case ADC_IMM2SX_8: return "ADC_IMM2SX_8";
	case ADD_IMM2_8: return "ADD_IMM2_8";
	case ADD_IMM2: return "ADD_IMM2";
	case ADD_IMM2SX_8: return "ADD_IMM2SX_8";
	case ADD_2MEM_8: return "ADD_2MEM_8";
	case ADD_2MEM: return "ADD_2MEM";
	case ADD_MEM2_8: return "ADD_MEM2_8";
	case ADD_MEM2: return "ADD_MEM2";
	case AND_MEM2_8: return "AND_MEM2_8";
	case AND_MEM2: return "AND_MEM2";
	case AND_2MEM_8: return "AND_2MEM_8";
	case AND_2MEM: return "AND_2MEM";
	case AND_IMM2_8: return "AND_IMM2_8";
	case AND_IMM2: return "AND_IMM2";
	case AND_IMM2SX_8: return "AND_IMM2SX_8";
	case OR_2MEM_8: return "OR_2MEM_8";
	case OR_2MEM: return "OR_2MEM";
	case OR_MEM2_8: return "OR_MEM2_8";
	case OR_MEM2: return "OR_MEM2";
	case OR_IMM2_8: return "OR_IMM2_8";
	case OR_IMM2: return "OR_IMM2";
	case OR_IMM2SX_8: return "OR_IMM2SX_8";
	case SUB_2MEM_8: return "SUB_2MEM_8";
	case SUB_2MEM: return "SUB_2MEM";
	case SUB_MEM2_8: return "SUB_MEM2_8";
	case SUB_MEM2: return "SUB_MEM2";
	case SUB_IMM2_8: return "SUB_IMM2_8";
	case SUB_IMM2: return "SUB_IMM2";
	case SUB_IMM2SX_8: return "SUB_IMM2SX_8";
	case XOR_2MEM_8: return "XOR_2MEM_8";
	case XOR_2MEM: return "XOR_2MEM";
	case XOR_MEM2_8: return "XOR_MEM2_8";
	case XOR_MEM2: return "XOR_MEM2";
	case XOR_IMM2_8: return "XOR_IMM2_8";
	case XOR_IMM2: return "XOR_IMM2";
	case XOR_IMM2SX_8: return "XOR_IMM2SX_8";
	case INC_8: return "INC_8";
	case INC: return "INC";
	case DEC_8: return "DEC_8";
	case DEC: return "DEC";
	case NEG_8: return "NEG_8";
	case NEG: return "NEG"; 
	case NOT_8: return "NOT_8";
	case NOT: return "NOT";
	case XCHG_8: return "XCHG_8";
	case XCHG: return "XCHG";
	case SETB: return "SETB";
	case SETBE: return "SETBE";
	case SETL: return "SETL";
	case SETLE: return "SETLE";
	case SETNB: return "SETNB";
	case SETNBE: return "SETNBE";
	case SETNL: return "SETNL";
	case SETNLE: return "SETNLE";
	case SETNO: return "SETNO";
	case SETNP: return "SETNP";
	case SETNS: return "SETNS";
	case SETNZ: return "SETNZ";
	case SETP: return "SETP";
	case SETS: return "SETS";
	case SETZ: return "SETZ";
	case SETO: return "SETO";
	case STOS_8: return "STOS_8";
	case STOS: return "STOS";

	case INVALID_INSTR:
	default:
	    return "INVALID_INSTR";
    }
}
