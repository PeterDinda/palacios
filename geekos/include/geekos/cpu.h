#ifndef _cpu
#define _cpu


// EFLAGS register

#define EFLAGS_CF         (1<<0)  // carry out
// next bit reserved and must be 1
#define EFLAGS_PF         (1<<2)  // ?
// next bit reserved and must be zero 
#define EFLAGS_AF         (1<<4)  // ?
// next bit reserved and must be zero 
#define EFLAGS_ZF         (1<<6)  // zero
#define EFLAGS_SF         (1<<7)  // sign
#define EFLAGS_TF         (1<<8)  // Trap flag
#define EFLAGS_IF         (1<<9)  // Interrupt Enable
#define EFLAGS_DF         (1<<10) // ?
#define EFLAGS_OF         (1<<11) // overflow
#define EFLAGS_IOPL_LO    (1<<12) // IO privilege level low bit
#define EFLAGS_IOPL_HI    (1<<13) // IO privilege level low bit
#define EFLAGS_NT         (1<<14) // Nested Task
// next bit reserved and must be zero 
#define EFLAGS_RF         (1<<16) // Resume Flag
#define EFLAGS_VM         (1<<17) // V8086 mode
#define EFLAGS_AC         (1<<18) // Alignment Check
#define EFLAGS_VIF        (1<<19) // Virtual interrupt flag
#define EFLAGS_VIP        (1<<20) // Virtual interrupt pending
#define EFLAGS_ID         (1<<21) // identification flag
// remaining bits reserved and must be zero 

// Helpers for EFLAGS
#define IOPL(r) ( ( (r) & (EFLAGS_IOPL_LO | EFLAGS_IOPL_HI)) >> 12 )



// CR0 register

#define CR0_PE            (1<<0)  // protection enabled (protected mode)
#define CR0_MP            (1<<1)  // monitor coprocessor exists?
#define CR0_EM            (1<<2)  // Emulation (set = no FPU)
#define CR0_TS            (1<<3)  // Task switched (enable = avoid FPU/SSE context save on HW task switch)
#define CR0_ET            (1<<4)  // extension type (FPU on 386/486, reserved afterward)
#define CR0_NE            (1<<5)  // numeric error enable (FPU) (PC-style = disabled)
// next group of bits are reserved
#define CR0_WP            (1<<16) // write protect (disallow supervisor access to user pages)
// next bit is reserved
#define CR0_AM            (1<<17) // alignment mask
// next group of bits are reserved
#define CR0_NW            (1<<29) // not write through
#define CR0_CD            (1<<30) // cache disable
#define CR0_PG            (1<<31) // paging enable

// CR1
//
// All of CR1 is reserved


// CR2
//
// CR2 is the linear address of a page fault (output only)
// only useful in a page fault context
//

// CR3
//
// CR3 is the page table base register and friends
//
#define CR3_PWT           (1<<3)  // page-level writes transparent 
#define CR3_PCD           (1<<4)  // page-level cache disable  (avoid caching the first level page table)

#define CR3_PAGE_DIRECTORY_BASE_ADDRESS(reg) ((reg)&0xfffff000)

// CR4
//

#define CR4_VME           (1<<0)  // V8086 mode extensions (set = inter/except handling extensions on)
#define CR4_PVI           (1<<1)  // Protected mode virtual interrupts (VIF)
#define CR4_TSD           (1<<2)  // Time stamp disable (disallow rdstc for ring>0)
#define CR4_DE            (1<<3)  // Debugging extensions (debug registers)
#define CR4_PSE           (1<<4)  // Page size extensions (allow 4 MB pages)
#define CR4_PAE           (1<<5)  // Physical Address Extensions (36+ bit addressing (PAE + IA32e)
#define CR4_MCE           (1<<6)  // Machine Check enable (allow generation of MC exception)
#define CR4_PGE           (1<<7)  // Page global enable (allow global pages)
#define CR4_PCE           (1<<8)  // Performance counter enable (allow access to any ring)
#define CR4_OSFXSR        (1<<9)  // OS Support for FXSAVE/RSTOR (fast FPU/SSE context instructions)
#define CR4_OSMMEXCEPT    (1<<10)  // OS Support for unmasked SIMD FP exceptions (SSE)
// next two bits reserved and set to zero
#define CR4_VMXE          (1<<13)  // Allow the VMX instruction
// rest of the bits reserved and set to zero






#endif
