
#define VMX_SUCCESS         0
#define VMX_FAIL_INVALID    1
#define VMX_FAIL_VALID      2

#define VMCS_HOST_RSP       0x00006C14

#if defined(__V3_64BIT__)

#define r(reg) %r##reg

#define PUSHA    \
    push %rax;   \
    push %rbx;   \
    push %rcx;   \
    push %rdx;   \
    push %rbp;   \
    push %rdi;   \
    push %rsi;   \
    push %r8 ;   \
    push %r9 ;   \
    push %r10;   \
    push %r11;   \
    push %r12;   \
    push %r13;   \
    push %r14;   \
    push %r15;   

#define POPA     \
    pop %r15;    \
    pop %r14;    \
    pop %r13;    \
    pop %r12;    \
    pop %r11;    \
    pop %r10;    \
    pop %r9 ;    \
    pop %r8 ;    \
    pop %rsi;    \
    pop %rdi;    \
    pop %rbp;    \
    pop %rdx;    \
    pop %rcx;    \
    pop %rbx;    \
    pop %rax;    
#else

#define r(reg) %e##reg
  
#define PUSHA    \
    push %eax;   \
    push %ebx;   \
    push %ecx;   \
    push %edx;   \
    push %ebp;   \
    push %edi;   \
    push %esi;

#define POPA     \
    pop %esi;    \
    pop %edi;    \
    pop %ebp;    \
    pop %edx;    \
    pop %ecx;    \
    pop %ebx;    \
    pop %eax;

#endif

.align 8
.globl v3_vmx_exit_handler
v3_vmx_exit_handler:
    PUSHA
    call v3_vmx_handle_exit
    POPA

v3_vmx_vmresume:
    vmresume
    sti
    jz .Lfail_valid
    jc .Lfail_invalid
    jmp .Lreturn

.globl v3_vmx_vmlaunch
v3_vmx_vmlaunch:
    cli 
    pushf
    PUSHA

    mov r(sp), r(ax)
    mov $VMCS_HOST_RSP, r(bx)
    vmwrite r(bx), r(ax)
    jz .Lfail_valid
    jc .Lfail_invalid

    vmlaunch
    sti
    jz .Lfail_valid
    jc .Lfail_invalid
    jmp .Lreturn

.Lfail_valid:
    mov $VMX_FAIL_VALID, r(ax)
    jmp .Lreturn

.Lfail_invalid:
    mov $VMX_FAIL_INVALID, r(ax)
    jmp .Lreturn

.Lreturn:
    POPA
    popf
    ret




