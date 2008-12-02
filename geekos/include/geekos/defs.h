/*
 * Misc. kernel definitions
 * Copyright (c) 2001,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.9 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#ifndef GEEKOS_DEFS_H
#define GEEKOS_DEFS_H


/*Zheng 08/01/2008*/
#define SYSSEG             0x1000
#define COMMAND_LINE_SIZE  1024

#define SETUPSECTS    4            /* default nr of setup-sectors */
#define SYSSIZE       0x7f00       /* system size: # of 16-byte clicks */
#define ROOT_DEV      0            /* ROOT_DEV is now written by "build" */
#define SWAP_DEV      0            /* SWAP_DEV is now written by "build" */

/*
 * Kernel code and data segment selectors.
 * Keep these up to date with defs.asm.
 */
#define KERNEL_CS  (1<<3)
#define KERNEL_DS  (2<<3)


/*
 * Address where kernel is loaded
 */
#define KERNEL_START_ADDR 0x100000

/*
 * Kernel and user privilege levels
 */
#define KERNEL_PRIVILEGE 0
#define USER_PRIVILEGE 3


/*
 * Software interrupt for syscalls
 */
#define SYSCALL_INT 0x90

/*
 * The windows versions of gcc use slightly different
 * names for the bss begin and end symbols than the Linux version.
 */
#if defined(GNU_WIN32)
#  define BSS_START _bss_start__
#  define BSS_END _bss_end__
#else
#  define BSS_START __bss_start
#  define BSS_END end
#endif

/*
 * x86 has 4096 byte pages
 */
#define PAGE_POWER 12
#define PAGE_SIZE (1<<PAGE_POWER)
#define PAGE_MASK (~(0xffffffff << PAGE_POWER))

#define KERNEL_THREAD_OBJECT (PAGE_SIZE)
#define KERNEL_STACK (PAGE_SIZE * 2)



// Where we load the vm's kernel image (1MB)
//#define VM_KERNEL_TARGET (0x100000) 





#endif  /* GEEKOS_DEFS_H */
