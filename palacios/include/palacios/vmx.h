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


#ifndef __VMX_H__
#define __VMX_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vmcs.h>
#include <palacios/vmm.h>
#include <palacios/vm_guest.h>


#define VMX_SUCCESS        0
#define VMX_FAIL_INVALID   1
#define VMX_FAIL_VALID     2
#define VMM_ERROR          3


struct vmx_pin_ctrls {
    union {
        uint32_t value;
	struct {
	    uint_t ext_int_exit            : 1;
	    uint_t rsvd1                   : 2; 
	    uint_t nmi_exit                : 1;
	    uint_t rsvd2                   : 1;
	    uint_t virt_nmi                : 1;
	    uint_t active_preempt_timer    : 1;
	    uint_t rsvd3                   : 25;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


struct vmx_pri_proc_ctrls {
    union {
        uint32_t value;
	struct {
	    uint_t rsvd1           : 2;
	    uint_t int_wndw_exit   : 1;
	    uint_t tsc_offset      : 1;
	    uint_t rsvd2           : 3;
	    uint_t hlt_exit        : 1;
	    uint_t rsvd3           : 1;
	    uint_t invlpg_exit     : 1;
	    uint_t mwait_exit      : 1;
	    uint_t rdpmc_exit      : 1;
	    uint_t rdtsc_exit      : 1;
	    uint_t rsvd4           : 2;
	    uint_t cr3_ld_exit     : 1;
	    uint_t cr3_str_exit    : 1;
	    uint_t rsvd5           : 2;
	    uint_t cr8_ld_exit     : 1;
	    uint_t cr8_str_exit    : 1;
	    uint_t tpr_shdw        : 1;
	    uint_t nmi_wndw_exit   : 1;
	    uint_t mov_dr_exit     : 1;
	    uint_t uncon_io_exit   : 1;
	    uint_t use_io_bitmap   : 1;
	    uint_t rsvd6           : 1;
	    uint_t monitor_trap    : 1;
	    uint_t use_msr_bitmap  : 1;
	    uint_t monitor_exit    : 1;
	    uint_t pause_exit      : 1;
	    uint_t sec_ctrls       : 1;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct vmx_sec_proc_ctrls {
    union {
        uint32_t value;
	struct {
	    uint_t virt_apic_acc   : 1;
	    uint_t enable_ept      : 1;
	    uint_t desc_table_exit : 1;
	    uint_t enable_rdtscp   : 1;
	    uint_t virt_x2apic     : 1;
	    uint_t enable_vpid     : 1;
	    uint_t wbinvd_exit     : 1;
	    uint_t unrstrct_guest  : 1; /* un restricted guest (CAN RUN IN REAL MODE) */
	    uint_t rsvd1           : 2;
	    uint_t pause_loop_exit : 1;
	    uint_t rsvd2           : 21;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct vmx_exit_ctrls {
    union {
        uint32_t value;
	struct {
	    uint_t rsvd1                : 2;
	    uint_t save_dbg_ctrls       : 1;
	    uint_t rsvd2                : 6;
	    uint_t host_64_on           : 1;
	    uint_t rsvd3                : 2;
	    uint_t ld_perf_glbl_ctrl    : 1;
	    uint_t rsvd4                : 2;
	    uint_t ack_int_on_exit      : 1;
	    uint_t rsvd5                : 2;
	    uint_t save_pat             : 1;
	    uint_t ld_pat               : 1;
	    uint_t save_efer            : 1;
	    uint_t ld_efer              : 1;
	    uint_t save_preempt_timer   : 1;
	    uint_t rsvd6                : 9;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

struct vmx_entry_ctrls {
    union {
        uint32_t value;
	struct {
	    uint_t rsvd1                : 2;
	    uint_t ld_dbg_ctrls         : 1;
	    uint_t rsvd2                : 6;
	    uint_t guest_ia32e          : 1;
	    uint_t smm_entry            : 1;
	    uint_t no_dual_monitor      : 1;
	    uint_t rsvd3                : 1;
	    uint_t ld_perf_glbl_ctrl    : 1;
	    uint_t ld_pat               : 1;
	    uint_t ld_efer              : 1;
	    uint_t rsvd4                : 16;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));


typedef enum { 
    VMXASSIST_DISABLED,
    VMXASSIST_ENABLED
} vmxassist_state_t;

typedef enum {
    VMX_UNLAUNCHED,
    VMX_LAUNCHED
} vmx_state_t;

struct tss_descriptor {
    uint16_t    limit1;
    uint16_t    base1;
    uint_t      base2       : 8;
    /* In IA32, type follows the form 10B1b, where B is the busy flag */
    uint_t      type        : 4; 
    uint_t      zero1       : 1;
    uint_t      dpl         : 2;
    uint_t      present     : 1;
    uint_t      limit2      : 4;
    uint_t      available   : 1;
    uint_t      zero2       : 1;
    uint_t      zero3       : 1;
    uint_t      granularity : 1;
    uint_t      base3       : 8;
#ifdef __V3_64BIT__
    uint32_t    base4;
    uint_t      rsvd1       : 8;
    uint_t      zero4       : 5;
    uint_t      rsvd2       : 19;
#endif
} __attribute__((packed));

struct vmcs_host_state {
    struct v3_segment  gdtr;
    struct v3_segment  idtr;
    struct v3_segment  tr;
};


struct vmcs_msr_save_area {
    union {
	struct vmcs_msr_entry guest_msrs[4];
	struct {
	    struct vmcs_msr_entry guest_star;
	    struct vmcs_msr_entry guest_lstar;
	    struct vmcs_msr_entry guest_fmask;
	    struct vmcs_msr_entry guest_kern_gs;
	} __attribute__((packed));
    } __attribute__((packed));

    union {
	struct vmcs_msr_entry host_msrs[4];
	struct {
	    struct vmcs_msr_entry host_star;
	    struct vmcs_msr_entry host_lstar;
	    struct vmcs_msr_entry host_fmask;
	    struct vmcs_msr_entry host_kern_gs;
	} __attribute__((packed));
    } __attribute__((packed)); 

} __attribute__((packed)); 


struct vmx_data {
    vmx_state_t state;
    vmxassist_state_t assist_state;
    struct vmcs_host_state host_state;



    addr_t vmcs_ptr_phys;

    v3_reg_t guest_cr4; /// corresponds to the CR4 Read shadow


    /* VMX Control Fields */
    struct vmx_pin_ctrls pin_ctrls;
    struct vmx_pri_proc_ctrls pri_proc_ctrls;
    struct vmx_sec_proc_ctrls sec_proc_ctrls;
    struct vmx_exit_ctrls exit_ctrls;
    struct vmx_entry_ctrls entry_ctrls;

    struct vmx_exception_bitmap excp_bmap;

    addr_t msr_area_paddr;
    struct vmcs_msr_save_area * msr_area;
};

int v3_is_vmx_capable();

void v3_init_vmx_cpu(int cpu_id);
void v3_deinit_vmx_cpu(int cpu_id);

int v3_init_vmx_vmcs(struct guest_info * info, v3_vm_class_t vm_class);
int v3_deinit_vmx_vmcs(struct guest_info * core);

int v3_start_vmx_guest(struct guest_info* info);
int v3_reset_vmx_vm_core(struct guest_info * core, addr_t rip);
void v3_flush_vmx_vm_core(struct guest_info * core);

int v3_vmx_enter(struct guest_info * info);

int v3_vmx_load_core(struct guest_info * core, void * ctx);
int v3_vmx_save_core(struct guest_info * core, void * ctx);





#endif // ! __V3VEE__

#endif 


