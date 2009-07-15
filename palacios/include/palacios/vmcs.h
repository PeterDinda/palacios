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
    IO_BITMAP_A_ADDR             = 0x00002000,
    IO_BITMAP_A_ADDR_HIGH        = 0x00002001,
    IO_BITMAP_B_ADDR             = 0x00002002,
    IO_BITMAP_B_ADDR_HIGH        = 0x00002003,
    MSR_BITMAPS                  = 0x00002004,
    MSR_BITMAPS_HIGH             = 0x00002005,
    VM_EXIT_MSR_STORE_ADDR       = 0x00002006,
    VM_EXIT_MSR_STORE_ADDR_HIGH  = 0x00002007,
    VM_EXIT_MSR_LOAD_ADDR        = 0x00002008,
    VM_EXIT_MSR_LOAD_ADDR_HIGH   = 0x00002009,
    VM_ENTRY_MSR_LOAD_ADDR       = 0x0000200A,
    VM_ENTRY_MSR_LOAD_ADDR_HIGH  = 0x0000200B,
    VMCS_EXEC_PTR                = 0x0000200C,
    VMCS_EXEC_PTR_HIGH           = 0x0000200D,
    TSC_OFFSET                   = 0x00002010,
    TSC_OFFSET_HIGH              = 0x00002011,
    VIRT_APIC_PAGE_ADDR          = 0x00002012,
    VIRT_APIC_PAGE_ADDR_HIGH     = 0x00002013,
    /* 64 bit guest state fields */
    VMCS_LINK_PTR                = 0x00002800,
    VMCS_LINK_PTR_HIGH           = 0x00002801,
    GUEST_IA32_DEBUGCTL          = 0x00002802,
    GUEST_IA32_DEBUGCTL_HIGH     = 0x00002803,
    GUEST_IA32_PERF_GLOBAL_CTRL  = 0x00002808,
    GUEST_IA32_PERF_GLOBAL_CTRL_HIGH = 0x00002809,
    /* 32 bit control fields */
    PIN_VM_EXEC_CTRLS            = 0x00004000,
    PROC_VM_EXEC_CTRLS           = 0x00004002,
    EXCEPTION_BITMAP             = 0x00004004,
    PAGE_FAULT_ERROR_MASK        = 0x00004006,
    PAGE_FAULT_ERROR_MATCH       = 0x00004008,
    CR3_TARGET_COUNT             = 0x0000400A,
    VM_EXIT_CTRLS                = 0x0000400C,
    VM_EXIT_MSR_STORE_COUNT      = 0x0000400E,
    VM_EXIT_MSR_LOAD_COUNT       = 0x00004010,
    VM_ENTRY_CTRLS               = 0x00004012,
    VM_ENTRY_MSR_LOAD_COUNT      = 0x00004014,
    VM_ENTRY_INT_INFO_FIELD      = 0x00004016,
    VM_ENTRY_EXCEPTION_ERROR     = 0x00004018,
    VM_ENTRY_INSTR_LENGTH        = 0x0000401A,
    TPR_THRESHOLD                = 0x0000401C,
    /* 32 bit Read Only data fields */
    VM_INSTR_ERROR               = 0x00004400,
    EXIT_REASON                  = 0x00004402,
    VM_EXIT_INT_INFO             = 0x00004404,
    VM_EXIT_INT_ERROR            = 0x00004406,
    IDT_VECTOR_INFO              = 0x00004408,
    IDT_VECTOR_ERROR             = 0x0000440A,
    VM_EXIT_INSTR_LENGTH         = 0x0000440C,
    VMX_INSTR_INFO               = 0x0000440E,
    /* 32 bit Guest state fields */
    GUEST_ES_LIMIT               = 0x00004800,
    GUEST_CS_LIMIT               = 0x00004802,
    GUEST_SS_LIMIT               = 0x00004804,
    GUEST_DS_LIMIT               = 0x00004806,
    GUEST_FS_LIMIT               = 0x00004808,
    GUEST_GS_LIMIT               = 0x0000480A,
    GUEST_LDTR_LIMIT             = 0x0000480C,
    GUEST_TR_LIMIT               = 0x0000480E,
    GUEST_GDTR_LIMIT             = 0x00004810,
    GUEST_IDTR_LIMIT             = 0x00004812,
    GUEST_ES_ACCESS              = 0x00004814,
    GUEST_CS_ACCESS              = 0x00004816,
    GUEST_SS_ACCESS              = 0x00004818,
    GUEST_DS_ACCESS              = 0x0000481A,
    GUEST_FS_ACCESS              = 0x0000481C,
    GUEST_GS_ACCESS              = 0x0000481E,
    GUEST_LDTR_ACCESS            = 0x00004820,
    GUEST_TR_ACCESS              = 0x00004822,
    GUEST_INT_STATE              = 0x00004824,
    GUEST_ACTIVITY_STATE         = 0x00004826,
    GUEST_SMBASE                 = 0x00004828,
    GUEST_IA32_SYSENTER_CS       = 0x0000482A,
    /* 32 bit host state field */
    HOST_IA32_SYSENTER_CS        = 0x00004C00,
    /* Natural Width Control Fields */
    CR0_GUEST_HOST_MASK          = 0x00006000,
    CR4_GUEST_HOST_MASK          = 0x00006002,
    CR0_READ_SHADOW              = 0x00006004,
    CR4_READ_SHADOW              = 0x00006006,
    CR3_TARGET_VALUE_0           = 0x00006008,
    CR3_TARGET_VALUE_1           = 0x0000600A,
    CR3_TARGET_VALUE_2           = 0x0000600C,
    CR3_TARGET_VALUE_3           = 0x0000600E,
    /* Natural Width Read Only Fields */
    EXIT_QUALIFICATION           = 0x00006400,
    IO_RCX                       = 0x00006402,
    IO_RSI                       = 0x00006404,
    IO_RDI                       = 0x00006406,
    IO_RIP                       = 0x00006408,
    GUEST_LINEAR_ADDR            = 0x0000640A,
    /* Natural Width Guest State Fields */
    GUEST_CR0                    = 0x00006800,
    GUEST_CR3                    = 0x00006802,
    GUEST_CR4                    = 0x00006804,
    GUEST_ES_BASE                = 0x00006806,
    GUEST_CS_BASE                = 0x00006808,
    GUEST_SS_BASE                = 0x0000680A,
    GUEST_DS_BASE                = 0x0000680C,
    GUEST_FS_BASE                = 0x0000680E,
    GUEST_GS_BASE                = 0x00006810,
    GUEST_LDTR_BASE              = 0x00006812,
    GUEST_TR_BASE                = 0x00006814,
    GUEST_GDTR_BASE              = 0x00006816,
    GUEST_IDTR_BASE              = 0x00006818,
    GUEST_DR7                    = 0x0000681A,
    GUEST_RSP                    = 0x0000681C,
    GUEST_RIP                    = 0x0000681E,
    GUEST_RFLAGS                 = 0x00006820,
    GUEST_PENDING_DEBUG_EXCS     = 0x00006822,
    GUEST_IA32_SYSENTER_ESP      = 0x00006824,
    GUEST_IA32_SYSENTER_EIP      = 0x00006826,
    /* Natural Width Host State Fields */
    HOST_CR0                     = 0x00006C00,
    HOST_CR3                     = 0x00006C02,
    HOST_CR4                     = 0x00006C04,
    HOST_FS_BASE                 = 0x00006C06,
    HOST_GS_BASE                 = 0x00006C08,
    HOST_TR_BASE                 = 0x00006C0A,
    HOST_GDTR_BASE               = 0x00006C0C,
    HOST_IDTR_BASE               = 0x00006C0E,
    HOST_IA32_SYSENTER_ESP       = 0x00006C10,
    HOST_IA32_SYSENTER_EIP       = 0x00006C12,
    HOST_RSP                     = 0x00006C14,
    HOST_RIP                     = 0x00006C16,
    /* Pin Based VM Execution Controls */
    /* INTEL MANUAL: 20-10 vol 3B */
    EXTERNAL_INTERRUPT_EXITING   = 0x00000001,
    NMI_EXITING                  = 0x00000008,
    VIRTUAL_NMIS                 = 0x00000020,
    /* Processor Based VM Execution Controls */
    /* INTEL MANUAL: 20-11 vol. 3B */
    INTERRUPT_WINDOWS_EXIT       = 0x00000004,
    USE_TSC_OFFSETTING           = 0x00000008,
    HLT_EXITING                  = 0x00000080,
    INVLPG_EXITING               = 0x00000200,
    MWAIT_EXITING                = 0x00000400,
    RDPMC_EXITING                = 0x00000800,
    RDTSC_EXITING                = 0x00001000,
    CR8_LOAD_EXITING             = 0x00080000,
    CR8_STORE_EXITING            = 0x00100000,
    USE_TPR_SHADOW               = 0x00200000,
    NMI_WINDOW_EXITING           = 0x00400000,
    MOVDR_EXITING                = 0x00800000,
    UNCONDITION_IO_EXITING       = 0x01000000,
    USE_IO_BITMAPS               = 0x02000000,
    USE_MSR_BITMAPS              = 0x10000000,
    MONITOR_EXITING              = 0x20000000,
    PAUSE_EXITING                = 0x40000000,
    /* VM-Exit Controls */
    /* INTEL MANUAL: 20-16 vol. 3B */
    HOST_ADDR_SPACE_SIZE         = 0x00000200,
    ACK_IRQ_ON_EXIT              = 0x00008000
} vmcs_field_t;

int vmcs_field_length(vmcs_field_t field);
char* vmcs_field_name(vmcs_field_t field);



/* VMCS Exit QUALIFICATIONs */
struct VMExitIOQual {
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
	    uint32_t    DB          : 1; 
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

struct vmcs_pending_debug {
    uint32_t    B0  : 1;
    uint32_t    B1  : 1;
    uint32_t    B2  : 1;
    uint32_t    B3  : 1;
    uint32_t    rsvd1   : 8;
    uint32_t    break_enabled   : 1;
    uint32_t    rsvd2   :   1;
    uint32_t    bs      :   1;
    uint32_t    rsvd3   :   50;
} __attribute__((packed));


struct VMCSExecCtrlFields {
    uint32_t pinCtrls ; // Table 20-5, Vol 3B. (pg. 20-10)
    uint32_t procCtrls ; // Table 20-6, Vol 3B. (pg. 20-11)
    uint32_t execBitmap ; 
    uint32_t pageFaultErrorMask ; 
    uint32_t pageFaultErrorMatch ;
    uint32_t ioBitmapA ; 
    uint32_t ioBitmapB ;
    uint64_t tscOffset ;
    uint32_t cr0GuestHostMask ; // Should be 64 bits?
    uint32_t cr0ReadShadow ; // Should be 64 bits?
    uint32_t cr4GuestHostMask ; // Should be 64 bits?
    uint32_t cr4ReadShadow ; // Should be 64 bits?
    uint32_t cr3TargetValue0 ; // should be 64 bits?
    uint32_t cr3TargetValue1 ; // should be 64 bits?
    uint32_t cr3TargetValue2 ; // should be 64 bits?
    uint32_t cr3TargetValue3 ; // should be 64 bits?
    uint32_t cr3TargetCount ;
    


    /* these fields enabled if "use TPR shadow"==1 */
    /* may not need them */
    uint64_t virtApicPageAddr ;
    // uint32_t virtApicPageAddrHigh 
    uint32_t tprThreshold ;
    /**/

    uint64_t MSRBitmapsBaseAddr;

    uint64_t vmcsExecPtr ;
};

int CopyOutVMCSExecCtrlFields(struct VMCSExecCtrlFields *p);
int CopyInVMCSExecCtrlFields(struct VMCSExecCtrlFields *p);




struct VMCSExitCtrlFields {
    uint32_t exitCtrls ; // Table 20-7, Vol. 3B (pg. 20-16)
    uint32_t msrStoreCount ;
    uint64_t msrStoreAddr ;
    uint32_t msrLoadCount ;
    uint64_t msrLoadAddr ;
};

int CopyOutVMCSExitCtrlFields(struct VMCSExitCtrlFields *p);
int CopyInVMCSExitCtrlFields(struct VMCSExitCtrlFields *p);



struct VMCSEntryCtrlFields {
    uint32_t entryCtrls ; // Table 20-9, Vol. 3B (pg. 20-18) 
    uint32_t msrLoadCount ;
    uint64_t msrLoadAddr ;
    uint32_t intInfo ; // Table 20-10, Vol. 3B (pg. 20-19)
    uint32_t exceptionErrorCode ;
    uint32_t instrLength ;
};


int CopyOutVMCSEntryCtrlFields(struct VMCSEntryCtrlFields *p);
int CopyInVMCSEntryCtrlFields(struct VMCSEntryCtrlFields *p);


struct VMCSExitInfoFields {
    uint32_t reason; // Table 20-11, Vol. 3B (pg. 20-20)
    uint32_t qualification ; // Should be 64 bits?
    uint32_t intInfo ;
    uint32_t intErrorCode ;
    uint32_t idtVectorInfo ;
    uint32_t idtVectorErrorCode ;
    uint32_t instrLength ;
    uint64_t guestLinearAddr ; // Should be 64 bits?
    uint32_t instrInfo ;
    uint64_t ioRCX ; // Should be 64 bits?
    uint64_t ioRSI ; // Should be 64 bits?
    uint64_t ioRDI ; // Should be 64 bits?
    uint64_t ioRIP ; // Should be 64 bits?
    uint32_t instrErrorField ;

};


int CopyOutVMCSExitInfoFields(struct VMCSExitInfoFields *p);



typedef struct vmcs_data {
    uint32_t revision ;
    uint32_t abort    ;
} __attribute__((packed)) vmcs_data_t;


int CopyOutVMCSData(struct VMCSData *p);
int CopyInVMCSData(struct VMCSData *p);

struct VMXRegs {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
};
  
void PrintTrace_VMX_Regs(struct VMXRegs *regs);
void PrintTrace_VMCSData(struct VMCSData * vmcs);
void PrintTrace_VMCSGuestStateArea(struct VMCSGuestStateArea * guestState);
void PrintTrace_VMCSHostStateArea(struct VMCSHostStateArea * hostState);
void PrintTrace_VMCSExecCtrlFields(struct VMCSExecCtrlFields * execCtrls);
void PrintTrace_VMCSExitCtrlFields(struct VMCSExitCtrlFields * exitCtrls);
void PrintTrace_VMCSEntryCtrlFields(struct VMCSEntryCtrlFields * entryCtrls);
void PrintTrace_VMCSExitInfoFields(struct VMCSExitInfoFields * exitInfo);
void PrintTrace_VMCSSegment(char * segname, struct VMCSSegment * seg, int abbr);


//uint_t VMCSRead(uint_t tag, void * val);


#endif // ! __V3VEE__


#endif 
