/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *         Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMCS_H__
#define __VMCS_H__

#ifdef __V3VEE__


#include <palacios/vmm_types.h>
    /* Pin Based VM Execution Controls */
    /* INTEL MANUAL: 20-10 vol 3B */
#define   EXT_INTR_EXIT                 0x00000001
#define   NMI_EXIT                      0x00000008
#define   VIRTUAL_NMIS                  0x00000020
/* Processor Based VM Execution Controls */
/* INTEL MANUAL: 20-11 vol. 3B */
#define   INTR_WIN_EXIT                 0x00000004
#define   USE_TSC_OFFSET                0x00000008
#define   HLT_EXIT                      0x00000080
#define   INVLPG_EXIT                   0x00000200
#define   MWAIT_EXIT                    0x00000400
#define   RDPMC_EXIT                    0x00000800
#define   RDTSC_EXIT                    0x00001000
#define   CR3_LOAD_EXIT                 0x00008000
#define   CR3_STORE_EXIT                0x00010000
#define   CR8_LOAD_EXIT                 0x00080000
#define   CR8_STORE_EXIT                0x00100000
#define   USE_TPR_SHADOW                0x00200000
#define   NMI_WINDOW_EXIT               0x00400000
#define   MOVDR_EXIT                    0x00800000
#define   UNCOND_IO_EXIT                0x01000000
#define   USE_IO_BITMAPS                0x02000000
#define   USE_MSR_BITMAPS               0x10000000
#define   MONITOR_EXIT                  0x20000000
#define   PAUSE_EXIT                    0x40000000
#define   ACTIVE_SEC_CTRLS              0x80000000
/* VM-Exit Controls */
/* INTEL MANUAL: 20-16 vol. 3B */
#define   HOST_ADDR_SPACE_SIZE          0x00000200
#define   ACK_IRQ_ON_EXIT               0x00008000

typedef enum {
    VMCS_GUEST_ES_SELECTOR       = 0x00000800,
    VMCS_GUEST_CS_SELECTOR       = 0x00000802,
    VMCS_GUEST_SS_SELECTOR       = 0x00000804,
    VMCS_GUEST_DS_SELECTOR       = 0x00000806,
    VMCS_GUEST_FS_SELECTOR       = 0x00000808,
    VMCS_GUEST_GS_SELECTOR       = 0x0000080A,
    VMCS_GUEST_LDTR_SELECTOR     = 0x0000080C,
    VMCS_GUEST_TR_SELECTOR       = 0x0000080E,
    /* 16 bit host state */
    VMCS_HOST_ES_SELECTOR        = 0x00000C00,
    VMCS_HOST_CS_SELECTOR        = 0x00000C02,
    VMCS_HOST_SS_SELECTOR        = 0x00000C04,
    VMCS_HOST_DS_SELECTOR        = 0x00000C06,
    VMCS_HOST_FS_SELECTOR        = 0x00000C08,
    VMCS_HOST_GS_SELECTOR        = 0x00000C0A,
    VMCS_HOST_TR_SELECTOR        = 0x00000C0C,
    /* 64 bit control fields */
    VMCS_IO_BITMAP_A_ADDR             = 0x00002000,
    VMCS_IO_BITMAP_A_ADDR_HIGH        = 0x00002001,
    VMCS_IO_BITMAP_B_ADDR             = 0x00002002,
    VMCS_IO_BITMAP_B_ADDR_HIGH        = 0x00002003,
    VMCS_MSR_BITMAP                   = 0x00002004,
    VMCS_MSR_BITMAP_HIGH              = 0x00002005,
    VMCS_EXIT_MSR_STORE_ADDR          = 0x00002006,
    VMCS_EXIT_MSR_STORE_ADDR_HIGH     = 0x00002007,
    VMCS_EXIT_MSR_LOAD_ADDR           = 0x00002008,
    VMCS_EXIT_MSR_LOAD_ADDR_HIGH      = 0x00002009,
    VMCS_ENTRY_MSR_LOAD_ADDR          = 0x0000200A,
    VMCS_ENTRY_MSR_LOAD_ADDR_HIGH     = 0x0000200B,
    VMCS_EXEC_PTR                     = 0x0000200C,
    VMCS_EXEC_PTR_HIGH                = 0x0000200D,
    VMCS_TSC_OFFSET                   = 0x00002010,
    VMCS_TSC_OFFSET_HIGH              = 0x00002011,
    VMCS_VAPIC_ADDR                   = 0x00002012,
    VMCS_VAPIC_ADDR_HIGH              = 0x00002013,
    VMCS_APIC_ACCESS_ADDR             = 0x00002014,
    VMCS_APIC_ACCESS_ADDR_HIGH        = 0x00002015,
    /* 64 bit guest state fields */
    VMCS_LINK_PTR                     = 0x00002800,
    VMCS_LINK_PTR_HIGH                = 0x00002801,
    VMCS_GUEST_DBG_CTL               = 0x00002802,
    VMCS_GUEST_DBG_CTL_HIGH          = 0x00002803,
    VMCS_GUEST_PERF_GLOBAL_CTRL       = 0x00002808,
    VMCS_GUEST_PERF_GLOBAL_CTRL_HIGH  = 0x00002809,

    VMCS_HOST_PERF_GLOBAL_CTRL        = 0x00002c04,
    VMCS_HOST_PERF_GLOBAL_CTRL_HIGH   = 0x00002c05,
    /* 32 bit control fields */
    VMCS_PIN_CTRLS                    = 0x00004000,
    VMCS_PROC_CTRLS                   = 0x00004002,
    VMCS_EXCP_BITMAP                  = 0x00004004,
    VMCS_PG_FAULT_ERR_MASK            = 0x00004006,
    VMCS_PG_FAULT_ERR_MATCH           = 0x00004008,
    VMCS_CR3_TGT_CNT                  = 0x0000400A,
    VMCS_EXIT_CTRLS                   = 0x0000400C,
    VMCS_EXIT_MSR_STORE_CNT           = 0x0000400E,
    VMCS_EXIT_MSR_LOAD_CNT            = 0x00004010,
    VMCS_ENTRY_CTRLS                  = 0x00004012,
    VMCS_ENTRY_MSR_LOAD_CNT           = 0x00004014,
    VMCS_ENTRY_INT_INFO               = 0x00004016,
    VMCS_ENTRY_EXCP_ERR               = 0x00004018,
    VMCS_ENTRY_INSTR_LEN              = 0x0000401A,
    VMCS_TPR_THRESHOLD                = 0x0000401C,
    VMCS_SEC_PROC_CTRLS               = 0x0000401e,
    /* 32 bit Read Only data fields */
    VMCS_INSTR_ERR                    = 0x00004400,
    VMCS_EXIT_REASON                  = 0x00004402,
    VMCS_EXIT_INT_INFO                = 0x00004404,
    VMCS_EXIT_INT_ERR                 = 0x00004406,
    VMCS_IDT_VECTOR_INFO              = 0x00004408,
    VMCS_IDT_VECTOR_ERR               = 0x0000440A,
    VMCS_EXIT_INSTR_LEN               = 0x0000440C,
    VMCS_VMX_INSTR_INFO               = 0x0000440E,
    /* 32 bit Guest state fields */
    VMCS_GUEST_ES_LIMIT               = 0x00004800,
    VMCS_GUEST_CS_LIMIT               = 0x00004802,
    VMCS_GUEST_SS_LIMIT               = 0x00004804,
    VMCS_GUEST_DS_LIMIT               = 0x00004806,
    VMCS_GUEST_FS_LIMIT               = 0x00004808,
    VMCS_GUEST_GS_LIMIT               = 0x0000480A,
    VMCS_GUEST_LDTR_LIMIT             = 0x0000480C,
    VMCS_GUEST_TR_LIMIT               = 0x0000480E,
    VMCS_GUEST_GDTR_LIMIT             = 0x00004810,
    VMCS_GUEST_IDTR_LIMIT             = 0x00004812,
    VMCS_GUEST_ES_ACCESS              = 0x00004814,
    VMCS_GUEST_CS_ACCESS              = 0x00004816,
    VMCS_GUEST_SS_ACCESS              = 0x00004818,
    VMCS_GUEST_DS_ACCESS              = 0x0000481A,
    VMCS_GUEST_FS_ACCESS              = 0x0000481C,
    VMCS_GUEST_GS_ACCESS              = 0x0000481E,
    VMCS_GUEST_LDTR_ACCESS            = 0x00004820,
    VMCS_GUEST_TR_ACCESS              = 0x00004822,
    VMCS_GUEST_INT_STATE              = 0x00004824,
    VMCS_GUEST_ACTIVITY_STATE         = 0x00004826,
    VMCS_GUEST_SMBASE                 = 0x00004828,
    VMCS_GUEST_SYSENTER_CS            = 0x0000482A,
    /* 32 bit host state field */
    VMCS_HOST_SYSENTER_CS             = 0x00004C00,
    /* Natural Width Control Fields */
    VMCS_CR0_MASK                     = 0x00006000,
    VMCS_CR4_MASK                     = 0x00006002,
    VMCS_CR0_READ_SHDW                = 0x00006004,
    VMCS_CR4_READ_SHDW                = 0x00006006,
    VMCS_CR3_TGT_VAL_0                = 0x00006008,
    VMCS_CR3_TGT_VAL_1                = 0x0000600A,
    VMCS_CR3_TGT_VAL_2                = 0x0000600C,
    VMCS_CR3_TGT_VAL_3                = 0x0000600E,
    /* Natural Width Read Only Fields */
    VMCS_EXIT_QUAL                    = 0x00006400,
    VMCS_IO_RCX                       = 0x00006402,
    VMCS_IO_RSI                       = 0x00006404,
    VMCS_IO_RDI                       = 0x00006406,
    VMCS_IO_RIP                       = 0x00006408,
    VMCS_GUEST_LINEAR_ADDR            = 0x0000640A,
    /* Natural Width Guest State Fields */
    VMCS_GUEST_CR0                    = 0x00006800,
    VMCS_GUEST_CR3                    = 0x00006802,
    VMCS_GUEST_CR4                    = 0x00006804,
    VMCS_GUEST_ES_BASE                = 0x00006806,
    VMCS_GUEST_CS_BASE                = 0x00006808,
    VMCS_GUEST_SS_BASE                = 0x0000680A,
    VMCS_GUEST_DS_BASE                = 0x0000680C,
    VMCS_GUEST_FS_BASE                = 0x0000680E,
    VMCS_GUEST_GS_BASE                = 0x00006810,
    VMCS_GUEST_LDTR_BASE              = 0x00006812,
    VMCS_GUEST_TR_BASE                = 0x00006814,
    VMCS_GUEST_GDTR_BASE              = 0x00006816,
    VMCS_GUEST_IDTR_BASE              = 0x00006818,
    VMCS_GUEST_DR7                    = 0x0000681A,
    VMCS_GUEST_RSP                    = 0x0000681C,
    VMCS_GUEST_RIP                    = 0x0000681E,
    VMCS_GUEST_RFLAGS                 = 0x00006820,
    VMCS_GUEST_PENDING_DBG_EXCP       = 0x00006822,
    VMCS_GUEST_SYSENTER_ESP           = 0x00006824,
    VMCS_GUEST_SYSENTER_EIP           = 0x00006826,
    /* Natural Width Host State Fields */
    VMCS_HOST_CR0                     = 0x00006C00,
    VMCS_HOST_CR3                     = 0x00006C02,
    VMCS_HOST_CR4                     = 0x00006C04,
    VMCS_HOST_FS_BASE                 = 0x00006C06,
    VMCS_HOST_GS_BASE                 = 0x00006C08,
    VMCS_HOST_TR_BASE                 = 0x00006C0A,
    VMCS_HOST_GDTR_BASE               = 0x00006C0C,
    VMCS_HOST_IDTR_BASE               = 0x00006C0E,
    VMCS_HOST_SYSENTER_ESP            = 0x00006C10,
    VMCS_HOST_SYSENTER_EIP            = 0x00006C12,
    VMCS_HOST_RSP                     = 0x00006C14,
    VMCS_HOST_RIP                     = 0x00006C16,
} vmcs_field_t;

int v3_vmcs_get_field_len(vmcs_field_t field);
const char* v3_vmcs_field_to_str(vmcs_field_t field);
void v3_print_vmcs();



/* VMCS Exit QUALIFICATIONs */
struct vmcs_io_qual {
    uint32_t accessSize : 3; // (0: 1 Byte ;; 1: 2 Bytes ;; 3: 4 Bytes)
    uint32_t dir        : 1; // (0: Out ;; 1: In)
    uint32_t string     : 1; // (0: not string ;; 1: string)
    uint32_t REP        : 1; // (0: not REP ;; 1: REP)
    uint32_t opEnc      : 1; // (0: DX ;; 1: immediate)
    uint32_t rsvd       : 9; // Set to 0
    uint32_t port       : 16; // IO Port Number
} __attribute__((packed));



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

struct VMExitCRQual {
    uint32_t crID       : 4; // cr number (0 for CLTS and LMSW) (bit 3 always 0, on 32bit)
    uint32_t accessType : 2; // (0: MOV to CR ; 1: MOV from CR ; 2: CLTS ; 3: LMSW)
    uint32_t lmswOpType : 1; // (0: register ; 1: memory)
    uint32_t rsvd1      : 1; // reserved to 0
    uint32_t gpr        : 4; // (0:RAX+[CLTS/LMSW], 1:RCX, 2:RDX, 3:RBX, 4:RSP, 5:RBP, 6:RSI, 6:RDI, 8-15:64bit regs)
    uint32_t rsvd2      : 4; // reserved to 0
    uint32_t lmswSrc    : 16; // src data for lmsw
} __attribute__((packed));

struct VMExitMovDRQual {
    uint32_t regID      : 3; // debug register number
    uint32_t rsvd1      : 1; // reserved to 0
    uint32_t dir        : 1; // (0: MOV to DR , 1: MOV from DR)
    uint32_t rsvd2      : 3; // reserved to 0
    uint32_t gpr        : 4; // (0:RAX, 1:RCX, 2:RDX, 3:RBX, 4:RSP, 5:RBP, 6:RSI, 6:RDI, 8-15:64bit regs)
} __attribute__((packed));

/* End Exit Qualifications */

/* Exit Vector Info */
struct VMExitIntInfo {
    uint32_t nr         : 8; // IRQ number, exception vector, NMI = 2 
    uint32_t type       : 3; // (0: ext. IRQ , 2: NMI , 3: hw exception , 6: sw exception
    uint32_t errorCode  : 1; // 1: error Code present
    uint32_t iret       : 1; // something to do with NMIs and IRETs (Intel 3B, sec. 23.2.2) 
    uint32_t rsvd       : 18; // always 0
    uint32_t valid      : 1; // always 1 if valid
} __attribute__((packed));




/*  End Exit Vector Info */




/* Segment Selector Access Rights (32 bits) */
/* INTEL Manual: 20-4 vol 3B */


struct vmcs_segment_access {
    union {
	uint32_t value;
	struct {
	    uint32_t    type        : 4;
	    uint32_t    desc_type   : 1; 
	    uint32_t    dpl         : 2;
	    uint32_t    present     : 1;
	    uint32_t    rsvd1       : 4;
	    uint32_t    avail       : 1;
	    uint32_t    long_mode   : 1; // CS only (64 bit active), reserved otherwise
	    uint32_t    db          : 1; 
	    uint32_t    granularity : 1;
	    uint32_t    unusable    : 1; 
	    uint32_t    rsvd2       : 15;
	} __attribute__((packed));
    } __attribute__((packed));
}__attribute__((packed));


struct vmcs_interrupt_state {
    uint32_t    sti_blocking    : 1;
    uint32_t    mov_ss_blocking : 1;
    uint32_t    smi_blocking    : 1;
    uint32_t    nmi_blocking    : 1;
    uint32_t    rsvd1           : 28;
} __attribute__((packed));



struct vmcs_data {
    uint32_t revision ;
    uint32_t abort    ;
} __attribute__((packed));


//uint_t VMCSRead(uint_t tag, void * val);


#endif // ! __V3VEE__


#endif 
