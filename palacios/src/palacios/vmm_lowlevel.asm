; -*- fundamental -*-
;;
;; This file is part of the Palacios Virtual Machine Monitor developed
;; by the V3VEE Project with funding from the United States National 
;; Science Foundation and the Department of Energy.  
;;
;; The V3VEE Project is a joint project between Northwestern University
;; and the University of New Mexico.  You can find out more at 
;; http://www.v3vee.org
;;
;; Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
;; Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
;; All rights reserved.
;;
;; Author: Jack Lange <jarusl@cs.northwestern.edu>
;;
;; This is free software.  You are permitted to use,
;; redistribute, and modify it as specified in the file "V3VEE_LICENSE"
;;

%ifndef VMM_ASM
%define VMM_ASM

%include "vmm_symbol.asm"

EXPORT DisableInts
EXPORT EnableInts

EXPORT GetGDTR
EXPORT GetIDTR
EXPORT GetTR






align 8
DisableInts:
	cli
	ret


align 8
EnableInts:
	sti
	ret

align 8
GetGDTR:
	push	ebp
	mov	ebp, esp
	pusha	
	mov	ebx, [ebp + 8]
	sgdt	[ebx]
	
	popa
	pop	ebp
	ret


align 8
GetIDTR:
	push	ebp
	mov	ebp, esp
	pusha	

	mov	ebx, [ebp + 8]
	sidt	[ebx]
	
	popa
	pop	ebp
	ret



align 8
GetTR:
	push	ebp
	mov	ebp, esp
	pusha	
	mov	ebx, [ebp + 8]
	str	[ebx]
	
	popa
	pop	ebp
	ret







%endif
