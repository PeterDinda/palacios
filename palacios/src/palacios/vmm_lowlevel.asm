; -*- fundamental -*-


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