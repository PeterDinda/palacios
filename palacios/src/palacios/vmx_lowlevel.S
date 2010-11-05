#;  -*- fundamental -*-

#define VMX_SUCCESS         0
#define VMX_FAIL_INVALID    1
#define VMX_FAIL_VALID      2
#define VMM_FAILURE         3

#define VMCS_HOST_RSP       0x00006C14
#define VMCS_HOST_RIP       0x00006C16

#if defined(__V3_64BIT__)

#define save_registers(location) 	\
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
	movq	8(%rsp), %rbx;		\
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
	
#define restore_registers(location) \
	mov	location, %rax;		\
	mov	(%rax), %rdi;		\
	mov	8(%rax), %rsi;		\
	mov	16(%rax), %rbp;		\
	mov	32(%rax), %rbx;		\
	mov	40(%rax), %rdx;		\
	mov 48(%rax), %rcx;		\
					\
	mov	64(%rax), %r8;		\
	mov	72(%rax), %r9;		\
	mov	80(%rax), %r10;		\
	mov	88(%rax), %r11;		\
	mov	96(%rax), %r12;		\
	mov	104(%rax), %r13;	\
	mov	112(%rax), %r14;	\
	mov	120(%rax), %r15;	\
	pushq %rbx;                     \
	movq 56(%rax), %rbx;            \
	movq %rbx, %rax;                \
	popq %rbx;

#define save_ctrl_regs(location)  \
    pushq %rax;              \
    pushq %rbx;              \
    movq location, %rax;     \
    movq %cr2, %rbx;         \
    movq %rbx, 8(%rax);      \
    popq %rbx;               \
    popq %rax

#define restore_ctrl_regs(location)  \
    pushq %rax;              \
    pushq %rbx;              \
    movq location, %rax;     \
    movq 8(%rax), %rbx;      \
    movq %rbx, %cr2;         \
    popq %rbx;               \
    popq %rax

#define PUSHA    \
    push %rax;   \
    push %rbx;   \
    push %rcx;   \
    push %rdx;   \
    push %rbp;   \
    push %rdi;   \
    push %rsi;   \
    push %r8 ;   \
    push %r9 ;   \
    push %r10;   \
    push %r11;   \
    push %r12;   \
    push %r13;   \
    push %r14;   \
    push %r15;   

#define POPA     \
    pop %r15;    \
    pop %r14;    \
    pop %r13;    \
    pop %r12;    \
    pop %r11;    \
    pop %r10;    \
    pop %r9 ;    \
    pop %r8 ;    \
    pop %rsi;    \
    pop %rdi;    \
    pop %rbp;    \
    pop %rdx;    \
    pop %rcx;    \
    pop %rbx;    \
    pop %rax;    


#define PRE_LAUNCH(return_target)	\
    pushf;				\
    PUSHA;				\
    pushq %rdi;				\
    pushq %rdx;				\
    					\
    movq %rsp, %rax;			\
    movq $VMCS_HOST_RSP, %rbx;		\
    vmwrite %rax, %rbx;			\
    jz .Lfail_valid;			\
    jc .Lfail_invalid;			\
					\
    movq return_target, %rax;		\
    movq $VMCS_HOST_RIP, %rbx;		\
    vmwrite %rax, %rbx;			\
    jz .Lfail_valid;			\
    jc .Lfail_invalid;			\
					\
    restore_ctrl_regs(%rdx);		\
    restore_registers(%rdi);




.align 8
.globl v3_vmx_resume
// vm_regs = %rdi, guest_info * = %rsi, ctrl_regs = %rdx
v3_vmx_resume:

    PRE_LAUNCH($vmx_resume_ret);

    vmresume

vmx_resume_ret:
    jz .Lfail_valid
    jc .Lfail_invalid
    jmp .Lnormal_exit


.align 8
.globl v3_vmx_launch
// vm_regs = %rdi, guest_info * = %rsi, ctrl_regs = %rdx
v3_vmx_launch:

    PRE_LAUNCH($vmx_launch_ret);

    vmlaunch

vmx_launch_ret:
    jz .Lfail_valid
    jc .Lfail_invalid
    jmp .Lnormal_exit




.Lfail_valid:
    addq $16, %rsp
    POPA
    popf
    movq $VMX_FAIL_VALID, %rax
    jmp .Lreturn

.Lfail_invalid:
    addq $16, %rsp
    POPA
    popf
    movq $VMX_FAIL_INVALID, %rax
    jmp .Lreturn

.Lvmm_failure:
    addq $24, %rsp
    movq $VMM_FAILURE, %rax
    jmp .Lreturn


.Lnormal_exit:
    save_registers(16(%rsp));
    save_ctrl_regs(16(%rsp));
    addq $16, %rsp
    POPA
    popf
    xorq %rax, %rax
    jmp .Lreturn


.Lreturn:
    ret
    
#else

#define save_registers(location) 	\
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
	

#define restore_registers(location) \
	pushl	%eax;			\
	movl	location, %eax;		\
	movl	(%eax), %edi;		\
	movl	8(%eax), %esi;		\
	movl	16(%eax), %ebp;		\
	movl	32(%eax), %ebx;		\
	movl	40(%eax), %edx;		\
	movl 	48(%eax), %ecx;		\
	popl	%eax;
 

#define save_ctrl_regs(location)  \
    push %eax;              \
    push %ebx;              \
    movl location, %eax;     \
    movl %cr2, %ebx;         \
    movl %ebx, 8(%eax);      \
    popl %ebx;               \
    popl %eax

#define restore_ctrl_regs(location)  \
    push %eax;              \
    push %ebx;              \
    movl location, %eax;     \
    movl 8(%eax), %ebx;      \
    movl %ebx, %cr2;         \
    popl %ebx;               \
    popl %eax

#define PUSHA    \
    push %eax;   \
    push %ebx;   \
    push %ecx;   \
    push %edx;   \
    push %ebp;   \
    push %edi;   \
    push %esi;

#define POPA     \
    pop %esi;    \
    pop %edi;    \
    pop %ebp;    \
    pop %edx;    \
    pop %ecx;    \
    pop %ebx;    \
    pop %eax;


#define PRE_LAUNCH(return_target)	\
    push %ebp;				\
    movl %esp, %ebp; 			\
    pushf;				\
    PUSHA;				\
  					\
    movl 8(%ebp),  %edi;		\
    movl 12(%ebp), %esi; 		\
    movl 16(%ebp), %edx;		\
    					\
    movl %esp, %eax;			\
    movl $VMCS_HOST_RSP, %ebx;		\
    vmwrite %eax, %ebx;			\
    jz .Lfail_valid;			\
    jc .Lfail_invalid;			\
					\
    movl return_target, %eax;		\
    movl $VMCS_HOST_RIP, %ebx;		\
    vmwrite %eax, %ebx;			\
    jz .Lfail_valid;			\
    jc .Lfail_invalid;			\
					\
    restore_ctrl_regs(%edx);		\
    restore_registers(%edi);


// 32 bit GCC passes arguments via stack

.align 4
.globl v3_vmx_resume
v3_vmx_resume:

    PRE_LAUNCH($vmx_resume_ret);

    vmresume

vmx_resume_ret:
    jz .Lfail_valid
    jc .Lfail_invalid
    jmp .Lnormal_exit

.align 4
.globl v3_vmx_launch
// vm_regs = %edi
v3_vmx_launch:

    PRE_LAUNCH($vmx_launch_ret);

    vmlaunch

vmx_launch_ret:
    jz .Lfail_valid
    jc .Lfail_invalid
    jmp .Lnormal_exit

.Lfail_valid:
    addl $8, %esp
    POPA
    popf
    movl $VMX_FAIL_VALID, %eax
    jmp .Lreturn

.Lfail_invalid:
    addl $8, %esp
    POPA
    popf
    movl $VMX_FAIL_INVALID, %eax
    jmp .Lreturn

.Lvmm_failure:
    addl $12, %esp
    movl $VMM_FAILURE, %eax
    jmp .Lreturn


.Lnormal_exit:
    save_registers(8(%esp));
    save_ctrl_regs(8(%esp));
    addl $8, %esp
    POPA
    popf
    xorl %eax, %eax
    jmp .Lreturn

.Lreturn:
    pop  %ebp;
    ret

#endif

