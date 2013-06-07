/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2012,  Alexander Kudryavtsev <alexk@ispras.ru>
 * Copyright (c) 2012, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * Author: Alexander Kudryavtsev <alexk@ispras.ru>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_mem_hook.h>
#include <palacios/vmm_emulator.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_hashtable.h>
#include <palacios/vmm_decoder.h>

#include <quix86/quix86.h>

#ifdef V3_CONFIG_TM_FUNC
#include <extensions/trans_mem.h>
#endif

#ifndef V3_CONFIG_DEBUG_DECODER
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif

#define GPR_REGISTER     0
#define SEGMENT_REGISTER 1
#define CTRL_REGISTER    2
#define DEBUG_REGISTER   3

// QUIX86 does not have to be initialised or deinitialised.
int v3_init_decoder(struct guest_info * core) {
    return 0;
}
int v3_deinit_decoder(struct guest_info * core) {
    return 0;
}

static int get_opcode(qx86_insn *inst, struct guest_info *core);
static int qx86_register_to_v3_reg(struct guest_info * info, int qx86_reg,
                 addr_t * v3_reg, uint_t * reg_len);

static int callback(void *data, int rindex, int subreg, unsigned char *value) {
    void* reg_addr = 0;
    uint_t reg_size;

    struct guest_info *info = (struct guest_info*)data;
    int v3_reg_type = qx86_register_to_v3_reg(info,
        rindex,
        (addr_t*)&reg_addr, &reg_size);

    if(v3_reg_type == -1) {
        PrintError(info->vm_info, info, "Callback failed to get register index %d\n", rindex);
        return 0;
    }

    *(uint64_t*)value = 0;
    switch(subreg) {
    case QX86_SUBREG_BASE:
        *(uint64_t*)value = ((struct v3_segment*)reg_addr)->base;
        break;
    case QX86_SUBREG_LIMIT:
        *(uint32_t*)value = ((struct v3_segment*)reg_addr)->limit;
        break;
    case QX86_SUBREG_FLAGS:
        PrintError(info->vm_info, info, "Callback doesn't know how to give flags.\n");
        return 0;
    case QX86_SUBREG_NONE: {
        switch(qx86_rinfo(rindex)->size) {
        case 1: *(uint8_t* )value = *(uint8_t* )reg_addr; break;
        case 2: *(uint16_t*)value = *(uint16_t*)reg_addr; break;
        case 4: *(uint32_t*)value = *(uint32_t*)reg_addr; break;
        case 8: *(uint64_t*)value = *(uint64_t*)reg_addr; break;
        }
        break;
    }
    }

    return 1;
}

static inline int qx86_op_to_v3_op(struct guest_info *info, qx86_insn *qx86_insn,
        int op_num, struct x86_operand * v3_op) {
    int status = 0;
    qx86_operand *qx86_op = &qx86_insn->operands[op_num];
    if (qx86_op->ot == QX86_OPERAND_TYPE_REGISTER) {
        int v3_reg_type = qx86_register_to_v3_reg(info,
                qx86_op->u.r.rindex,
                &(v3_op->operand), &(v3_op->size));

        if (v3_reg_type == -1) {
            PrintError(info->vm_info, info, "Operand %d is an Unhandled Operand: %s\n", op_num,
                    qx86_rinfo(qx86_op->u.r.rindex)->name);
            v3_op->type = INVALID_OPERAND;
            return -1;
        } else if (v3_reg_type == SEGMENT_REGISTER) {
            struct v3_segment * seg_reg = (struct v3_segment *)(v3_op->operand);
            v3_op->operand = (addr_t)&(seg_reg->selector);
        }
        v3_op->type = REG_OPERAND;

    } else if(qx86_op->ot == QX86_OPERAND_TYPE_MEMORY) {
        PrintDebug(info->vm_info, info, "Memory operand (%d)\n", op_num);
        if((status = qx86_calculate_linear_address(qx86_insn, op_num,
                (qx86_uint64*)&v3_op->operand)) != QX86_SUCCESS) {
            PrintError(info->vm_info, info, "Could not get memory operand %d: "
                    "qx86_calculate_linear_address() returns %d\n", op_num, status);
            return -1;
        }
        v3_op->type = MEM_OPERAND;
        v3_op->size = qx86_op->size;

    } else if(qx86_op->ot == QX86_OPERAND_TYPE_IMMEDIATE) {
        v3_op->size = qx86_op->u.i.valueSize;

        if (v3_op->size > 4) {
            PrintError(info->vm_info, info, "Unhandled 64 bit immediates\n");
            return -1;
        }
        v3_op->operand = (addr_t)*(uint64_t*)qx86_op->u.i.value;
        v3_op->type = IMM_OPERAND;

    } else {
        PrintError(info->vm_info, info, "Unhandled Operand %d Type %d\n", op_num, qx86_op->ot);
        return -1;
    }

    if (qx86_op->attributes & QX86_OPERAND_ATTRIBUTE_READ) {
        v3_op->read = 1;
    }
    if (qx86_op->attributes & QX86_OPERAND_ATTRIBUTE_WRITTEN) {
        v3_op->write = 1;
    }
    return 0;
}

int v3_decode(struct guest_info * info, addr_t instr_ptr, struct x86_instr * instr) {
    int proc_mode;
    qx86_insn qx86_inst;
    uint8_t inst_buf[QX86_INSN_SIZE_MAX];

    /* 441-tm: add 'escape' trap for Haswell instructions, dont want to stumble
     * on them!
     */
#ifdef V3_CONFIG_TM_FUNC
    {
        struct v3_trans_mem * tm = (struct v3_trans_mem *)v3_get_ext_core_state(info, "trans_mem");
        if (tm->TM_MODE == TM_ON) {
          int byte1 = *(uint8_t *)(instr_ptr);
          int byte2 = *(uint8_t *)(instr_ptr + 1);
          int byte3 = *(uint8_t *)(instr_ptr + 2);
          if (byte1 == 0xc7 && byte2 == 0xf8) {  /* third byte is an immediate */
            //V3_Print("Decoding  %x %x %d\n", byte1, byte2, byte3);
            instr->instr_length = 6;
            return 0;
          } else if (byte1 == 0xc6 && byte2 == 0xf8) { /* third byte is an immediate */
            //V3_Print("Decoding XABORT %x %x %d\n", byte1, byte2, byte3);
            instr->instr_length = 3;
            return 0;
          } else if (byte1 == 0x0f && byte2 == 0x01 && byte3 == 0xd5) {
            //V3_Print("Decoding XEND %x %x %x\n", byte1, byte2, byte3);
            instr->instr_length = 3;
            return 0;
          }
        }
    }
#endif

    memset(instr, 0, sizeof(struct x86_instr));
    memset(&qx86_inst, 0, sizeof(qx86_inst));

    v3_get_prefixes((uchar_t *)instr_ptr, &(instr->prefixes));

    switch(v3_get_vm_cpu_mode(info)) {
    case REAL: case LONG_16_COMPAT:
        proc_mode = QX86_SIZE_16; break;
    case PROTECTED: case PROTECTED_PAE: case LONG_32_COMPAT:
        proc_mode = QX86_SIZE_32; break;
    case LONG:
        proc_mode = QX86_SIZE_64; break;
    default:
        PrintError(info->vm_info, info, "Unsupported CPU mode: %d\n", info->cpu_mode);
        return -1;
    }

    int left_in_page = 0x1000 - (instr_ptr & 0xfff);
    if(left_in_page < QX86_INSN_SIZE_MAX) {
        addr_t instr_ptr2;
        int status = 0;

        if (info->mem_mode == PHYSICAL_MEM) {
            status = v3_gpa_to_hva(info, get_addr_linear(info,
                    (info->rip & ~0xfffULL) + 0x1000, &(info->segments.cs)), &instr_ptr2);
        } else {
            status = v3_gva_to_hva(info, get_addr_linear(info,
                    (info->rip & ~0xfffULL) + 0x1000, &(info->segments.cs)), &instr_ptr2);
        }
        if (status == -1) {
            PrintError(info->vm_info, info, "Could not translate Instruction Address at second stage "
                    "translation (%p)\n", (void *)(addr_t)info->rip);
            return -1;
        }

        if(((instr_ptr & ~0xfffUL) + 0x1000) != instr_ptr2) {
            PrintError(info->vm_info, info, "Note: physical page non-contiguous\n");
            memcpy(inst_buf, (const void*)instr_ptr, left_in_page);
            memcpy(inst_buf + left_in_page, (const void*)instr_ptr2,
                    QX86_INSN_SIZE_MAX - left_in_page);
            instr_ptr = (addr_t)inst_buf;
        } // in other case, address space is contiguous and everything is OK
    }

    qx86_inst.callback = callback;
    qx86_inst.data = info;

    int status = qx86_decode(&qx86_inst, proc_mode,
            (const void*)instr_ptr, QX86_INSN_SIZE_MAX);
    if(status != QX86_SUCCESS) {
        PrintError(info->vm_info, info, "qx86_decode() returned %d\n", status);
        return -1;
    }

    instr->instr_length = qx86_inst.rawSize;

    // 441 - dump memory for quix86 debugging
    if ((instr->op_type = get_opcode(&qx86_inst,info)) == V3_INVALID_OP) {
        PrintError(info->vm_info, info, "++==++ QX86 DECODE ++==++\n");
        v3_dump_mem((void *)instr_ptr, 15);
        PrintError(info->vm_info, info, "Could not get opcode. (mnemonic=%s)\n",
                qx86_minfo(qx86_inst.mnemonic)->name);
        return -1;
    }
    if ((instr->op_type = get_opcode(&qx86_inst, info)) == V3_INVALID_OP) {
        PrintError(info->vm_info, info, "Could not get opcode. (mnemonic=%s)\n",
                qx86_minfo(qx86_inst.mnemonic)->name);
        return -1;
    }

    if(instr->op_type == V3_OP_MOVS || instr->op_type == V3_OP_STOS) {
        instr->is_str_op = 1;
        if (instr->prefixes.rep == 1) {
            uint64_t a_mask = (~0ULL >>
                (64 - QX86_SIZE_OCTETS(qx86_inst.attributes.addressSize) * 8));

            instr->str_op_length = info->vm_regs.rcx & a_mask;
        } else {
            instr->str_op_length = 1;
        }
    } else {
        instr->is_str_op = 0;
        instr->str_op_length = 0;
    }

    instr->num_operands = qx86_inst.operandCount;

    // set first operand
    if (instr->num_operands >= 1) {
        if (qx86_op_to_v3_op(info, &qx86_inst, 0, &instr->dst_operand) != 0)
            return -1;
    }

    // set second operand
    if (instr->num_operands >= 2) {
        if (qx86_op_to_v3_op(info, &qx86_inst, 1, &instr->src_operand) != 0)
            return -1;
    }

    // set third operand
    if (instr->num_operands >= 3) {
        if (qx86_op_to_v3_op(info, &qx86_inst, 2, &instr->third_operand) != 0)
            return -1;
    }

#ifdef V3_CONFIG_DEBUG_DECODER
    qx86_print_options_intel opt;
    char buf[128];
    int buf_sz = 128;
    if(qx86_print_intel(&qx86_inst, &opt, buf, &buf_sz) != QX86_SUCCESS) {
        PrintDebug(info->vm_info, info, "Print failed!\n");
    } else {
        PrintDebug(info->vm_info, info, "Instruction (%p): %s\n", (void*)info->rip, buf);
    }
    PrintDebug(info->vm_info, info, "Operands: dst %p src %p 3rd %p\n", (void*)instr->dst_operand.operand,
            (void*)instr->src_operand.operand, (void*)instr->third_operand.operand);
#endif
    return 0;
}

static int get_opcode(qx86_insn *inst, struct guest_info *core) {
    switch (inst->mnemonic) {
#define IS_CR(op) inst->operands[op].ot == QX86_OPERAND_TYPE_REGISTER && \
    qx86_rinfo(inst->operands[op].u.r.rindex)->rclass == QX86_RCLASS_CREG

    /* MOV cases */
    case QX86_MNEMONIC_MOV: {
        if(inst->operands[0].ot == QX86_OPERAND_TYPE_MEMORY
                || inst->operands[1].ot == QX86_OPERAND_TYPE_MEMORY)
            return V3_OP_MOV;
        if(IS_CR(0))
            return V3_OP_MOV2CR;
        if(IS_CR(1))
            return V3_OP_MOVCR2;
        // 441 - mov reg reg is also ok
        if(inst->operands[0].ot == QX86_OPERAND_TYPE_REGISTER
                || inst->operands[1].ot == QX86_OPERAND_TYPE_REGISTER)
            return V3_OP_MOV;

        PrintError(core->vm_info, core, "Bad operand types for MOV: %d %d\n", inst->operands[0].ot,
                inst->operands[1].ot);
        return V3_INVALID_OP;
    }

    /* Control Instructions */
    case QX86_MNEMONIC_SMSW:
        return V3_OP_SMSW;

    case QX86_MNEMONIC_LMSW:
        return V3_OP_LMSW;

    case QX86_MNEMONIC_CLTS:
        return V3_OP_CLTS;

    case QX86_MNEMONIC_INVLPG:
        return V3_OP_INVLPG;

    /* Data Instructions */
    case QX86_MNEMONIC_ADC:
        return V3_OP_ADC;

    case QX86_MNEMONIC_ADD:
        return V3_OP_ADD;

    case QX86_MNEMONIC_AND:
        return V3_OP_AND;

    case QX86_MNEMONIC_SUB:
        return V3_OP_SUB;


    case QX86_MNEMONIC_MOVZX:
        return V3_OP_MOVZX;

    case QX86_MNEMONIC_MOVSX:
        return V3_OP_MOVSX;


    case QX86_MNEMONIC_DEC:
        return V3_OP_DEC;

    case QX86_MNEMONIC_INC:
        return V3_OP_INC;

    case QX86_MNEMONIC_OR:
        return V3_OP_OR;

    case QX86_MNEMONIC_XOR:
        return V3_OP_XOR;

    case QX86_MNEMONIC_NEG:
        return V3_OP_NEG;

    case QX86_MNEMONIC_NOT:
        return V3_OP_NOT;

    case QX86_MNEMONIC_XCHG:
        return V3_OP_XCHG;

    case QX86_MNEMONIC_SETB:
        return V3_OP_SETB;

    case QX86_MNEMONIC_SETBE:
        return V3_OP_SETBE;

    case QX86_MNEMONIC_SETL:
        return V3_OP_SETL;

    case QX86_MNEMONIC_SETLE:
        return V3_OP_SETLE;

    case QX86_MNEMONIC_SETAE:
        return V3_OP_SETNB;

    case QX86_MNEMONIC_SETA:
        return V3_OP_SETNBE;

    case QX86_MNEMONIC_SETGE:
        return V3_OP_SETNL;

    case QX86_MNEMONIC_SETG:
        return V3_OP_SETNLE;

    case QX86_MNEMONIC_SETNO:
        return V3_OP_SETNO;

    case QX86_MNEMONIC_SETNP:
        return V3_OP_SETNP;

    case QX86_MNEMONIC_SETNS:
        return V3_OP_SETNS;

    case QX86_MNEMONIC_SETNZ:
        return V3_OP_SETNZ;

    case QX86_MNEMONIC_SETO:
        return V3_OP_SETO;

    case QX86_MNEMONIC_SETP:
        return V3_OP_SETP;

    case QX86_MNEMONIC_SETS:
        return V3_OP_SETS;

    case QX86_MNEMONIC_SETZ:
        return V3_OP_SETZ;

    case QX86_MNEMONIC_MOVSB:
    case QX86_MNEMONIC_MOVSW:
    case QX86_MNEMONIC_MOVSD:
    case QX86_MNEMONIC_MOVSQ:
        return V3_OP_MOVS;

    case QX86_MNEMONIC_STOSB:
    case QX86_MNEMONIC_STOSW:
    case QX86_MNEMONIC_STOSD:
    case QX86_MNEMONIC_STOSQ:
        return V3_OP_STOS;

    /* 441-tm: add in CMP, POP, JLE, CALL cases */
    case QX86_MNEMONIC_CMP:
        return V3_OP_CMP;

    case QX86_MNEMONIC_POP:
        return V3_OP_POP;

    case QX86_MNEMONIC_JLE:
        return V3_OP_JLE;

    case QX86_MNEMONIC_CALL:
        return V3_OP_CALL;

    case QX86_MNEMONIC_TEST:
        return V3_OP_TEST;

    case QX86_MNEMONIC_PUSH:
        return V3_OP_PUSH;

    case QX86_MNEMONIC_JAE:
        return V3_OP_JAE;

    case QX86_MNEMONIC_JMP:
        return V3_OP_JMP;

    case QX86_MNEMONIC_JNZ:
        return V3_OP_JNZ;

    case QX86_MNEMONIC_JZ:
        return V3_OP_JZ;

    case QX86_MNEMONIC_RET:
        return V3_OP_RET;

    case QX86_MNEMONIC_IMUL:
        return V3_OP_IMUL;

    case QX86_MNEMONIC_LEA:
        return V3_OP_LEA;

    case QX86_MNEMONIC_JL:
        return V3_OP_JL;

    case QX86_MNEMONIC_CMOVZ:
        return V3_OP_CMOVZ;

    case QX86_MNEMONIC_MOVSXD:
        return V3_OP_MOVSXD;

    case QX86_MNEMONIC_JNS:
        return V3_OP_JNS;

    case QX86_MNEMONIC_CMOVS:
        return V3_OP_CMOVS;

    case QX86_MNEMONIC_SHL:
        return V3_OP_SHL;

    case QX86_MNEMONIC_INT:
        return V3_OP_INT;

    default:
        return V3_INVALID_OP;
    }
}

static int qx86_register_to_v3_reg(struct guest_info * info, int qx86_reg,
                 addr_t * v3_reg, uint_t * reg_len) {
    PrintDebug(info->vm_info, info, "qx86 Register: %s\n", qx86_rinfo(qx86_reg)->name);

    switch (qx86_reg) {
    case QX86_REGISTER_INVALID:
        *v3_reg = 0;
        *reg_len = 0;
        return -1;

    case QX86_REGISTER_RAX:
        *v3_reg = (addr_t)&(info->vm_regs.rax);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_EAX:
        *v3_reg = (addr_t)&(info->vm_regs.rax);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_AX:
        *v3_reg = (addr_t)&(info->vm_regs.rax);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_AH:
        *v3_reg = (addr_t)(&(info->vm_regs.rax)) + 1;
        *reg_len = 1;
        return GPR_REGISTER;
    case QX86_REGISTER_AL:
        *v3_reg = (addr_t)&(info->vm_regs.rax);
        *reg_len = 1;
        return GPR_REGISTER;

    case QX86_REGISTER_RCX:
        *v3_reg = (addr_t)&(info->vm_regs.rcx);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_ECX:
        *v3_reg = (addr_t)&(info->vm_regs.rcx);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_CX:
        *v3_reg = (addr_t)&(info->vm_regs.rcx);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_CH:
        *v3_reg = (addr_t)(&(info->vm_regs.rcx)) + 1;
        *reg_len = 1;
        return GPR_REGISTER;
    case QX86_REGISTER_CL:
        *v3_reg = (addr_t)&(info->vm_regs.rcx);
        *reg_len = 1;
        return GPR_REGISTER;

    case QX86_REGISTER_RDX:
        *v3_reg = (addr_t)&(info->vm_regs.rdx);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_EDX:
        *v3_reg = (addr_t)&(info->vm_regs.rdx);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_DX:
        *v3_reg = (addr_t)&(info->vm_regs.rdx);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_DH:
        *v3_reg = (addr_t)(&(info->vm_regs.rdx)) + 1;
        *reg_len = 1;
        return GPR_REGISTER;
    case QX86_REGISTER_DL:
        *v3_reg = (addr_t)&(info->vm_regs.rdx);
        *reg_len = 1;
        return GPR_REGISTER;

    case QX86_REGISTER_RBX:
        *v3_reg = (addr_t)&(info->vm_regs.rbx);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_EBX:
        *v3_reg = (addr_t)&(info->vm_regs.rbx);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_BX:
        *v3_reg = (addr_t)&(info->vm_regs.rbx);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_BH:
        *v3_reg = (addr_t)(&(info->vm_regs.rbx)) + 1;
        *reg_len = 1;
        return GPR_REGISTER;
    case QX86_REGISTER_BL:
        *v3_reg = (addr_t)&(info->vm_regs.rbx);
        *reg_len = 1;
        return GPR_REGISTER;


    case QX86_REGISTER_RSP:
        *v3_reg = (addr_t)&(info->vm_regs.rsp);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_ESP:
        *v3_reg = (addr_t)&(info->vm_regs.rsp);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_SP:
        *v3_reg = (addr_t)&(info->vm_regs.rsp);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_SPL:
        *v3_reg = (addr_t)&(info->vm_regs.rsp);
        *reg_len = 1;
        return GPR_REGISTER;

    case QX86_REGISTER_RBP:
        *v3_reg = (addr_t)&(info->vm_regs.rbp);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_EBP:
        *v3_reg = (addr_t)&(info->vm_regs.rbp);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_BP:
        *v3_reg = (addr_t)&(info->vm_regs.rbp);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_BPL:
        *v3_reg = (addr_t)&(info->vm_regs.rbp);
        *reg_len = 1;
        return GPR_REGISTER;



    case QX86_REGISTER_RSI:
        *v3_reg = (addr_t)&(info->vm_regs.rsi);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_ESI:
        *v3_reg = (addr_t)&(info->vm_regs.rsi);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_SI:
        *v3_reg = (addr_t)&(info->vm_regs.rsi);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_SIL:
        *v3_reg = (addr_t)&(info->vm_regs.rsi);
        *reg_len = 1;
        return GPR_REGISTER;


    case QX86_REGISTER_RDI:
        *v3_reg = (addr_t)&(info->vm_regs.rdi);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_EDI:
        *v3_reg = (addr_t)&(info->vm_regs.rdi);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_DI:
        *v3_reg = (addr_t)&(info->vm_regs.rdi);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_DIL:
        *v3_reg = (addr_t)&(info->vm_regs.rdi);
        *reg_len = 1;
        return GPR_REGISTER;





    case QX86_REGISTER_R8:
        *v3_reg = (addr_t)&(info->vm_regs.r8);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_R8D:
        *v3_reg = (addr_t)&(info->vm_regs.r8);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_R8W:
        *v3_reg = (addr_t)&(info->vm_regs.r8);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_R8B:
        *v3_reg = (addr_t)&(info->vm_regs.r8);
        *reg_len = 1;
        return GPR_REGISTER;

    case QX86_REGISTER_R9:
        *v3_reg = (addr_t)&(info->vm_regs.r9);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_R9D:
        *v3_reg = (addr_t)&(info->vm_regs.r9);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_R9W:
        *v3_reg = (addr_t)&(info->vm_regs.r9);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_R9B:
        *v3_reg = (addr_t)&(info->vm_regs.r9);
        *reg_len = 1;
        return GPR_REGISTER;

    case QX86_REGISTER_R10:
        *v3_reg = (addr_t)&(info->vm_regs.r10);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_R10D:
        *v3_reg = (addr_t)&(info->vm_regs.r10);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_R10W:
        *v3_reg = (addr_t)&(info->vm_regs.r10);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_R10B:
        *v3_reg = (addr_t)&(info->vm_regs.r10);
        *reg_len = 1;
        return GPR_REGISTER;

    case QX86_REGISTER_R11:
        *v3_reg = (addr_t)&(info->vm_regs.r11);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_R11D:
        *v3_reg = (addr_t)&(info->vm_regs.r11);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_R11W:
        *v3_reg = (addr_t)&(info->vm_regs.r11);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_R11B:
        *v3_reg = (addr_t)&(info->vm_regs.r11);
        *reg_len = 1;
        return GPR_REGISTER;

    case QX86_REGISTER_R12:
        *v3_reg = (addr_t)&(info->vm_regs.r12);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_R12D:
        *v3_reg = (addr_t)&(info->vm_regs.r12);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_R12W:
        *v3_reg = (addr_t)&(info->vm_regs.r12);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_R12B:
        *v3_reg = (addr_t)&(info->vm_regs.r12);
        *reg_len = 1;
        return GPR_REGISTER;

    case QX86_REGISTER_R13:
        *v3_reg = (addr_t)&(info->vm_regs.r13);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_R13D:
        *v3_reg = (addr_t)&(info->vm_regs.r13);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_R13W:
        *v3_reg = (addr_t)&(info->vm_regs.r13);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_R13B:
        *v3_reg = (addr_t)&(info->vm_regs.r13);
        *reg_len = 1;
        return GPR_REGISTER;

    case QX86_REGISTER_R14:
        *v3_reg = (addr_t)&(info->vm_regs.r14);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_R14D:
        *v3_reg = (addr_t)&(info->vm_regs.r14);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_R14W:
        *v3_reg = (addr_t)&(info->vm_regs.r14);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_R14B:
        *v3_reg = (addr_t)&(info->vm_regs.r14);
        *reg_len = 1;
        return GPR_REGISTER;

    case QX86_REGISTER_R15:
        *v3_reg = (addr_t)&(info->vm_regs.r15);
        *reg_len = 8;
        return GPR_REGISTER;
    case QX86_REGISTER_R15D:
        *v3_reg = (addr_t)&(info->vm_regs.r15);
        *reg_len = 4;
        return GPR_REGISTER;
    case QX86_REGISTER_R15W:
        *v3_reg = (addr_t)&(info->vm_regs.r15);
        *reg_len = 2;
        return GPR_REGISTER;
    case QX86_REGISTER_R15B:
        *v3_reg = (addr_t)&(info->vm_regs.r15);
        *reg_len = 1;
        return GPR_REGISTER;


    case QX86_REGISTER_RIP:
        *v3_reg = (addr_t)&(info->rip);
        *reg_len = 8;
        return CTRL_REGISTER;
    case QX86_REGISTER_EIP:
        *v3_reg = (addr_t)&(info->rip);
        *reg_len = 4;
        return CTRL_REGISTER;
    case QX86_REGISTER_IP:
        *v3_reg = (addr_t)&(info->rip);
        *reg_len = 2;
        return CTRL_REGISTER;

    case QX86_REGISTER_FLAGS:
        *v3_reg = (addr_t)&(info->ctrl_regs.rflags);
        *reg_len = 2;
        return CTRL_REGISTER;
    case QX86_REGISTER_EFLAGS:
        *v3_reg = (addr_t)&(info->ctrl_regs.rflags);
        *reg_len = 4;
        return CTRL_REGISTER;
    case QX86_REGISTER_RFLAGS:
        *v3_reg = (addr_t)&(info->ctrl_regs.rflags);
        *reg_len = 8;
        return CTRL_REGISTER;

    case QX86_REGISTER_CR0:
        *v3_reg = (addr_t)&(info->ctrl_regs.cr0);
        *reg_len = 4;
        return CTRL_REGISTER;
    case QX86_REGISTER_CR2:
        *v3_reg = (addr_t)&(info->ctrl_regs.cr2);
        *reg_len = 4;
        return CTRL_REGISTER;
    case QX86_REGISTER_CR3:
        *v3_reg = (addr_t)&(info->ctrl_regs.cr3);
        *reg_len = 4;
        return CTRL_REGISTER;
    case QX86_REGISTER_CR4:
        *v3_reg = (addr_t)&(info->ctrl_regs.cr4);
        *reg_len = 4;
        return CTRL_REGISTER;
    case QX86_REGISTER_CR8:
        *v3_reg = (addr_t)&(info->ctrl_regs.apic_tpr);
        *reg_len = 4;
        return CTRL_REGISTER;

    case QX86_REGISTER_CR1:
    case QX86_REGISTER_CR5:
    case QX86_REGISTER_CR6:
    case QX86_REGISTER_CR7:
    case QX86_REGISTER_CR9:
    case QX86_REGISTER_CR10:
    case QX86_REGISTER_CR11:
    case QX86_REGISTER_CR12:
    case QX86_REGISTER_CR13:
    case QX86_REGISTER_CR14:
    case QX86_REGISTER_CR15:
        return -1;


    case QX86_REGISTER_CS:
        *v3_reg = (addr_t)&(info->segments.cs);
        *reg_len = 8;
        return SEGMENT_REGISTER;
    case QX86_REGISTER_DS:
        *v3_reg = (addr_t)&(info->segments.ds);
        *reg_len = 8;
        return SEGMENT_REGISTER;
    case QX86_REGISTER_ES:
        *v3_reg = (addr_t)&(info->segments.es);
        *reg_len = 8;
        return SEGMENT_REGISTER;
    case QX86_REGISTER_SS:
        *v3_reg = (addr_t)&(info->segments.ss);
        *reg_len = 8;
        return SEGMENT_REGISTER;
    case QX86_REGISTER_FS:
        *v3_reg = (addr_t)&(info->segments.fs);
        *reg_len = 8;
        return SEGMENT_REGISTER;
    case QX86_REGISTER_GS:
        *v3_reg = (addr_t)&(info->segments.gs);
        *reg_len = 8;
        return SEGMENT_REGISTER;


    case QX86_REGISTER_DR0:
    case QX86_REGISTER_DR1:
    case QX86_REGISTER_DR2:
    case QX86_REGISTER_DR3:
    case QX86_REGISTER_DR4:
    case QX86_REGISTER_DR5:
    case QX86_REGISTER_DR6:
    case QX86_REGISTER_DR7:
    case QX86_REGISTER_DR8:
    case QX86_REGISTER_DR9:
    case QX86_REGISTER_DR10:
    case QX86_REGISTER_DR11:
    case QX86_REGISTER_DR12:
    case QX86_REGISTER_DR13:
    case QX86_REGISTER_DR14:
    case QX86_REGISTER_DR15:
        return -1;


    case QX86_REGISTER_XMM0:
    case QX86_REGISTER_XMM1:
    case QX86_REGISTER_XMM2:
    case QX86_REGISTER_XMM3:
    case QX86_REGISTER_XMM4:
    case QX86_REGISTER_XMM5:
    case QX86_REGISTER_XMM6:
    case QX86_REGISTER_XMM7:
    case QX86_REGISTER_XMM8:
    case QX86_REGISTER_XMM9:
    case QX86_REGISTER_XMM10:
    case QX86_REGISTER_XMM11:
    case QX86_REGISTER_XMM12:
    case QX86_REGISTER_XMM13:
    case QX86_REGISTER_XMM14:
    case QX86_REGISTER_XMM15:

    case QX86_REGISTER_YMM0:
    case QX86_REGISTER_YMM1:
    case QX86_REGISTER_YMM2:
    case QX86_REGISTER_YMM3:
    case QX86_REGISTER_YMM4:
    case QX86_REGISTER_YMM5:
    case QX86_REGISTER_YMM6:
    case QX86_REGISTER_YMM7:
    case QX86_REGISTER_YMM8:
    case QX86_REGISTER_YMM9:
    case QX86_REGISTER_YMM10:
    case QX86_REGISTER_YMM11:
    case QX86_REGISTER_YMM12:
    case QX86_REGISTER_YMM13:
    case QX86_REGISTER_YMM14:
    case QX86_REGISTER_YMM15:

    case QX86_REGISTER_MMX0:
    case QX86_REGISTER_MMX1:
    case QX86_REGISTER_MMX2:
    case QX86_REGISTER_MMX3:
    case QX86_REGISTER_MMX4:
    case QX86_REGISTER_MMX5:
    case QX86_REGISTER_MMX6:
    case QX86_REGISTER_MMX7:

    case QX86_REGISTER_ST0:
    case QX86_REGISTER_ST1:
    case QX86_REGISTER_ST2:
    case QX86_REGISTER_ST3:
    case QX86_REGISTER_ST4:
    case QX86_REGISTER_ST5:
    case QX86_REGISTER_ST6:
    case QX86_REGISTER_ST7:
        return -1;

    }


    return 0;
}
