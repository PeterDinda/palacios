# -*- fundamental -*-

.global MyBuzzVM
MyBuzzVM:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%ebx
	inb $97, %al		
	movl	$97, %ecx
	movb	%al, %bl
	orl	$2, %ebx
	movl	%eax, %esi
.L24:
	movb	%bl, %al
	movl	%ecx, %edx
	outb    %al, %dx
	movl	$0, %edx
.L30:
	incl	%edx
	cmpl	$999999, %edx
	jle	.L30
	movl	%esi, %eax
	movl	%ecx, %edx
	outb %al, %dx
	movl	$0, %edx
.L35:
	incl	%edx
	cmpl	$999999, %edx
	jle	.L35
	jmp	.L24
	
	
	
	
	
	
	
	
	
	
	
	
