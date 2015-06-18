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
#define MB_TAG_MB64_HRT 0xf00d
typedef struct mb_mb64_hrt {
    mb_tag_t       tag;
    uint32_t       hrt_flags;
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
