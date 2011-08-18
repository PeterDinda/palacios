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
#include <palacios/vmm_msr.h>

// Intel VMX Feature MSRs


uint32_t v3_vmx_get_ctrl_features(struct vmx_ctrl_field * fields) {
    // features are available if they are hardwired to 1, or the mask is 0 (they can be changed)
    uint32_t features = 0;
   
    features = fields->req_val;
    features |= ~(fields->req_mask);

    return features;
}


static int get_ex_ctrl_caps(struct vmx_hw_info * hw_info, struct vmx_ctrl_field * field, 
			    uint32_t old_msr, uint32_t true_msr) {
    uint32_t old_0;  /* Bit is 1 => MB1 */
    uint32_t old_1;  /* Bit is 0 => MBZ */
    uint32_t true_0; /* Bit is 1 => MB1 */
    uint32_t true_1; /* Bit is 0 => MBZ */

    v3_get_msr(old_msr, &old_1, &old_0);
    field->def_val = old_0;

    if (hw_info->basic_info.def1_maybe_0) {
	v3_get_msr(true_msr, &true_1, &true_0);
    } else {
	true_0 = old_0;
	true_1 = old_1;
    }
    
    field->req_val = true_0;
    field->req_mask = ~(true_1 ^ true_0);

    return 0;
}


static int get_ctrl_caps(struct vmx_ctrl_field * field, uint32_t msr) {
    uint32_t mbz = 0; /* Bit is 0 => MBZ */
    uint32_t mb1 = 0; /* Bit is 1 => MB1 */
    
    v3_get_msr(msr, &mbz, &mb1);
    
    field->def_val = mb1;
    field->req_val = mb1;
    field->req_mask = ~(mbz ^ mb1);

    return 0;
}



static int get_cr_fields(struct vmx_cr_field * field, uint32_t fixed_1_msr, uint32_t fixed_0_msr) {
    struct v3_msr mbz; /* Bit is 0 => MBZ */
    struct v3_msr mb1; /* Bit is 0 => MBZ */

    v3_get_msr(fixed_1_msr, &(mbz.hi), &(mbz.lo));
    v3_get_msr(fixed_0_msr, &(mb1.hi), &(mb1.lo));
     
    field->def_val = mb1.value;
    field->req_val = mb1.value;
    field->req_mask = ~(mbz.value ^ mb1.value);

    return 0;
}





int v3_init_vmx_hw(struct vmx_hw_info * hw_info) {
    //  extern v3_cpu_arch_t v3_cpu_types[];

    memset(hw_info, 0, sizeof(struct vmx_hw_info));

    v3_get_msr(VMX_BASIC_MSR, &(hw_info->basic_info.hi), &(hw_info->basic_info.lo));
    v3_get_msr(VMX_MISC_MSR, &(hw_info->misc_info.hi), &(hw_info->misc_info.lo));


    PrintError("BASIC_MSR: Lo: %x, Hi: %x\n", hw_info->basic_info.lo, hw_info->basic_info.hi);

    get_ex_ctrl_caps(hw_info, &(hw_info->pin_ctrls), VMX_PINBASED_CTLS_MSR, VMX_TRUE_PINBASED_CTLS_MSR);
    get_ex_ctrl_caps(hw_info, &(hw_info->proc_ctrls), VMX_PROCBASED_CTLS_MSR, VMX_TRUE_PROCBASED_CTLS_MSR);
    get_ex_ctrl_caps(hw_info, &(hw_info->exit_ctrls), VMX_EXIT_CTLS_MSR, VMX_TRUE_EXIT_CTLS_MSR);
    get_ex_ctrl_caps(hw_info, &(hw_info->entry_ctrls), VMX_ENTRY_CTLS_MSR, VMX_TRUE_ENTRY_CTLS_MSR);


    /* Get secondary PROCBASED controls if secondary controls are available (optional or required) */
    /* Intel Manual 3B. Sect. G.3.3 */
    if ( ((hw_info->proc_ctrls.req_mask & 0x80000000) == 0) || 
	 ((hw_info->proc_ctrls.req_val & 0x80000000) == 1) ) {
      
	get_ctrl_caps(&(hw_info->sec_proc_ctrls), VMX_PROCBASED_CTLS2_MSR);

        /* Get EPT data only if available - Intel 3B, G.10 */
        /* EPT is available if processor has secondary controls (already tested) */
        /* and if procbased_ctls2[33]==1  or procbased_ctrls2[37]==1 */

        struct v3_msr proc2;

        v3_get_msr(VMX_PROCBASED_CTLS2_MSR,&(proc2.hi),&(proc2.lo));
	
	if ( (proc2.hi & 0x2) || (proc2.hi & 0x20) ) {
	  v3_get_msr(VMX_EPT_VPID_CAP_MSR, &(hw_info->ept_info.hi), &(hw_info->ept_info.lo));
	}
    }

    get_cr_fields(&(hw_info->cr0), VMX_CR0_FIXED1_MSR, VMX_CR0_FIXED0_MSR);
    get_cr_fields(&(hw_info->cr4), VMX_CR4_FIXED1_MSR, VMX_CR4_FIXED0_MSR);

    return 0;
}
