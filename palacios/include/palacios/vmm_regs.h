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

#ifndef __VMM_REGS_H__
#define __VMM_REGS_H__

#ifdef __V3VEE__

#include <palacios/vmm.h>

struct v3_gprs {
    v3_reg_t rdi;
    v3_reg_t rsi;
    v3_reg_t rbp;
    v3_reg_t rsp;
    v3_reg_t rbx;
    v3_reg_t rdx;
    v3_reg_t rcx;
    v3_reg_t rax;

    v3_reg_t r8;
    v3_reg_t r9;
    v3_reg_t r10;
    v3_reg_t r11;
    v3_reg_t r12;
    v3_reg_t r13;
    v3_reg_t r14;
    v3_reg_t r15;
  
} __attribute__((packed));


struct v3_ctrl_regs {
    v3_reg_t cr0;
    v3_reg_t cr2;
    v3_reg_t cr3;
    v3_reg_t cr4;
    v3_reg_t cr8;
    v3_reg_t rflags;
    v3_reg_t efer;
};



struct v3_dbg_regs {
    v3_reg_t dr0;
    v3_reg_t dr1;
    v3_reg_t dr2;
    v3_reg_t dr3;
    v3_reg_t dr6;
    v3_reg_t dr7;
};

struct v3_segment {
    uint16_t selector;
    uint_t limit;
    uint64_t base;
    uint_t type           : 4;
    uint_t system         : 1;
    uint_t dpl            : 2;
    uint_t present        : 1;
    uint_t avail          : 1;
    uint_t long_mode      : 1;
    uint_t db             : 1;
    uint_t granularity    : 1;
    uint_t unusable       : 1;
} __attribute__((packed));


struct v3_segments {
    struct v3_segment cs;
    struct v3_segment ds;
    struct v3_segment es;
    struct v3_segment fs;
    struct v3_segment gs;
    struct v3_segment ss;
    struct v3_segment ldtr;
    struct v3_segment gdtr;
    struct v3_segment idtr;
    struct v3_segment tr;
};


#endif

#endif
