#ifndef __SVM_H
#define __SVM_H

#include <geekos/vmm_util.h>
#include <geekos/vmm.h>

#define CPUID_FEATURE_IDS 0x80000001
#define CPUID_FEATURE_IDS_ecx_svm_avail 0x00000004

#define CPUID_SVM_REV_AND_FEATURE_IDS 0x8000000a
#define CPUID_SVM_REV_AND_FEATURE_IDS_edx_svml 0x00000004


#define EFER_MSR                 0xc0000080
#define EFER_MSR_svm_enable      0x00001000

/************/
/* SVM MSRs */
/************/
/* AMD Arch Vol 3, sec. 15.28, pg 420 */
/************/

// SVM VM_CR MSR 
#define SVM_VM_CR_MSR             0xc0010114
#define SVM_VM_CR_MSR_dpd         0x00000001
#define SVM_VM_CR_MSR_r_init      0x00000002
#define SVM_VM_CR_MSR_dis_a20m    0x00000004
#define SVM_VM_CR_MSR_lock        0x00000008
#define SVM_VM_CR_MSR_svmdis      0x00000010

#define SVM_IGNNE_MSR             0xc0010115

// SMM Signal Control Register 
#define SVM_SMM_CTL_MSR           0xc0010116
#define SVM_SMM_CTL_MSR_dismiss   0x00000001
#define SVM_SMM_CTL_MSR_enter     0x00000002
#define SVM_SMM_CTL_MSR_smi_cycle 0x00000004
#define SVM_SMM_CTL_MSR_exit      0x00000008
#define SVM_SMM_CTL_MSR_rsm_cycle 0x00000010

#define SVM_VM_HSAVE_PA_MSR      0xc0010117

#define SVM_KEY_MSR              0xc0010118

/******/



/******************************************/
/* SVM Intercep Exit Codes                */
/* AMD Arch Vol 3, Appendix C, pg 477-478 */
/******************************************/
#define VMEXIT_CR_READ_MASK 0xfffffff0
#define VMEXIT_CR0_READ   0x00000000
#define VMEXIT_CR1_READ   0x00000001
#define VMEXIT_CR2_READ   0x00000002
#define VMEXIT_CR3_READ   0x00000003
#define VMEXIT_CR4_READ   0x00000004
#define VMEXIT_CR5_READ   0x00000005
#define VMEXIT_CR6_READ   0x00000006
#define VMEXIT_CR7_READ   0x00000007
#define VMEXIT_CR8_READ   0x00000008
#define VMEXIT_CR9_READ   0x00000009
#define VMEXIT_CR10_READ  0x0000000a
#define VMEXIT_CR11_READ  0x0000000b
#define VMEXIT_CR12_READ  0x0000000c
#define VMEXIT_CR13_READ  0x0000000d
#define VMEXIT_CR14_READ  0x0000000e
#define VMEXIT_CR15_READ  0x0000000f

#define VMEXIT_CR_WRITE_MASK 0xffffffe0
#define VMEXIT_CR0_WRITE   0x00000010
#define VMEXIT_CR1_WRITE   0x00000011
#define VMEXIT_CR2_WRITE   0x00000012
#define VMEXIT_CR3_WRITE   0x00000013
#define VMEXIT_CR4_WRITE   0x00000014
#define VMEXIT_CR5_WRITE   0x00000015
#define VMEXIT_CR6_WRITE   0x00000016
#define VMEXIT_CR7_WRITE   0x00000017
#define VMEXIT_CR8_WRITE   0x00000018
#define VMEXIT_CR9_WRITE   0x00000019
#define VMEXIT_CR10_WRITE  0x0000001a
#define VMEXIT_CR11_WRITE  0x0000001b
#define VMEXIT_CR12_WRITE  0x0000001c
#define VMEXIT_CR13_WRITE  0x0000001d
#define VMEXIT_CR14_WRITE  0x0000001e
#define VMEXIT_CR15_WRITE  0x0000001f

#define VMEXIT_DR_READ_MASK 0xffffffd0
#define VMEXIT_DR0_READ   0x00000020
#define VMEXIT_DR1_READ   0x00000021
#define VMEXIT_DR2_READ   0x00000022
#define VMEXIT_DR3_READ   0x00000023
#define VMEXIT_DR4_READ   0x00000024
#define VMEXIT_DR5_READ   0x00000025
#define VMEXIT_DR6_READ   0x00000026
#define VMEXIT_DR7_READ   0x00000027
#define VMEXIT_DR8_READ   0x00000028
#define VMEXIT_DR9_READ   0x00000029
#define VMEXIT_DR10_READ  0x0000002a
#define VMEXIT_DR11_READ  0x0000002b
#define VMEXIT_DR12_READ  0x0000002c
#define VMEXIT_DR13_READ  0x0000002d
#define VMEXIT_DR14_READ  0x0000002e
#define VMEXIT_DR15_READ  0x0000002f

#define VMEXIT_DR_WRITE_MASK     0xffffffc0
#define VMEXIT_DR0_WRITE   0x0000003f
#define VMEXIT_DR1_WRITE   0x00000031
#define VMEXIT_DR2_WRITE   0x00000032
#define VMEXIT_DR3_WRITE   0x00000033
#define VMEXIT_DR4_WRITE   0x00000034
#define VMEXIT_DR5_WRITE   0x00000035
#define VMEXIT_DR6_WRITE   0x00000036
#define VMEXIT_DR7_WRITE   0x00000037
#define VMEXIT_DR8_WRITE   0x00000038
#define VMEXIT_DR9_WRITE   0x00000039
#define VMEXIT_DR10_WRITE  0x0000003a
#define VMEXIT_DR11_WRITE  0x0000003b
#define VMEXIT_DR12_WRITE  0x0000003c
#define VMEXIT_DR13_WRITE  0x0000003d
#define VMEXIT_DR14_WRITE  0x0000003e
#define VMEXIT_DR15_WRITE  0x0000003f

#define VMEXIT_EXCP_MASK   0xffffffa0
#define VMEXIT_EXCP0       0x00000040
#define VMEXIT_EXCP1       0x00000041
#define VMEXIT_EXCP2       0x00000042
#define VMEXIT_EXCP3       0x00000043
#define VMEXIT_EXCP4       0x00000044
#define VMEXIT_EXCP5       0x00000045
#define VMEXIT_EXCP6       0x00000046
#define VMEXIT_EXCP7       0x00000047
#define VMEXIT_EXCP8       0x00000048
#define VMEXIT_EXCP9       0x00000049
#define VMEXIT_EXCP10      0x0000004a
#define VMEXIT_EXCP11      0x0000004b
#define VMEXIT_EXCP12      0x0000004c
#define VMEXIT_EXCP13      0x0000004d
#define VMEXIT_EXCP14      0x0000004e
#define VMEXIT_EXCP15      0x0000004f
#define VMEXIT_EXCP16      0x00000050
#define VMEXIT_EXCP17      0x00000051
#define VMEXIT_EXCP18      0x00000052
#define VMEXIT_EXCP19      0x00000053
#define VMEXIT_EXCP20      0x00000054
#define VMEXIT_EXCP21      0x00000055
#define VMEXIT_EXCP22      0x00000056
#define VMEXIT_EXCP23      0x00000057
#define VMEXIT_EXCP24      0x00000058
#define VMEXIT_EXCP25      0x00000059
#define VMEXIT_EXCP26      0x0000005a
#define VMEXIT_EXCP27      0x0000005b
#define VMEXIT_EXCP28      0x0000005c
#define VMEXIT_EXCP29      0x0000005d
#define VMEXIT_EXCP30      0x0000005e
#define VMEXIT_EXCP31      0x0000005f


#define VMEXIT_INTR                 0x00000060
#define VMEXIT_NMI                  0x00000061
#define VMEXIT_SMI                  0x00000062
#define VMEXIT_INIT                 0x00000063
#define VMEXIT_VINITR               0x00000064
#define VMEXIT_CR0_SEL_WRITE        0x00000065
#define VMEXIT_IDTR_READ            0x00000066
#define VMEXIT_GDTR_READ            0x00000067
#define VMEXIT_LDTR_READ            0x00000068
#define VMEXIT_TR_READ              0x00000069
#define VMEXIT_IDTR_WRITE           0x0000006a
#define VMEXIT_GDTR_WRITE           0x0000006b
#define VMEXIT_LDTR_WRITE           0x0000006c
#define VMEXIT_TR_WRITE             0x0000006d
#define VMEXIT_RDTSC                0x0000006e
#define VMEXIT_RDPMC                0x0000006f
#define VMEXIT_PUSHF                0x00000070
#define VMEXIT_POPF                 0x00000071
#define VMEXIT_CPUID                0x00000072
#define VMEXIT_RSM                  0x00000073
#define VMEXIT_IRET                 0x00000074
#define VMEXIT_SWINT                0x00000075
#define VMEXIT_INVD                 0x00000076
#define VMEXIT_PAUSE                0x00000077
#define VMEXIT_HLT                  0x00000078
#define VMEXIT_INVLPG               0x00000079
#define VMEXIT_INVLPGA              0x0000007a
#define VMEXIT_IOIO                 0x0000007b
#define VMEXIT_MSR                  0x0000007c
#define VMEXIT_TASK_SWITCH          0x0000007d
#define VMEXIT_FERR_FREEZE          0x0000007e
#define VMEXIT_SHUTDOWN             0x0000007f
#define VMEXIT_VMRUN                0x00000080
#define VMEXIT_VMMCALL              0x00000081
#define VMEXIT_VMLOAD               0x00000082
#define VMEXIT_VMSAVE               0x00000083
#define VMEXIT_STGI                 0x00000084
#define VMEXIT_CLGI                 0x00000085
#define VMEXIT_SKINIT               0x00000086
#define VMEXIT_RDTSCP               0x00000087
#define VMEXIT_ICEBP                0x00000088
#define VMEXIT_WBINVD               0x00000089
#define VMEXIT_MONITOR              0x0000008a
#define VMEXIT_MWAIT                0x0000008b
#define VMEXIT_MWAIT_CONDITIONAL    0x0000008c

#define VMEXIT_INVALID_VMCB         -1

/******************************************/




void Init_SVM();
int is_svm_capable();




#endif
