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


; CPUID functions
EXPORT cpuid_ecx
EXPORT cpuid_eax
EXPORT cpuid_edx

; Utility Functions
EXPORT Set_MSR
EXPORT Get_MSR



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


;
; cpuid_edx - return the edx register from cpuid
;
align 8
cpuid_edx:
	push	ebp
	mov	ebp, esp
	push	edx
	push	ecx
	push 	ebx

	mov 	eax, [ebp + 8]
	cpuid
	mov 	eax, edx

	pop	ebx
	pop	ecx
	pop	edx
	pop 	ebp
	ret


;
; cpuid_ecx - return the ecx register from cpuid
;
align 8
cpuid_ecx:
	push	ebp
	mov	ebp, esp
	push	edx
	push	ecx
	push 	ebx

	mov 	eax, [ebp + 8]
	cpuid
	mov 	eax, ecx

	pop	ebx
	pop	ecx
	pop	edx
	pop 	ebp
	ret

;
; cpuid_eax - return the eax register from cpuid
;
align 8
cpuid_eax:
	push	ebp
	mov	ebp, esp
	push	edx
	push	ecx
	push	ebx

	mov 	eax, [esp+4]
	cpuid

	pop	ebx
	pop	ecx
	pop	edx
	pop	ebp
	ret

;
; Set_MSR  - Set the value of a given MSR
;
align 8
Set_MSR:
	push	ebp
	mov 	ebp, esp
	pusha
	mov	eax, [ebp+16]
	mov	edx, [ebp+12]
	mov	ecx, [ebp+8]
	wrmsr
	popa
	pop	ebp
	ret



;
; Get_MSR  -  Get the value of a given MSR
; void Get_MSR(int MSR, void * high_byte, void * low_byte);
;
align 8
Get_MSR:
	push	ebp
	mov 	ebp, esp
	pusha
	mov	ecx, [ebp+8]
	rdmsr
	mov 	ebx, [ebp+12]
	mov	[ebx], edx
	mov 	ebx, [ebp+16]
	mov	[ebx], eax
	popa
	pop	ebp
	ret





%endif
