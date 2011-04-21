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


#ifndef __VMX_HW_INFO_H__
#define __VMX_HW_INFO_H__

#ifdef __V3VEE__


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




struct vmx_basic_msr {
    union {
	uint32_t lo;
	uint32_t hi;
	struct {
	    uint32_t revision;
	    uint32_t regionSize   : 13;
	    uint8_t rsvd1         : 4; /* Always 0 */
	    uint8_t physWidth     : 1; /* VMCS address field widths 
					  (1=32bits, 0=natural width) */
	    uint8_t smm           : 1; // Always 1
	    uint8_t memType       : 4; /* 0 = UC, 6 = WriteBack */
	    uint8_t io_str_info   : 1;
	    uint8_t def1_maybe_0  : 1; /* 1="Any VMX ctrls that default to 1 may be cleared to 0" */
	    uint32_t rsvd2        : 8; /* Always 0 */
	}  __attribute__((packed));
    }  __attribute__((packed));
}  __attribute__((packed));





struct vmx_hw_info {
    struct vmx_basic_msr basic_info;
    


};


int v3_init_vmx_hw();




#endif

#endif
