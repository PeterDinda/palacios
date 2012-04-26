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

#ifndef __SVM_HANDLER_H
#define __SVM_HANDLER_H

#ifdef __V3VEE__

#include <palacios/svm.h>
#include <palacios/vmcb.h>
#include <palacios/vmm.h>



/******************************************/
/* SVM Intercept Exit Codes               */
/* AMD Arch Vol 3, Appendix C, pg 477-478 */
/******************************************/
#define SVM_EXIT_CR_READ_MASK 0xfffffff0
#define SVM_EXIT_CR0_READ   0x00000000
#define SVM_EXIT_CR1_READ   0x00000001
#define SVM_EXIT_CR2_READ   0x00000002
#define SVM_EXIT_CR3_READ   0x00000003
#define SVM_EXIT_CR4_READ   0x00000004
#define SVM_EXIT_CR5_READ   0x00000005
#define SVM_EXIT_CR6_READ   0x00000006
#define SVM_EXIT_CR7_READ   0x00000007
#define SVM_EXIT_CR8_READ   0x00000008
#define SVM_EXIT_CR9_READ   0x00000009
#define SVM_EXIT_CR10_READ  0x0000000a
#define SVM_EXIT_CR11_READ  0x0000000b
#define SVM_EXIT_CR12_READ  0x0000000c
#define SVM_EXIT_CR13_READ  0x0000000d
#define SVM_EXIT_CR14_READ  0x0000000e
#define SVM_EXIT_CR15_READ  0x0000000f

#define SVM_EXIT_CR_WRITE_MASK 0xffffffe0
#define SVM_EXIT_CR0_WRITE   0x00000010
#define SVM_EXIT_CR1_WRITE   0x00000011
#define SVM_EXIT_CR2_WRITE   0x00000012
#define SVM_EXIT_CR3_WRITE   0x00000013
#define SVM_EXIT_CR4_WRITE   0x00000014
#define SVM_EXIT_CR5_WRITE   0x00000015
#define SVM_EXIT_CR6_WRITE   0x00000016
#define SVM_EXIT_CR7_WRITE   0x00000017
#define SVM_EXIT_CR8_WRITE   0x00000018
#define SVM_EXIT_CR9_WRITE   0x00000019
#define SVM_EXIT_CR10_WRITE  0x0000001a
#define SVM_EXIT_CR11_WRITE  0x0000001b
#define SVM_EXIT_CR12_WRITE  0x0000001c
#define SVM_EXIT_CR13_WRITE  0x0000001d
#define SVM_EXIT_CR14_WRITE  0x0000001e
#define SVM_EXIT_CR15_WRITE  0x0000001f

#define SVM_EXIT_DR_READ_MASK 0xffffffd0
#define SVM_EXIT_DR0_READ   0x00000020
#define SVM_EXIT_DR1_READ   0x00000021
#define SVM_EXIT_DR2_READ   0x00000022
#define SVM_EXIT_DR3_READ   0x00000023
#define SVM_EXIT_DR4_READ   0x00000024
#define SVM_EXIT_DR5_READ   0x00000025
#define SVM_EXIT_DR6_READ   0x00000026
#define SVM_EXIT_DR7_READ   0x00000027
#define SVM_EXIT_DR8_READ   0x00000028
#define SVM_EXIT_DR9_READ   0x00000029
#define SVM_EXIT_DR10_READ  0x0000002a
#define SVM_EXIT_DR11_READ  0x0000002b
#define SVM_EXIT_DR12_READ  0x0000002c
#define SVM_EXIT_DR13_READ  0x0000002d
#define SVM_EXIT_DR14_READ  0x0000002e
#define SVM_EXIT_DR15_READ  0x0000002f

#define SVM_EXIT_DR_WRITE_MASK     0xffffffc0
#define SVM_EXIT_DR0_WRITE   0x00000030 // ? this was previously 3f
#define SVM_EXIT_DR1_WRITE   0x00000031
#define SVM_EXIT_DR2_WRITE   0x00000032
#define SVM_EXIT_DR3_WRITE   0x00000033
#define SVM_EXIT_DR4_WRITE   0x00000034
#define SVM_EXIT_DR5_WRITE   0x00000035
#define SVM_EXIT_DR6_WRITE   0x00000036
#define SVM_EXIT_DR7_WRITE   0x00000037
#define SVM_EXIT_DR8_WRITE   0x00000038
#define SVM_EXIT_DR9_WRITE   0x00000039
#define SVM_EXIT_DR10_WRITE  0x0000003a
#define SVM_EXIT_DR11_WRITE  0x0000003b
#define SVM_EXIT_DR12_WRITE  0x0000003c
#define SVM_EXIT_DR13_WRITE  0x0000003d
#define SVM_EXIT_DR14_WRITE  0x0000003e
#define SVM_EXIT_DR15_WRITE  0x0000003f

#define SVM_EXIT_EXCP_MASK   0xffffffa0
#define SVM_EXIT_EXCP0       0x00000040
#define SVM_EXIT_EXCP1       0x00000041
#define SVM_EXIT_EXCP2       0x00000042
#define SVM_EXIT_EXCP3       0x00000043
#define SVM_EXIT_EXCP4       0x00000044
#define SVM_EXIT_EXCP5       0x00000045
#define SVM_EXIT_EXCP6       0x00000046
#define SVM_EXIT_EXCP7       0x00000047
#define SVM_EXIT_EXCP8       0x00000048
#define SVM_EXIT_EXCP9       0x00000049
#define SVM_EXIT_EXCP10      0x0000004a
#define SVM_EXIT_EXCP11      0x0000004b
#define SVM_EXIT_EXCP12      0x0000004c
#define SVM_EXIT_EXCP13      0x0000004d
#define SVM_EXIT_EXCP14      0x0000004e
#define SVM_EXIT_EXCP15      0x0000004f
#define SVM_EXIT_EXCP16      0x00000050
#define SVM_EXIT_EXCP17      0x00000051
#define SVM_EXIT_EXCP18      0x00000052
#define SVM_EXIT_EXCP19      0x00000053
#define SVM_EXIT_EXCP20      0x00000054
#define SVM_EXIT_EXCP21      0x00000055
#define SVM_EXIT_EXCP22      0x00000056
#define SVM_EXIT_EXCP23      0x00000057
#define SVM_EXIT_EXCP24      0x00000058
#define SVM_EXIT_EXCP25      0x00000059
#define SVM_EXIT_EXCP26      0x0000005a
#define SVM_EXIT_EXCP27      0x0000005b
#define SVM_EXIT_EXCP28      0x0000005c
#define SVM_EXIT_EXCP29      0x0000005d
#define SVM_EXIT_EXCP30      0x0000005e
#define SVM_EXIT_EXCP31      0x0000005f


#define SVM_EXIT_INTR                 0x00000060
#define SVM_EXIT_NMI                  0x00000061
#define SVM_EXIT_SMI                  0x00000062
#define SVM_EXIT_INIT                 0x00000063
#define SVM_EXIT_VINITR               0x00000064
#define SVM_EXIT_CR0_SEL_WRITE        0x00000065
#define SVM_EXIT_IDTR_READ            0x00000066
#define SVM_EXIT_GDTR_READ            0x00000067
#define SVM_EXIT_LDTR_READ            0x00000068
#define SVM_EXIT_TR_READ              0x00000069
#define SVM_EXIT_IDTR_WRITE           0x0000006a
#define SVM_EXIT_GDTR_WRITE           0x0000006b
#define SVM_EXIT_LDTR_WRITE           0x0000006c
#define SVM_EXIT_TR_WRITE             0x0000006d
#define SVM_EXIT_RDTSC                0x0000006e
#define SVM_EXIT_RDPMC                0x0000006f
#define SVM_EXIT_PUSHF                0x00000070
#define SVM_EXIT_POPF                 0x00000071
#define SVM_EXIT_CPUID                0x00000072
#define SVM_EXIT_RSM                  0x00000073
#define SVM_EXIT_IRET                 0x00000074
#define SVM_EXIT_SWINT                0x00000075
#define SVM_EXIT_INVD                 0x00000076
#define SVM_EXIT_PAUSE                0x00000077
#define SVM_EXIT_HLT                  0x00000078
#define SVM_EXIT_INVLPG               0x00000079
#define SVM_EXIT_INVLPGA              0x0000007a
#define SVM_EXIT_IOIO                 0x0000007b
#define SVM_EXIT_MSR                  0x0000007c
#define SVM_EXIT_TASK_SWITCH          0x0000007d
#define SVM_EXIT_FERR_FREEZE          0x0000007e
#define SVM_EXIT_SHUTDOWN             0x0000007f
#define SVM_EXIT_VMRUN                0x00000080
#define SVM_EXIT_VMMCALL              0x00000081
#define SVM_EXIT_VMLOAD               0x00000082
#define SVM_EXIT_VMSAVE               0x00000083
#define SVM_EXIT_STGI                 0x00000084
#define SVM_EXIT_CLGI                 0x00000085
#define SVM_EXIT_SKINIT               0x00000086
#define SVM_EXIT_RDTSCP               0x00000087
#define SVM_EXIT_ICEBP                0x00000088
#define SVM_EXIT_WBINVD               0x00000089
#define SVM_EXIT_MONITOR              0x0000008a
#define SVM_EXIT_MWAIT                0x0000008b
#define SVM_EXIT_MWAIT_CONDITIONAL    0x0000008c

#define SVM_EXIT_NPF                  0x00000400

#define SVM_EXIT_INVALID_VMCB         -1

/******************************************/


int v3_handle_svm_exit(struct guest_info * info, addr_t exit_code, 
		       addr_t exit_info1, addr_t exit_info2);
const char * v3_svm_exit_code_to_str(uint_t exit_code);


#endif // ! __V3VEE__

#endif
