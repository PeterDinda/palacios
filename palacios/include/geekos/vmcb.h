#ifndef __VMCB_H
#define __VMCB_H




#if __TINYC__
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif

union Ctrl_Registers {
  ushort_t bitmap                PACKED;
  struct {
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
  } crs;
};


union Debug_Registers {
  ushort_t bitmap                PACKED;
  struct {
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
  } drs;
};


union Exception_Vectors {
  ulong_t bitmap                  PACKED;
  struct {
    uint_t ex0         : 1        PACKED;
    uint_t ex1         : 1        PACKED;
    uint_t ex2         : 1        PACKED;
    uint_t ex3         : 1        PACKED;
    uint_t ex4         : 1        PACKED;
    uint_t ex5         : 1        PACKED;
    uint_t ex6         : 1        PACKED;
    uint_t ex7         : 1        PACKED;
    uint_t ex8         : 1        PACKED;
    uint_t ex9         : 1        PACKED;
    uint_t ex10        : 1        PACKED;
    uint_t ex11        : 1        PACKED;
    uint_t ex12        : 1        PACKED;
    uint_t ex13        : 1        PACKED;
    uint_t ex14        : 1        PACKED;
    uint_t ex15        : 1        PACKED;
    uint_t ex16        : 1        PACKED;
    uint_t ex17        : 1        PACKED;
    uint_t ex18        : 1        PACKED;
    uint_t ex19        : 1        PACKED;
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
    uint_t ex30        : 1        PACKED;
    uint_t ex31        : 1        PACKED;
  } exceptions;
};


union Instr_Intercepts {
  ulong_t bitmap                  PACKED;
  struct {
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
    uint_t INVPLG      : 1        PACKED;
    uint_t INVPLGA     : 1        PACKED;
    uint_t IOIO_PROT   : 1        PACKED;
    uint_t MSR_PROT    : 1        PACKED;
    uint_t task_switch : 1        PACKED;
    uint_t FERR_FREEZE : 1        PACKED;
    uint_t shutdown_evts: 1       PACKED;
  } instrs;
};

union SVM_Instr_Intercepts { 
  ulong_t bitmap                  PACKED;
  struct {
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
    ulong_t reserved  : 19        PACKED;
  } instrs;
};


union Guest_Control {
  ulong_t bitmap                  PACKED;
  struct {
    uchar_t V_TPR                 PACKED;
    uint_t V_IRQ      : 1         PACKED;
    uint_t rsvd1      : 7         PACKED;
    uint_t V_INTR_PRIO : 4        PACKED;
    uint_t V_IGN_TPR  : 1         PACKED;
    uint_t rsvd2      : 3         PACKED;
    uint_t V_INTR_MASKING : 1     PACKED;
    uint_t rsvd3      : 7         PACKED;
    uchar_t V_INTR_VECTOR         PACKED;
    uint_t rsvd4      : 24        PACKED;
  } ctrls;
};



typedef struct VMCB_Control_Area {
  // offset 0x0
  union Ctrl_Registers cr_reads        PACKED;
  union Ctrl_Registers cr_writes       PACKED;
  union Debug_Registers dr_reads       PACKED;
  union Debug_Registers dr_writes      PACKED;
  union Exception_Vectors exceptions   PACKED;
  union Instr_Intercepts instrs        PACKED;
  union SVM_Instr_Intercepts svm_instrs PACKED;

  uchar_t rsvd1[43]                    PACKED;

  // offset 0x040
  ullong_t IOPM_BASE_PA                PACKED;
  ullong_t MSRPM_BASE_PA               PACKED;
  ullong_t TSC_OFFSET                  PACKED;

  ulong_t guest_ASID                   PACKED;
  uchar_t TLB_CONTROL                  PACKED;

  uchar_t rsvd2[3]                     PACKED;

  union Guest_Control guest_ctrl       PACKED;
  
  ulong_t interrupt_shadow  : 1        PACKED;
  ulong_t rsvd3             : 31       PACKED;
  ulong_t rsvd4                        PACKED;

  ullong_t exit_code                   PACKED;
  ullong_t exit_info1                  PACKED;
  ullong_t exit_info2                  PACKED;

  /* This could be a typo in the manual....
   * It doesn't actually say that there is a reserved bit
   * But it does say that the EXITINTINFO field is in bits 63-1
   * ALL other occurances mention a 1 bit reserved field
   */
  ulong_t rsvd5             : 1        PACKED;
  ullong_t exit_int_info    : 63       PACKED;
  /* ** */

  ulong_t NP_ENABLE         : 1        PACKED;
  ullong_t rsvd6            : 63       PACKED;

  uchar_t rsvd7[15]                    PACKED;

  // Offset 0xA8
  ullong_t EVENTINJ                    PACKED;


  /* This could be a typo in the manual....
   * It doesn't actually say that there is a reserved bit
   * But it does say that the EXITINTINFO field is in bits 63-1
   * ALL other occurances mention a 1 bit reserved field
   */
  ulong_t rsvd8             : 1        PACKED;
  ullong_t N_CR3            : 63       PACKED;
  /* ** */

  ulong_t LBR_VIRTUALIZATION_ENABLE  : 1  PACKED;
  ullong_t rsvd9            : 63       PACKED;

} vmcb_ctrl_t;






struct vmcb_selector {
  ushort_t selector                   PACKED;
  ushort_t attrib                     PACKED;
  ulong_t  limit                      PACKED;
  ullong_t base                       PACKED;
}







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

  uchar_t rsvd1[42]                  PACKED;

  //offset 0x0cb
  uchar_t cpl                        PACKED; // if the guest is real-mode then the CPL is forced to 0
                                             // if the guest is virtual-mode then the CPL is forced to 3

  ulong_t rsvd2                      PACKED;

  // offset 0x0d0
  ullong_t efer                      PACKED;

  uchar_t rsvd3[111]                 PACKED;
  
  //offset 0x148
  ullong_t cr4                       PACKED;
  ullong_t cr3                       PACKED;
  ullong_t cr0                       PACKED;
  ullong_t dr7                       PACKED;
  ullong_t dr6                       PACKED;
  ullong_t rflags                    PACKED;
  ullong_t rip                       PACKED;

  uchar_t rsvd4[87]                  PACKED;
  
  //offset 0x1d8
  ullong_t rsp                       PACKED;

  uchar_t rsvd5[23]                  PACKED;

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


  uchar_t rsvd6[31]                  PACKED;

  //offset 0x268
  ullong_t g_pat                     PACKED; // Guest PAT                          -- only used if nested paging is enabled
  ullong_t dbgctl                    PACKED; // Guest DBGCTL MSR                   -- only used if the LBR registers are virtualized
  ullong_t br_from                   PACKED; // Guest LastBranchFromIP MSR         -- only used if the LBR registers are virtualized
  ullong_t br_to                     PACKED; // Guest LastBranchToIP MSR           -- only used if the LBR registers are virtualized
  ullong_t lastexcpfrom              PACKED; // Guest LastExceptionFromIP MSR      -- only used if the LBR registers are virtualized
  ullong_t lastexcpto                PACKED; // Guest LastExceptionToIP MSR        -- only used if the LBR registers are virtualized

} vmcb_saved_state_t;


#endif
