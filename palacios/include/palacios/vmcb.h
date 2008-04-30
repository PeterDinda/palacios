#ifndef __VMCB_H
#define __VMCB_H

#include <palacios/vmm_types.h>


#define VMCB_CTRL_AREA_OFFSET                   0x0
#define VMCB_STATE_SAVE_AREA_OFFSET             0x400


#define GET_VMCB_CTRL_AREA(page)         (page + VMCB_CTRL_AREA_OFFSET)
#define GET_VMCB_SAVE_STATE_AREA(page)   (page + VMCB_STATE_SAVE_AREA_OFFSET)


#if __TINYC__
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif


typedef void vmcb_t;


struct Ctrl_Registers {
    uint_t cr0        : 1        PACKED;
    uint_t cr1        : 1        PACKED;
    uint_t cr2        : 1        PACKED;
    uint_t cr3        : 1        PACKED;
    uint_t cr4        : 1        PACKED;
    uint_t cr5        : 1        PACKED;
    uint_t cr6        : 1        PACKED;
    uint_t cr7        : 1        PACKED;
    uint_t cr8        : 1        PACKED;
    uint_t cr9        : 1        PACKED;
    uint_t cr10       : 1        PACKED;
    uint_t cr11       : 1        PACKED;
    uint_t cr12       : 1        PACKED;
    uint_t cr13       : 1        PACKED;
    uint_t cr14       : 1        PACKED;
    uint_t cr15       : 1        PACKED;
};


struct Debug_Registers {
    uint_t dr0        : 1        PACKED;
    uint_t dr1        : 1        PACKED;
    uint_t dr2        : 1        PACKED;
    uint_t dr3        : 1        PACKED;
    uint_t dr4        : 1        PACKED;
    uint_t dr5        : 1        PACKED;
    uint_t dr6        : 1        PACKED;
    uint_t dr7        : 1        PACKED;
    uint_t dr8        : 1        PACKED;
    uint_t dr9        : 1        PACKED;
    uint_t dr10       : 1        PACKED;
    uint_t dr11       : 1        PACKED;
    uint_t dr12       : 1        PACKED;
    uint_t dr13       : 1        PACKED;
    uint_t dr14       : 1        PACKED;
    uint_t dr15       : 1        PACKED;
};


struct Exception_Vectors {
    uint_t de          : 1        PACKED; // (0) divide by zero
    uint_t db          : 1        PACKED; // (1) Debug
    uint_t nmi         : 1        PACKED; // (2) Non-maskable interrupt
    uint_t bp          : 1        PACKED; // (3) Breakpoint
    uint_t of          : 1        PACKED; // (4) Overflow
    uint_t br          : 1        PACKED; // (5) Bound-Range
    uint_t ud          : 1        PACKED; // (6) Invalid-Opcode
    uint_t nm          : 1        PACKED; // (7) Device-not-available
    uint_t df          : 1        PACKED; // (8) Double Fault
    uint_t ex9         : 1        PACKED; 
    uint_t ts          : 1        PACKED; // (10) Invalid TSS
    uint_t np          : 1        PACKED; // (11) Segment-not-present
    uint_t ss          : 1        PACKED; // (12) Stack
    uint_t gp          : 1        PACKED; // (13) General Protection Fault
    uint_t pf          : 1        PACKED; // (14) Page fault
    uint_t ex15        : 1        PACKED;
    uint_t mf          : 1        PACKED; // (15) Floating point exception
    uint_t ac          : 1        PACKED; // (16) Alignment-check
    uint_t mc          : 1        PACKED; // (17) Machine Check
    uint_t xf          : 1        PACKED; // (18) SIMD floating-point
    uint_t ex20        : 1        PACKED;
    uint_t ex21        : 1        PACKED;
    uint_t ex22        : 1        PACKED;
    uint_t ex23        : 1        PACKED;
    uint_t ex24        : 1        PACKED;
    uint_t ex25        : 1        PACKED;
    uint_t ex26        : 1        PACKED;
    uint_t ex27        : 1        PACKED;
    uint_t ex28        : 1        PACKED;
    uint_t ex29        : 1        PACKED;
    uint_t sx          : 1        PACKED; // (30) Security Exception
    uint_t ex31        : 1        PACKED;
};


struct Instr_Intercepts {
    uint_t INTR        : 1        PACKED;
    uint_t NMI         : 1        PACKED;
    uint_t SMI         : 1        PACKED;
    uint_t INIT        : 1        PACKED;
    uint_t VINTR       : 1        PACKED;
    uint_t CR0         : 1        PACKED;
    uint_t RD_IDTR     : 1        PACKED;
    uint_t RD_GDTR     : 1        PACKED;
    uint_t RD_LDTR     : 1        PACKED;
    uint_t RD_TR       : 1        PACKED;
    uint_t WR_IDTR     : 1        PACKED;
    uint_t WR_GDTR     : 1        PACKED;
    uint_t WR_LDTR     : 1        PACKED;
    uint_t WR_TR       : 1        PACKED;
    uint_t RDTSC       : 1        PACKED;
    uint_t RDPMC       : 1        PACKED;
    uint_t PUSHF       : 1        PACKED;
    uint_t POPF        : 1        PACKED;
    uint_t CPUID       : 1        PACKED;
    uint_t RSM         : 1        PACKED;
    uint_t IRET        : 1        PACKED;
    uint_t INTn        : 1        PACKED;
    uint_t INVD        : 1        PACKED;
    uint_t PAUSE       : 1        PACKED;
    uint_t HLT         : 1        PACKED;
    uint_t INVLPG      : 1        PACKED;
    uint_t INVLPGA     : 1        PACKED;
    uint_t IOIO_PROT   : 1        PACKED;
    uint_t MSR_PROT    : 1        PACKED;
    uint_t task_switch : 1        PACKED;
    uint_t FERR_FREEZE : 1        PACKED;
    uint_t shutdown_evts: 1       PACKED;
};

struct SVM_Instr_Intercepts { 
  uint_t VMRUN      : 1         PACKED;
  uint_t VMMCALL    : 1         PACKED;
  uint_t VMLOAD     : 1         PACKED;
  uint_t VMSAVE     : 1         PACKED;
  uint_t STGI       : 1         PACKED;
  uint_t CLGI       : 1         PACKED;
  uint_t SKINIT     : 1         PACKED;
  uint_t RDTSCP     : 1         PACKED;
  uint_t ICEBP      : 1         PACKED;
  uint_t WBINVD     : 1         PACKED;
  uint_t MONITOR    : 1         PACKED;
  uint_t MWAIT_always : 1       PACKED;
  uint_t MWAIT_if_armed : 1     PACKED;
  uint_t reserved  : 19         PACKED;  // Should be 0
};


struct Guest_Control {
  uchar_t V_TPR                 PACKED;
  uint_t V_IRQ      : 1         PACKED;
  uint_t rsvd1      : 7         PACKED;  // Should be 0
  uint_t V_INTR_PRIO : 4        PACKED;
  uint_t V_IGN_TPR  : 1         PACKED;
  uint_t rsvd2      : 3         PACKED;  // Should be 0
  uint_t V_INTR_MASKING : 1     PACKED;
  uint_t rsvd3      : 7         PACKED;  // Should be 0
  uchar_t V_INTR_VECTOR         PACKED;
  uint_t rsvd4      : 24        PACKED;  // Should be 0
};

#define SVM_INJECTION_EXTERNAL_INTR 0
#define SVM_INJECTION_VIRTUAL_INTR  0
#define SVM_INJECTION_NMI           2
#define SVM_INJECTION_EXCEPTION     3
#define SVM_INJECTION_SOFT_INTR     4

struct Interrupt_Info {
  uint_t vector       : 8         PACKED;
  uint_t type         : 3         PACKED;
  uint_t ev           : 1         PACKED;
  uint_t rsvd         : 19        PACKED;
  uint_t valid        : 1         PACKED;
  uint_t error_code   : 32        PACKED;
};


typedef struct VMCB_Control_Area {
  // offset 0x0
  struct Ctrl_Registers cr_reads         PACKED;
  struct Ctrl_Registers cr_writes        PACKED;
  struct Debug_Registers dr_reads        PACKED;
  struct Debug_Registers dr_writes       PACKED;
  struct Exception_Vectors exceptions    PACKED;
  struct Instr_Intercepts instrs         PACKED;
  struct SVM_Instr_Intercepts svm_instrs PACKED;

  uchar_t rsvd1[44]                     PACKED;  // Should be 0

  // offset 0x040
  ullong_t IOPM_BASE_PA                 PACKED;
  ullong_t MSRPM_BASE_PA                PACKED;
  ullong_t TSC_OFFSET                   PACKED;

  uint_t guest_ASID                     PACKED;
  uchar_t TLB_CONTROL                   PACKED;

  uchar_t rsvd2[3]                      PACKED;  // Should be 0

  struct Guest_Control guest_ctrl        PACKED;
  
  uint_t interrupt_shadow  : 1          PACKED;
  uint_t rsvd3             : 31         PACKED;  // Should be 0
  uint_t rsvd4                          PACKED;  // Should be 0

  ullong_t exit_code                    PACKED;
  ullong_t exit_info1                   PACKED;
  ullong_t exit_info2                   PACKED;

  /* This could be a typo in the manual....
   * It doesn't actually say that there is a reserved bit
   * But it does say that the EXITINTINFO field is in bits 63-1
   * ALL other occurances mention a 1 bit reserved field
   */
  //  uint_t rsvd5             : 1          PACKED;
  //ullong_t exit_int_info   : 63         PACKED;
  /* ** */

  // AMD Manual 2, pg 391, sect: 15.19
  struct Interrupt_Info exit_int_info  PACKED;

  //  uint_t NP_ENABLE         : 1          PACKED;
  //ullong_t rsvd6           : 63         PACKED;  // Should be 0 
  ullong_t NP_ENABLE                    PACKED;

  uchar_t rsvd7[16]                     PACKED;  // Should be 0

  // Offset 0xA8
  struct Interrupt_Info EVENTINJ                     PACKED;


  /* This could be a typo in the manual....
   * It doesn't actually say that there is a reserved bit
   * But it does say that the EXITINTINFO field is in bits 63-1
   * ALL other occurances mention a 1 bit reserved field
   */
  //  uint_t rsvd8              : 1         PACKED;
  //ullong_t N_CR3            : 63        PACKED;
  ullong_t N_CR3                        PACKED;
  /* ** */


  uint_t LBR_VIRTUALIZATION_ENABLE : 1  PACKED;
  ullong_t rsvd9            : 63        PACKED;   // Should be 0

} vmcb_ctrl_t;






struct vmcb_selector {
  ushort_t selector                   PACKED;

  /* These attributes are basically a direct map of the attribute fields of a segment desc.
   * The segment limit in the middle is removed and the fields are fused together
   * There IS empty space at the end... See AMD Arch vol3, sect. 4.7.1,  pg 78
   */
  union {
    ushort_t raw                      PACKED;
    struct {
      uint_t type              : 4    PACKED; // segment type, [see Intel vol. 3b, sect. 3.4.5.1 (because I have the books)]
      uint_t S                 : 1    PACKED; // System=0, code/data=1
      uint_t dpl               : 2    PACKED; // priviledge level, corresonds to protection ring
      uint_t P                 : 1    PACKED; // present flag
      uint_t avl               : 1    PACKED; // available for use by system software
      uint_t L                 : 1    PACKED; // long mode (64 bit?)
      uint_t db                : 1    PACKED; // default op size (0=16 bit seg, 1=32 bit seg)
      uint_t G                 : 1    PACKED; // Granularity, (0=bytes, 1=4k)
      uint_t rsvd              : 4    PACKED;
    } fields;
  } attrib;
  uint_t  limit                       PACKED;
  ullong_t base                       PACKED;
};


typedef struct VMCB_State_Save_Area {
  struct vmcb_selector es            PACKED; // only lower 32 bits of base are implemented
  struct vmcb_selector cs            PACKED; // only lower 32 bits of base are implemented
  struct vmcb_selector ss            PACKED; // only lower 32 bits of base are implemented
  struct vmcb_selector ds            PACKED; // only lower 32 bits of base are implemented
  struct vmcb_selector fs            PACKED; 
  struct vmcb_selector gs            PACKED; 

  struct vmcb_selector gdtr          PACKED; // selector+attrib are reserved, only lower 16 bits of limit are implemented
  struct vmcb_selector ldtr          PACKED; 
  struct vmcb_selector idtr          PACKED; // selector+attrib are reserved, only lower 16 bits of limit are implemented
  struct vmcb_selector tr            PACKED; 

  uchar_t rsvd1[43]                  PACKED;

  //offset 0x0cb
  uchar_t cpl                        PACKED; // if the guest is real-mode then the CPL is forced to 0
                                             // if the guest is virtual-mode then the CPL is forced to 3

  uint_t rsvd2                       PACKED;

  // offset 0x0d0
  ullong_t efer                      PACKED;

  uchar_t rsvd3[112]                 PACKED;
  
  //offset 0x148
  ullong_t cr4                       PACKED;
  ullong_t cr3                       PACKED;
  ullong_t cr0                       PACKED;
  ullong_t dr7                       PACKED;
  ullong_t dr6                       PACKED;
  ullong_t rflags                    PACKED;
  ullong_t rip                       PACKED;

  uchar_t rsvd4[88]                  PACKED;
  
  //offset 0x1d8
  ullong_t rsp                       PACKED;

  uchar_t rsvd5[24]                  PACKED;

  //offset 0x1f8
  ullong_t rax                       PACKED;
  ullong_t star                      PACKED;
  ullong_t lstar                     PACKED;
  ullong_t cstar                     PACKED;
  ullong_t sfmask                    PACKED;
  ullong_t KernelGsBase              PACKED;
  ullong_t sysenter_cs               PACKED;
  ullong_t sysenter_esp              PACKED;
  ullong_t sysenter_eip              PACKED;
  ullong_t cr2                       PACKED;


  uchar_t rsvd6[32]                  PACKED;

  //offset 0x268
  ullong_t g_pat                     PACKED; // Guest PAT                     
                                             //   -- only used if nested paging is enabled
  ullong_t dbgctl                    PACKED; // Guest DBGCTL MSR               
                                             //   -- only used if the LBR registers are virtualized
  ullong_t br_from                   PACKED; // Guest LastBranchFromIP MSR
                                             //   -- only used if the LBR registers are virtualized
  ullong_t br_to                     PACKED; // Guest LastBranchToIP MSR   
                                             //   -- only used if the LBR registers are virtualized
  ullong_t lastexcpfrom              PACKED; // Guest LastExceptionFromIP MSR
                                             //   -- only used if the LBR registers are virtualized
  ullong_t lastexcpto                PACKED; // Guest LastExceptionToIP MSR 
                                             //   -- only used if the LBR registers are virtualized

} vmcb_saved_state_t;

void PrintDebugVMCB(vmcb_t * vmcb);


#endif
