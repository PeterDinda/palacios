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



typedef struct VMCB_Control_Area {
  union Ctrl_Registers cr_reads        PACKED;
  union Ctrl_Registers cr_writes       PACKED;
  union Debug_Registers dr_reads       PACKED;
  union Debug_Registers dr_writes      PACKED;
  union Exception_Vectors exceptions   PACKED;
  union Instr_Intercepts instrs        PACKED;
  union SVM_Instr_Intercepts svm_instrs PACKED;
  char rsvd1[43]                       PACKED;
  


} svm_vmcb_t;




#endif
