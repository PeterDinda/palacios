[BITS 32]
[ORG 0x0]
lea eax, [ebx]
mov eax, [ebx]
mov ax, [ebx]
mov ax, [ebx +8*edi]
mov  [ebx], eax
mov  [ebx +4*edi], eax
es mov [ebx], eax
movsw
rep movsw

mov eax, ebx
popa
pusha
push eax


add eax, ebx
out 0x64, al

