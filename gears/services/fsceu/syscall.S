/* Kyle C. Hale 2011 */

#include "syscall_decode.h"

.text 

/* Because SYSCALL doesn't put a kernel stack in place for us, we have to jump
 * through some hoops. Linux uses the nifty swapgs instruction to pull
 * a pointer to its data structures and replace it with the user gs (hence the
 * name). The problem is that the kernel stack is at a fixed offset from the
 * kernel gs, but in this module we don't have access to that offset (unless we
 * can maybe find it through a symbol lookup, but I wanted to keep things
 * compact). So, this module allocates 2 pages to use as a personal kernel stack.
 * This should be enough because interrupts are off and since the code is small,
 * I only expect a few page faults.
 */

/* You might be wondering, "he said interrupts are off, but I don't see a cli!"
 * Well, it's because Linux sets the SFMask MSR such that when SYSCALL
 * is invoked (how we got here), the IF flag is cleared. The linux SYSCALL
 * entry point later enables them. We won't bother. It's just asking for trouble.
 */

ENTRY(syscall_stub)
    pushq %rdi;  /* this is bad, shouldn't be using user-stack, any ideas? */
    movq state_save_area, %rdi;
    popq (%rdi);
    pushq SYSCALL_ENTRY_OFFSET(%rdi);
    SAVE_ALL 
    leaq SYSCALL_ENTRY_OFFSET(%rdi), %rsp; /* create our own little kernel stack*/

    movq syscall_map, %rsi;   
    leaq (%rsi,%rax,1), %rsi;
    cmpb $0x0, (%rsi);
    je sysentry;
    mov $SYSCALL_DISPATCH_HCALL, %eax;
    vmmcall;

sysentry:
    RESTORE_ALL
    movq (%rdi), %rdi;
    retq;
