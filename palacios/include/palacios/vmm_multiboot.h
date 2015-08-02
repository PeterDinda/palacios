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


#ifndef __VMM_MULTIBOOT_H
#define __VMM_MULTIBOOT_H


#ifdef __V3VEE__ 

#include <palacios/vmm_types.h>


/******************************************************************
     Data contained in the ELF file we will attempt to boot  
******************************************************************/

#define ELF_MAGIC    0x464c457f
#define MB2_MAGIC    0xe85250d6

typedef struct mb_header {
    uint32_t magic;
    uint32_t arch; 
#define ARCH_X86 0
    uint32_t headerlen;
    uint32_t checksum;
} __attribute__((packed)) mb_header_t;

typedef struct mb_tag {
    uint16_t type;
    uint16_t flags;
    uint32_t size;
} __attribute__((packed)) mb_tag_t;

#define MB_TAG_INFO    1
typedef struct mb_info_req {
    mb_tag_t tag;
    uint32_t types[0];
} __attribute__((packed)) mb_info_t;


typedef uint32_t u_virt, u_phys;

#define MB_TAG_ADDRESS 2
typedef struct mb_addr {
    mb_tag_t tag;
    u_virt   header_addr;
    u_virt   load_addr;
    u_virt   load_end_addr;
    u_virt   bss_end_addr;
} __attribute__((packed)) mb_addr_t;

#define MB_TAG_ENTRY 3
typedef struct mb_entry {
    mb_tag_t tag;
    u_virt   entry_addr;
} __attribute__((packed)) mb_entry_t;

#define MB_TAG_FLAGS 4
typedef struct mb_flags {
    mb_tag_t tag;
    uint32_t console_flags;
} __attribute__((packed)) mb_flags_t;

#define MB_TAG_FRAMEBUF 5
typedef struct mb_framebuf {
    mb_tag_t tag;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} __attribute__((packed)) mb_framebuf_t;

#define MB_TAG_MODALIGN 6
typedef struct mb_modalign {
    mb_tag_t tag;
    uint32_t size;
} __attribute__((packed)) mb_modalign_t;


// For HVM, which can use a pure 64 bit variant
// version of multiboot.  The existence of
// this tag indicates that this special mode is
// requested
#define MB_TAG_MB64_HRT           0xf00d
typedef struct mb_mb64_hrt {
    mb_tag_t       tag;
    uint64_t       hrt_flags;
    // whether this kernel is relocable
#define MB_TAG_MB64_HRT_FLAG_RELOC      0x1
    // How to map the memory in the initial PTs
    // highest set bit wins
#define MB_TAG_MB64_HRT_FLAG_MAP_4KB    0x100
#define MB_TAG_MB64_HRT_FLAG_MAP_2MB    0x200
#define MB_TAG_MB64_HRT_FLAG_MAP_1GB    0x400
#define MB_TAG_MB64_HRT_FLAG_MAP_512GB  0x800

    // How much physical address space to map in the
    // initial page tables (bytes)
    // 
    uint64_t       max_mem_to_map;
    // offset of the GVA->GPA mappings (GVA of GPA 0)
    uint64_t       gva_offset;
    // 64 bit entry address (=0 to use entry tag (which will be offset by gva_offset))
    uint64_t       gva_entry;
    // desired address of the page the VMM, HRT, and ROS share
    // for communication.  "page" here a 4 KB quantity
    uint64_t       comm_page_gpa;
    // desired interrupt vector that should be used for upcalls
    // the default for this is 255
    uint8_t        hrt_int_vector;
    uint8_t        reserved[7];
    
} __attribute__((packed)) mb_mb64_hrt_t;

typedef struct mb_data {
    mb_header_t   *header;
    mb_info_t     *info;
    mb_addr_t     *addr;
    mb_entry_t    *entry;
    mb_flags_t    *flags;
    mb_framebuf_t *framebuf;
    mb_modalign_t *modalign;
    mb_mb64_hrt_t *mb64_hrt;
} mb_data_t;



// We are not doing:
//
// - BIOS Boot Device
// - Modules
// - ELF symbols
// - Boot Loader name
// - APM table
// - VBE info
// - Framebuffer info
//



/******************************************************************
     Data we will pass to the kernel via rbx
******************************************************************/

#define MB2_INFO_MAGIC    0x36d76289


typedef struct mb_info_header {
    uint32_t  totalsize;
    uint32_t  reserved;
} __attribute__((packed)) mb_info_header_t;

// A tag of type 0, size 8 indicates last value
//
typedef struct mb_info_tag {
    uint32_t  type;
    uint32_t  size;
} __attribute__((packed)) mb_info_tag_t;


#define MB_INFO_MEM_TAG  4
typedef struct mb_info_mem {
    mb_info_tag_t tag;
    uint32_t  mem_lower; // 0..640K in KB 
    uint32_t  mem_upper; // in KB to first hole - 1 MB
} __attribute__((packed)) mb_info_mem_t;

#define MB_INFO_CMDLINE_TAG  1
// note alignment of 8 bytes required for each... 
typedef struct mb_info_cmdline {
    mb_info_tag_t tag;
    uint32_t  size;      // includes zero termination
    uint8_t   string[];  // zero terminated
} __attribute__((packed)) mb_info_cmdline_t;


#define MEM_RAM   1
#define MEM_ACPI  3
#define MEM_RESV  4

typedef struct mb_info_memmap_entry {
    uint64_t  base_addr;
    uint64_t  length;
    uint32_t  type;
    uint32_t  reserved;
} __attribute__((packed)) mb_info_memmap_entry_t;

#define MB_INFO_MEMMAP_TAG  6
// note alignment of 8 bytes required for each... 
typedef struct mb_info_memmap {
    mb_info_tag_t tag;
    uint32_t  entry_size;     // multiple of 8
    uint32_t  entry_version;  // 0
    mb_info_memmap_entry_t  entries[];
} __attribute__((packed)) mb_info_memmap_t;

#define MB_INFO_HRT_TAG 0xf00df00d
typedef struct mb_info_hrt {
    mb_info_tag_t  tag;
    // apic ids are 0..num_apics-1
    // ioapics follow
    // apic and ioapic addresses are the well known places
    uint32_t       total_num_apics;
    // first apic the HRT owns (HRT core 0)
    uint32_t       first_hrt_apic_id;
    // can the HRT use an ioapic?
    uint32_t       have_hrt_ioapic;
    // if so, this is the first entry on the
    // ioapic that can be used by the HRT
    uint32_t       first_hrt_ioapic_entry;
    // CPU speed
    uint64_t       cpu_freq_khz;
    // copy of the HRT flags from the kernel (indicating 
    // page table mapping type, position independence, etc.
    // these reflect how it has actually been mapped
    uint64_t       hrt_flags;
    // the amount of physical address space that has been mapped
    // initially. 
    uint64_t       max_mem_mapped; 
    // The first physical address the HRT should
    // (nominally) use.   Physical addresses below this are
    // visible to the ROS
    uint64_t       first_hrt_gpa;
    // Where the intial boot state starts in the physical address
    // space.   This includes INT HANDLER,IDT,GDT,TSS, PAGETABLES,
    // and MBINFO, but not the scratch stacks
    // This is essentially the content of CR3 - 1 page on boot
    uint64_t       boot_state_gpa;
    // Where GPA 0 is mapped in the virtual address space
    uint64_t       gva_offset;   

    // Typically:
    //     first_hrt_vaddr==first_hrt_paddr => no address space coalescing
    //     first_hrt_vaddr>first_hrt_paddr => address space coalescing
    // For example, first_hrt_vaddr might be set to the start of linux kernel
    // This then allows us to coalesce user portion of the address space of 
    // a linux process and the HRT
    // for communication.  "page" here a 4 KB quantity

    // address of the page the VMM, HRT, and ROS share
    uint64_t       comm_page_gpa;
    // interrupt vector used to upcall to HRT (==0 if none)
    // downcalls are done with HVM hypercall 0xf00df00d
    uint8_t        hrt_int_vector;
    uint8_t        reserved[7];
} __attribute__((packed)) mb_info_hrt_t;




struct v3_vm_multiboot {
    uint8_t   is_multiboot;
    struct v3_cfg_file *mb_file;
    mb_data_t mb_data;
    // GPA where we put the MB record, GDT, TSS, etc
    // The kernel load address and size are as in mb_data
    void     *mb_data_gpa; 
};

// There is no core structure for
// multiboot capability


struct v3_xml;

int v3_init_multiboot();
int v3_deinit_multiboot();

int v3_init_multiboot_vm(struct v3_vm_info *vm, struct v3_xml *config);
int v3_deinit_multiboot_vm(struct v3_vm_info *vm);

int v3_init_multiboot_core(struct guest_info *core);
int v3_deinit_multiboot_core(struct guest_info *core);

int v3_setup_multiboot_vm_for_boot(struct v3_vm_info *vm);
int v3_setup_multiboot_core_for_boot(struct guest_info *core);

int v3_handle_multiboot_reset(struct guest_info *core);

// The following are utility functions that HVM builds on
int      v3_parse_multiboot_header(struct v3_cfg_file *file, mb_data_t *result);
int      v3_write_multiboot_kernel(struct v3_vm_info *vm, mb_data_t *mb, struct v3_cfg_file *file, 
				   void *base, uint64_t limit);
// The multiboot table is prepared from the perspective of the given
// core - this allows it to be generated appropriately for ROS and HRT cores
// when used in an HVM
uint64_t v3_build_multiboot_table(struct guest_info *core, uint8_t *dest, uint64_t size);

#endif /* ! __V3VEE__ */


#endif
