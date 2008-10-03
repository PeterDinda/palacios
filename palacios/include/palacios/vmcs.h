
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

#ifndef __VMCS_H
#define __VMCS_H

#include <palacios/vmm_types.h>


/* 16 bit guest state */
#define VMCS_GUEST_ES_SELECTOR  0x00000800
#define VMCS_GUEST_CS_SELECTOR  0x00000802
#define VMCS_GUEST_SS_SELECTOR  0x00000804
#define VMCS_GUEST_DS_SELECTOR  0x00000806
#define VMCS_GUEST_FS_SELECTOR  0x00000808
#define VMCS_GUEST_GS_SELECTOR  0x0000080A
#define VMCS_GUEST_LDTR_SELECTOR  0x0000080C
#define VMCS_GUEST_TR_SELECTOR  0x0000080E

/* 16 bit host state */
#define VMCS_HOST_ES_SELECTOR 0x00000C00
#define VMCS_HOST_CS_SELECTOR 0x00000C02
#define VMCS_HOST_SS_SELECTOR 0x00000C04
#define VMCS_HOST_DS_SELECTOR 0x00000C06
#define VMCS_HOST_FS_SELECTOR 0x00000C08
#define VMCS_HOST_GS_SELECTOR 0x00000C0A
#define VMCS_HOST_TR_SELECTOR 0x00000C0C

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
#define INTERRUPT_WINDOWS_EXIT   0x00000004
#define USE_TSC_OFFSETTING       0x00000008
#define HLT_EXITING              0x00000080
#define INVLPG_EXITING           0x00000200
#define MWAIT_EXITING            0x00000400
#define RDPMC_EXITING            0x00000800
#define RDTSC_EXITING            0x00001000
#define CR8_LOAD_EXITING         0x00080000
#define CR8_STORE_EXITING        0x00100000
#define USE_TPR_SHADOW           0x00200000
#define NMI_WINDOW_EXITING       0x00400000
#define MOVDR_EXITING            0x00800000
#define UNCONDITION_IO_EXITING   0x01000000
#define USE_IO_BITMAPS           0x02000000
#define USE_MSR_BITMAPS          0x10000000
#define MONITOR_EXITING          0x20000000
#define PAUSE_EXITING            0x40000000

/* VM-Exit Controls */
/* INTEL MANUAL: 20-16 vol. 3B */
#define HOST_ADDR_SPACE_SIZE     0x00000200
#define ACK_IRQ_ON_EXIT          0x00008000

// Exit Reasons
#define VM_EXIT_REASON_INFO_EXCEPTION_OR_NMI 0
#define VM_EXIT_REASON_EXTERNAL_INTR         1
#define VM_EXIT_REASON_TRIPLE_FAULT          2
#define VM_EXIT_REASON_INIT_SIGNAL           3
#define VM_EXIT_REASON_STARTUP_IPI           4
#define VM_EXIT_REASON_IO_SMI                5
#define VM_EXIT_REASON_OTHER_SMI             6
#define VM_EXIT_REASON_INTR_WINDOW           7
#define VM_EXIT_REASON_NMI_WINDOW            8
#define VM_EXIT_REASON_TASK_SWITCH           9
#define VM_EXIT_REASON_CPUID                10
#define VM_EXIT_REASON_HLT                  12
#define VM_EXIT_REASON_INVD                 13
#define VM_EXIT_REASON_INVLPG               14
#define VM_EXIT_REASON_RDPMC                15
#define VM_EXIT_REASON_RDTSC                16
#define VM_EXIT_REASON_RSM                  17
#define VM_EXIT_REASON_VMCALL               18
#define VM_EXIT_REASON_VMCLEAR              19
#define VM_EXIT_REASON_VMLAUNCH             20
#define VM_EXIT_REASON_VMPTRLD              21
#define VM_EXIT_REASON_VMPTRST              22
#define VM_EXIT_REASON_VMREAD               23
#define VM_EXIT_REASON_VMRESUME             24
#define VM_EXIT_REASON_VMWRITE              25
#define VM_EXIT_REASON_VMXOFF               26
#define VM_EXIT_REASON_VMXON                27
#define VM_EXIT_REASON_CR_REG_ACCESSES      28
#define VM_EXIT_REASON_MOV_DR               29
#define VM_EXIT_REASON_IO_INSTR             30
#define VM_EXIT_REASON_RDMSR                31
#define VM_EXIT_REASON_WRMSR                32
#define VM_EXIT_REASON_ENTRY_FAIL_INVALID_GUEST_STATE     33
#define VM_EXIT_REASON_ENTRY_FAIL_MSR_LOAD                34
#define VM_EXIT_REASON_MWAIT                36
#define VM_EXIT_REASON_MONITOR              39
#define VM_EXIT_REASON_PAUSE                40
#define VM_EXIT_REASON_ENTRY_FAILURE_MACHINE_CHECK        41
#define VM_EXIT_REASON_TPR_BELOW_THRESHOLD                43


extern char *exception_names[];
extern char *exception_type_names[];




typedef void VMCS;


#if __TINYC__
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif



/* VMCS Exit QUALIFICATIONs */
struct VMExitIOQual {
  uint_t accessSize : 3     PACKED; // (0: 1 Byte ;; 1: 2 Bytes ;; 3: 4 Bytes)
  uint_t dir        : 1     PACKED; // (0: Out ;; 1: In)
  uint_t string     : 1     PACKED; // (0: not string ;; 1: string)
  uint_t REP        : 1     PACKED; // (0: not REP ;; 1: REP)
  uint_t opEnc      : 1     PACKED; // (0: DX ;; 1: immediate)
  uint_t rsvd       : 9     PACKED; // Set to 0
  uint_t port       : 16    PACKED; // IO Port Number
};



struct VMExitDBGQual {
  uint_t B0         : 1     PACKED; // Breakpoint 0 condition met
  uint_t B1         : 1     PACKED; // Breakpoint 1 condition met
  uint_t B2         : 1     PACKED; // Breakpoint 2 condition met
  uint_t B3         : 1     PACKED; // Breakpoint 3 condition met
  uint_t rsvd       : 9     PACKED; // reserved to 0
  uint_t BD         : 1     PACKED; // detected DBG reg access
  uint_t BS         : 1     PACKED; // cause either single instr or taken branch
};


struct VMExitTSQual {
  uint_t selector   : 16    PACKED; // selector of destination TSS 
  uint_t rsvd       : 14    PACKED; // reserved to 0
  uint_t src        : 2     PACKED; // (0: CALL ; 1: IRET ; 2: JMP ; 3: Task gate in IDT)
};

struct VMExitCRQual {
  uint_t crID       : 4     PACKED; // cr number (0 for CLTS and LMSW) (bit 3 always 0, on 32bit)
  uint_t accessType : 2     PACKED; // (0: MOV to CR ; 1: MOV from CR ; 2: CLTS ; 3: LMSW)
  uint_t lmswOpType : 1     PACKED; // (0: register ; 1: memory)
  uint_t rsvd1      : 1     PACKED; // reserved to 0
  uint_t gpr        : 4     PACKED; // (0:RAX+[CLTS/LMSW], 1:RCX, 2:RDX, 3:RBX, 4:RSP, 5:RBP, 6:RSI, 6:RDI, 8-15:64bit regs)
  uint_t rsvd2      : 4     PACKED; // reserved to 0
  uint_t lmswSrc    : 16    PACKED; // src data for lmsw
};

struct VMExitMovDRQual {
  uint_t regID      : 3     PACKED; // debug register number
  uint_t rsvd1      : 1     PACKED; // reserved to 0
  uint_t dir        : 1     PACKED; // (0: MOV to DR , 1: MOV from DR)
  uint_t rsvd2      : 3     PACKED; // reserved to 0
  uint_t gpr        : 4     PACKED; // (0:RAX, 1:RCX, 2:RDX, 3:RBX, 4:RSP, 5:RBP, 6:RSI, 6:RDI, 8-15:64bit regs)
};

/* End Exit Qualifications */

/* Exit Vector Info */
struct VMExitIntInfo {
  uint_t nr         : 8     PACKED; // IRQ number, exception vector, NMI = 2 
  uint_t type       : 3     PACKED; // (0: ext. IRQ , 2: NMI , 3: hw exception , 6: sw exception
  uint_t errorCode  : 1     PACKED; // 1: error Code present
  uint_t iret       : 1     PACKED; // something to do with NMIs and IRETs (Intel 3B, sec. 23.2.2) 
  uint_t rsvd       : 18    PACKED; // always 0
  uint_t valid      : 1     PACKED; // always 1 if valid
};




/*  End Exit Vector Info */




/* Segment Selector Access Rights (32 bits) */
/* INTEL Manual: 20-4 vol 3B */
union SegAccess {
  struct {
    uchar_t  type                PACKED;
    uint_t   descType    : 1     PACKED; 
    uint_t   dpl         : 2     PACKED;
    uint_t   present     : 1     PACKED;
    uchar_t  rsvd1               PACKED;
    uint_t   avail       : 1    PACKED ;
    uint_t   L           : 1    PACKED ; // CS only (64 bit active), reserved otherwise
    uint_t   DB          : 1    PACKED ; 
    uint_t   granularity : 1    PACKED ;
    uint_t   unusable    : 1   PACKED  ; 
    uint_t   rsvd2       : 15  PACKED ;
  } as_fields;
  uint_t as_dword;
};


struct VMCSSegment {
  ushort_t selector   ; 
  union SegAccess access;
  uint_t limit        ; 
  uint_t baseAddr     ; // should be 64 bits?
};



struct VMCSGuestStateArea {
  /* (1) Guest State Area */
  /* (1.1) Guest Register State */
  uint_t cr0   ; // should be 64 bits?
  uint_t cr3   ; // should be 64 bits?
  uint_t cr4   ; // should be 64 bits?
  uint_t dr7   ; // should be 64 bits?
  uint_t rsp   ; // should be 64 bits?
  uint_t rip   ; // should be 64 bits?
  uint_t rflags   ; // should be 64 bits?
  

  struct VMCSSegment cs  ;
  struct VMCSSegment ss  ;
  struct VMCSSegment ds  ;
  struct VMCSSegment es  ;
  struct VMCSSegment fs  ;
  struct VMCSSegment gs  ;
  struct VMCSSegment ldtr  ;
  struct VMCSSegment tr  ;

  struct VMCSSegment gdtr  ;
  struct VMCSSegment idtr ;

  // MSRs
  ullong_t dbg_ctrl     ; 
  uint_t sysenter_cs    ;
  ullong_t sysenter_esp   ; // should be 64 bits?
  ullong_t sysenter_eip   ; // should be 64 bits?

  uint_t smbase         ; 

  /* (1.2) Guest Non-register State */
  uint_t activity       ; /* (0=Active, 1=HLT, 2=Shutdown, 3=Wait-for-SIPI) 
				   (listed in MSR: IA32_VMX_MISC) */

  uint_t interrupt_state  ; // see Table 20-3 (page 20-6) INTEL MANUAL 3B 

  ullong_t pending_dbg_exceptions  ; // should be 64 bits?
                                         /* Table 20-4 page 20-8 INTEL MANUAL 3B */

  ullong_t vmcs_link   ; // should be set to 0xffffffff_ffffffff
};


int CopyOutVMCSGuestStateArea(struct VMCSGuestStateArea *p);
int CopyInVMCSGuestStateArea(struct VMCSGuestStateArea *p);



struct VMCSHostStateArea {
  /* (2) Host State Area */
  ullong_t cr0  ; // Should be 64 bits?
  ullong_t cr3   ; // should be 64 bits?
  ullong_t cr4   ; // should be 64 bits?
  ullong_t rsp   ; // should be 64 bits?
  ullong_t rip   ; // should be 64 bits?

  ushort_t csSelector ;
  ushort_t ssSelector ;
  ushort_t dsSelector ;
  ushort_t esSelector ;
  ushort_t fsSelector ;
  ushort_t gsSelector ;
  ushort_t trSelector ;

  ullong_t fsBaseAddr ; // Should be 64 bits?
  ullong_t gsBaseAddr ; // Should be 64 bits?
  ullong_t trBaseAddr ; // Should be 64 bits?
  ullong_t gdtrBaseAddr ; // Should be 64 bits?
  ullong_t idtrBaseAddr ; // Should be 64 bits?
  

  /* MSRs */
  uint_t sysenter_cs ;
  ullong_t sysenter_esp ; // Should be 64 bits?
  ullong_t sysenter_eip ; // Should be 64 bits?

};

int CopyOutVMCSHostStateArea(struct VMCSHostStateArea *p);
int CopyInVMCSHostStateArea(struct VMCSHostStateArea *p);


struct VMCSExecCtrlFields {
  uint_t pinCtrls ; // Table 20-5, Vol 3B. (pg. 20-10)
  uint_t procCtrls ; // Table 20-6, Vol 3B. (pg. 20-11)
  uint_t execBitmap ; 
  uint_t pageFaultErrorMask ; 
  uint_t pageFaultErrorMatch ;
  uint_t ioBitmapA ; 
  uint_t ioBitmapB ;
  ullong_t tscOffset ;
  uint_t cr0GuestHostMask ; // Should be 64 bits?
  uint_t cr0ReadShadow ; // Should be 64 bits?
  uint_t cr4GuestHostMask ; // Should be 64 bits?
  uint_t cr4ReadShadow ; // Should be 64 bits?
  uint_t cr3TargetValue0 ; // should be 64 bits?
  uint_t cr3TargetValue1 ; // should be 64 bits?
  uint_t cr3TargetValue2 ; // should be 64 bits?
  uint_t cr3TargetValue3 ; // should be 64 bits?
  uint_t cr3TargetCount ;



  /* these fields enabled if "use TPR shadow"==1 */
  /* may not need them */
  ullong_t virtApicPageAddr ;
  // uint_t virtApicPageAddrHigh 
  uint_t tprThreshold ;
  /**/

  ullong_t MSRBitmapsBaseAddr;


  ullong_t vmcsExecPtr ;

};

int CopyOutVMCSExecCtrlFields(struct VMCSExecCtrlFields *p);
int CopyInVMCSExecCtrlFields(struct VMCSExecCtrlFields *p);




struct VMCSExitCtrlFields {
  uint_t exitCtrls ; // Table 20-7, Vol. 3B (pg. 20-16)
  uint_t msrStoreCount ;
  ullong_t msrStoreAddr ;
  uint_t msrLoadCount ;
  ullong_t msrLoadAddr ;
};

int CopyOutVMCSExitCtrlFields(struct VMCSExitCtrlFields *p);
int CopyInVMCSExitCtrlFields(struct VMCSExitCtrlFields *p);



struct VMCSEntryCtrlFields {
  uint_t entryCtrls ; // Table 20-9, Vol. 3B (pg. 20-18) 
  uint_t msrLoadCount ;
  ullong_t msrLoadAddr ;
  uint_t intInfo ; // Table 20-10, Vol. 3B (pg. 20-19)
  uint_t exceptionErrorCode ;
  uint_t instrLength ;
};


int CopyOutVMCSEntryCtrlFields(struct VMCSEntryCtrlFields *p);
int CopyInVMCSEntryCtrlFields(struct VMCSEntryCtrlFields *p);


struct VMCSExitInfoFields {
  uint_t reason; // Table 20-11, Vol. 3B (pg. 20-20)
  uint_t qualification ; // Should be 64 bits?
  uint_t intInfo ;
  uint_t intErrorCode ;
  uint_t idtVectorInfo ;
  uint_t idtVectorErrorCode ;
  uint_t instrLength ;
  ullong_t guestLinearAddr ; // Should be 64 bits?
  uint_t instrInfo ;
  ullong_t ioRCX ; // Should be 64 bits?
  ullong_t ioRSI ; // Should be 64 bits?
  ullong_t ioRDI ; // Should be 64 bits?
  ullong_t ioRIP ; // Should be 64 bits?
  uint_t instrErrorField ;

};


int CopyOutVMCSExitInfoFields(struct VMCSExitInfoFields *p);



struct VMCSData {
  uint_t revision ;
  uint_t abort    ;
  uint_t exitCtrlFlags;
  struct VMCSGuestStateArea guestStateArea ; 
  struct VMCSHostStateArea hostStateArea ;
  struct VMCSExecCtrlFields execCtrlFields ;
  struct VMCSExitCtrlFields exitCtrlFields ;
  struct VMCSEntryCtrlFields entryCtrlFields ;
  struct VMCSExitInfoFields exitInfoFields ;
};


int CopyOutVMCSData(struct VMCSData *p);
int CopyInVMCSData(struct VMCSData *p);

struct VMXRegs {
  uint_t edi;
  uint_t esi;
  uint_t ebp;
  uint_t esp;
  uint_t ebx;
  uint_t edx;
  uint_t ecx;
  uint_t eax;
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

VMCS * CreateVMCS();
extern uint_t VMCS_WRITE();
extern uint_t VMCS_READ();

//uint_t VMCSRead(uint_t tag, void * val);


#include <palacios/vmcs_gen.h>

#endif 
