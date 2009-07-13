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


/* 16 bit guest state */
#define VMCS_GUEST_ES_SELECTOR       0x00000800
#define VMCS_GUEST_CS_SELECTOR       0x00000802
#define VMCS_GUEST_SS_SELECTOR       0x00000804
#define VMCS_GUEST_DS_SELECTOR       0x00000806
#define VMCS_GUEST_FS_SELECTOR       0x00000808
#define VMCS_GUEST_GS_SELECTOR       0x0000080A
#define VMCS_GUEST_LDTR_SELECTOR     0x0000080C
#define VMCS_GUEST_TR_SELECTOR       0x0000080E

/* 16 bit host state */
#define VMCS_HOST_ES_SELECTOR        0x00000C00
#define VMCS_HOST_CS_SELECTOR        0x00000C02
#define VMCS_HOST_SS_SELECTOR        0x00000C04
#define VMCS_HOST_DS_SELECTOR        0x00000C06
#define VMCS_HOST_FS_SELECTOR        0x00000C08
#define VMCS_HOST_GS_SELECTOR        0x00000C0A
#define VMCS_HOST_TR_SELECTOR        0x00000C0C

/* 64 bit control fields */
#define IO_BITMAP_A_ADDR             0x00002000
#define IO_BITMAP_A_ADDR_HIGH        0x00002001
#define IO_BITMAP_B_ADDR             0x00002002
#define IO_BITMAP_B_ADDR_HIGH        0x00002003
// Only with "Use MSR Bitmaps" enabled
#define MSR_BITMAPS                  0x00002004
#define MSR_BITMAPS_HIGH             0x00002005
//
#define VM_EXIT_MSR_STORE_ADDR       0x00002006
#define VM_EXIT_MSR_STORE_ADDR_HIGH  0x00002007
#define VM_EXIT_MSR_LOAD_ADDR        0x00002008
#define VM_EXIT_MSR_LOAD_ADDR_HIGH   0x00002009
#define VM_ENTRY_MSR_LOAD_ADDR       0x0000200A
#define VM_ENTRY_MSR_LOAD_ADDR_HIGH  0x0000200B
#define VMCS_EXEC_PTR                0x0000200C
#define VMCS_EXEC_PTR_HIGH           0x0000200D
#define TSC_OFFSET                   0x00002010
#define TSC_OFFSET_HIGH              0x00002011
// Only with "Use TPR Shadow" enabled
#define VIRT_APIC_PAGE_ADDR          0x00002012  
#define VIRT_APIC_PAGE_ADDR_HIGH     0x00002013
//


/* 64 bit guest state fields */
#define VMCS_LINK_PTR                0x00002800
#define VMCS_LINK_PTR_HIGH           0x00002801
#define GUEST_IA32_DEBUGCTL          0x00002802
#define GUEST_IA32_DEBUGCTL_HIGH     0x00002803
#define GUEST_IA32_PERF_GLOBAL_CTRL  0x00002808
#define GUEST_IA32_PERF_GLOBAL_CTRL_HIGH 0x00002809

/* 32 bit control fields */
#define PIN_VM_EXEC_CTRLS            0x00004000
#define PROC_VM_EXEC_CTRLS           0x00004002
#define EXCEPTION_BITMAP             0x00004004
#define PAGE_FAULT_ERROR_MASK        0x00004006
#define PAGE_FAULT_ERROR_MATCH       0x00004008
#define CR3_TARGET_COUNT             0x0000400A
#define VM_EXIT_CTRLS                0x0000400C
#define VM_EXIT_MSR_STORE_COUNT      0x0000400E
#define VM_EXIT_MSR_LOAD_COUNT       0x00004010
#define VM_ENTRY_CTRLS               0x00004012
#define VM_ENTRY_MSR_LOAD_COUNT      0x00004014
#define VM_ENTRY_INT_INFO_FIELD      0x00004016
#define VM_ENTRY_EXCEPTION_ERROR     0x00004018
#define VM_ENTRY_INSTR_LENGTH        0x0000401A
// Only with "Use TPR Shadow" Enabled
#define TPR_THRESHOLD                0x0000401C
//




/* 32 bit Read Only data fields */
#define VM_INSTR_ERROR               0x00004400
#define EXIT_REASON                  0x00004402
#define VM_EXIT_INT_INFO             0x00004404
#define VM_EXIT_INT_ERROR            0x00004406
#define IDT_VECTOR_INFO              0x00004408
#define IDT_VECTOR_ERROR             0x0000440A
#define VM_EXIT_INSTR_LENGTH         0x0000440C
#define VMX_INSTR_INFO               0x0000440E

/* 32 bit Guest state fields */
#define GUEST_ES_LIMIT               0x00004800
#define GUEST_CS_LIMIT               0x00004802
#define GUEST_SS_LIMIT               0x00004804
#define GUEST_DS_LIMIT               0x00004806
#define GUEST_FS_LIMIT               0x00004808
#define GUEST_GS_LIMIT               0x0000480A
#define GUEST_LDTR_LIMIT             0x0000480C
#define GUEST_TR_LIMIT               0x0000480E
#define GUEST_GDTR_LIMIT             0x00004810
#define GUEST_IDTR_LIMIT             0x00004812
#define GUEST_ES_ACCESS              0x00004814
#define GUEST_CS_ACCESS              0x00004816
#define GUEST_SS_ACCESS              0x00004818
#define GUEST_DS_ACCESS              0x0000481A
#define GUEST_FS_ACCESS              0x0000481C
#define GUEST_GS_ACCESS              0x0000481E
#define GUEST_LDTR_ACCESS            0x00004820
#define GUEST_TR_ACCESS              0x00004822
#define GUEST_INT_STATE              0x00004824
#define GUEST_ACTIVITY_STATE         0x00004826
#define GUEST_SMBASE                 0x00004828
#define GUEST_IA32_SYSENTER_CS       0x0000482A


/* 32 bit host state field */
#define HOST_IA32_SYSENTER_CS        0x00004C00

/* Natural Width Control Fields */
#define CR0_GUEST_HOST_MASK          0x00006000
#define CR4_GUEST_HOST_MASK          0x00006002
#define CR0_READ_SHADOW              0x00006004
#define CR4_READ_SHADOW              0x00006006
#define CR3_TARGET_VALUE_0           0x00006008
#define CR3_TARGET_VALUE_1           0x0000600A
#define CR3_TARGET_VALUE_2           0x0000600C
#define CR3_TARGET_VALUE_3           0x0000600E


/* Natural Width Read Only Fields */
#define EXIT_QUALIFICATION           0x00006400
#define IO_RCX                       0x00006402
#define IO_RSI                       0x00006404
#define IO_RDI                       0x00006406
#define IO_RIP                       0x00006408
#define GUEST_LINEAR_ADDR            0x0000640A

/* Natural Width Guest State Fields */
#define GUEST_CR0                    0x00006800
#define GUEST_CR3                    0x00006802
#define GUEST_CR4                    0x00006804
#define GUEST_ES_BASE                0x00006806
#define GUEST_CS_BASE                0x00006808
#define GUEST_SS_BASE                0x0000680A
#define GUEST_DS_BASE                0x0000680C
#define GUEST_FS_BASE                0x0000680E
#define GUEST_GS_BASE                0x00006810
#define GUEST_LDTR_BASE              0x00006812
#define GUEST_TR_BASE                0x00006814
#define GUEST_GDTR_BASE              0x00006816
#define GUEST_IDTR_BASE              0x00006818
#define GUEST_DR7                    0x0000681A
#define GUEST_RSP                    0x0000681C
#define GUEST_RIP                    0x0000681E
#define GUEST_RFLAGS                 0x00006820
#define GUEST_PENDING_DEBUG_EXCS     0x00006822
#define GUEST_IA32_SYSENTER_ESP      0x00006824
#define GUEST_IA32_SYSENTER_EIP      0x00006826


/* Natural Width Host State Fields */
#define HOST_CR0                     0x00006C00
#define HOST_CR3                     0x00006C02
#define HOST_CR4                     0x00006C04
#define HOST_FS_BASE                 0x00006C06
#define HOST_GS_BASE                 0x00006C08
#define HOST_TR_BASE                 0x00006C0A
#define HOST_GDTR_BASE               0x00006C0C
#define HOST_IDTR_BASE               0x00006C0E
#define HOST_IA32_SYSENTER_ESP       0x00006C10
#define HOST_IA32_SYSENTER_EIP       0x00006C12
#define HOST_RSP                     0x00006C14
#define HOST_RIP                     0x00006C16

/* Pin Based VM Execution Controls */
/* INTEL MANUAL: 20-10 vol 3B */
#define EXTERNAL_INTERRUPT_EXITING   0x00000001
#define NMI_EXITING                  0x00000008
#define VIRTUAL_NMIS                 0x00000020


/* Processor Based VM Execution Controls */
/* INTEL MANUAL: 20-11 vol. 3B */ 
#define INTERRUPT_WINDOWS_EXIT       0x00000004
#define USE_TSC_OFFSETTING           0x00000008
#define HLT_EXITING                  0x00000080
#define INVLPG_EXITING               0x00000200
#define MWAIT_EXITING                0x00000400
#define RDPMC_EXITING                0x00000800
#define RDTSC_EXITING                0x00001000
#define CR8_LOAD_EXITING             0x00080000
#define CR8_STORE_EXITING            0x00100000
#define USE_TPR_SHADOW               0x00200000
#define NMI_WINDOW_EXITING           0x00400000
#define MOVDR_EXITING                0x00800000
#define UNCONDITION_IO_EXITING       0x01000000
#define USE_IO_BITMAPS               0x02000000
#define USE_MSR_BITMAPS              0x10000000
#define MONITOR_EXITING              0x20000000
#define PAUSE_EXITING                0x40000000

/* VM-Exit Controls */
/* INTEL MANUAL: 20-16 vol. 3B */
#define HOST_ADDR_SPACE_SIZE         0x00000200
#define ACK_IRQ_ON_EXIT              0x00008000





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
}__attribute__((packed);;


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
