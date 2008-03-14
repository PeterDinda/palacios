;  -*- fundamental -*-


%ifndef SVM_ASM
%define SVM_ASM

%include "defs.asm"
%include "symbol.asm"


EXPORT DisableInts

EXPORT GetGDTR
EXPORT GetIDTR
EXPORT GetTR

EXPORT exit_test

EXTERN handle_svm_exit

EXPORT launch_svm
EXPORT safe_svm_launch


;; These need to be kept similar with the svm return values in svm.h
SVM_HANDLER_SUCCESS  equ 0x00
SVM_HANDLER_ERROR equ  0x1
SVM_HANDLER_HALT equ 0x2

[BITS 32]


; Save and restore registers needed by SVM
%macro Save_SVM_Registers 1
	mov	[%1], ebx
	mov	[%1 + 8], ecx
	mov	[%1 + 16], edx
	mov	[%1 + 24], esi
	mov	[%1 + 32], edi
	mov	[%1 + 40], ebp
%endmacro


%macro Restore_SVM_Registers 1
	mov	ebx, [%1]
	mov	ecx, [%1 + 8]
	mov	edx, [%1 + 16]
	mov	esi, [%1 + 24]
	mov	edi, [%1 + 32]
	mov	ebp, [%1 + 40]
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
	pusha   		;; Save Host state


	push 	dword [ebp + 12]  ;; pointer to the guest GPR save area
	push	dword [ebp + 8]   ;; pointer to the VMCB pointer

	mov	eax, [esp + 4]    ;; mov guest GPR pointer to eax

	Restore_SVM_Registers eax ;; Restore Guest GPR state
	pop	eax               ;; pop VMCB pointer into eax

	vmload
	vmrun
	vmsave

	pop	eax		  ;; pop Guest GPR pointer into eax
	Save_SVM_Registers eax    ;; save guest GPRs

	popa			  ;; Restore Host state
	popf
	pop 	ebp
	ret



;;align 8
;;safe_svm_launch:
;;	push	ebp
;;	mov	ebp, esp
;;	pushf
;;	pusha
;;
;.vmm_loop:
;	mov	eax, [ebp + 8]
;	vmrun
;	Save_SVM_Registers
;
;	call 	handle_svm_exit
;
;	mov	[ebp + 12], eax
;
;	and 	eax, eax
;
;	Restore_SVM_Registers
;
;	jz	.vmm_loop
;
;	popa
;	popf
;	pop	ebp
;	ret


%endif


