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

#ifndef __VM_GUEST_H__
#define __VM_GUEST_H__

#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vmm_mem.h>
#include <palacios/vmm_mem_hook.h>
#include <palacios/vmm_io.h>
#include <palacios/vmm_shadow_paging.h>
#include <palacios/vmm_intr.h>
#include <palacios/vmm_excp.h>
#include <palacios/vmm_dev_mgr.h>
#include <palacios/vmm_time.h>
#include <palacios/vmm_host_events.h>
#include <palacios/vmm_msr.h>
#include <palacios/vmm_hypercall.h>
#include <palacios/vmm_cpuid.h>
#include <palacios/vmm_regs.h>
#include <palacios/vmm_extensions.h>
#include <palacios/vmm_barrier.h>


#ifdef V3_CONFIG_TELEMETRY
#include <palacios/vmm_telemetry.h>
#endif


#ifdef V3_CONFIG_SYMBIOTIC
#include <palacios/vmm_symbiotic.h>
struct v3_sym_core_state;
#endif



#include <palacios/vmm_config.h>

struct v3_intr_state;




/* per-core state */
struct guest_info {
    char exec_name[256];
    
    uint64_t rip;

    uint_t cpl;

    struct vm_time time_state;

    v3_paging_mode_t shdw_pg_mode;
    struct v3_shdw_pg_state shdw_pg_state;
    addr_t direct_map_pt;
    

    union {
	uint32_t flags;
	struct {
	    uint8_t use_large_pages        : 1;    /* Enable virtual page tables to use large pages */
	    uint8_t use_giant_pages        : 1;    /* Enable virtual page tables to use giant (1GB) pages */
	    uint32_t rsvd                  : 30;
	} __attribute__((packed));
    } __attribute__((packed));


    /* This structure is how we get interrupts for the guest */
    struct v3_intr_core_state intr_core_state;

    /* This structure is how we get exceptions for the guest */
    struct v3_excp_state excp_state;


    v3_cpu_mode_t cpu_mode;
    v3_mem_mode_t mem_mode;


    struct v3_gprs vm_regs;
    struct v3_ctrl_regs ctrl_regs;
    struct v3_dbg_regs dbg_regs;
    struct v3_segments segments;


    void * vmm_data;

    uint64_t yield_start_cycle;
    
    uint64_t num_exits;

#ifdef V3_CONFIG_TELEMETRY
    struct v3_core_telemetry core_telem;
#endif


    /* struct v3_core_dev_mgr core_dev_mgr; */

    void * decoder_state;

#ifdef V3_CONFIG_SYMBIOTIC
    /* Symbiotic state */
    struct v3_sym_core_state sym_core_state;
#endif

    /* Per-core config tree data. */
    v3_cfg_tree_t * core_cfg_data;

    struct v3_vm_info * vm_info;

    v3_core_operating_mode_t core_run_state;

    void * core_thread; /* thread struct for virtual core */

    /* the logical cpu on which this core runs */
    uint32_t pcpu_id;
    
    /* The virtual core # of this cpu (what the guest sees this core as) */
    uint32_t vcpu_id;
     
};



/* shared state across cores */
struct v3_vm_info {
    char name[128];

    v3_vm_class_t vm_class;

    addr_t mem_size; /* In bytes for now */
    uint32_t mem_align;
    struct v3_mem_map mem_map;

    struct v3_mem_hooks mem_hooks;

    struct v3_shdw_impl_state shdw_impl;

    struct v3_io_map io_map;
    struct v3_msr_map msr_map;
    struct v3_cpuid_map cpuid_map;

    v3_hypercall_map_t hcall_map;


    struct v3_intr_routers intr_routers;

    /* device_map */
    struct vmm_dev_mgr  dev_mgr;

    struct v3_host_events host_event_hooks;

    struct v3_config * cfg_data;

    v3_vm_operating_mode_t run_state;

    struct v3_barrier barrier;


    struct v3_extensions extensions;

#ifdef V3_CONFIG_SYMBIOTIC
    /* Symbiotic state */
    struct v3_sym_vm_state sym_vm_state;
#endif

#ifdef V3_CONFIG_TELEMETRY
    uint_t enable_telemetry;
    struct v3_telemetry_state telemetry;
#endif


    uint64_t yield_cycle_period;  


    void * host_priv_data;

    int num_cores;

    // JRL: This MUST be the last entry...
    struct guest_info cores[0];
};

int v3_init_vm(struct v3_vm_info * vm);
int v3_init_core(struct guest_info * core);

int v3_free_vm_internal(struct v3_vm_info * vm);
int v3_free_core(struct guest_info * core);


uint_t v3_get_addr_width(struct guest_info * info);
v3_cpu_mode_t v3_get_vm_cpu_mode(struct guest_info * info);
v3_mem_mode_t v3_get_vm_mem_mode(struct guest_info * info);


const uchar_t * v3_cpu_mode_to_str(v3_cpu_mode_t mode);
const uchar_t * v3_mem_mode_to_str(v3_mem_mode_t mode);

int v3_translate_segment(struct guest_info * info, uint16_t selector, struct v3_segment * seg);


void v3_print_guest_state(struct guest_info * info);

void v3_print_segments(struct v3_segments * segs);
void v3_print_ctrl_regs(struct guest_info * info);
void v3_print_GPRs(struct guest_info * info);

void v3_print_stack(struct guest_info * info);

#endif /* ! __V3VEE__ */

void v3_print_guest_state_all(struct v3_vm_info * vm);

#endif
