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

#ifndef __VMX_HANDLER_H__
#define __VMX_HANDLER_H__

#ifdef __V3VEE__

#include <palacios/vm_guest.h>


/******************************************/
/* VMX Intercept Exit Codes               */
/******************************************/
typedef enum {
    VMX_EXIT_INFO_EXCEPTION_OR_NMI            = 0,
    VMX_EXIT_EXTERNAL_INTR                    = 1,
    VMX_EXIT_TRIPLE_FAULT                     = 2,
    VMX_EXIT_INIT_SIGNAL                      = 3,
    VMX_EXIT_STARTUP_IPI                      = 4,
    VMX_EXIT_IO_SMI                           = 5,
    VMX_EXIT_OTHER_SMI                        = 6,
    VMX_EXIT_INTR_WINDOW                      = 7,
    VMX_EXIT_NMI_WINDOW                       = 8,
    VMX_EXIT_TASK_SWITCH                      = 9,
    VMX_EXIT_CPUID                            = 10,
    VMX_EXIT_HLT                              = 12,
    VMX_EXIT_INVD                             = 13,
    VMX_EXIT_INVLPG                           = 14,
    VMX_EXIT_RDPMC                            = 15,
    VMX_EXIT_RDTSC                            = 16,
    VMX_EXIT_RSM                              = 17,
    VMX_EXIT_VMCALL                           = 18,
    VMX_EXIT_VMCLEAR                          = 19,
    VMX_EXIT_VMLAUNCH                         = 20,
    VMX_EXIT_VMPTRLD                          = 21,
    VMX_EXIT_VMPTRST                          = 22,
    VMX_EXIT_VMREAD                           = 23,
    VMX_EXIT_VMRESUME                         = 24,
    VMX_EXIT_VMWRITE                          = 25,
    VMX_EXIT_VMXOFF                           = 26,
    VMX_EXIT_VMXON                            = 27,
    VMX_EXIT_CR_REG_ACCESSES                  = 28,
    VMX_EXIT_MOV_DR                           = 29,
    VMX_EXIT_IO_INSTR                         = 30,
    VMX_EXIT_RDMSR                            = 31,
    VMX_EXIT_WRMSR                            = 32,
    VMX_EXIT_INVALID_GUEST_STATE              = 33,
    VMX_EXIT_INVALID_MSR_LOAD                 = 34,
    VMX_EXIT_MWAIT                            = 36,
    VMX_EXIT_MONITOR                          = 39,
    VMX_EXIT_PAUSE                            = 40,
    VMX_EXIT_INVALID_MACHINE_CHECK            = 41,
    VMX_EXIT_TPR_BELOW_THRESHOLD              = 43,
    VMX_EXIT_APIC                             = 44,
    VMX_EXIT_GDTR_IDTR                        = 46,
    VMX_EXIT_LDTR_TR                          = 47,
    VMX_EXIT_EPT_VIOLATION                    = 48,
    VMX_EXIT_EPT_CONFIG                       = 49,
    VMX_EXIT_INVEPT                           = 50,
    VMX_EXIT_RDTSCP                           = 51,
    VMX_EXIT_EXPIRED_PREEMPT_TIMER            = 52,
    VMX_EXIT_INVVPID                          = 53,
    VMX_EXIT_WBINVD                           = 54,
    VMX_EXIT_XSETBV                           = 55
} vmx_exit_t;

/* VMCS Exit QUALIFICATIONs */
struct vmx_exit_io_qual {
    union {
        uint32_t value;
	struct {
	    uint32_t access_size : 3; // (0: 1 Byte ;; 1: 2 Bytes ;; 3: 4 Bytes)
	    uint32_t dir        : 1; // (0: Out ;; 1: In)
	    uint32_t string     : 1; // (0: not string ;; 1: string)
	    uint32_t rep        : 1; // (0: not REP ;; 1: REP)
	    uint32_t op_enc      : 1; // (0: DX ;; 1: immediate)
	    uint32_t rsvd       : 9; // Set to 0
	    uint32_t port       : 16; // IO Port Number
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct vmx_exit_io_instr_info {
    union {
        uint32_t value;
	struct {
	    uint32_t undef1     : 7;
	    uint32_t addr_size  : 3;
	    uint32_t undef2     : 5;
	    uint32_t seg_reg    : 3;
	    uint32_t undef3     : 14;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/* Exit Interrupt Vector Info */
struct vmx_exit_int_info {
    union {
        uint32_t value;
	struct {
	    uint32_t vector     : 8; // IRQ number, exception vector, NMI = 2 
	    uint32_t type       : 3; // (0: ext. IRQ , 2: NMI , 3: hw exception , 6: sw exception
	    uint32_t error_code : 1; // 1: error Code present
	    uint32_t nmi_unblock : 1; // something to do with NMIs and IRETs (Intel 3B, sec. 23.2.2) 
	    uint32_t rsvd       : 18; // always 0
	    uint32_t valid      : 1; // always 1 if valid
	} __attribute__ ((packed));
    } __attribute__ ((packed));
} __attribute__((packed));

/* VMX entry interrupt format */
struct vmx_entry_int_info {
    union {
        uint32_t value;
	struct {
	    uint32_t vector     : 8; // IRQ/exception vector number
	    uint32_t type       : 3; // (0: ext. IRQ, 2: NMI, 3: hw excp, 4: sw int, 5: priv. sw excp, 6: sw excp, 7: other
	    uint32_t error_code : 1; // 1: deliver error code
	    uint32_t rsvd       : 19;
	    uint32_t valid      : 1; // 1: valid
	} __attribute__ ((packed));
    } __attribute__ ((packed));
} __attribute__ ((packed));




struct VMExitDBGQual {
    uint32_t B0         : 1; // Breakpoint 0 condition met
    uint32_t B1         : 1; // Breakpoint 1 condition met
    uint32_t B2         : 1; // Breakpoint 2 condition met
    uint32_t B3         : 1; // Breakpoint 3 condition met
    uint32_t rsvd       : 9; // reserved to 0
    uint32_t BD         : 1; // detected DBG reg access
    uint32_t BS         : 1; // cause either single instr or taken branch
} __attribute__((packed));


 struct VMExitTSQual {
    uint32_t selector   : 16; // selector of destination TSS 
    uint32_t rsvd       : 14; // reserved to 0
    uint32_t src        : 2; // (0: CALL ; 1: IRET ; 2: JMP ; 3: Task gate in IDT)
} __attribute__((packed));

struct vmx_exit_cr_qual {
    union {
        uint32_t value;
	struct {
	    uint32_t cr_id       : 4; // cr number (0 for CLTS and LMSW) (bit 3 always 0, on 32bit)
	    uint32_t access_type : 2; // (0: MOV to CR ; 1: MOV from CR ; 2: CLTS ; 3: LMSW)
	    uint32_t lmsw_op_type : 1; // (0: register ; 1: memory)
	    uint32_t rsvd1      : 1; // reserved to 0
	    uint32_t gpr        : 4; // (0:RAX+[CLTS/LMSW], 1:RCX, 2:RDX, 3:RBX, 4:RSP, 5:RBP, 6:RSI, 6:RDI, 8-15:64bit regs)
	    uint32_t rsvd2      : 4; // reserved to 0
	    uint32_t lmsw_src    : 16; // src data for lmsw
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct VMExitMovDRQual {
    uint32_t regID      : 3; // debug register number
    uint32_t rsvd1      : 1; // reserved to 0
    uint32_t dir        : 1; // (0: MOV to DR , 1: MOV from DR)
    uint32_t rsvd2      : 3; // reserved to 0
    uint32_t gpr        : 4; // (0:RAX, 1:RCX, 2:RDX, 3:RBX, 4:RSP, 5:RBP, 6:RSI, 6:RDI, 8-15:64bit regs)
} __attribute__((packed));

/* End Exit Qualifications */

struct vmx_exit_idt_vec_info {
    union {
        uint32_t value;
	struct {
	    uint32_t vector     : 8;
	    uint32_t type       : 3;
	    uint32_t error_code : 1;
	    uint32_t undef      : 1;
	    uint32_t rsvd       : 18;
	    uint32_t valid      : 1;
	} __attribute__ ((packed));
    } __attribute__ ((packed));
} __attribute__ ((packed));



struct vmx_basic_exit_info {
    union {
	uint32_t value;
	struct {
	    uint16_t reason;
	    uint16_t rsvd1         :12;
	    uint8_t  mtf_pending   : 1;
	    uint8_t  vmx_root_op   : 1;
	    uint8_t  rsvd2         : 1;
	    uint8_t  entry_error   : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct vmx_exit_info {
    uint32_t instr_len;
    uint32_t instr_info;

    uint32_t exit_reason;
    addr_t exit_qual;

    uint32_t int_info;
    uint32_t int_err;

    addr_t guest_linear_addr;

    /* EPT INFO */
    addr_t ept_fault_addr;

};




int v3_handle_atomic_vmx_exit(struct guest_info * info, struct vmx_exit_info * exit_info);
int v3_handle_vmx_exit(struct guest_info * info, struct vmx_exit_info * exit_info);
const char * v3_vmx_exit_code_to_str(vmx_exit_t exit);

#endif

#endif
