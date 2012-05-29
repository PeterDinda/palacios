/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2011, Kyle C. Hale <kh@u.northwestern.edu> 
 * Copyright (c) 2011, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */

#ifndef __SYSCALL_HIJACK_H__
#define __SYSCALL_HIJACK_H__

#ifdef V3_CONFIG_EXT_SELECTIVE_SYSCALL_EXIT
int v3_syscall_on (void * ginfo, uint8_t syscall_nr);
int v3_syscall_off (void * ginfo, uint8_t syscall_nr);
int v3_syscall_stat (void * ginfo, uint8_t syscall_nr);

#endif
#ifdef __V3VEE__

#define STAR_MSR                 0xc0000081 /* Legacy mode SYSCALL target */
#define LSTAR_MSR                0xc0000082 /* Long mode SYSCALL target */
#define CSTAR_MSR                0xc0000083 /* compat mode SYSCALL target */
#define SF_MASK_MSR              0xc0000084 /* EFLAGS mask for syscall */
#define SYSENTER_CS_MSR          0x00000174 /* SYSENTER/EXIT are for legacy mode only on AMD */
#define SYSENTER_ESP_MSR         0x00000175
#define SYSENTER_EIP_MSR         0x00000176

/* Intel specific */
#define IA32_SYSENTER_CS_MSR     0x00000174
#define IA32_SYSENTER_ESP_MSR    0x00000175
#define IA32_SYSENTER_EIP_MSR    0x00000176

#define MAX_CHARS 256
#ifndef max
    #define max(a, b) ( ((a) > (b)) ? (a) : (b) )
#endif

#define SYSCALL_INT_VECTOR   0x80
#define SYSCALL_CPUID_NUM    0x80000001
#define SYSENTER_CPUID_NUM   0x00000001

#define SYSCALL_MAGIC_ADDR       0xffffffffffffffff

#define KERNEL_PHYS_LOAD_ADDR    0x1000000

// hcall numbers for fast system call exiting utility
#define SYSCALL_HANDLE_HCALL   0x5CA11
#define SYSCALL_SETUP_HCALL    0x5CA12
#define SYSCALL_CLEANUP_HCALL  0x5CA13

struct v3_syscall_info {
    uint64_t target_addr;
    uint8_t  syscall_map_injected;
    char * syscall_page_backup;
    uint8_t * syscall_map;
    addr_t syscall_stub;
    // state save area
    addr_t ssa;
};

int v3_hook_syscall (struct guest_info * core,
    uint_t syscall_nr,
    int (*handler)(struct guest_info * core, uint_t syscall_nr, void * priv_data), 
    void * priv_data);

int v3_hook_passthrough_syscall (struct guest_info * core, uint_t syscall_nr);
int v3_syscall_handler (struct guest_info * core, uint8_t vector, void * priv_data);

#endif

#endif
