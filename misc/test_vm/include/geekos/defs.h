/*
 * Misc. kernel definitions
 * Copyright (c) 2001,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.1 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#ifndef GEEKOS_DEFS_H
#define GEEKOS_DEFS_H




/*
 * Kernel code and data segment selectors.
 * Keep these up to date with defs.asm.
 */
#define KERNEL_CS  (1<<3)
#define KERNEL_DS  (2<<3)


/*
 * Address where kernel is loaded INITIALLY
 * we move it up in memory soon
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



/* Ultimate Memory Layout

0:
4096: KernelThreadObject (one page)
      KernelStack (one page)
      GDT (one page)
      TSS (one page)
      IDT (one page)
0x100000: KernelImage
          Page List          (varies - must be an integral number of pages)
	  KernelHeap         (varies - KERNEL_HEAP_SIZE; must be integral number of pages)
*/




#define GDT_SIZE PAGE_SIZE
#define TSS_SIZE PAGE_SIZE
#define IDT_SIZE PAGE_SIZE
//#define KERNEL_HEAP_SIZE (256*PAGE_SIZE)
#define KERNEL_STACK_SIZE PAGE_SIZE
#define KERNEL_THREAD_OBJECT_SIZE PAGE_SIZE




/*
 *  * Keep these up to date with defs.asm.
 */
#define GDT_LOCATION          (PAGE_SIZE * 3)
#define TSS_LOCATION          (GDT_LOCATION + TSS_SIZE)
#define IDT_LOCATION          (TSS_LOCATION + IDT_SIZE)




  #define KERNEL_THREAD_OBJECT  (PAGE_SIZE)
  #define KERNEL_STACK          (KERNEL_THREAD_OBJECT + KERNEL_THREAD_OBJECT_SIZE)
/*
  #define KERNEL_HEAP           (KERNEL_STACK + KERNEL_STACK_SIZE)
  #define KERNEL_PAGELIST       (KERNEL_HEAP + KERNEL_HEAP_SIZE)
*/

/*
 * PC memory map
 */
#define ISA_HOLE_START 0x0A0000
#define ISA_HOLE_END   0x100000


#endif  /* GEEKOS_DEFS_H */
