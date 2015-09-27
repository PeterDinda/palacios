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

        48bitsblank | return SS (16 bits)
	Return RSP
	Return RFLAGS
	48bitsblank | return CS (16 bits)
	Return RIP
	ERROR CODE (HVM-specific non-zero number here)  <- RSP on entry
	
	We then need simply to do this:

	save regs
	call the handler (if installed)
	restore regs
	iret
	
*/

.global __v3_hvm_ros_signal_handler_stub
__v3_hvm_ros_signal_handler_stub:
	GPR_SAVE()
	movabs __v3_hvm_ros_signal_handler, %rax
	testq %rax, %rax
	jz skip_handler
	movq 120(%rsp), %rdi /* error code becomes argument */
	callq *%rax	
skip_handler:	
	GPR_LOAD()
	addq $8, %rsp /* nuke the error code */
	iretq
	
