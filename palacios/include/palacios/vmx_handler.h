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
#define VMEXIT_INFO_EXCEPTION_OR_NMI              0
#define VMEXIT_EXTERNAL_INTR                      1
#define VMEXIT_TRIPLE_FAULT                       2
#define VMEXIT_INIT_SIGNAL                        3
#define VMEXIT_STARTUP_IPI                        4
#define VMEXIT_IO_SMI                             5
#define VMEXIT_OTHER_SMI                          6
#define VMEXIT_INTR_WINDOW                        7
#define VMEXIT_NMI_WINDOW                         8
#define VMEXIT_TASK_SWITCH                        9
#define VMEXIT_CPUID                              10
#define VMEXIT_HLT                                12
#define VMEXIT_INVD                               13
#define VMEXIT_INVLPG                             14
#define VMEXIT_RDPMC                              15
#define VMEXIT_RDTSC                              16
#define VMEXIT_RSM                                17
#define VMEXIT_VMCALL                             18
#define VMEXIT_VMCLEAR                            19
#define VMEXIT_VMLAUNCH                           20
#define VMEXIT_VMPTRLD                            21
#define VMEXIT_VMPTRST                            22
#define VMEXIT_VMREAD                             23
#define VMEXIT_VMRESUME                           24
#define VMEXIT_VMWRITE                            25
#define VMEXIT_VMXOFF                             26
#define VMEXIT_VMXON                              27
#define VMEXIT_CR_REG_ACCESSES                    28
#define VMEXIT_MOV_DR                             29
#define VMEXIT_IO_INSTR                           30
#define VMEXIT_RDMSR                              31
#define VMEXIT_WRMSR                              32
#define VMEXIT_ENTRY_FAIL_INVALID_GUEST_STATE     33
#define VMEXIT_ENTRY_FAIL_MSR_LOAD                34
#define VMEXIT_MWAIT                              36
#define VMEXIT_MONITOR                            39
#define VMEXIT_PAUSE                              40
#define VMEXIT_ENTRY_FAILURE_MACHINE_CHECK        41
#define VMEXIT_TPR_BELOW_THRESHOLD                43

int v3_handle_vmx_exit(struct guest_info * info);

#endif

#endif
