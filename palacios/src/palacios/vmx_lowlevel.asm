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
;; redistribute, and modify it as specified in the file "V3VEE_LICENSE".
;;

%ifndef VMX_ASM
%define VMX_ASM


%include "defs.asm"
%include "symbol.asm"


%include "vmcs_fields.asm"

VMX_SUCCESS equ	0x00000000
VMX_FAIL_INVALID equ 0x00000001
VMX_FAIL_VALID	equ 0x00000002
VMM_ERROR	equ 0x00000003

[BITS 32]

IMPORT Do_VMM


; VMX Functions
EXPORT VMCS_READ
EXPORT VMCS_WRITE
EXPORT VMCS_CLEAR
EXPORT VMCS_LOAD
EXPORT VMCS_STORE
EXPORT Enable_VMX
EXPORT Disable_VMX
EXPORT Launch_VM
EXPORT VMCS_LAUNCH
EXPORT VMCS_RESUME
EXPORT RunVMM
EXPORT SAFE_VM_LAUNCH
EXPORT Init_VMCS_HostState
EXPORT Init_VMCS_GuestState
	

;
; VMCS_LAUNCH
;
align 8
VMCS_LAUNCH:
	vmlaunch
	jz	.error_code
	jc	.error

	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return
	ret



;
; VMCS_RESUME
;
align 8
VMCS_RESUME:
	vmresume
	jz	.error_code
	jc	.error

	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return
	ret

align 8
SAFE_VM_LAUNCH:
	pushf
	pusha
	mov	eax, HOST_RSP
	vmwrite	eax, esp
	jz 	.esp_err
	jc 	.esp_err
	jmp	.vm_cont

.esp_err
	popa
	jz 	.error_code
	jc	.error
.vm_cont
	vmlaunch
	popa
	jz	.error_code
	jc	.error	

	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return
	popf
	ret


;
; RunVMM
;
align 8
RunVMM:
	pusha
	call 	Do_VMM
	and 	eax, eax
	jnz 	.vmm_error
	jmp 	.vm_cont

.vmm_error
	popa
	popa
	mov	eax, VMM_ERROR
	jmp	.return

.vm_cont
	popa
	vmresume
	popa	; we only get here if there is an error in the vmresume
		; we restore the host state and return an error code

	jz	.error_code
	jc	.error

	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return
	popf
	ret




;
; Setup_VMCS_GuestState
; Copy all of the Guest registers into the guest state of a vmcs 
;

align 8
InitGuestSelectors:
	push	ebp
	mov	ebp, esp
	push	ebx
	push	ebx

	mov	ebx, VMCS_GUEST_ES_SELECTOR
	mov	eax, es
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	ebx, VMCS_GUEST_CS_SELECTOR
	mov	eax, cs
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	ebx, VMCS_GUEST_SS_SELECTOR
	mov	eax, ss
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	ebx, VMCS_GUEST_DS_SELECTOR
	mov	eax, ds
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	ebx, VMCS_GUEST_FS_SELECTOR
	mov	eax, fs
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	ebx, VMCS_GUEST_GS_SELECTOR
	mov	eax, gs
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	str	[esp]
	mov	eax, [esp]
	mov	ebx, VMCS_GUEST_TR_SELECTOR
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return
	pop	ebx
	pop	ebx
	pop	ebp
	ret
ret

align 8
InitGuestDescRegs:
	push	ebp
	mov	ebp, esp
	push	ebx
	sub	esp, 6


	sgdt	[esp]
	mov	eax, [esp]
	and	eax, 0xffff
	mov	ebx, GUEST_GDTR_LIMIT
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, [esp+2]
	mov	ebx, GUEST_GDTR_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error


	sidt	[esp]
	mov	eax, [esp]
	and	eax, 0xffff
	mov	ebx, GUEST_IDTR_LIMIT
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, [esp+2]
	mov	ebx, GUEST_IDTR_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error


	sldt	[esp]
	mov	eax, [esp]	
	mov	ebx, GUEST_LDTR_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error


	mov	eax, 0x00000000
	mov	ebx, GUEST_LDTR_LIMIT
	vmwrite	ebx, eax
	jz	.error_code	
	jc	.error


	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return

	add	esp, 6
	pop	ebx
	pop	ebp
	ret





align 8
InitGuestSegBases:
	push	ebp
	mov	ebp, esp
	push	ebx


	mov	eax, dword 0
	mov	ebx, GUEST_ES_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, dword 0
	mov	ebx, GUEST_CS_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, dword 0
	mov	ebx, GUEST_SS_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, dword 0
	mov	ebx, GUEST_DS_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, dword 0
	mov	ebx, GUEST_FS_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, dword 0
	mov	ebx, GUEST_GS_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

;	mov	eax, dword 0
	mov	eax, 0x000220a0
	mov	ebx, GUEST_TR_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return

	pop	ebx
	pop	ebp
	ret

align 8
InitGuestSegsAccess:
	push	ebp
	mov	ebp, esp
	push	ebx

	mov	eax, 1100000010010011b
	mov 	ebx, GUEST_ES_ACCESS
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error



	mov	eax, 1100000010011001b
;	mov	eax, 0x0000c099
	mov 	ebx, GUEST_CS_ACCESS
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

;	mov	eax, 1100000010010111b
	mov	eax, 1100000010010011b
	mov 	ebx, GUEST_SS_ACCESS
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, 1100000010010011b
	mov 	ebx, GUEST_DS_ACCESS
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error


	mov	eax, 1100000010010011b
	mov 	ebx, GUEST_FS_ACCESS
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error


	mov	eax, 1100000010010011b
	mov 	ebx, GUEST_GS_ACCESS
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, 0x10000
	mov 	ebx, GUEST_LDTR_ACCESS
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, 01000000010001011b
	mov 	ebx, GUEST_TR_ACCESS
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

; 

	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return
	pop	ebx
	pop	ebp
	ret

;; Do seg limit
align 8
InitGuestSegsLimits:
	push	ebp
	mov	ebp, esp
	push	ebx

	
;	mov	eax, 0xffffffff
	mov	eax, 0xffffffff
	mov	ebx, GUEST_ES_LIMIT
	vmwrite	ebx, eax
	jz	.error_code	
	jc	.error

;	mov	eax, 0xffffffff
	mov	eax, 0xffffffff
	mov	ebx, GUEST_CS_LIMIT
	vmwrite	ebx, eax
	jz	.error_code	
	jc	.error

;	mov	eax, 0xffffffff
	mov	eax, 0xffffffff
	mov	ebx, GUEST_SS_LIMIT
	vmwrite	ebx, eax
	jz	.error_code	
	jc	.error

;	mov	eax, 0xffffffff
	mov	eax, 0xffffffff
	mov	ebx, GUEST_DS_LIMIT
	vmwrite	ebx, eax
	jz	.error_code	
	jc	.error

;	mov	eax, 0xffffffff
	mov	eax, 0xffffffff
	mov	ebx, GUEST_FS_LIMIT
	vmwrite	ebx, eax
	jz	.error_code	
	jc	.error

;	mov	eax, 0xffffffff
	mov	eax, 0xffffffff
	mov	ebx, GUEST_GS_LIMIT
	vmwrite	ebx, eax
	jz	.error_code	
	jc	.error

;	mov	eax, 0xffffffff
	mov	eax, 0x68fff
	mov	ebx, GUEST_TR_LIMIT
	vmwrite	ebx, eax
	jz	.error_code	
	jc	.error

	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return
	pop	ebx
	pop	ebp
	ret


align 8
Init_VMCS_GuestState:
	push	ebp
	mov	ebp, esp
	push	ebx

	mov	ebx, GUEST_CR3
	mov	eax, cr3
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	call 	InitGuestSelectors
	and	eax, 0xffffffff
	jz	.selDone
	jmp	.return
.selDone

	call 	InitGuestDescRegs
	and	eax, 0xffffffff
	jz	.descRegsDone
	jmp	.return
.descRegsDone

	call 	InitGuestSegBases
	and	eax, 0xffffffff
	jz	.descSegBasesDone
	jmp	.return
.descSegBasesDone


	call 	InitGuestSegsLimits
	and	eax, 0xffffffff
	jz	.segsLimitsDone
	jmp	.return
.segsLimitsDone

	call 	InitGuestSegsAccess
	and	eax, 0xffffffff
	jz	.segsAccessDone
	jmp	.return
.segsAccessDone

	mov	ebx, GUEST_RSP
	mov	eax, esp
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	ebx, GUEST_RFLAGS
	mov	eax, dword 0x00000002
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	ebx, GUEST_DR7
	mov	eax, dword 0x00000400
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return
	pop	ebx
	pop	ebp
	ret

;
; Setup_VMCS_HostState
; Copy all of the host registers into the host state of a vmcs 
;

align 8
InitHostSelectors:
	push	ebp
	mov	ebp, esp
	push	ebx
	push	ebx

	mov	ebx, VMCS_HOST_ES_SELECTOR
	mov	eax, es
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	ebx, VMCS_HOST_CS_SELECTOR
	mov	eax, cs
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	ebx, VMCS_HOST_SS_SELECTOR;
    PrintDebug("VMX revision: 0x%p\n", (void*)vmxon_ptr);

    if (v3_enable_vmx(vmxon_ptr) == 0) {
        PrintDebug("VMX Enabled\n");
    } else {
        PrintError("VMX initialization failure\n");
        return;
    }
	


	mov	eax, ss
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	ebx, VMCS_HOST_DS_SELECTOR
	mov	eax, ds
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	ebx, VMCS_HOST_FS_SELECTOR
	mov	eax, fs
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	ebx, VMCS_HOST_GS_SELECTOR
	mov	eax, gs
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	str	[esp]
	mov	eax, [esp]
	mov	ebx, VMCS_HOST_TR_SELECTOR
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return
	pop	ebx
	pop	ebx
	pop	ebp
	ret
ret





align 8
InitHostBaseRegs:
	push	ebp
	mov	ebp, esp
	push	ebx
	sub	esp, 6

	sgdt	[esp]
	mov	eax, [esp+2]
	mov	ebx, HOST_GDTR_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	sidt	[esp]
	mov	eax, [esp+2]
	mov	ebx, HOST_IDTR_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error


	mov	eax, dword 0
	mov	ebx, HOST_FS_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, dword 0
	mov	ebx, HOST_GS_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, dword 0
	mov	ebx, HOST_TR_BASE
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return

	add	esp, 6
	pop	ebx
	pop	ebp
	ret


align 8
Init_VMCS_HostState:
	push	ebp
	mov	ebp, esp
	push	ebx
	
	mov	ebx, HOST_CR3
	mov	eax, cr3
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error


	mov	ebx, HOST_RSP
	mov	eax, esp
	vmwrite	ebx, eax
	jz	.error_code
	jc	.error

;	push 	esp
	call 	InitHostSelectors
	and	eax, 0xffffffff
	jz	.selDone
	jmp	.return
.selDone
;	push 	esp
	call 	InitHostBaseRegs
	and	eax, 0xffffffff
	jz	.baseRegsDone
	jmp	.return
.baseRegsDone


	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	mov	eax, VMX_FAIL_INVALID
	jmp	.return
.error_code
	mov	eax, VMX_FAIL_VALID
.return
	pop	ebx
	pop	ebp
	ret

;
; Launch_VM - inits a vmcs with an ip and launches it
; [eip = ebp + 8], [vmcs = ebp + 12]
; int Launch_VM(ullont_t VMCS, uint_t eip);
;
align 8
Launch_VM:
	push 	ebp
	mov 	ebp, esp
	push 	ebx
	mov 	ebx, dword 0
	vmclear	[ebp+8]
	jz	.error_code
	jc	.error
	add	ebx, dword 1
	vmptrld	[ebp+8]
	jz	.error_code
	jc	.error
	mov	eax, dword 0x0000681E
	add	ebx, dword 1
	vmwrite eax, [ebp+16]
	jz	.error_code
	jc	.error
	add 	ebx, dword 1
	vmlaunch
	jz	.error_code
	jc	.error
	mov	eax, VMX_SUCCESS
	jmp	.return
.error
	shl	ebx, 4
	mov	eax, VMX_FAIL_INVALID
	or 	eax, ebx
	jmp	.return
.error_code
	shl	ebx, 4
	mov	eax, VMX_FAIL_VALID
	or	eax, ebx
	mov	ebx, dword 0x00004400
	vmread  eax, ebx
.return
	pop	ebx
	pop	ebp

	ret


%endif
