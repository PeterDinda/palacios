;  -*- fundamental -*-

;; Northwestern University 
;; (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 

%ifndef SVM_ASM
%define SVM_ASM

;%include "defs.asm"
%include "vmm_symbol.asm"

SVM_ERROR equ 0xFFFFFFFF
SVM_SUCCESS equ 0x00000000

EXPORT DisableInts
EXPORT EnableInts


EXPORT exit_test

EXTERN handle_svm_exit

EXPORT launch_svm
EXPORT safe_svm_launch

EXPORT STGI
EXPORT CLGI



;; These need to be kept similar with the svm return values in svm.h
SVM_HANDLER_SUCCESS  equ 0x00
SVM_HANDLER_ERROR equ  0x1
SVM_HANDLER_HALT equ 0x2

[BITS 32]


; Save and restore registers needed by SVM
%macro Save_SVM_Registers 1
	push	eax
	mov	eax, dword %1
	mov	[eax], edi
	mov	[eax + 8], esi
	mov	[eax + 16], ebp
	mov	[eax + 24], dword 0         	;; esp
	mov	[eax + 32], ebx
	mov	[eax + 40], edx
	mov	[eax + 48], ecx

	push	ebx
	mov	ebx, [esp + 4]
	mov	[eax + 56], ebx		;; eax
	pop	ebx

	pop	eax
%endmacro


%macro Restore_SVM_Registers 1
	push	eax
	mov	eax, dword %1
	mov	edi, [eax]
	mov	esi, [eax + 8]
	mov	ebp, [eax + 16]
;;	mov	esp, [eax + 24]
	mov	ebx, [eax + 32]
	mov	edx, [eax + 40]
	mov	ecx, [eax + 48]
;;	mov	eax, [eax + 56]
	pop	eax
%endmacro

%macro vmrun 0
	db	00fh, 001h, 0d8h
%endmacro

%macro vmsave 0
	db	00fh, 001h, 0dbh
%endmacro

%macro vmload 0
	db	00fh, 001h, 0dah
%endmacro

%macro stgi 0
	db	00fh, 001h, 0dch
%endmacro

%macro clgi 0
	db	00fh, 001h, 0ddh
%endmacro

;VMRUN  equ db 0Fh, 01h, D8h
;VMLOAD equ db 0x0F,0x01,0xDA
;VMSAVE equ db 0x0F,0x01,0xDB
;STGI   equ db 0x0F,0x01,0xDC
;CLGI   equ db 0x0F,0x01,0xDD


align 8
DisableInts:
	cli
	ret

align 8
EnableInts:
	sti
	ret


align 8
CLGI:
	clgi
	ret

align 8
STGI:
	stgi
	ret



; I think its safe to say that there are some pretty serious register issues...
align 8
launch_svm:
	push 	ebp
	mov	ebp, esp
	pusha
	
	mov	eax, [ebp + 8]
	vmrun
;	db	00fh, 001h, 0d8h
	popa
	pop	ebp
	ret




exit_test: 
	mov	cr4, eax
	ret


;; Need to check this..
;; save_svm_launch(rax, struct guest_gprs * regs)
align 8
safe_svm_launch:
	push	ebp
	mov	ebp, esp
	pushf
	push    fs
	push    gs
	pusha   				;; Save Host state


	push 	dword [ebp + 12]  		;; pointer to the guest GPR save area
	push	dword [ebp + 8]   		;; pointer to the VMCB pointer

;;	mov	eax, [esp + 4]    		;; mov guest GPR pointer to eax

	;; this is plus 8 because we push eax in the macro
	Restore_SVM_Registers [esp + 8] 	;; Restore Guest GPR state
	pop	eax    		           	;; pop VMCB pointer into eax

	vmload
	vmrun
	vmsave

;;	pop	eax		  		;; pop Guest GPR pointer into eax
	;; this is plus 4 because we push eax in the macro NEED TO CHANGE
	Save_SVM_Registers  [esp+4]    		;; save guest GPRs
	
	add	esp, 4				;; skip past the gpr ptr
	
	popa			  		;; Restore Host state
	pop     gs
	pop     fs
	popf
	pop 	ebp
	ret



%endif


