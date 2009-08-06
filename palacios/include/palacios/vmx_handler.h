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
typedef enum {
    VMEXIT_INFO_EXCEPTION_OR_NMI            = 0,
    VMEXIT_EXTERNAL_INTR                    = 1,
    VMEXIT_TRIPLE_FAULT                     = 2,
    VMEXIT_INIT_SIGNAL                      = 3,
    VMEXIT_STARTUP_IPI                      = 4,
    VMEXIT_IO_SMI                           = 5,
    VMEXIT_OTHER_SMI                        = 6,
    VMEXIT_INTR_WINDOW                      = 7,
    VMEXIT_NMI_WINDOW                       = 8,
    VMEXIT_TASK_SWITCH                      = 9,
    VMEXIT_CPUID                            = 10,
    VMEXIT_HLT                              = 12,
    VMEXIT_INVD                             = 13,
    VMEXIT_INVLPG                           = 14,
    VMEXIT_RDPMC                            = 15,
    VMEXIT_RDTSC                            = 16,
    VMEXIT_RSM                              = 17,
    VMEXIT_VMCALL                           = 18,
    VMEXIT_VMCLEAR                          = 19,
    VMEXIT_VMLAUNCH                         = 20,
    VMEXIT_VMPTRLD                          = 21,
    VMEXIT_VMPTRST                          = 22,
    VMEXIT_VMREAD                           = 23,
    VMEXIT_VMRESUME                         = 24,
    VMEXIT_VMWRITE                          = 25,
    VMEXIT_VMXOFF                           = 26,
    VMEXIT_VMXON                            = 27,
    VMEXIT_CR_REG_ACCESSES                  = 28,
    VMEXIT_MOV_DR                           = 29,
    VMEXIT_IO_INSTR                         = 30,
    VMEXIT_RDMSR                            = 31,
    VMEXIT_WRMSR                            = 32,
    VMEXIT_ENTRY_FAIL_INVALID_GUEST_STATE   = 33,
    VMEXIT_ENTRY_FAIL_MSR_LOAD              = 34,
    VMEXIT_MWAIT                            = 36,
    VMEXIT_MONITOR                          = 39,
    VMEXIT_PAUSE                            = 40,
    VMEXIT_ENTRY_FAILURE_MACHINE_CHECK      = 41,
    VMEXIT_TPR_BELOW_THRESHOLD              = 43,
    VMEXIT_APIC                             = 44,
    VMEXIT_GDTR_IDTR                        = 46,
    VMEXIT_LDTR_TR                          = 47,
    VMEXIT_EPT_VIOLATION                    = 48,
    VMEXIT_EPT_CONFIG                       = 49,
    VMEXIT_INVEPT                           = 50,
    VMEXIT_RDTSCP                           = 51,
    VMEXIT_EXPIRED_PREEMPT_TIMER            = 52,
    VMEXIT_INVVPID                          = 53,
    VMEXIT_WBINVD                           = 54,
    VMEXIT_XSETBV                           = 55
} vmx_exit_t;

int v3_handle_vmx_exit(struct v3_gprs * gprs);

#endif

#endif
