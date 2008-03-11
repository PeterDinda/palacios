;  -*- fundamental -*-


%ifndef SVM_ASM
%define SVM_ASM

%include "defs.asm"
%include "symbol.asm"


EXPORT GetGDTR
EXPORT GetIDTR



EXTERN handle_svm_exit

EXPORT launch_svm


[BITS 32]

%macro vmrun 0
	db	00fh, 001h, 0d8h
%endmacro


;VMRUN  equ db 0Fh, 01h, D8h
;VMLOAD equ db 0x0F,0x01,0xDA
;VMSAVE equ db 0x0F,0x01,0xDB
;STGI   equ db 0x0F,0x01,0xDC
;CLGI   equ db 0x0F,0x01,0xDD




GetGDTR:
	push	ebp
	mov	ebp, esp
	pusha	
	mov	ebx, [ebp + 8]
	sgdt	[ebx]
	
	popa
	pop	ebp
	ret



GetIDTR:
	push	ebp
	mov	ebp, esp
	pusha	
	mov	ebx, [ebp + 8]
	sgdt	[ebx]
	
	popa
	pop	ebp
	ret



; I think its safe to say that there are some pretty serious register issues...
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



; eventual svm_launch
;   pusha
;   pushf
;
; .vmm_loop
;	vmrun
;	push guest GPRs
;	call handle_svm_exit
;	jz .vmm_loop
;  popf
;  popa
;  ret
;
;
;



;; Need to check this..
;; Since RAX/EAX is saved in the VMCB, we should probably just 
;;      do our own replacement for pusha/popa that excludes [e|r]ax
safe_svm_launch:
	push	ebp
	mov	ebp, esp
	pushf
	pusha

.vmm_loop:
	mov	eax, [ebp + 8]
	vmrun
	pusha
	call 	handle_svm_exit
	and 	eax, eax
	popa			;; restore the guest GPRs, (DOES THIS AFFECT E/RFLAGS?)
	jz	.vmm_loop

	;; HOW DO WE GET THE RETURN VALUE OF HANDLE_SVM_EXIT BACK TO THE CALLER
	popf
	popa
	pop	ebp
	ret


%endif


