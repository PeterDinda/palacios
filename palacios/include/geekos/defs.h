/*
 * Misc. kernel definitions
 * Copyright (c) 2001,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.3 $
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
#define KERNEL_START_ADDR 0x10000

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

TOP_OF_MEM:    
        GDT (one page)
        TSS (one page)
        IDT (one page)
        KernelImage
          VMBootPackage
             rombios (2nd copy)
             vmxassist
             vgabios
             rombios (1st copy)
        PageList           (varies - must be an integral number of pages)
	KernelHeap         (varies - KERNEL_HEAP_SIZE; must be integral number of pages)
        KernelStack        (one page)
        KernelThreadObject (one page)
0+VM_SIZE:
        VM (including ISA hole)          
        (KernelImage, GDT, and IDT start down here and are moved up)
        (VMBoot Package is copied down to the first MB to start the VM boot)
        (VMXAssist is used to provide real mode emulation for initial VM boot steps)
0:      
*/

// This is for a 1 GB Machine
// The last address (+1) at which physical memory ends
#define TOP_OF_MEM (0x30000000)
// How much space to reserve for the VM
#define VM_SIZE    (0x20000000)
// Where the VM starts in physical memory
#define START_OF_VM (0x0)
// Where we load the vm's kernel image (1MB)
#define VM_KERNEL_TARGET (0x100000) 



#define GDT_SIZE PAGE_SIZE
#define TSS_SIZE PAGE_SIZE
#define IDT_SIZE PAGE_SIZE
#define KERNEL_HEAP_SIZE (256*PAGE_SIZE)
#define KERNEL_STACK_SIZE PAGE_SIZE
#define KERNEL_THREAD_OBJECT_SIZE PAGE_SIZE




/*
 *  * Keep these up to date with defs.asm.
 */
#define GDT_LOCATION          (TOP_OF_MEM-GDT_SIZE)
#define TSS_LOCATION          (GDT_LOCATION-TSS_SIZE)
#define IDT_LOCATION          (TSS_LOCATION-IDT_SIZE)


#define FINAL_KERNEL_START    (IDT_LOCATION-MAX_VMM)
#define FINAL_KERNEL_END      (FINAL_KERNEL_START+KERNEL_CORE_LENGTH-1)
#define FINAL_VM_KERNEL_START      (FINAL_KERNEL_START+KERNEL_CORE_LENGTH)
#define FINAL_VM_KERNEL_END        (FINAL_VM_KERNEL_START+VM_KERNEL_LENGTH-1)
#define FINAL_VMBOOTSTART     (FINAL_VM_KERNEL_START)
#define FINAL_VMBOOTEND       (VM_BOOT_PACKAGE_END)

#if (FINAL_VMBOOTEND>IDT_LOCATION)
#error VMM_MAX is too small!
#endif


#define KERNEL_THREAD_OBJECT  (START_OF_VM+VM_SIZE)
#define KERNEL_STACK          (KERNEL_THREAD_OBJECT+KERNEL_THREAD_OBJECT_SIZE)
#define KERNEL_HEAP           (KERNEL_STACK+KERNEL_STACK_SIZE)
#define KERNEL_PAGELIST       (KERNEL_HEAP+KERNEL_HEAP_SIZE)


/*
 * PC memory map
 */
#define ISA_HOLE_START 0x0A0000
#define ISA_HOLE_END   0x100000


#endif  /* GEEKOS_DEFS_H */
