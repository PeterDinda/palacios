#ifndef __SYSCALL_DECODE__
#define __SYSCALL_DECODE__

// hypercall numbers
#define SYSCALL_DISPATCH_HCALL 0x5CA11
#define SYSCALL_SETUP_HCALL    0x5CA12
#define SYSCALL_CLEANUP_HCALL  0x5CA13

#define NUM_SYSCALLS 256

#define NUM_SAVE_REGS 16
#define SYSCALL_ENTRY_OFFSET   (NUM_SAVE_REGS*8)

#ifdef __ASSEMBLY__

#define SAVE_ALL            \
    movq %rsi, 8(%rdi);     \
    movq %rbp, 16(%rdi);    \
    movq %rsp, 24(%rdi);    \
    movq %rbx, 32(%rdi);    \
    movq %rdx, 40(%rdi);    \
    movq %rcx, 48(%rdi);    \
    movq %rax, 56(%rdi);    \
    movq %r8,  64(%rdi);    \
    movq %r9,  72(%rdi);    \
    movq %r10, 80(%rdi);    \
    movq %r11, 88(%rdi);    \
    movq %r12, 96(%rdi);    \
    movq %r13, 104(%rdi);   \
    movq %r14, 112(%rdi);   \
    movq %r15, 120(%rdi);   \

#define RESTORE_ALL         \
    movq 8(%rdi),  %rsi;    \
    movq 16(%rdi), %rbp;    \
    movq 24(%rdi), %rsp;    \
    movq 32(%rdi), %rbx;    \
    movq 40(%rdi), %rdx;    \
    movq 48(%rdi), %rcx;    \
    movq 56(%rdi), %rax;    \
    movq 64(%rdi), %r8;     \
    movq 72(%rdi), %r9;     \
    movq 80(%rdi), %r10;    \
    movq 88(%rdi), %r11;    \
    movq 96(%rdi), %r12;    \
    movq 104(%rdi),%r13;    \
    movq 112(%rdi),%r14;    \
    movq 120(%rdi),%r15;    \


/* align on word boundary with nops */
#define ALIGN  .align 8, 0x90

#ifndef ENTRY

#define ENTRY(name) \
    .global name;   \
    ALIGN;          \
    name:           \

#endif


#else

#include <linux/types.h>

#endif 
#endif
