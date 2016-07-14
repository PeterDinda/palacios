.section .text
	
.extern __v3_hvm_ros_signal_handler  

/* 
   This is the entry point for signal dispatch
   from the VMM.
	
   VMM invokes this stub when a user signal is
   raised and:	the relevant address space
   is active, and we are at user-level.
   It will be invoked  on exactly one core,  
   although there are  no guarantees on which one.
   
*/


#define GPR_SAVE()                                               \
	pushq %rbp              ;                                \
	pushq %rax      	;                                \
	pushq %rbx      	;                                \
	pushq %rcx      	;                                \
	pushq %rdx      	;                                \
	pushq %rsi      	;                                \
	pushq %rdi      	;                                \
	pushq %r8       	;                                \
	pushq %r9       	;                                \
	pushq %r10      	;                                \
	pushq %r11      	;                                \
	pushq %r12      	;                                \
	pushq %r13      	;                                \
	pushq %r14      	;                                \
	pushq %r15      	;                                \

#define GPR_LOAD()                       \
	popq %r15       	;                                \
	popq %r14       	;                                \
	popq %r13       	;                                \
	popq %r12       	;                                \
	popq %r11       	;                                \
	popq %r10       	;                                \
	popq %r9        	;                                \
	popq %r8        	;                                \
	popq %rdi       	;                                \
	popq %rsi       	;                                \
	popq %rdx       	;                                \
	popq %rcx       	;                                \
	popq %rbx       	;                                \
	popq %rax               ;                                \
	popq %rbp               ;          

	
/*
   The VMM puts us here with what looks like a long mode 
   interrupt dispatch, but it's from CPL 3 to CPL 3 and
   it's not done as an interrupt injection per se, so
   we can be interrupted, and deliverable interrupts
   automatically prioritize over us.

   The start of the stack (before the VMM pushes the fake
   interrupt frame is  16 byte aligned

        48bitsblank | return SS (16 bits)
	Return RSP
	Return RFLAGS
	48bitsblank | return CS (16 bits)
	Return RIP
	ERROR CODE (HVM-specific non-zero number here)  <- RSP on entry
	
	We then need simply to do this:

	save regs
	arrange 16 byte alignment at entry of call
	call the handler (if installed)
	restore regs
	iret
	
*/

#define DEBUG_ENTRY 0
	
.global __v3_hvm_ros_signal_handler_stub
__v3_hvm_ros_signal_handler_stub:   /* we are 16 byte aligned on entry 16 + 6*8 for interrupt frame */
	GPR_SAVE()                  /* push 15 words, all but rsp, now not aligned - need 1 more word  */
        subq $8, %rsp               /* make us 16 byte aligned */

#if DEBUG_ENTRY                     /* print out something if we are debugging - Note this is danerous code */
	pushq %rdi
	pushq %rax
	movabsq $printf, %rax
	movabsq $string, %rdi
	callq *%rax
	popq %rax
	popq %rdi
#endif
	movq %rsp, %rbp             /* give us a stack frame for any callee that needs it */
	movabs __v3_hvm_ros_signal_handler, %rax  /* find the user-level handler */
	testq %rax, %rax                          /* return immediately if it doesn't exist */
	jz skip_handler                           /* " */
	movq 128(%rsp), %rdi        /* error code becomes argument for user-level handler */
	callq *%rax                 /* call handelr - 16 byte aligned at call */
	jmp done
skip_handler:
done:
	addq $8, %rsp               /* get rid of stack alignment pad */
	GPR_LOAD()                  /* rbp is restored here */
	addq $8, %rsp               /* nuke the error code */
	iretq                       /* restore rip, rsp, and rflags */
	

string:
    .asciz "Got to signal handler stub\12"
