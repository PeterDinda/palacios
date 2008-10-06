# -*- fundamental -*-
# This file is part of the Palacios Virtual Machine Monitor developed
# by the V3VEE Project with funding from the United States National 
# Science Foundation and the Department of Energy.  
#
# The V3VEE Project is a joint project between Northwestern University
# and the University of New Mexico.  You can find out more at 
# http://www.v3vee.org
#
# Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
# Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
# All rights reserved.
#
# Author: Jack Lange <jarusl@cs.northwestern.edu>
#
# This is free software.  You are permitted to use,
# redistribute, and modify it as specified in the file "V3VEE_LICENSE".


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
	
	
	
	
	
	
	
	
	
	
	
	
