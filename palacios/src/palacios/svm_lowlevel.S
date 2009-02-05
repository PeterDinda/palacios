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
	pushq	%rax; 			\
	movq	location, %rax;		\
	movq	%rdi, (%rax);		\
	movq	%rsi, 8(%rax);		\
	movq	%rbp, 16(%rax);		\
	movq	$0, 24(%rax);		\
	movq 	%rbx, 32(%rax);		\
	movq 	%rdx, 40(%rax);		\
	movq 	%rcx, 48(%rax);		\
	pushq	%rbx;			\
	movq	16(%rsp), %rbx;		\
	movq	%rbx, 56(%rax);		\
	popq	%rbx;			\
					\
	movq	%r8, 64(%rax);		\
	movq	%r9, 72(%rax);		\
	movq	%r10, 80(%rax);		\
	movq	%r11, 88(%rax);		\
	movq	%r12, 96(%rax);		\
	movq	%r13, 104(%rax);	\
	movq	%r14, 112(%rax);	\
	movq	%r15, 120(%rax);	\
	popq 	%rax;			
	

#define Restore_SVM_Registers(location) \
	push	%rax;			\
	mov	location, %rax;		\
	mov	(%rax), %rdi;		\
	mov	8(%rax), %rsi;		\
	mov	16(%rax), %rbp;		\
	mov	32(%rax), %rbx;		\
	mov	40(%rax), %rdx;		\
	mov 	48(%rax), %rcx;		\
					\
	mov	64(%rax), %r8;		\
	mov	72(%rax), %r9;		\
	mov	80(%rax), %r10;		\
	mov	88(%rax), %r11;		\
	mov	96(%rax), %r12;		\
	mov	104(%rax), %r13;	\
	mov	112(%rax), %r14;	\
	mov	120(%rax), %r15;	\
	pop	%rax;




#define PUSHA				\
	pushq %rbp;			\
	pushq %rbx;			\
	pushq %r8;			\
	pushq %r9;			\
	pushq %r10;			\
	pushq %r11;			\
	pushq %r12;			\
	pushq %r13;			\
	pushq %r14;			\
	pushq %r15;			


#define POPA				\
	popq %r15;			\
	popq %r14;			\
	popq %r13;			\
	popq %r12;			\
	popq %r11;			\
	popq %r10;			\
	popq %r9;			\
	popq %r8;			\
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
	
	
