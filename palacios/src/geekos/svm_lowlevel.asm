;  -*- fundamental -*-


%ifndef SVM_ASM
%define SVM_ASM

%include "defs.asm"
%include "symbol.asm"



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

%endif




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