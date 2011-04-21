/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#include <palacios/vmm.h>
#include <palacios/vmm_lowlevel.h>
#include <palacios/vmx_hw_info.h>


// Intel VMX Feature MSRs

#define VMX_BASIC_MSR               0x00000480
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







int v3_init_vmx_hw(struct vmx_hw_info * hw_info) {
    //  extern v3_cpu_arch_t v3_cpu_types[];

    memset(hw_info, 0, sizeof(struct vmx_hw_info));

    v3_get_msr(VMX_BASIC_MSR, &(hw_info->basic_info.hi), &(hw_info->basic_info.lo));
    


    /*
    if (has_vmx_nested_paging() == 1) {
        v3_cpu_types[cpu_id] = V3_VMX_EPT_CPU;
    } else {
        v3_cpu_types[cpu_id] = V3_VMX_CPU;
    }
    */
    return 0;
}
