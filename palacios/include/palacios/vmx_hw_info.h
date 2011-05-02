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
#define VMX_PROCBASED_CTLS2_MSR     0x0000048B
#define VMX_EPT_VPID_CAP_MSR        0x0000048C
#define VMX_TRUE_PINBASED_CTLS_MSR  0x0000048D
#define VMX_TRUE_PROCBASED_CTLS_MSR 0x0000048E
#define VMX_TRUE_EXIT_CTLS_MSR      0x0000048F
#define VMX_TRUE_ENTRY_CTLS_MSR     0x00000490



struct vmx_basic_msr {
    union {
	struct {
	    uint32_t lo;
	    uint32_t hi;
	} __attribute__((packed));

	struct {    uint32_t revision;
	    uint64_t regionSize   : 13;
	    uint64_t rsvd1         : 3; /* Always 0 */
	    uint64_t physWidth     : 1; /* VMCS address field widths 
					  (1=32bits, 0=natural width) */
	    uint64_t smm           : 1;
	    uint64_t memType       : 4; /* 0 = UC, 6 = WriteBack */
	    uint64_t io_str_info   : 1;
	    uint64_t def1_maybe_0  : 1; /* 1="Any VMX ctrls that default to 1 may be cleared to 0" */
	    uint64_t rsvd2        : 8; /* Always 0 */
	}  __attribute__((packed));
    }  __attribute__((packed));
}  __attribute__((packed));


struct vmx_misc_msr {
    union {
	struct {
	    uint32_t lo;
	    uint32_t hi;
	} __attribute__((packed));

	struct {
	    uint64_t tsc_multiple       : 5; /* Bit position in TSC field that drives vmx timer step */
	    uint64_t exits_store_LMA    : 1;
	    uint64_t can_halt           : 1;
	    uint64_t can_shtdown        : 1;
	    uint64_t can_wait_for_sipi  : 1;
	    uint64_t rsvd1              : 7;
	    uint64_t num_cr3_targets    : 9;
	    uint64_t max_msr_cache_size : 3; /* (512 * (max_msr_cache_size + 1)) == max msr load/store list size */
	    uint64_t SMM_ctrl_avail     : 1;
	    uint64_t rsvd2              : 3; 
	    uint64_t MSEG_rev_id;
	}  __attribute__((packed));
    }  __attribute__((packed));
} __attribute__((packed));


struct vmx_ept_msr {
    union {
	struct {
	    uint32_t lo;
	    uint32_t hi;
	} __attribute__((packed));

	struct {
	    uint64_t exec_only_ok             : 1;
	    uint64_t rsvd1                    : 5;
	    uint64_t pg_walk_len4             : 1; /* support for a page walk of length 4 */
	    uint64_t rsvd2                    : 1;
	    uint64_t ept_uc_ok                : 1; /* EPT page tables can be uncacheable */
	    uint64_t rsvd3                    : 5;
	    uint64_t ept_wb_ok                : 1; /* EPT page tables can be writeback */
	    uint64_t rsvd4                    : 1;
	    uint64_t ept_2MB_ok               : 1; /* 2MB EPT pages supported */
	    uint64_t ept_1GB_ok               : 1; /* 1GB EPT pages supported */
	    uint64_t rsvd5                    : 2;
	    uint64_t INVEPT_avail             : 1; /* INVEPT instruction is available */
	    uint64_t rsvd6                    : 4;
	    uint64_t INVEPT_single_ctx_avail  : 1;
	    uint64_t INVEPT_all_ctx_avail     : 1;
	    uint64_t rsvd7                    : 5;
	    uint64_t INVVPID_avail            : 1;
	    uint64_t rsvd8                    : 7;
	    uint64_t INVVPID_1addr_avail      : 1;
	    uint64_t INVVPID_single_ctx_avail : 1;
	    uint64_t INVVPID_all_ctx_avail    : 1;
	    uint64_t INVVPID_single_ctx_w_glbls_avail : 1;
	    uint64_t rsvd9                   : 20;
	}  __attribute__((packed));
    }  __attribute__((packed));
}__attribute__((packed));


struct vmx_ctrl_field {
    uint32_t def_val;
    uint32_t req_val;  /* Required values: field_val & req_mask == req_val */ 
    uint32_t req_mask; /* If a mask bit is set it's value is restricted (i.e. the VMM cannot change it) */
};


struct vmx_cr_field {
    uint64_t def_val;
    uint64_t req_val;  /* Required values: field_val & req_mask == req_val */ 
    uint64_t req_mask; /* If a mask bit is set it's value is restricted (i.e. the VMM cannot change it) */
};




struct vmx_hw_info {
    struct vmx_basic_msr basic_info;
    struct vmx_misc_msr misc_info;
    struct vmx_ept_msr ept_info;

    struct vmx_ctrl_field pin_ctrls;
    struct vmx_ctrl_field proc_ctrls;
    struct vmx_ctrl_field exit_ctrls;
    struct vmx_ctrl_field entry_ctrls;
    struct vmx_ctrl_field sec_proc_ctrls;

    struct vmx_cr_field cr0;
    struct vmx_cr_field cr4;

};




int v3_init_vmx_hw(struct vmx_hw_info * hw_info);

uint32_t v3_vmx_get_ctrl_features(struct vmx_ctrl_field * fields);



#endif

#endif
