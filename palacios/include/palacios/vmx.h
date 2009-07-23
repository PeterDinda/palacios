
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
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMX_H
#define __VMX_H

#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vmcs.h>
#include <palacios/vmm.h>

// Intel VMX Specific MSRs
#define VMX_FEATURE_CONTROL_MSR     0x0000003a
#define VMX_BASIC_MSR          0x00000480
#define VMX_PINBASED_CTLS_MSR       0x00000481
#define VMX_PROCBASED_CTLS_MSR      0x00000482
#define VMX_EXIT_CTLS_MSR           0x00000483
#define VMX_ENTRY_CTLS_MSR          0x00000484
#define VMX_MISC_MSR                0x00000485
#define VMX_CR0_FIXED0_MSR          0x00000486
#define VMX_CR0_FIXED1_MSR          0x00000487
#define VMX_CR4_FIXED0_MSR          0x00000488
#define VMX_CR4_FIXED1_MSR          0x00000489
#define VMX_VMCS_ENUM_MSR           0x0000048A

#define VMX_SUCCESS        0
#define VMX_FAIL_INVALID   1
#define VMX_FAIL_VALID     2
#define VMM_ERROR          3

#define FEATURE_CONTROL_LOCK  0x00000001
#define FEATURE_CONTROL_VMXON 0x00000004
#define FEATURE_CONTROL_VALID ( FEATURE_CONTROL_LOCK | FEATURE_CONTROL_VMXON )


#define CPUID_1_ECX_VTXFLAG 0x00000020



struct vmx_basic_msr {
    uint32_t revision;
    uint_t regionSize   : 13;
    uint_t rsvd1        : 4; // Always 0
    uint_t physWidth    : 1;
    uint_t smm          : 1; // Always 1
    uint_t memType      : 4;
    uint_t rsvd2        : 10; // Always 0
}  __attribute__((packed));

typedef enum { 
    VMXASSIST_STARTUP,
    VMXASSIST_V8086_BIOS,
    VMXASSIST_V8086,
    NORMAL 
} vmx_state_t;

struct vmx_data {
    vmx_state_t state;
    addr_t vmcs_ptr_phys;
};


enum InstructionType { VM_UNKNOWN_INST, VM_MOV_TO_CR0 } ;

struct Instruction {
  enum InstructionType type;
  uint_t          address;
  uint_t          size;
  uint_t          input1;
  uint_t          input2;
  uint_t          output;
};




int v3_is_vmx_capable();
void v3_init_vmx(struct v3_ctrl_ops* vm_ops);



#endif // ! __V3VEE__

#endif 

