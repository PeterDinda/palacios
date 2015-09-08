/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2015, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMM_HVM_H
#define __VMM_HVM_H


#ifdef __V3VEE__ 

#include <palacios/vmm_types.h>
#include <palacios/vmm_multiboot.h>

struct v3_ros_event {
    enum { ROS_NONE=0, ROS_PAGE_FAULT=1, ROS_SYSCALL=2 } event_type;
    uint64_t       last_ros_event_result; // valid when ROS_NONE
    union {
	struct {   // valid when ROS_PAGE_FAULT
	    uint64_t rip;
	    uint64_t cr2;
	    enum {ROS_READ, ROS_WRITE} action;
	} page_fault;
	struct { // valid when ROS_SYSCALL
	    uint64_t args[8];
	} syscall;
    };
};

struct v3_vm_hvm {
    uint8_t   is_hvm;
    uint32_t  first_hrt_core;
    uint64_t  first_hrt_gpa;
    struct v3_cfg_file *hrt_file;
    uint64_t  hrt_entry_addr;
    enum { HRT_BLOB, HRT_ELF64, HRT_MBOOT2, HRT_MBOOT64 } hrt_type;

    // The following parallel the content of mb_info_hrt_t in
    // the extended multiboot header.   They reflect how the 
    // HRT has actually been mapped, as opposed to the requested
    // mapping/flags from the mb_mb64_hrt_t
    uint64_t  hrt_flags; 
    uint64_t  max_mem_mapped;
    uint64_t  gva_offset;
    uint64_t  gva_entry;
    uint64_t  comm_page_gpa;
    uint8_t   hrt_int_vector;

    void     *comm_page_hpa;
    void     *comm_page_hva;

    enum {HRT_IDLE=0, HRT_CALL=1, HRT_PARCALL=2, HRT_SYNCSETUP=3, HRT_SYNC=4, HRT_SYNCTEARDOWN=5, HRT_MERGE=6} trans_state;
    uint64_t  trans_count;

    // the ROS event to be handed back
    struct v3_ros_event ros_event;

};

struct v3_core_hvm {
    uint8_t   is_hrt;
    uint64_t  last_boot_start;
};



struct v3_xml;

int v3_init_hvm();
int v3_deinit_hvm();

int v3_init_hvm_vm(struct v3_vm_info *vm, struct v3_xml *config);
int v3_deinit_hvm_vm(struct v3_vm_info *vm);


int v3_init_hvm_core(struct guest_info *core);
int v3_deinit_hvm_core(struct guest_info *core);


uint64_t v3_get_hvm_ros_memsize(struct v3_vm_info *vm);
uint64_t v3_get_hvm_hrt_memsize(struct v3_vm_info *vm);
int      v3_is_hvm_ros_mem_gpa(struct v3_vm_info *vm, addr_t gpa);
int      v3_is_hvm_hrt_mem_gpa(struct v3_vm_info *vm, addr_t gpa);

uint32_t v3_get_hvm_ros_cores(struct v3_vm_info *vm);
uint32_t v3_get_hvm_hrt_cores(struct v3_vm_info *vm);
int      v3_is_hvm_ros_core(struct guest_info *core);
int      v3_is_hvm_hrt_core(struct guest_info *core);


int      v3_hvm_should_deliver_ipi(struct guest_info *src, struct guest_info *dest);
void     v3_hvm_find_apics_seen_by_core(struct guest_info *core, struct v3_vm_info *vm, 
					uint32_t *start_apic, uint32_t *num_apics);


int v3_build_hrt_multiboot_tag(struct guest_info *core, mb_info_hrt_t *hrt);

int v3_setup_hvm_vm_for_boot(struct v3_vm_info *vm);
int v3_setup_hvm_hrt_core_for_boot(struct guest_info *core);

int v3_handle_hvm_reset(struct guest_info *core);

/*
  HVM/HRT interaction is as follows:

  1. MB_TAG_MB64_HRT tag in the HRT multiboot kernel signifies it
     is handled by the HVM.
  2. The flags and other info in the the tag indicate the properties of the HRT
     to the HVM.  (see vmm_multiboot.h), in particular:
         - position independence
         - ability to be initially mapped with an offset
           between virtual and physical addresses, for example  
           to hoist it into the same position that the ROS kernel
           will occupy in the virtual address space of a ROS
           process
         - how much physical address space we will intiially map
           and what kind of page tables are used to map it
         - what physical page (4KB) should we reserve for use
           in HVM/HRT communication (particularly upcalls)
         - the interrupt vector used to upcall from the HVM to the HRT
  3. The MB_INFO_HRT_TAG within the multiboot info structures the
     HRT sees on boot indicates that HRT functionality is established and
     gives details of operation to the HRT, including the following.
     See vmm_multiboot.c for more info
         - apics and ioapic ids, and indications of which apics
           and which entries on ioapics are exclusively for HRT use
         - physical address range that is exclusively for HRT use
         - where the the physical address range exclusively for HRT use 
           is mapped into the virtual address space (offset).  The
           ROS part of the physical address space is always identity mapped 
           initially.
         - the amount of physical memory that has been mapped
         - the physical address of the page the HVM will use to 
           communicate with the HRT
         - the interrupt vector the HVM will use to upcall the HRT
         - flags copied from the HRT's HRT tag (position independence, 
           page table model, offset, etc)
  4. Downcalls:
         hypercall 0xf00df00d with arguments depending on operation
         with examples described below.
  5. Upcalls
         interrupt injected by VMM or a magic #PF
         communication via a shared memory page, contents below

  Upcalls

   Type of upcall is determined by the first 64 bits in the commm page

   0x0  =>  Null (test)
   0x20 =>  Invoke function in HRT 
            Next 64 bits contains address of structure
            describing function call.   This is typically the ROS
            trying to get the HRT to run a function for it. 
            ROS is resposible for assuring that this address
            (and other addresses) are correct with respect to
            mappings.   That is, for a non-merged address space,
            the ROS needs to supply physical addresses so that
            they can be used (with the identity-mapped ROS physical
            memory.)  If it wants to use virtual addresses, it
            needs to first merge the address spaces. 
   0x21 =>  Invoke function in HRT in parallel
            Exactly like previos, but the upcall is happening 
            simultaneously on all HRT cores. 
   0x30 =>  Merge address space
            Next 64 bits contains the ROS CR3 that we will use
            to map the user portion of ROS address space into
            the HRT address space
   0x31 =>  Unmerge address space
            return the ROS memory mapping to normal (physical/virtual identity)

  Downcalls

   HVM_HCALL is the general hypercall number used to talk to the HVM
     The first argument is the request number (below).   The other arguments
     depend on the first.

   0x0  =>  Null, just for timing
   0x1  =>  Reboot ROS
   0x2  =>  Reboot HRT
   0x3  =>  Reboot Both
   0xf  =>  Get HRT transaction state and current ROS event
            first argument is pointer to the ROS event state 
	    to be filled out

   0x10 =>  ROS event request (HRT->ROS)
            first argument is pointer where to write the ROS event state
   0x1f =>  ROS event completion (ROS->HRT)
            first argument is the result code

   0x20 =>  Invoke function (ROS->HRT)
            first argument is pointer to structure describing call
   0x21 =>  Invoke function in parallel (ROS->HRT)
            same as above, but simultaneously on all HRT cores
   0x2f =>  Function execution complete (HRT->ROS, once per core)
   0x30 =>  Merge address space (ROS->HRT)
            no arguments (CR3 implicit).   Merge the current
            address space in the ROS with the address space on 
            the HRT
   0x31 =>  Unmerge address apce (ROS->HRT)
            release any address space merger and restore identity mapping
   0x3f =>  Merge request complete (HRT->ROS)

*/     
     


#endif /* ! __V3VEE__ */


#endif
