/*
 * GeekOS interrupt handling data structures and functions
 * Copyright (c) 2001, David H. Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.1 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

/*
 * This module describes the C interface which must be implemented
 * by interrupt handlers, and has the initialization function
 * for the interrupt system as a whole.
 */

#ifndef GEEKOS_INT_H
#define GEEKOS_INT_H

#include <geekos/kassert.h>
#include <geekos/ktypes.h>
#include <geekos/defs.h>

/*
 * This struct reflects the contents of the stack when
 * a C interrupt handler function is called.
 * It must be kept up to date with the code in "lowlevel.asm".
 */
struct Interrupt_State {
    /*
     * The register contents at the time of the exception.
     * We save these explicitly.
     */
    uint_t gs;
    uint_t fs;
    uint_t es;
    uint_t ds;
    uint_t ebp;
    uint_t edi;
    uint_t esi;
    uint_t edx;
    uint_t ecx;
    uint_t ebx;
    uint_t eax;

    /*
     * We explicitly push the interrupt number.
     * This makes it easy for the handler function to determine
     * which interrupt occurred.
     */
    uint_t intNum;

    /*
     * This may be pushed by the processor; if not, we push
     * a dummy error code, so the stack layout is the same
     * for every type of interrupt.
     */
    uint_t errorCode;

    /* These are always pushed on the stack by the processor. */
    uint_t eip;
    uint_t cs;
    uint_t eflags;
};

/*
 * An interrupt that occurred in user mode.
 * If Is_User_Interrupt(state) returns true, then the
 * Interrupt_State object may be cast to this kind of struct.
 */
struct User_Interrupt_State {
    struct Interrupt_State state;
    uint_t espUser;
    uint_t ssUser;
};

static __inline__ bool Is_User_Interrupt(struct Interrupt_State *state)
{
    return (state->cs & 3) == USER_PRIVILEGE;
}


/*
 * The signature of an interrupt handler.
 */
typedef void (*Interrupt_Handler)(struct Interrupt_State* state);

/*
 * Perform all low- and high-level initialization of the
 * interrupt system.
 */
void Init_Interrupts(void);

/*
 * Query whether or not interrupts are currently enabled.
 */
bool Interrupts_Enabled(void);

/*
 * Block interrupts.
 */
static __inline__ void __Disable_Interrupts(void)
{
    __asm__ __volatile__ ("cli");
}
#define Disable_Interrupts()		\
do {					\
    KASSERT(Interrupts_Enabled());	\
    __Disable_Interrupts();		\
} while (0)

/*
 * Unblock interrupts.
 */
static __inline__ void __Enable_Interrupts(void)
{
    __asm__ __volatile__ ("sti");
}
#define Enable_Interrupts()		\
do {					\
    KASSERT(!Interrupts_Enabled());	\
    __Enable_Interrupts();		\
} while (0)

/*
 * Dump interrupt state struct to screen
 */
void Dump_Interrupt_State(struct Interrupt_State* state);

/**
 * Start interrupt-atomic region.
 * @return true if interrupts were enabled at beginning of call,
 * false otherwise.
 */
static __inline__ bool Begin_Int_Atomic(void) 
{
    bool enabled = Interrupts_Enabled();
    if (enabled)
	Disable_Interrupts();
    return enabled;
}

/**
 * End interrupt-atomic region.
 * @param iflag the value returned from the original Begin_Int_Atomic() call.
 */
static __inline__ void End_Int_Atomic(bool iflag)
{
    KASSERT(!Interrupts_Enabled());
    if (iflag) {
	/* Interrupts were originally enabled, so turn them back on */
	Enable_Interrupts();
    }
}

#define EXCEPTION_DE   0   // Divide by zero
#define EXCEPTION_DB   1   // reserved
#define EXCEPTION_NMI  2   // NMI
#define EXCEPTION_BP   3   // Breakpoint
#define EXCEPTION_OF   4   // Overflow
#define EXCEPTION_BR   5   // Bound range exceeded
#define EXCEPTION_UD   6   // Undefined opcode
#define EXCEPTION_NM   7   // Math coprocessor gone missing
#define EXCEPTION_DF   8   // Double fault
#define EXCEPTION_CO   9   // Coprocessor segment overrrun
#define EXCEPTION_TS   10  // Invalid TSS
#define EXCEPTION_NP   11  // Segment not present
#define EXCEPTION_SS   12  // Stack segment fault
#define EXCEPTION_GP   13  // General Protection fault
#define EXCEPTION_PF   14  // Page Fault
#define EXCEPTION_RES  15  // reserved
#define EXCEPTION_MF   16  // Math fault
#define EXCEPTION_AC   17  // Alignment check
#define EXCEPTION_MC   18  // Machine check
#define EXCEPTION_XF   19  // SIMD FP exception
// 20+ are reserved




#endif  /* GEEKOS_INT_H */
