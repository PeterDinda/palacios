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
	movl	4(%esp), %ebx;		\
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



#elif __V3_64BIT__

#endif


v3_stgi:
	stgi;
	ret;

v3_clgi:
	clgi;
	ret;
	
	
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
