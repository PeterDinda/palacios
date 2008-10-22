#;  -*- fundamental -*-


.text
.align 4

.globl v3_svm_launch
.globl v3_stgi
.globl v3_clgi

#define SVM_ERROR .dword 0xffffffff
#define SVM_SUCCESS .dword 0x00000000

#define vmrun .byte 0x0f,0x01,0xd8
#define vmload .byte 0x0F,0x01,0xDA
#define vmsave .byte 0x0F,0x01,0xDB
#define stgi   .byte 0x0F,0x01,0xDC
#define clgi   .byte 0x0F,0x01,0xDD


#ifdef __V3_32BIT__

#define Save_SVM_Registers(location) 	\
	pushl	%eax; 			\
	movl	location, %eax;		\
	movl	%edi, (%eax);		\
	movl	%esi, 8(%eax);		\
	movl	%ebp, 16(%eax);		\
	movl	$0, 24(%eax);		\
	movl 	%ebx, 32(%eax);		\
	movl 	%edx, 40(%eax);		\
	movl 	%ecx, 48(%eax);		\
	pushl	%ebx;			\
	movl	8(%esp), %ebx;		\
	movl	%ebx, 56(%eax);		\
	popl	%ebx;			\
	popl 	%eax;			
	

#define Restore_SVM_Registers(location) \
	pushl	%eax;			\
	movl	location, %eax;		\
	movl	(%eax), %edi;		\
	movl	8(%eax), %esi;		\
	movl	16(%eax), %ebp;		\
	movl	32(%eax), %ebx;		\
	movl	40(%eax), %edx;		\
	movl 	48(%eax), %ecx;		\
	popl	%eax;


v3_svm_launch:
	push	%ebp;
	movl	%esp, %ebp;
	pushf;
	push	%fs;
	push	%gs;
	pusha;

	pushl	12(%ebp);
	pushl 	8(%ebp);

	Restore_SVM_Registers(8(%esp));
	popl	%eax;

	vmload;
	vmrun;
	vmsave;

	Save_SVM_Registers(4(%esp));

	addl	$4, %esp;

	popa;
	pop	%gs;
	pop	%fs;
	popf;
	pop	%ebp;
	ret



#elif __V3_64BIT__

#define Save_SVM_Registers(location) 	\
	push	%rax; 			\
	mov	location, %rax;		\
	mov	%rdi, (%rax);		\
	mov	%rsi, 8(%rax);		\
	mov	%rbp, 16(%rax);		\
	movq	$0, 24(%rax);		\
	mov 	%rbx, 32(%rax);		\
	mov 	%rdx, 40(%rax);		\
	mov 	%rcx, 48(%rax);		\
	push	%rbx;			\
	mov	16(%rsp), %rbx;		\
	mov	%rbx, 56(%rax);		\
	pop	%rbx;			\
	pop 	%rax;			
	

#define Restore_SVM_Registers(location) \
	push	%rax;			\
	mov	location, %rax;		\
	mov	(%rax), %rdi;		\
	mov	8(%rax), %rsi;		\
	mov	16(%rax), %rbp;		\
	mov	32(%rax), %rbx;		\
	mov	40(%rax), %rdx;		\
	mov 	48(%rax), %rcx;		\
	pop	%rax;




#define PUSHA				\
	pushq %rbp;			\
	pushq %rbx;			\
	pushq %r12;			\
	pushq %r13;			\
	pushq %r14;			\
	pushq %r15;			


#define POPA				\
	popq %r15;			\
	popq %r14;			\
	popq %r13;			\
	popq %r12;			\
	popq %rbx;			\
	popq %rbp;			

// VMCB => RDI
// vm_regs => RSI

v3_svm_launch:
	pushf;
	push	%fs;
	push	%gs;
	PUSHA


	pushq %rsi

	movq	%rdi, %rax
	Restore_SVM_Registers(%rsi);




	vmload;
	vmrun;
	vmsave;


	
	Save_SVM_Registers(8(%rsp));

	addq $8, %rsp

	POPA
	pop	%gs;
	pop	%fs;
	popf;
	ret


#endif


v3_stgi:
	stgi;
	ret;

v3_clgi:
	clgi;
	ret;
	
	
